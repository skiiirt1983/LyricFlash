#!/usr/bin/env python3
"""0.8.91: cross-page center draw timing and safe restored Home/Settings settle repaint."""
from pathlib import Path
root=Path(__file__).resolve().parents[1]
ui=(root/"main/ui.c").read_text()
header=(root/"main/ui.h").read_text()
net=(root/"main/network.c").read_text()
commit=ui.split("static void primary_pager_commit(int index)",1)[1].split("/* PrintSphere-like",1)[0]
assert "if(settled_move && primary_pager_pages[index])" in commit
assert "lv_obj_invalidate(primary_pager_pages[index]);" in commit
assert "if(settled_move && index==0 && primary_pager_pages[0])" not in commit
assert "lv_obj_remove_flag(clock_scene,LV_OBJ_FLAG_HIDDEN);" in commit
assert "if(!settled_move)lv_obj_invalidate(clock_scene);" in commit
assert "lf_clock_update();" in commit
assert "lv_obj_set_scroll_dir(primary_pager," not in commit
assert "pager_mid_draw_pair=0;" in ui
assert "offset>=150 && offset<=316 && x<932" in ui
assert "pager_mid_draw_pair=(uint8_t)(x<466?1:2);" in ui
assert "if(pager_mid_draw_pair){" in ui
assert "atomic_fetch_add(&pager_mid_draw_samples[pair],1);" in ui
for k in ("pager_clock_home_mid_draw_last_us","pager_clock_home_mid_draw_max_us","pager_clock_home_mid_draw_samples","pager_home_settings_mid_draw_last_us","pager_home_settings_mid_draw_max_us","pager_home_settings_mid_draw_samples"):
    assert k in header and k in ui and chr(34)+k+chr(34) in net
assert "LF_PAGER_STALL_GUARD_MS 1600U" in ui
assert "lv_obj_scroll_to_x(primary_pager,target*466,LV_ANIM_ON);" in ui
assert "atomic_store(&wake_overlay_pending,false);" in ui
print("PASS: midpoint draw profiler, Clock repaint safety, no unnecessary Home/Settings whole-page invalidation")
