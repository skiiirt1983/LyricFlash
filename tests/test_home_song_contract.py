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
check('LF_VERSION "0.8.60-alpha"' in version,"0.8.47 alpha version not set")
check('home_scan_button=lv_button_create(parent);' in home,"Missing single circular play surface")
check(home.count('lv_button_create(')==1,"Home must expose only one visual control")
check('button(parent,' not in home,"Old rectangular navigation buttons still on Home")
check('lv_obj_set_size(home_scan_button,154,154);' in home,"Insufficient play touch target")
check('LV_RADIUS_CIRCLE' in home,"Play control must be circular")
check('LV_SYMBOL_PLAY' in home and 'LV_SYMBOL_STOP' in refresh,"Play must switch to Stop when busy")
check('lv_font_montserrat_48' in home,"Large play symbol is missing")
check('home_decor_ring(parent,200' in home and 'home_decor_ring(parent,184' in home,

      "Missing light-weight circular visual rings")
check('lv_obj_add_event_cb(home_scan_button,on_home_scan,LV_EVENT_CLICKED,NULL);' in home,
      "Play not wired to manual song scan")
check('lf_recog_start()' in ui and 'lf_recog_stop()' in on_click,
      "Play must manually start and stop recognition")
check('lf_recog_has_token()' in on_click and 'lf_network_is_connected()' in on_click,
      "Missing actionable failure states")
check('lf_recog_snapshot(&song)' in refresh and 'No Song Lyrics Available' in refresh,
      "Home must show a single error only when song lyrics are unavailable")
check('home_play_hint=home_caption(' not in home and
      'home_scan_status=home_caption(' not in home and
      'home_lyric_line=home_caption(' not in home,
      "Remove redundant TAP PLAY, MIC OFF and READY labels from Home")
check('home_song_artist=home_caption(' not in home,
      "Remove duplicate below-button song description")
check('lv_obj_add_event_cb(details[i],on_preview,LV_EVENT_CLICKED,NULL);' not in home,
      "Home should have only the one manual Play button")
check('LV_OBJ_FLAG_GESTURE_BUBBLE' in home and 'on_lvgl_gesture' in home,
      "Play touch must preserve swipe navigation")
check('battery_status=home_caption' in home,"Home lost charging/battery indicator")
check('lv_obj_set_size(brand_row,218,88);' in home and
      'lv_obj_set_pos(mark,4,2);' in home and
      'lv_obj_set_pos(wordmark_col,73,2);' in home and
      'lv_obj_set_size(mark,84,84);' in home and
      'lv_obj_set_flex_flow(wordmark_col,LV_FLEX_FLOW_COLUMN);' in home,
      "Brand icon and labels must use one compact flex row")
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
check('LF_HOME_PLAYER_CENTER_Y 18' in ui and
      'lv_obj_align(ring,LV_ALIGN_CENTER,0,LF_HOME_PLAYER_CENTER_Y);' in ui and
      'lv_obj_align(home_scan_button,LV_ALIGN_CENTER,0,LF_HOME_PLAYER_CENTER_Y);' in home,
      "Decorative rings and button must share one exact optical center")
check('LF_HOME_SCAN_SETTLE_MS 500U' in ui and
      'home_scan_armed=true;' in on_click and
      'current-home_scan_released_ms>=LF_HOME_SCAN_SETTLE_MS' in ui and
      'if(page!=0 || asleep || nav_transition_active || nav_queued)' in ui,
      "Recording must start after finger release and cancel on navigation/sleep")
print("PASS: compact circular-safe Home, single Play button, error-only caption, native brand")
