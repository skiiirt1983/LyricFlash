#!/usr/bin/env python3
"""0.8.45 manual microphone visualization and clean lyric transitions."""
from pathlib import Path
p=Path(__file__).resolve().parents[1]
ui=(p/"main/ui.c").read_text()
header=(p/"main/recognition.h").read_text()
worker=(p/"main/recognition.c").read_text()
version=(p/"main/version.h").read_text()
assert 'LF_VERSION "0.8.117-alpha"' in version
# Feed the renderer a scalar only: the existing capture is still manual.
assert "uint8_t capture_visual_level;" in header
assert "int8_t capture_visual_signed;" in header
assert "static void set_count(uint32_t frames,const int16_t *tdm)" in worker
assert "set_count(frames,tdm);" in worker
assert "result.capture_visual_level=level;" in worker
assert "result.capture_visual_signed=signed_peak;" in worker
assert "result.capture_visual_level=0;" in worker
assert "lf_recog_start()" in ui and "lf_recog_stop()" in ui
assert "LF_RECORD_WAVE_STRIP_POINTS 26U" in ui
assert "lv_point_precise_t home_record_wave_points" in ui
assert "static void home_recording_update(" in ui
assert "home_recording_update(&song,current);" in ui
assert "home_recording_update(&scan,current);" in ui
assert "song->captured_frames*360U" in ui
assert "song->capture_visual_signed" in ui
assert "home_record_wave_bias_q8+=((signed_peak*256)-home_record_wave_bias_q8)/8;" in ui
assert "const int centered=signed_peak-home_record_wave_bias_q8/256;" in ui
assert "int swing=magnitude*8;" in ui
assert "if(swing>108)swing=108;" in ui
assert "int y=130+(centered<0?swing:-swing);" in ui
assert "const static int8_t phases[8]" not in ui
assert "positive_peak" in worker and "negative_peak" in worker
assert "if(y<12)y=12;" in ui and "if(y>248)y=248;" in ui
assert "lv_obj_set_style_line_width(strip,3,0);" in ui
assert "lv_obj_set_style_border_width(record_ring,6,0);" in ui
assert 'lv_label_set_text(home_record_loading,"IDENTIFYING...")' in ui
assert "const bool opening=song->state==LF_RECOG_MATCH" in ui
assert ('lv_label_set_text(lyric_mark,"LYRIC");' in ui and
        'lv_label_set_text(flash_mark,"FLASH");' in ui)

assert "lv_line_set_points(home_record_wave_segments[strip]," in ui
assert 'lv_color_hex(0xFFD02C)' in ui
assert 'lv_color_hex(0x258854)' in ui
assert "song->state==LF_RECOG_CAPTURE" in ui
assert "song->state==LF_RECOG_UPLOAD" in ui
assert "song->state==LF_RECOG_LYRICS" in ui
assert "lv_style_set_transform_width(&press_style,12)" in ui
assert "No Song Lyrics Available" in ui
assert "auto_open_lyrics_deferred" in ui
assert "const bool recognition_open=auto_open_transition;" in ui
assert "auto_open_attempt=timed_scan_attempt;" in ui
assert "if(page==1 && timed_lyrics_mode && timed_line_count>0)" in ui
assert "lv_obj_invalidate(screen);" in ui
assert "ui_pointer_is_down()" in ui
assert "lv_obj_has_flag(home_record_layer,LV_OBJ_FLAG_HIDDEN)" in ui
assert "lv_obj_move_foreground(home_record_layer);" in ui

assert "lyric_logo_at_top(screen);" not in ui
assert "lv_line_set_points(strike,bolt,7)" not in ui  # no logo on sacred lyrics page
assert "LV_SYMBOL_CHARGE" in ui
assert "ESTIMATED SYNC" not in ui
assert '"SYNC %02lu:%02lu  |  offset %c%ld.%01lds"' in ui
print("PASS: large bounded waveform and acknowledged, retryable recognition-to-lyrics navigation")
