#!/usr/bin/env python3
"""Read-only structural guards for PrintSphere-inspired, physically smooth paging.
No firmware build is triggered by this test.
"""
from pathlib import Path
ui=(Path(__file__).resolve().parents[1]/"main/ui.c").read_text()
def region(start,end):
    assert ui.count(start)==1, start
    assert ui.count(end)>=1, end
    return ui.split(start,1)[1].split(end,1)[0]
pager=region("static void pager_build(int index)\n{","static void pager_request_async(void *arg)")
assert pager.index("lv_screen_load(handoff)") < pager.index("lv_obj_delete(old)") < pager.index("lv_obj_t *root=lv_obj_create(NULL)")
assert "lv_obj_create(container)" in pager
assert pager.count("lv_obj_create(container)") == 3
assert "lv_obj_set_scroll_snap_x(container,LV_SCROLL_SNAP_NONE)" in pager
assert "pager_render_target=pager_settings;render_settings();" in pager
assert "pager_render_target=pager_home;render_home();" in pager
assert "pager_render_target=pager_clock;render_clock();" in pager
assert pager.index("pager_render_target=NULL") < pager.index("lv_screen_load(root)")
scroll=region("static void pager_scroll_event(lv_event_t *e)\n{","static void pager_build(int index)")
assert "lv_indev_get_state(indev)==LV_INDEV_STATE_PRESSED" in scroll
assert "if(abs(movement)>=LF_PAGER_WIDTH/5)" in scroll
assert "lv_obj_scroll_to_x(pager_scroll,target_x,LV_ANIM_ON)" in scroll
assert "pager_select(nearest)" in scroll
assert "lv_obj_delete" not in scroll
weather=region("static void build_weather_details(void)\\n{","static void build_weather_location(void)")
assert weather.index("lv_screen_load(handoff)") < weather.index("lv_obj_delete(cached)") < weather.index("lf_weather_visual_build(screen)")
assert "if(pager_root && lv_screen_active()==pager_root)" in weather
assert "lv_screen_load" not in scroll
intent=region("static void swipe_intent(lv_dir_t direction)\n{","static void observe_touch_position(void)")
assert "lv_screen_active()==pager_root" in intent
assert "direction==LV_DIR_LEFT || direction==LV_DIR_RIGHT" in intent
makebase=region("static void make_base(void)\n{","static void on_wifi(lv_event_t *e)")
assert "if(pager_render_target)" in makebase
assert "if(!pager_render_target)" in makebase
assert "lv_obj_delete_async(old_screen)" in makebase
assert "lv_obj_delete(old_screen)" not in makebase
assert "lv_obj_set_style_translate_x(" not in ui
assert "nav_settle(" not in ui
print("PASS: cached core pages follow touch; Weather cleanup is deferred; root screens never translate")
