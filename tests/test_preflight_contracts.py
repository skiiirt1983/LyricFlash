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
require("selector.disabled=!entries.length;el('install').disabled=true;" in pages["firmware"]
        and "selection===''?null" in pages["firmware"],
        "OTA install button must require an explicit version selection")
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
require("lv_screen_load(screen);" in ui and
        "lv_obj_delete_async(old_screen);" in ui and
        "pager_scroll" not in ui and
        "pager_build(" not in ui,
        "Safe one-screen-only LVGL navigation must remain in place")
require("nav_settle(direction)" in ui and
        "lv_obj_set_style_translate_x(screen,amount,0);" in ui and
        "lv_anim_set_duration(&a,175);" in ui and
        "lv_anim_delete(old_screen,nav_translate_x);" in ui,
        "Swipe must move with finger and settle without overlapping screens")
require("page == 2" in ui and "case 7:" in ui,
        "Swipe back-navigation omits settings screens")
settings_nav = ui.split("else if (page == 2)", 1)
require(len(settings_nav) == 2 and
        "LV_DIR_LEFT || direction == LV_DIR_RIGHT" in settings_nav[1].split("else if (page == 6)", 1)[0] and
        "swipe_candidate_page = 0;" in settings_nav[1].split("else if (page == 6)", 1)[0],
        "Settings page must swipe home in both horizontal directions")
require('class="lf-mark"' in pages["settings"] and 'class="lf-mark"' in pages["firmware"],
        "LyricFlash logo/header branding missing")
require('class="toolbar-links"' in pages["settings"] and 'class="toolbar-links"' in pages["firmware"]
        and 'class="firmware-link active"' in pages["firmware"],
        "Firmware navigation must use the same toolbar as Settings")
require('id="refresh-builds"' in pages["firmware"] and 'loadBuilds(true)' in pages["firmware"]
        and 'entries.sort((a,b)=>versionCompare' in pages["firmware"]
        and 'OLDER THAN INSTALLED' in pages["firmware"] and ' — NEW' not in pages["firmware"],
        "Firmware picker must support refresh and correctly order versions")
require("esp_sleep_enable_ext0_wakeup(GPIO_NUM_11,0)" in ui and
        "deep_sleep_retry_after_ms=current+60000ULL" in ui and
        "esp_deep_sleep_start();" in ui,
        "Safe opt-in CST9217 GPIO11 deep sleep and wake timeout missing")
require('id="pref-deep_sleep_minutes"' in pages["settings"] and
        'id="pref-scroll_speed" min="0"' in pages["settings"] and
        'margin-top:34px!important' in pages["settings"],
        "Slow range, deep sleep and lowered menu missing")
require('class="subnav"' in pages["settings"], "Settings section menu missing")
for key in ("network", "clocks", "lyrics", "device", "status-section"):
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
require("segment_count" in read("main/basic_clock.c"),
        "Basic ring still creates 60 individual LVGL objects")
require("charging_color" in read("main/settings.h") and
        "charging_color" in pages["settings"],
        "Configurable charging indicator absent")
# This requirement is intentionally a release blocker until implemented.
require(bool(re.search(r'id=["\']clock-preview["\']|<canvas\b',
                       pages["studio"], flags=re.I)),
        "Clock Studio still lacks the agreed live preview")
print(f"PASS: {tests_passed} source-only LyricFlash preflight assertions")
