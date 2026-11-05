#!/usr/bin/env python3
"""Fast source-only release blockers. No ESP-IDF build or firmware generation.

Checks actual embedded browser JavaScript, links/routes, and source contracts.
This CANNOT replace the final ESP-IDF compile or real Waveshare testing.
"""
import ast
from html.parser import HTMLParser
from pathlib import Path
import re
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
def read(name):
    return (ROOT / name).read_text(encoding="utf-8")

network = read("main/network.c")
ui = read("main/ui.c")
weather = read("main/weather.c")
settings = read("main/settings.c")
web = read("main/web_device_html.h")
studio = read("main/web_basic_html.h")
cmake = read("main/CMakeLists.txt")
tests_passed = 0

def require(condition, message):
    global tests_passed
    if not condition:
        raise AssertionError("Preflight FAILED: " + message)
    tests_passed += 1

def embedded_html(source, symbol):
    lines = source.splitlines()
    line = next(i for i, s in enumerate(lines)
                if "static const char " + symbol + "[]" in s) + 1
    parts = []
    for value in lines[line:]:
        value = value.strip()
        if not value:
            continue
        if value == ";":
            return "".join(parts)
        require(value.startswith('"') and value.endswith('"'),
                "Malformed embedded C string in " + symbol)
        parts.append(ast.literal_eval(value))
    raise AssertionError("Missing C-string terminator: " + symbol)

class Inspector(HTMLParser):
    def __init__(self):
        super().__init__()
        self.ids = set()
        self.links = []
    def handle_starttag(self, tag, attrs):
        attrs = dict(attrs)
        if "id" in attrs:
            require(attrs["id"] not in self.ids,
                    "Duplicate HTML id " + attrs["id"])
            self.ids.add(attrs["id"])
        if tag == "a":
            self.links.append(attrs.get("href", ""))

pages = {
    "settings": embedded_html(web, "lf_device_settings_html"),
    "firmware": embedded_html(web, "lf_device_firmware_html"),
    "studio": embedded_html(studio, "lf_basic_settings_html"),
}
for name, html in pages.items():
    require("<html" in html and "</html>" in html,
            name + " HTML missing page wrapper")
    parser = Inspector()
    parser.feed(html)
    script = html.split("<script>", 1)[-1].split("</script>", 1)[0]
    require("<script>" in html and "</script>" in html, name + " missing JavaScript")
    ids_used = set(re.findall(
        r"(?:document\.getElementById|\bel)\(['\"]([^'\"]+)['\"]\)", script))
    missing = {item for item in ids_used if item not in parser.ids
               and not (item.endswith(("-", "_")) and
                        any(actual.startswith(item) for actual in parser.ids))}
    require(not missing,
            name + " JS references missing HTML IDs: " + str(missing))
    with tempfile.TemporaryDirectory() as directory:
        js_path = Path(directory) / (name + ".js")
        js_path.write_text(script, encoding="utf-8")
        result = subprocess.run(["node", "--check", str(js_path)],
                                capture_output=True, text=True)
        require(result.returncode == 0,
                name + " JavaScript syntax: " + result.stderr)
    if name != "studio":
        require("/" in parser.links and "/firmware" in parser.links,
                name + " missing Settings/Firmware navigation")

require("pin_matches" not in network and "X-LyricFlash-PIN" not in network,
        "PIN gate remains in firmware")
require("config_pin" not in studio and "lf_network_arm_ota" not in ui,
        "PIN/ARM control remains in UI")
require("HTTPD_409_CONFLICT" not in network,
        "Undefined ESP-IDF HTTP constant (use httpd_resp_set_status)")
# These are the HTTP server symbols verified against ESP-IDF v5.5.4.
allowed_httpd = {
    "HTTPD_RESP_USE_STRLEN", "HTTPD_500_INTERNAL_SERVER_ERROR",
    "HTTPD_400_BAD_REQUEST", "HTTPD_SOCK_ERR_TIMEOUT",
    "HTTPD_408_REQ_TIMEOUT", "HTTPD_403_FORBIDDEN", "HTTPD_DEFAULT_CONFIG",
}
used_httpd = set(re.findall(r"\bHTTPD_[A-Z0-9_]+\b", network))
require(not (used_httpd - allowed_httpd),
        "Unreviewed HTTPD symbols: " + str(used_httpd-allowed_httpd))
require("esp_https_ota" in cmake and "esp_https_ota_get_img_desc" in network
        and "esp_https_ota_finish" in network, "OTA API/dependency missing")
require("allowed_github_ota_url" in network and "handle_github_install" in network,
        "GitHub OTA URL authorization missing")
require('"/firmware"' in network and '"/api/install"' in network
        and '"/settings"' in network, "Expected device Web UI routes missing")
require("/api/install" in pages["firmware"]
        and "skiiirt1983.github.io/LyricFlash/beta/" in pages["firmware"] and "catalog.json" in pages["firmware"],
        "GitHub build selector not linked to install endpoint")
require("selector.disabled=!hasInstallable;" in pages["firmware"]
        and "el('install').disabled=!choice;" in pages["firmware"]
        and "const b=selectedInstall();" in pages["firmware"]
        and "if(!b)return info('Only a verified Current reinstall" in pages["firmware"],
        "OTA install must require verified Current or newer New and user confirmation")
require("lf_weather_save_config" in network and "xTaskNotifyGive(weather_task)" in weather,
        "Weather refresh on save missing")
require("weather_numeric_valid = false;" in weather and
        "view_cache.valid = false;" in weather,
        "Weather failure must clear a stale forecast")
require("Connect to the device setup AP to change Wi-Fi" not in network,
        "Wi-Fi settings are inaccessible while connected to home Wi-Fi")
require("if (schema > 0 && schema < 6)" in settings and
        "3.0f+0.605f*(float)prefs->scroll_speed" in ui and
        "0.5f+(20.65f/900.0f)" in ui,
        "Saved lyric speed migration or cap missing")
old_top = 18.0 + 1.3 * 35
new_top = 3.0 + 0.605 * 100
require(abs(old_top-new_top) < 0.00001,
        "New 100% lyric scroll cap does not equal old 35%")
require("lv_label_set_long_mode(s_text,LV_LABEL_LONG_CLIP)" in ui,
        "Clock settings dialog long label not protected")
# Recovery release: the previous native three-screen pager repeatedly
# left the Waveshare display black/overlapping and touch unresponsive.
# Enforce one LVGL screen and prohibit all root-slide effects.
require('static void pager_build(' not in ui and
        'lv_obj_set_flex_flow(p,LV_FLEX_FLOW_ROW);' not in ui and
        'lv_obj_scroll_to_x(pager_scroll' not in ui and
        'lv_obj_set_style_translate_x(screen,' not in ui and
        'lv_anim_set_exec_cb(' not in ui,
        "Recovery must not restore the broken multi-screen pager or root animations")
require('static void build_home(void) { nav_schedule(build_home_now); }' in ui and
        'static void build_settings(void) { nav_schedule(build_settings_now); }' in ui and
        'static void build_clock(void) { nav_schedule(build_clock_now); }' in ui and
        'if(!target)return;' in ui and
        'target();' in ui and
        'lv_async_call(nav_drain,NULL)' in ui,
        "Every main-screen navigation must be deferred until after touch dispatch")
require('lv_screen_load(screen);' in ui and
        'lv_obj_delete_async(old_screen);' in ui and
        'lv_obj_delete(old_screen)' not in ui and
        'lv_async_call(commit_swipe, NULL)' in ui and
        'lv_async_call(weather_back_deferred,NULL)' in ui,
        "No synchronous screen delete within LVGL click/gesture callbacks")
require('if (!touch_held && !recognition_touch_locked() &&' in ui and
        'is_swipe_page() && !clock_settings_popup)' in ui and
        'lv_indev_get_state(ptr)==LV_INDEV_STATE_PRESSED' in ui and
        'lv_indev_get_state(gesture_indev) == LV_INDEV_STATE_RELEASED' in ui and
        'lv_obj_add_flag(scrollbox,LV_OBJ_FLAG_EVENT_BUBBLE);' in read("main/weather_visual.c"),
        "Weather scroll gesture must be captured from physical touch input")
require("raw.githubusercontent.com/skiiirt1983/LyricFlash/alpha-ota/releases/" in network and
        "esp_http_client_get_and_clear_last_tls_error" in network and
        "ota_socket_errno" in network,
        "Direct HTTPS OTA host and TLS diagnostics are required")
require('d.ota_detail' in pages["firmware"] and
        'd.ota_http_status' in pages["firmware"] and
        "'no_internet','time_unsynced'" in pages["firmware"],
        "OTA dropdown must show actionable HTTPS, clock, and network errors")
require('"PCT7"' in network and 'PST8PDT' not in network,
        "BC clock must use permanent Pacific Time UTC-7 without DST")
require("page == 2" in ui and "case 7:" in ui,
        "Swipe back-navigation omits settings screens")
settings_nav = ui.split("else if (page == 2)", 1)
require(len(settings_nav) == 2 and
        "LV_DIR_LEFT || direction == LV_DIR_RIGHT" in settings_nav[1].split("else if (page == 6)", 1)[0] and
        "swipe_candidate_page = 0;" in settings_nav[1].split("else if (page == 6)", 1)[0],
        "Settings page must swipe home in both horizontal directions")
require('class="lf-mark"' in pages["studio"] and
        '<div class="toolbar-links">' in pages["studio"] and
        '<nav class="subnav"' in pages["studio"] and
        'href="/settings" class="active"' in pages["studio"] and
        'flex-direction:column!important' in pages["studio"] and
        'gap:24px!important' in pages["settings"] and
        'gap:24px!important' in pages["firmware"],
        "LyricFlash Web UI logo, menu spacing and Clock Studio navigation must match")
require('class="lf-mark"' in pages["settings"] and 'class="lf-mark"' in pages["firmware"],
        "LyricFlash logo/header branding missing")
require('class="toolbar-links"' in pages["settings"] and 'class="toolbar-links"' in pages["firmware"]
        and 'class="firmware-link active"' in pages["firmware"],
        "Firmware navigation must use the same toolbar as Settings")
require('id="refresh-builds"' in pages["firmware"] and 'loadBuilds(true)' in pages["firmware"]
        and 'function selectFirmwareRows(available,current)' in pages["firmware"]
        and 'entries=selectFirmwareRows(entries,installed);' in pages["firmware"]
        and "rows.push({kind:'Current'" in pages["firmware"]
        and "kind:'Old'" in pages["firmware"]
        and "kind:'New'" in pages["firmware"]
        and 'option.disabled=!!b.disabled;' in pages["firmware"]
        and 'b.version===installed' in pages["firmware"]
        and 'versionCompare(b.version,installed)>0' in pages["firmware"],
        "OTA picker must show only Old/Current/New with disabled downgrade and verified Current reinstall/newer update")
require("esp_sleep_enable_ext0_wakeup(GPIO_NUM_11,0)" in ui and
        "deep_sleep_retry_after_ms=current+60000ULL" in ui and
        "esp_deep_sleep_start();" in ui,
        "Safe opt-in CST9217 GPIO11 deep sleep and wake timeout missing")
require('id="pref-deep_sleep_minutes"' in pages["settings"] and
        'id="pref-scroll_speed" min="0"' in pages["settings"] and
        'margin-top:34px!important' in pages["settings"],
        "Slow range, deep sleep and lowered menu missing")
require('class="subnav"' in pages["settings"], "Settings section menu missing")
for key in ("network", "clocks", "lyrics", "device", "maintenance"):
    require('id="'+key+'"' in pages["settings"],
            "Settings menu target missing: "+key)
require('"/api/preferences"' in network and "lf_ui_apply_device_settings" in network,
        "Browser global preferences cannot be saved")
require("face_profiles" in network and "face_profiles" not in pages["settings"],
        "Clock profiles must be saved by Clock Studio, not global preferences")
for required in ("face_background", "face_foreground", "face_accent",
                 "face_show_date", "face_show_seconds", "face_show_battery", "face_24h"):
    require(required in pages["studio"], "Live Clock Studio missing "+required)
require("strlcpy(ap_pass, \"lyricflash\", sizeof ap_pass);" in network,
        "Simple fixed setup AP credential not configured")
require("lv_draw_line_dsc_init" in read("main/clock_face.c"),
        "Analog ticks still create 60 separate LVGL objects")
basic_clock=read("main/basic_clock.c")
require("basic_ring_draw" not in basic_clock and
        "lv_draw_arc(layer,&dsc);" not in basic_clock and
        "lv_arc_create(" not in basic_clock,
        "Disabled Basic Clock ring must not allocate or draw arcs")
require("charging_color" in read("main/settings.h") and
        "charging_color" in pages["settings"],
        "Configurable charging indicator absent")
# This requirement is intentionally a release blocker until implemented.
require(bool(re.search(r'id=["\']clock-preview["\']|<canvas\b',
                       pages["studio"], flags=re.I)),
        "Clock Studio still lacks the agreed live preview")
print(f"PASS: {tests_passed} source-only LyricFlash preflight assertions")

# First-touch lockup regression: the active LVGL root must remain constant and
# no scene rebuild may occur inside a CLICKED/PRESSED event handler.
require('if (!screen) {' in ui and
        'lv_obj_get_child_count(screen)' in ui and
        'if (child != clock_scene) lv_obj_delete(child);' in ui and
        ui.count('screen=lv_obj_create(NULL);')==1 and
        ui.count('lv_screen_load(screen);')==1 and
        'static void nav_drain(void *arg)' in ui and
        'lv_async_call(nav_drain,NULL);' in ui,
        "Single-root screen lifetime and deferred navigation missing")
for name in ('build_home','build_preview','build_settings','build_clock',
             'build_weather_details','build_wifi','build_clock_settings'):
    require(f'static void {name}(void) {{ nav_schedule({name}_now); }}' in ui,
            f'Input callback can still rebuild {name} synchronously')
on_wake=ui.split('static void on_wake(lv_event_t *e)\n{',1)[1].split('static void sleep_deep',1)[0]
sleep=ui.split('static void sleep_display(void)\n{',1)[1].split('static void on_tick',1)[0]
require('build_home();' not in on_wake and 'make_base();' not in sleep and
        'sleep_overlay=lv_obj_create(screen);' in sleep and
        'lv_obj_set_style_bg_opa(sleep_overlay,LV_OPA_TRANSP,0);' in on_wake and
        'lv_async_call(wake_finish_async,NULL);' not in on_wake and
        'lv_obj_delete(cover);' in ui and
        'if(asleep) {' in ui,
        "Wake must preserve the current page, not destroy the touch target")

# Button-tap safety: a short LVGL gesture cannot navigate until it passes
# the same displacement threshold as the physical swipe tracker.
gesture=ui.split('static void on_lvgl_gesture(lv_event_t *e)\n{',1)[1].split('static void on_touch_release',1)[0]
threshold='if(dx*dx+dy*dy < LF_SWIPE_MIN_PX*LF_SWIPE_MIN_PX) return;'
require('observe_touch_position();' in gesture and threshold in gesture and
        gesture.index('observe_touch_position();') < gesture.index(threshold) <
        gesture.index('swipe_intent(lv_indev_get_gesture_dir(indev));') and
        'if(swipe_candidate_page<0)' in gesture,
        "Short LVGL gestures could steal button taps or double-trigger navigation")

# AMOLED stack regression: preserve explicit startup/wake repaint and
# telemetry; page rebuilds should rely on LVGL invalidated scene regions
# instead of redundantly forcing a full root repaint after every navigation.
require('invalidate_full_screen();' in ui and
        'lv_obj_invalidate(screen);' in ui and
        'lv_async_call(wake_finish_async,NULL);' in ui and
        'static void wake_finish_async(void *arg)' in ui and
        'lv_obj_delete(cover);' in ui and
        'if(!target)return;' in ui and
        'target();' in ui and
        'lv_display_add_event_cb(disp,display_flush_start_event,LV_EVENT_FLUSH_START,NULL);' in ui and
        'lv_display_add_event_cb(disp,display_flush_finish_event,LV_EVENT_FLUSH_FINISH,NULL);' in ui,
        "AMOLED full redraw and flush lifecycle metrics missing")
for key in ('ui_touch_presses','ui_navigation_commits','ui_full_redraws',
            'display_render_count','display_flush_starts',
            'display_flush_finishes','display_refresh_ready'):
    require('"' + key + '"' in network, "Missing diagnostic JSON key " + key)
require('lf_ui_display_stats_t' in read('main/ui.h') and
        'static atomic_uint ui_flush_start_count' in ui and
        'static atomic_uint ui_flush_finish_count' in ui,
        "Thread-safe display metrics contract missing")

# Hardware display recovery: local independently licensed BSP has PSRAM -> DMA
# staging and a one-slot SPI transfer queue. TE wait is disabled here
# to isolate the 0.8.16 second-flush deadlock.
bsp=read('components/esp32_s3_touch_amoled_1_75/esp32_s3_touch_amoled_1_75.c')
display_header=read('components/esp32_s3_touch_amoled_1_75/include/bsp/display.h')
app_main=read('main/main.c')
sdk=read('sdkconfig.defaults')
require('esp_ptr_external_ram(color_data)' in bsp and
        'bsp_rotation_panel_draw_staged(' in bsp and
        'BSP_LCD_DMA_STAGING_ROWS (8)' in bsp and
        'BSP_LCD_DMA_FALLBACK_ROWS (4)' in bsp and
        'ESP_LV_ADAPTER_TEAR_AVOID_MODE_NONE' in app_main and
        'ESP_LV_ADAPTER_TEAR_AVOID_MODE_TE_SYNC' not in app_main and
        'BSP_LCD_TE_GPIO' in display_header and
        'display_cfg.lv_adapter_cfg.task_core_id = 1;' in app_main and
        'CONFIG_BSP_LCD_TRANS_QUEUE_DEPTH=1' in sdk and
        'bsp_display_brightness_set(asleep ? 1 : prefs->brightness);' in ui and
        (ROOT/'components/esp32_s3_touch_amoled_1_75/LICENSE').is_file(),
        "CO5300 staged DMA, TE-off test, or license missing")

# Basic Clock should not redraw unchanged clock fields or force a full-screen
# redraw after normal page transitions (safe renderer retained from 0.8.17).
clock_face=read("main/clock_face.c")
nav_drain=ui.split('static void nav_drain(void *arg)\n{',1)[1].split('static void nav_schedule',1)[0]
require('invalidate_full_screen();' not in nav_drain and
        'if (child != clock_scene) lv_obj_delete(child);' in ui and
        'lv_async_call(wake_finish_async,NULL);' in ui and
        'invalidate_full_screen();' in ui,
        "Navigation still triggers redundant root invalidation or wake is unprotected")
require('static void update_label_if_changed(' in clock_face and
        'strcmp(shown, value) != 0' in clock_face and
        'update_label_if_changed(time_label, t);' in clock_face and
        'update_label_if_changed(battery_label, text);' in clock_face and
        'update_label_if_changed(weather_label, value);' in clock_face and
        'displayed_battery_color' in clock_face,
        "Basic Clock unchanged-value LVGL invalidation guard missing")

# The clock is kept as one child of the persistent three-page pager, still
# under the ONLY LVGL root screen; invalidate it on settings changes.
pager_clock=ui.split('static void primary_pager_build_clock(void)\n{',1)[1].split('static void primary_pager_create',1)[0]
make_base=ui.split('static void make_base(void)\n{',1)[1].split('static void on_wifi',1)[0]
require('static lv_obj_t *clock_scene;' in ui and
        'if (clock_scene) lv_obj_add_flag(clock_scene,LV_OBJ_FLAG_HIDDEN);' in make_base and
        'if (child != clock_scene) lv_obj_delete(child);' in make_base and
        'clock_scene_stale || clock_scene_style != prefs->clock_face' in make_base and
        'lf_clock_build(clock_scene,prefs->clock_face,prefs);' in pager_clock and
        'lv_obj_remove_flag(clock_scene,LV_OBJ_FLAG_HIDDEN);' in pager_clock and
        'lf_clock_update();' in ui and
        'clock_scene_stale=true;' in ui and
        ui.count('screen=lv_obj_create(NULL);') == 1,
        "Clock must remain cached as one child of the only LVGL screen")
for key in ('clock_scene_builds','clock_cache_hits','clock_build_last_ms',
            'clock_build_max_ms','clock_present_last_ms','clock_present_max_ms'):
    require('"' + key + '"' in network and key in read('main/ui.h'),
            "Clock load timing/scene cache diagnostics missing: "+key)

# Single-root QSPI transition contract: never restore the broken multi-screen
# flex pager and never animate two full 466px framebuffers at the same time.
require('static void nav_transition_begin(' in ui and
        'static void nav_transition_tick(' in ui and
        'static void nav_transition_end(' in ui and
        'lv_display_enable_invalidation(disp,false);' in ui and
        'lv_display_enable_invalidation(disp,true);' in ui and
        'lv_obj_invalidate_area(screen,&region);' in ui and
        'LF_NAV_REVEAL_STEPS 12' in ui and
        'nav_transition_catcher=lv_obj_create(screen);' in ui and
        'if (nav_transition_active) return;' in ui and
        'nav_transition_hint = pending_swipe_direction;' in ui and
        'if(nav_transition_active)nav_transition_end(true);' in ui and
        'lv_obj_update_layout(screen);' in ui,
        "Panel-paced transition cannot safely update one LVGL root")
require('nav_reveal_completions' in network and
        'nav_reveal_last_ms' in network and
        'nav_reveal_completions' in read('main/ui.h'),
        "Missing page reveal timing diagnostics")

# 0.8.21: filmed bright green line on Settings->Clock transition was the
# 0.8.20 synthetic 6px user-accent bar, NOT the CO5300 panel driver.
# Never reintroduce that decorative geometry into the QSPI reveal.
require('nav_transition_edge' not in ui and
        'lv_obj_set_style_bg_color(nav_transition_edge,' not in ui and
        'lv_obj_invalidate_area(screen,&region);' in ui and
        'nav_transition_catcher=lv_obj_create(screen);' in ui and
        'if(screen && interrupted) lv_obj_invalidate(screen);' in ui,
        "Page transitions must reveal content without green accent divider")

# 0.8.22: actual page carousel rather than 0.8.21 post-release strip wipe.
# Three live sibling panels scroll WITH the user's finger and snap on release.
# Still only one lv_screen_load() root, and Weather/submenus stay deferred.
watchdog=ui.split('static void primary_pager_watchdog(uint64_t current)\n{',1)[1].split('static void primary_pager_scroll_event',1)[0]
pager_impl=ui.split('static void primary_pager_scroll_event(lv_event_t *e)\n{',1)[1].split('static lv_obj_t *primary_page_create',1)[0]
pager_scene=ui.split('static void primary_pager_create(int target_index)\n{',1)[1].split('static void primary_pager_enter(',1)[0]
require('static lv_obj_t *primary_pager;' in ui and
        'primary_pager_pages[3]' in ui and
        'primary_pager_origin' in pager_impl and
        'LV_EVENT_SCROLL_BEGIN' in pager_impl and
        'LV_EVENT_SCROLL_END' in pager_impl and
        'primary_pager_start_snap(target);' not in pager_impl and
        'lv_event_get_target(e)!=primary_pager' in pager_impl and
        'primary_pager_last_move_ms=now_ms();' in pager_impl and
        'primary_pager_start_snap(target);' in watchdog and
        'abs(displacement)>=55' in ui and
        'current-primary_pager_last_move_ms>75' in ui and
        'lv_obj_set_scroll_dir(primary_pager,LV_DIR_HOR);' in pager_scene and
        'LV_SCROLL_SNAP_NONE' in pager_scene and
        'lv_obj_scroll_to_x(primary_pager,target_index*466,LV_ANIM_OFF);' in pager_scene and
        'primary_pager_pages[i]=primary_page_create(primary_pager,i)' in pager_scene and
        'primary_render_home_content(primary_pager_pages[1]);' in pager_scene and
        'primary_render_settings_content(primary_pager_pages[2]);' in pager_scene and
        'primary_pager_build_clock();' in pager_scene and
        'lv_obj_set_parent(clock_scene,screen);' in make_base and
        'if(was_pager && primary_pager){' in nav_drain and
        ui.count('lv_screen_load(screen);')==1,
        "Finger pager must contain 3 live pages inside single safe LVGL root")
require('if(primary_pager && (page==0 || page==2 || page==6)' in ui and
        'LV_DIR_LEFT || direction==LV_DIR_RIGHT' in ui and
        'lv_indev_set_scroll_throw(touch,90);' in ui and
        'if(nav_transition_active)nav_transition_end(true);' in ui,
        "Native horizontal dragging must exclude duplicate release navigation")

# The 0.8.22 physical test showed mid-page freezes and a laggy clock. Guard
# against missed LVGL scroll-end events with a one-shot page alignment fallback
# that NEVER takes ownership while the user is still pressing the display.
require('primary_pager_snap_started_ms' in ui and
        'current-primary_pager_snap_started_ms>LF_PAGER_STALL_GUARD_MS' in watchdog and
        'lv_obj_scroll_to_x(primary_pager,target*466,LV_ANIM_OFF);' in watchdog and
        'primary_pager_commit(target);' in watchdog and
        'lv_indev_get_state(indev)==LV_INDEV_STATE_PRESSED)return;' in watchdog and
        'if(primary_pager && !timed_overlay)primary_pager_watchdog(current);' in ui,
        "Mid-page scroll watchdog / finger ownership regression")
# User hardware regression: occasional black Clock on return, revealed by
# a tap after screenshot/live visibility swapping in 0.8.23–0.8.24.
# No snapshot image, no off-screen proxy, no dynamic hiding of real clock.
# Repaint on Clock settle *without requiring an additional touch*.
commit=ui.split('static void primary_pager_commit(int index)\n{',1)[1].split('static void primary_pager_start_snap',1)[0]
require('lv_snapshot_take(' not in ui and
        'primary_clock_snapshot' not in ui and
        'primary_clock_image' not in ui and
        'CONFIG_LV_USE_SNAPSHOT=y' not in read('sdkconfig.defaults') and
        'lv_obj_remove_flag(clock_scene,LV_OBJ_FLAG_HIDDEN);' in commit and
        'lv_obj_invalidate(clock_scene);' in commit and
        'atomic_fetch_add(&clock_repaint_recoveries,1);' in commit and
        'primary_pager_watchdog(current);' in ui and
        '"clock_repaint_recoveries"' in network and
        'pager_forced_snaps' in network,
        "Clock must remain live and receive repaint when pager settles")

# Finger-release repaint protects the active pager page from black frames.
require('const bool settled_move=primary_pager_snapping || pager_scrolling ||' in commit and
        'lv_obj_invalidate(primary_pager_pages[index]);' in commit and
        'lv_event_get_target(e)!=primary_pager' in pager_impl and
        'primary_pager_start_snap(target);' not in pager_impl and
        'primary_pager_start_snap(target);' in watchdog,
        "Finger-up must not reenter LVGL scroll dispatch or leave blank page")

# 0.8.26: controlled short snaps must not remove hardware regression safety.
require('lv_event_get_scroll_anim(e)' in ui and
        'lv_anim_set_duration(anim,185U+' in ui and
        'lv_anim_set_path_cb(anim,lv_anim_path_ease_out)' in ui and
        'lv_obj_remove_flag(primary_pager,LV_OBJ_FLAG_SCROLL_MOMENTUM);' in ui and
        '"pager_snap_last_ms"' in network and
        '"render_cycle_last_ms"' in network and
        'lv_obj_invalidate(clock_scene);' in ui and
        'current-primary_pager_snap_started_ms>LF_PAGER_STALL_GUARD_MS' in watchdog,
        "Paced pager snap and retained Clock recovery contract")

# 0.8.30: physical-device regression with a 96-row rendering buffer.
# Preserve the proven 24-row LVGL buffer and 1-slot queue; 0.8.88 restores
# only with 8-row 0.8.86 DMA staging and disables LVGL style caching.
require('LVGL_BUFFER_HEIGHT_PSRAM (24)' in bsp and
        'LVGL_BUFFER_HEIGHT_PSRAM (96)' not in bsp and
        'BSP_LCD_SPI_TRANSFER_ROWS' not in bsp and
        'BSP_LCD_H_RES * buffer_height * BSP_LCD_BITS_PER_PIXEL / 8' in bsp and
        'BSP_LCD_DMA_STAGING_ROWS (8)' in bsp and
        'BSP_LCD_DMA_FALLBACK_ROWS (4)' in bsp and
        'CONFIG_BSP_LCD_TRANS_QUEUE_DEPTH=1' in sdk and
        'lv_obj_invalidate(clock_scene);' in ui,
        "0.8.88 preserve tested LVGL geometry, 8-row staged DMA and SPI queue depth")

# 0.8.29: one bounded microphone-controller probe must never start I2S
# or do audio capture. Probe POST only, initiated by explicit Web UI click.
mic_probe=read("main/mic_probe.c")
mic_header=read("main/mic_probe.h")
require('i2c_master_probe(bus, LF_ES7210_I2C_ADDR' in mic_probe and
        '#define LF_ES7210_I2C_ADDR (0x40)' in mic_probe and
        'LF_MIC_PROBE_TIMEOUT_MS (100)' in mic_probe and
        'bsp_i2c_get_handle()' in mic_probe and
        'mic_probe.c' in cmake and
        'LF_MIC_PRESENT' in mic_header and
        'i2s_channel_read' not in mic_probe and
        'i2s_channel_enable' not in mic_probe,
        "0.8.29 ES7210 shared-I2C electrical presence test only")
require('"/api/hardware/mic-check"' in network and
        'HTTP_POST,.handler=handle_mic_check' in network and
        'microphone_adc_status' in network and
        'mic-check-btn' in web and
        '/api/hardware/mic-check' in web and
        'audio_capture_active' in network,
        "0.8.29 explicit local mic check route, button and no-audio status")

# 0.8.31: manual, bounded PCM test and preservation of physical display fix.
pcm=read("main/mic_pcm.c")
pcm_h=read("main/mic_pcm.h")
require('LF_PCM_LIMIT_US (3000000LL)' in pcm and
        'esp_codec_dev_read(codec,pcm,sizeof(pcm))' in pcm and
        'esp_codec_dev_close(codec)' in pcm and
        'atomic_load(&stop_requested)' in pcm and
        'LF_PCM_RUNNING' in pcm_h and
        'mic_pcm.c' in cmake,
        "0.8.31 bounded PCM capture and Stop lifecycle")
require('"/api/hardware/mic-test"' in network and
        '"/api/hardware/mic-test/start"' in network and
        '"/api/hardware/mic-test/stop"' in network and
        'mic-pcm-start' in web and 'mic-pcm-stop' in web and
        'X-LyricFlash-Manual-Test' in network and
        'X-LyricFlash-Manual-Test' in web and
        'audio_uploaded' in network and 'audio_saved' in network and
        'max_uri_handlers=28' in network,
        "0.8.31 manual-only Web UI test and no raw audio exposure")

# 0.8.32: frame the physical ES7210 correctly; never touch the display path.
require('i2s_channel_init_tdm_mode(i2s_rx_chan, &rx_cfg)' in bsp and
        'I2S_TDM_CLK_DEFAULT_CONFIG(24000)' in bsp and
        'I2S_TDM_SLOT0 | I2S_TDM_SLOT1 | I2S_TDM_SLOT2 | I2S_TDM_SLOT3' in bsp and
        'rx_cfg.slot_cfg.total_slot = 4' in bsp and
        'rx_cfg.clk_cfg.bclk_div = 8' in bsp and
        'bsp_audio_init_voice_24k()' in bsp and
        '.mic_selected = (1U << 0) | (1U << 1) | (1U << 2)' in bsp,
        "0.8.32 four-slot 24kHz Waveshare ES7210 TDM receive configuration")
require('LF_PCM_RATE (24000)' in pcm and
        'LF_PCM_TDM_SLOTS (4)' in pcm and
        'LF_PCM_TDM_SLOT_MASK (0x0FU)' in pcm and
        'LF_PCM_FRONT_PHYSICAL_MASK (0x03U)' in pcm and
        'esp_codec_dev_set_in_channel_gain' in pcm and
        'v2=pcm[i+2]' in pcm and
        'mic1_mean_abs' in network and 'mic2_peak_abs' in network and
        'mic1_mean_abs' in web and 'mic2_peak_abs' in web,
        "0.8.32 separate physical MIC1 and MIC2 levels, not reference/unused slots")

require('static esp_codec_dev_handle_t microphone_codec_dev = NULL;' in bsp and
        'if (microphone_codec_dev != NULL) return microphone_codec_dev;' in bsp and
        'microphone_codec_dev = esp_codec_dev_new(&codec_es7210_dev_cfg)' in bsp,
        "0.8.32 microphone codec handle must be shared across repeated tests")

# Preserve the proven I2S, upload and LCD paths; the enclosed-case alpha
# intentionally raises the ES7210 gain from 24 to 27 dB for both audio paths.
require('LF_PCM_FRONT_GAIN_DB (27.0f)' in pcm and
        'LF_PCM_RATE (24000)' in pcm and 'LF_PCM_TDM_SLOTS (4)' in pcm and
        'LF_PCM_FRONT_PHYSICAL_MASK (0x03U)' in pcm and
        'LF_PCM_LIMIT_US (3000000LL)' in pcm,
        "0.8.33 preserve proven dual TDM mic capture and manual gating")
require('LF_PCM_NEAR_RAIL (32760U)' in pcm and
        'mic1_sum_sq+=' in pcm and 'mic2_sum_sq+=' in pcm and
        'pcm_isqrt_u64' in pcm and
        'mic1_rms' in pcm and 'mic2_rms' in pcm and
        'mic1_near_rail' in pcm and 'mic2_near_rail' in pcm and
        'mic1_rms' in pcm_h and 'mic2_near_rail' in pcm_h,
        "0.8.33 per-microphone RMS / near-rail counting")
require('mic1_rms' in network and 'mic2_rms' in network and
        'mic1_near_rail' in network and 'mic2_near_rail' in network and
        'mic1_rms' in web and 'mic2_near_rail' in web,
        "0.8.33 browser telemetry exposes both microphones")

# Alpha 0.8.34 firmware UI-only release: do not modify microcontroller audio
# capture, known-good QSPI/LVGL rendering, NVS, network OTA endpoint, or routes.
require('selectFirmwareRows' in pages['firmware'] and
        'versionParts(installed)' in pages['firmware'] and
        'New firmware available:' in pages['firmware'] and
        'currentImage=known?unique.find(b=>b.version===current):null' in pages['firmware'] and
        "kind:'Current'" in pages['firmware'] and 'disabled:!currentImage' in pages['firmware'] and
        "b.kind==='Current'&&b.version===installed" in pages['firmware'] and
        'a reinstall is NOT a factory reset' in pages['firmware'] and
        'selector.disabled=!hasInstallable;' in pages['firmware'],
        "0.8.35 three-row picker, exact published Current reinstall safety")

# 0.8.35 permits only a signed/verified same-version image on re-install,
# preserving explicit user confirmation and ESP-IDF binary project/version check.
require('strcmp(descriptor.project_name,"lyricflash")!=0' in network and
        'strcmp(descriptor.version,job->version)!=0' in network and
        'esp_https_ota_finish(handle)' in network and
        'allowed_github_ota_url(url->valuestring)' in network,
        "0.8.35 retain device-side OTA project, version, URL and image integrity")

# 0.8.36 audio diagnostic: count contiguous near-rail runs across PCM reads,
# compare the two front microphone slots and expose robust representative
# signal level without hiding the original RMS/peak. No capture path changes.
require('LF_PCM_HALF_SCALE (16384U)' in pcm and
        'mic1_run=0' in pcm and 'mic2_run=0' in pcm and
        'both_rail++' in pcm and
        'pcm_isqrt_u64(mic1_below_sq/mic1_below_count)' in pcm and
        'mic1_rail_bursts' in pcm and 'mic2_longest_rail_burst' in pcm and
        'mic1_rms_below_rail' in pcm and 'mic2_rms_below_rail' in pcm,
        "0.8.36 noninvasive rail spike burst duration and sub-rail RMS")
require('mic1_rail_bursts' in network and
        'mic2_longest_rail_burst' in network and
        'both_near_rail_frames' in network and
        'mic1_rms_below_rail' in web and
        'both_near_rail_frames' in web,
        "0.8.36 microphone rail diagnostics must reach the device Web UI")

# Alpha 0.8.37: music ID must be a deliberate, bounded, protected operation.
song=read("main/recognition.c")
scan=read("main/recognition_audio.c")
require('LF_SCAN_SECONDS' in song and 'LF_SCAN_TDM_FRAMES' in song and
        'LF_SCAN_RATE 24000U' in read("main/recognition_audio.h") and
        'lf_scan_tdm_to_stereo' in song and 'esp_codec_dev_close(codec)' in song and
        'lf_recog_stop(void)' in song and 'LF_RECOG_TOKEN_NS' in song,
        "0.8.37 bounded/manual song recognition with NVS token")
require('https://api.audd.io/' in song and
        'esp_crt_bundle_attach' in song and
        'clear_secret(wav,LF_SCAN_WAV_BYTES)' in song and
        'lf_recog_busy()' in network and
        '/api/recognize/start' in network and
        '/api/recognize/token' in network and
        "X-LyricFlash-Manual-Test" in network,
        "0.8.37 private HTTPS-only, manually authorized audio upload")
require('id="song-token"' in pages["settings"] and
        'id="song-save"' in pages["settings"] and
        'id="song-clear"' in pages["settings"] and
        'id="song-start"' not in pages["settings"] and
        'id="song-stop"' not in pages["settings"] and
        'api/recognize/token' in pages["settings"] and
        '/api/recognize/start' in network,
        "0.8.61 setup-only WebUI, unchanged physical song recognition")

# Alpha 0.8.38 must look up synchronized lyrics only after explicit AudD scan.
lrclib=read("main/lrclib_client.c")
recognize=read("main/recognition.c")
require('https://lrclib.net/api/get?track_name=' in lrclib and
        '.crt_bundle_attach=esp_crt_bundle_attach' in lrclib and
        '.user_agent=client_user_agent' in lrclib and
        'if(status==429)' in lrclib and
        'lf_parse_lrc(synced->valuestring,out)' in lrclib and
        'MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT' in lrclib and
        'lf_lrclib_search_title' in lrclib,
        "0.8.38 secure bounded LRCLIB lookup with no rate-limit retries")
require('lookup_lyrics_after_match()' in recognize and
        'if(ok && !atomic_load(&stop_flag))lookup_lyrics_after_match();' in recognize and
        'lf_recog_lyric_line' in recognize and
        'atomic_compare_exchange_strong(&worker_active' in recognize and
        'status(LF_RECOG_LYRICS' in recognize,
        "0.8.38 manual lyrics and worker lifecycle")
require('lf_recog_lyric_line' in recognize and
        'id="song-token"' in pages['settings'] and
        'checkedAtLeastOneSource' in pages['firmware'] and
        'Some sources unavailable:' not in pages['firmware'],
        "0.8.61 retained synchronized lyric engine and friendly firmware update status")

# Alpha 0.8.39: use the proven circular UI to view real parsed LRCLIB lyrics.
clock=read("main/lyric_clock.c")
clock_h=read("main/lyric_clock.h")
require('lyric_clock.c' in cmake and
        'lf_lyric_clock_start' in clock and
        'lf_lyric_clock_pause' in clock and
        'lf_lyric_clock_resume' in clock and
        'lf_lyric_clock_adjust' in clock and
        'lf_lyric_clock_line_at' in clock and
        'lf_lyric_clock_t' in clock_h,
        "0.8.39 manual monotonic lyric clock with pause and offset")
require('#include "recognition.h"' in ui and
        'lf_recog_snapshot(&song)' in ui and
        'lf_recog_lyric_line((uint32_t)i,&line)' in ui and
        'strcmp(song.lyrics_state,"synced")==0' in ui and
        'build_timed_lyrics_now(&song)' in ui and
        'lf_lyric_clock_line_at(timed_stamps,timed_line_count,ms)' in ui and
        'timed_render(now_ms(),false)' in ui,
        "0.8.39 real song and timestamp focused lyric viewport")
require('lf_sync_estimate_position_ms(' in ui and
        'song->audd_timecode_valid' in ui and
        'timed_toggle_controls' in ui and
        'LV_EVENT_LONG_PRESSED' in ui and
        'PAUSE' in ui and 'RESUME' in ui and
        'timed_pause' in ui and 'timed_adjust' in ui and
        'timed_change_offset(timed_clock.correction_ms+(int32_t)(direction*500));' in ui and
        'timed_scan_attempt!=song->attempts' in ui,
        "0.8.42 estimated sync, hidden alignment controls and new scan reset")
require('lv_obj_delete_async(old_screen);' in ui and
        'lv_timer_create(on_tick, 50, NULL)' in ui and
        'timed_scroll_timer=lv_timer_create(timed_scroll_frame,65,NULL)' in ui and
        'timed_rows[4]' in ui and
        'for(int slot=0;slot<4;slot++)' in ui,
        "0.8.39 retain proven QSPI, pager and redraw-safe 3-row UI")

# Prevent another successful compile from silently dropping restored controls.
require(all(x in pages["studio"] for x in (
    'id="selected-battery-icon"', 'id="selected-battery-percent"',
    'id="selected-date-format"', 'id="selected-weather-visible"',
    'id="selected-font-style"', 'id="selected-color"',
    'data-spectrum="1"', "target.dispatchEvent(new Event('input'",
    "selectedPart+'_color'", 'function selectPart(part)')),
    "Clock Studio contextual controls or spectrum picker missing")
require(all(x in ui for x in (
    'lv_obj_set_scroll_dir(viewport,LV_DIR_VER)',
    'device_line(content,"IP ADDRESS"',
    'device_line(content,"WEB ADDRESS"',
    'device_line(content,"VERSION"',
    'lv_obj_set_size(slider,242,32)',
    'lv_obj_set_style_pad_all(slider,10,LV_PART_KNOB)')),
    "Device scroll, network/version details or brightness touch target regressed")
require(all(x in ui for x in (
    'lv_obj_set_style_bg_color(b, lv_color_hex(0x143553), 0)',
    'lv_obj_set_style_border_color(b, lv_color_hex(0x72C9FF), 0)',
    'lv_obj_set_style_text_color(l, lv_color_hex(0xEAF6FF), 0)')) and
    'const bool settings=(page==' not in ui,
    "Device buttons must use the same polished styling from startup")
require(all(x in pages["studio"] for x in (
    'function paintSpectrumSwatches()',
    'paintSpectrumSwatches();',
    'id="reset-face"',
    'id="basic-options" hidden')) and
    'id="reset-basic"' not in pages["studio"],
    "Clock Studio needs live colour swatches and only one face reset")

