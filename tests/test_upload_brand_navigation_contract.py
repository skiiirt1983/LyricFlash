#!/usr/bin/env python3
"""0.8.61: audio transport stall visibility, stacked logo, lyrics touch semantics."""
from pathlib import Path
r=Path(__file__).resolve().parents[1]
ui=(r/"main/ui.c").read_text()
worker=(r/"main/recognition.c").read_text()
header=(r/"main/recognition.h").read_text()
network=(r/"main/network.c").read_text()
assert 'LF_VERSION "0.8.79-alpha"' in (r/"main/version.h").read_text()
assert "uint64_t upload_progress_ms;" in header
assert "result.upload_progress_ms=lf_recog_now_ms();" in worker
assert "result.upload_bytes_sent+=(uint32_t)written;" in worker
assert '"upload_progress_age_ms"' in network
assert "current-song->upload_progress_ms>18000U" in ui
assert 'home_warning=upload_stalled?"Upload stalled - retry":' in ui
assert "lf_recog_stop(); /* cooperative cancel" in ui
home=ui.split("static void primary_render_home_content(lv_obj_t *parent)\n{",1)[1].split("static void render_home(void)",1)[0]
assert "lv_obj_set_size(brand_row,218,88);" in home
assert "lv_obj_set_size(mark,84,84);" in home
assert "lv_obj_set_flex_flow(wordmark_col,LV_FLEX_FLOW_COLUMN);" in home
assert 'lv_label_set_text(lyric_mark,"LYRIC");' in home
assert 'lv_label_set_text(flash_mark,"FLASH");' in home
assert 'lv_font_montserrat_28' in home
scene=ui.split("static void build_timed_lyrics_now(const lf_recog_snapshot_t *song)\n{",1)[1].split("static void build_preview_now(void)",1)[0]
assert "lv_obj_align(timed_battery,LV_ALIGN_TOP_MID,0,43);" in scene
assert "lv_obj_add_event_cb(surface,timed_double_tap_home,LV_EVENT_SHORT_CLICKED,NULL);" in scene
assert "lv_obj_add_event_cb(surface,timed_toggle_controls,LV_EVENT_LONG_PRESSED,NULL);" in scene
cb=ui.split("static void timed_double_tap_home(lv_event_t *e)\n{",1)[1].split("static void timed_toggle_controls",1)[0]
assert "current-timed_last_short_tap_ms<=360U" in cb
assert "build_home(); /* deferred navigation" in cb
assert "timed_controls" in cb and "LV_OBJ_FLAG_HIDDEN" in cb
assert "if(timed_overlay)lyrics_overlay_close();" in ui
assert "swipe_candidate_page=0;" in ui
print("PASS: stalled upload cancels; logo stacked; battery unclipped; short double tap Home, long hold and swipe preserved")
