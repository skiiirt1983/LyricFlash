#!/usr/bin/env python3
"""0.8.61: fixed 24px font, premeasured wraps, time gaps, independent timing offset."""
from pathlib import Path
p=Path(__file__).resolve().parents[1]
ui=(p/"main/ui.c").read_text()
sync_h=(p/"main/lyric_sync.h").read_text()
sync=(p/"main/lyric_sync.c").read_text()
version=(p/"main/version.h").read_text()
assert 'LF_VERSION "0.8.115-alpha"' in version
assert "LF_SYNC_DISPLAY_DELAY_MS 1000U" in sync_h
assert "lf_sync_visible_position_ms" in ui and "lf_sync_visible_position_ms" in sync
assert "lf_lyric_clock_position(&timed_clock,current)" in ui
assert "lf_song_cache_load_profile(" in ui
assert "lf_lyric_clock_adjust(&timed_clock,offset);" in ui
assert "lf_song_cache_store_profile(" in ui
a=ui.split("static void timed_set_row(int slot,const char *value)\n{",1)[1].split("static uint16_t timed_line_height_at",1)[0]
for bad in ("strlen(value)","n>115","n>90","n>62","n>39","lv_obj_set_style_text_font"):
    assert bad not in a,bad
scene=ui.split("static void build_timed_lyrics_now(const lf_recog_snapshot_t *song)\n{",1)[1].split("static void build_preview_now",1)[0]
assert "lv_text_get_size(&measured,line.text[0]?line.text:" in scene
assert "LF_TIMED_TEXT_WIDTH,LV_TEXT_FLAG_NONE" in scene
assert "timed_line_heights[timed_line_count]=(uint16_t)measured.y;" in scene
assert scene.count("timed_line_label(")==4
assert scene.count("LF_TIMED_TEXT_WIDTH,0x")==4
assert "lv_obj_set_style_text_font(line,&lv_font_montserrat_24,0);" in ui
assert "static void timed_position_rows(int index)" in ui
assert "lf_sync_row_pitch_px" in ui and "lf_sync_slide_fraction" in ui
assert "timed_row_centers[slot]+target" in ui
assert "timed_future_mask(ms,index)" in ui
assert "timed_visible_future_mask=future;" in ui
assert "3500ULL" in ui
assert "timed_scroll_timer=lv_timer_create(timed_scroll_frame,65,NULL)" in ui
print("PASS: fixed 24px font, preflight text heights, measured LRC gaps, 1s delay")
