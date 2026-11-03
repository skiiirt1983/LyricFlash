#!/usr/bin/env python3
"""Home-player visual, touch, privacy, and regression source-only contracts.
This runs without an ESP32 firmware compile. Physical visual QA is separate.
"""
from pathlib import Path

root=Path(__file__).resolve().parents[1]
ui=(root/"main/ui.c").read_text()
version=(root/"main/version.h").read_text()
recognition=(root/"main/recognition.c").read_text()

def check(condition,message):
    if not condition:
        raise AssertionError(message)

start=ui.index("static void primary_render_home_content(lv_obj_t *parent)\n{")
end=ui.index("\nstatic void render_home(void)",start)
home=ui[start:end]
refresh=ui[ui.index("static void home_refresh(uint64_t current)\n{"):start]
on_click=ui[ui.index("static void on_home_scan(lv_event_t *e)\n{"):ui.index("static lv_obj_t *home_decor_ring")]
check('LF_VERSION "0.8.43-alpha"' in version,"0.8.42 alpha version not set")
check('home_scan_button=lv_button_create(parent);' in home,"Missing single circular play surface")
check(home.count('lv_button_create(')==1,"Home must expose only one visual control")
check('button(parent,' not in home,"Old rectangular navigation buttons still on Home")
check('lv_obj_set_size(home_scan_button,184,184);' in home,"Insufficient play touch target")
check('LV_RADIUS_CIRCLE' in home,"Play control must be circular")
check('LV_SYMBOL_PLAY' in home and 'LV_SYMBOL_STOP' in refresh,"Play must switch to Stop when busy")
check('lv_font_montserrat_48' in home,"Large play symbol is missing")
check('home_decor_ring(parent,216' in home and 'home_decor_ring(parent,202' in home,
      "Missing light-weight circular visual rings")
check('lv_obj_add_event_cb(home_scan_button,on_home_scan,LV_EVENT_CLICKED,NULL);' in home,
      "Play not wired to manual song scan")
check('lf_recog_start()' in on_click and 'lf_recog_stop()' in on_click,
      "Play must manually start and stop recognition")
check('lf_recog_has_token()' in on_click and 'lf_network_is_connected()' in on_click,
      "Missing actionable failure states")
check('lf_recog_snapshot(&song)' in refresh and 'song.title' in refresh and 'song.artist' in refresh,
      "Home missing actual recognized song identity")
check('lf_recog_lyric_line(' in refresh and 'timed_clock.started' in refresh,
      "Home must display the manually timed line when available")
check('lv_obj_add_event_cb(details[i],on_preview,LV_EVENT_CLICKED,NULL);' in home,
      "Song details must open Lyrics without a rectangular button")
check('LV_OBJ_FLAG_GESTURE_BUBBLE' in home and 'on_lvgl_gesture' in home,
      "Play touch must preserve swipe navigation")
check('battery_status=home_caption' in home,"Home lost charging/battery indicator")
check('home_visual_busy!=(int)busy' in refresh,"Redundant QSPI full-circle redraws")
check('primary_render_home_content(primary_pager_pages[1]);' in ui and
      'primary_render_settings_content(primary_pager_pages[2]);' in ui and
      'primary_pager_build_clock();' in ui,
      "Clock/Home/Settings pager must remain unchanged")
check(ui.count('screen=lv_obj_create(NULL);')==1,
      "Multiple LVGL roots reintroduced")
check('if(song.state==LF_RECOG_MATCH && song.lyric_lines>0' in ui and
      'build_timed_lyrics_now(&song);' in ui,
      "Real LRC lyrics viewer must remain available")
check('No background listening' in recognition or
      'lf_recog_start(void)' in recognition,
      "Manual recognition entrypoint missing")
print("PASS: LyricFlash 0.8.41 large Play Home source contracts")
