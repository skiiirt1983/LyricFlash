#!/usr/bin/env python3
"""0.8.80: maintain a hard budget on recording visualization and safe pager motion."""
from pathlib import Path
root=Path(__file__).resolve().parents[1]
ui=(root/"main/ui.c").read_text()
worker=(root/"main/recognition.c").read_text()
wave=ui.split("static void home_recording_update(",1)[1].split("/* The Home screen",1)[0]
draw=ui.split("static void primary_render_home_content(",1)[1].split("static void render_home(",1)[0]
assert "#define LF_RECORD_WAVE_SEGMENTS 4U" in ui
assert "#define LF_RECORD_WAVE_STRIP_WIDTH 90U" in ui
assert "#define LF_RECORD_WAVE_STRIP_POINTS 26U" in ui
assert "#define LF_RECORD_WAVE_REFRESH_MS 140U" in ui
assert "home_record_wave_points[LF_RECORD_WAVE_SEGMENTS][LF_RECORD_WAVE_STRIP_POINTS]" in ui
assert "lv_obj_set_size(strip,LF_RECORD_WAVE_STRIP_WIDTH,260);" in draw
assert "lv_obj_align(strip,LV_ALIGN_CENTER,-135+(int32_t)i*90,-4);" in draw
assert "home_record_wave_segments[i]=strip;" in draw
assert "if(i)lv_obj_add_flag(strip,LV_OBJ_FLAG_HIDDEN);" in draw
assert "home_record_last_frames=UINT32_MAX;" in wave
assert "if(current-home_record_last_tick<LF_RECORD_WAVE_REFRESH_MS" in wave
assert "const unsigned strip=x>=360U?LF_RECORD_WAVE_SEGMENTS-1U:" in wave
assert "lv_line_set_points(home_record_wave_segments[strip]," in wave
assert "home_record_wave_counts[strip]=count;" in wave
assert "if(count<LF_RECORD_WAVE_STRIP_POINTS)" in wave
assert "if(home_record_cancel_pending_attempt==song->attempts)return;" in wave
assert "lv_line_set_points(home_record_wave,home_record_wave_points," not in ui
# 10 seconds sampled at 140ms gives ~18 samples/90px strip plus a seam point.
for strip in range(4):
    samples=sum(((360*ms//10000)//90 if ms<10000 else 3)==strip
                for ms in range(140,10001,140))
    assert samples+1<=26, (strip,samples)
assert "lv_anim_set_duration(anim,185U+(uint32_t)(105U*remaining/466U));" in ui # native easing
assert "primary_pager_frame_pending" in ui
assert "LF_PAGER_MAX_FRAME_STEP_PX" not in ui
assert "lv_obj_scroll_to_x(primary_pager,target*466,LV_ANIM_ON);" in ui
assert "#define LF_NAV_REVEAL_STEPS 12" in ui
assert "#define LF_NAV_REVEAL_PERIOD_MS 28" in ui
assert "lv_obj_invalidate(primary_pager_pages[index]);" in ui
assert "if(!settled_move)lv_obj_invalidate(clock_scene);" in ui
assert "lv_obj_set_scroll_dir(primary_pager,LV_DIR_HOR);" in ui
assert "lv_obj_remove_flag(primary_pager,LV_OBJ_FLAG_SCROLL_ELASTIC);" in ui
assert "lv_obj_remove_flag(primary_pager,LV_OBJ_FLAG_SCROLL_CHAIN_HOR);" in ui
assert "LF_RECOG_UPLOAD_BUDGET_MS 60000ULL" in worker
print("PASS: bounded segmented 7Hz waveform, paced page reveal and preserved scroll bounds")
