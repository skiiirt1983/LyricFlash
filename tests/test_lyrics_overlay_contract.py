#!/usr/bin/env python3
"""0.8.51: successful song-ID opens an isolated lyrics child of the existing screen."""
from pathlib import Path
p=Path(__file__).resolve().parents[1]
ui=(p/"main/ui.c").read_text()
net=(p/"main/network.c").read_text()
header=(p/"main/ui.h").read_text()
version=(p/"main/version.h").read_text()
assert 'LF_VERSION "0.8.67-alpha"' in version
assert "static lv_obj_t *timed_overlay;" in ui
scene=ui.split("static void build_timed_lyrics_now(const lf_recog_snapshot_t *song)\n{",1)[1].split("\nstatic void build_preview_now",1)[0]
assert "if(primary_pager && screen)" in scene
assert "timed_overlay=lv_obj_create(screen);" in scene
assert "lv_obj_add_flag(primary_pager,LV_OBJ_FLAG_HIDDEN);" in scene
assert "surface=timed_overlay;" in scene
assert "timed_viewport=lv_obj_create(surface);" in scene
assert "timed_controls=lv_obj_create(surface);" in scene
assert "lv_obj_add_event_cb(surface,timed_toggle_controls" in scene
assert "make_base();" in scene and scene.index("if(primary_pager && screen)") < scene.index("make_base();")
assert "timed_scroll_timer=lv_timer_create(timed_scroll_frame,65,NULL)" in scene
assert "static void lyrics_overlay_close(void)" in ui
assert "if(timed_overlay)lyrics_overlay_close();" in ui
pager=ui.split("static void primary_pager_enter(int index)\n{",1)[1]
assert pager.index("if(timed_overlay)lyrics_overlay_close();") < pager.index("if(!primary_pager)")
assert "if(!primary_pager||primary_pager_initializing||asleep||timed_overlay)return;" in ui
assert "const bool exiting_lyrics=timed_overlay!=NULL;" in ui
assert "const bool mute_invalidation=disp&&screen&&!recognition_open&&!exiting_lyrics;" in ui
assert "atomic_store(&ui_lyrics_scene_phase,6U);" in ui
assert "out->lyrics_scene_phase=atomic_load(&ui_lyrics_scene_phase);" in ui
assert "lyrics_scene_phase" in header and '"lyrics_scene_phase"' in net
print("PASS: cached pager lyrics overlay, 65ms redraw cadence, stage diagnostics")
