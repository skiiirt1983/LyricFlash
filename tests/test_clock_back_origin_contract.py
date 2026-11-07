#!/usr/bin/env python3
"""0.8.89 context-sensitive Clock Settings Back and one-pass pager return."""
from pathlib import Path
ui=(Path(__file__).resolve().parents[1]/"main/ui.c").read_text()
assert "static uint8_t clock_settings_return_page=2;" in ui
popup=ui.split("static void on_popup_settings(lv_event_t *e)",1)[1].split("static void show_clock_settings_popup",1)[0]
assert "clock_settings_return_page=6;" in popup
callbacks=ui.split("static void on_clock_options(lv_event_t *e)",2)[-1].split("static void on_cycle_face",1)[0]
assert "if(page==2)clock_settings_return_page=2;" in callbacks
assert "else if(page==6)clock_settings_return_page=6;" in callbacks
assert "if(clock_settings_return_page==6)build_clock();" in callbacks
assert "else build_settings();" in callbacks
clock=ui.split("static void build_basic_settings_now(void)\n{",1)[1].split("static void refresh_debug(void)\n{",1)[0]
assert "prefs->text_color, on_clock_settings_back, NULL" in clock
nav=ui.split("static void nav_drain(void *arg)",1)[1].split("static void nav_schedule",1)[0]
assert "if(screen && primary_pager && previous_page!=page && !asleep)" in nav
assert nav.index("if(screen && primary_pager && previous_page!=page && !asleep)")<nav.index("nav_transition_begin(direction);")
assert "if(was_pager && primary_pager){" in nav
print("PASS: Clock-popup Back goes to Clock, Settings-origin Back to Settings, pager entry full refresh")
