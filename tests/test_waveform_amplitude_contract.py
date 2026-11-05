#!/usr/bin/env python3
"""Waveform visual gain only; preserve real microphone waveform behavior."""
from pathlib import Path
p=Path(__file__).resolve().parents[1]
ui=(p/"main/ui.c").read_text()
wav=ui.split("static void home_recording_update(",1)[1].split("\n/* The Home screen",1)[0]
assert "const int signed_peak=(int)song->capture_visual_signed;" in wav
assert "home_record_wave_bias_q8+=((signed_peak*256)-home_record_wave_bias_q8)/8;" in wav
assert "const int centered=signed_peak-home_record_wave_bias_q8/256;" in wav
assert "int swing=magnitude*8;" in wav
assert "if(swing>108)swing=108;" in wav
assert "int y=130+(centered<0?swing:-swing);" in wav
assert "if(y<12)y=12;" in wav and "if(y>248)y=248;" in wav
assert "song->captured_frames*360U" in wav
assert "current-home_record_last_tick<LF_RECORD_WAVE_REFRESH_MS" in wav
assert "lv_line_set_points(home_record_wave_segments[strip]," in wav
assert "LF_RECORD_WAVE_SEGMENTS 4U" in ui
assert "LF_RECORD_WAVE_STRIP_WIDTH 90U" in ui
assert "LF_RECORD_WAVE_REFRESH_MS 140U" in ui
assert "home_record_wave_segments[strip]" in wav
assert "const static int8_t phases[8]" not in ui
# Signed shape is unchanged; only vertical gain increases until existing cap.
def y_at(peak):
    swing=min(abs(peak)*8,108)
    return max(12,min(248,130+(swing if peak<0 else -swing)))
assert y_at(0)==130
for pval in (1,3,6,10,13):
    assert 130-y_at(pval)==y_at(-pval)-130
    assert 130-y_at(pval)==pval*8
    assert 130-y_at(pval)>(pval*7)//2
assert y_at(20)==22 and y_at(-20)==238
print("PASS: waveform vertical visual gain only; signed audio, shape and timing unchanged")
