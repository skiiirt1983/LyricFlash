#!/usr/bin/env python3
"""0.8.45 three-line lyrics layout, battery status and NVS calibration gates."""
from pathlib import Path
root=Path(__file__).resolve().parents[1]
ui=(root/"main/ui.c").read_text()
settings=(root/"main/settings.c").read_text()
schema=(root/"main/settings.h").read_text()
scene=ui.split("static void build_timed_lyrics_now(const lf_recog_snapshot_t *song)\n{",1)[1].split("static void build_preview_now",1)[0]
assert "LF_CONFIG_SCHEMA 16U" in schema
assert "face_time_color[6]" in schema and "face_battery_color[6]" in schema
assert "face_weather_color[6]" in schema and "face_date_color[6]" in schema
assert ".timing_offset_ms = 0" in settings
assert "schema<13 && s->timing_offset_ms==-2000" in settings
assert 'nvs_set_i32(h, "offset_ms", s->timing_offset_ms)' in settings
assert "lf_song_cache_load_profile(" in ui
assert "lf_song_cache_store_profile(" in ui
assert "timed_song_custom" in ui
assert '"SAVE CURRENT"' not in ui and 'button(timed_controls,"RESET"' in ui
for token in ("timed_line_label(0,LF_TIMED_TEXT_WIDTH","timed_line_label(1,LF_TIMED_TEXT_WIDTH",
              "timed_line_label(2,LF_TIMED_TEXT_WIDTH","timed_battery=lv_label_create(surface)",
              "timed_controls=lv_obj_create(surface)",
              "timed_hint_until_ms=now_ms()+5000","RESET",
              "LV_EVENT_LONG_PRESSED"):
    assert token in scene,token
for token in ("lv_label_set_long_mode(line,LV_LABEL_LONG_WRAP)",
              "lv_obj_set_height(line,LV_SIZE_CONTENT)",
              "battery.charging?0x42DC89",
              "battery.percentage<=20)?0xFFD34D",
              "current>=timed_controls_until_ms"):
    assert token in ui,token
assert "lyric_logo_at_top(screen)" not in scene
assert "lv_obj_set_style_translate_y(timed_scroll_group,target,0)" in ui
assert "timed_viewport=lv_obj_create(surface);" in ui
assert "LV_OBJ_FLAG_OVERFLOW_VISIBLE" in ui
assert "lv_obj_set_style_text_opa(row,opacity,0);" in ui
assert "const int16_t target=-(int16_t)((uint32_t)progress*(uint16_t)pitch/1024U);" in ui
assert "timed_scroll_timer=lv_timer_create(timed_scroll_frame,65,NULL)" in ui
assert "song->title,38" not in scene and "song->artist,77" not in scene
assert "lv_obj_t *focus=lv_obj_create(screen)" not in scene
print("PASS: fixed clipped/faded lyric viewport and neutral timing offset persisted")
