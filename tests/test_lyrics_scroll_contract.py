#!/usr/bin/env python3
"""LyricFlash 0.8.44: real LRC-scrolling and reliable auto-open contracts."""
from pathlib import Path
p=Path(__file__).resolve().parents[1]
ui=(p/"main/ui.c").read_text()
clock=(p/"main/lyric_clock.c").read_text()
start=ui.index("static void timed_layout_rows(uint32_t ms,int index)")
end=ui.index("static void timed_start(lv_event_t *e)",start)
scene=ui[start:end]
assert "timed_rows[4]" in ui
assert "for(int slot=0;slot<4;slot++)" in scene
assert "const int centers[4]={-82,0,82,164};" in scene
assert "const uint32_t duration=end-start;" in scene
assert "(uint64_t)elapsed*1024U" in scene
assert "const int travel=(int)(progress*82U/1024U);" in scene
assert "timed_layout_rows(ms,index);" in scene
assert "current-timed_last_update_ms>=65" in ui
assert "timed_power_update_ms>=1000" in ui
assert "lf_lyric_clock_position(&timed_clock,current)" in scene
assert "timed_clock.running" in clock
assert 'timed_set_row(slot,line.text[0]?line.text:" ");' in scene
assert 'lf_recog_lyric_line((uint32_t)line_index,&line)' in scene
auto=ui[ui.index("static void auto_open_lyrics_deferred(void *unused)"):ui.index("static void home_refresh(uint64_t current)")]
assert "auto_open_pending=false;" in auto
assert "latest.attempts!=auto_open_attempt" in auto
assert "page==0 && !touch_held && !asleep" in auto
assert "auto_open_attempt=latest.attempts;" in auto
assert "build_preview();" in auto
assert "auto_open_pending=true;" in ui
assert "lv_async_call(auto_open_lyrics_deferred,NULL);" in ui
assert "lf_lyric_clock_adjust(&timed_clock,prefs->timing_offset_ms);" in ui
print("PASS: automatic lyrics opening after touch release, LRC-paced upward movement, cache throttling")
