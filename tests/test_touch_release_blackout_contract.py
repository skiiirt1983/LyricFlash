#!/usr/bin/env python3
"""Guard against a reentrant snap during swipe release and missing active-page repaint."""
from pathlib import Path
s=(Path(__file__).resolve().parents[1]/"main/ui.c").read_text()
def part(a,b):return s.split(a,1)[1].split(b,1)[0]
e=part("static void primary_pager_scroll_event(lv_event_t *e)\n{","static lv_obj_t *primary_page_create")
w=part("static void primary_pager_watchdog(uint64_t current)\n{","static void primary_pager_scroll_event")
c=part("static void primary_pager_commit(int index)\n{","static void primary_pager_start_snap")
assert "lv_event_get_target(e)!=primary_pager" in e
assert "primary_pager_start_snap(target);" not in e
assert "lv_obj_scroll_to_x(primary_pager" not in e
assert "primary_pager_last_move_ms=now_ms();" in e
assert "primary_pager_start_snap(target);" in w
assert "lv_indev_get_state(indev)==LV_INDEV_STATE_PRESSED)return;" in w
assert "if(settled_move && primary_pager_pages[index])" in c
assert "lv_obj_invalidate(primary_pager_pages[index]);" in c
assert "make_base();" not in e, "Never rebuild entire UI from pager scroll callback"
assert "lf_clock_clear();" not in e, "Do not destroy active Clock on release"
assert s.count("lv_screen_load(screen);")==1
print("PASS: finger-up snap deferred, bubbled scroll isolated, active scene repainted")
