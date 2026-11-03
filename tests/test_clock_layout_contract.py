#!/usr/bin/env python3
"""Clock face size, placement, original glyphs and firmware/UI persistence."""
from pathlib import Path
root=Path(__file__).resolve().parents[1]
settings=(root/"main/settings.c").read_text()
config=(root/"main/settings.h").read_text()
clock=(root/"main/clock_face.c").read_text()
basic=(root/"main/basic_clock.c").read_text()
html=(root/"main/web_basic_html.h").read_text()
api=(root/"main/network.c").read_text()
ui=(root/"main/ui.c").read_text()
for component in ("time","battery","date","weather"):
    for axis in ("x","y"):
        name=f"basic_{component}_{axis}"
        assert name in config and name in settings and name in api and name in ui and name in html,name
for name in ("basic_battery_size","basic_date_size","basic_weather_size"):
    assert name in config and name in settings and name in api and name in ui and name in html,name
assert "basic_battery_size = 85" in settings
assert "LV_SYMBOL_BATTERY_3" in clock and "LV_SYMBOL_BATTERY_1" in clock
assert "basic_element_font" in clock
assert "lf_basic_clock_update_time(t);" in clock
assert "lf_basic_clock_clear();" in clock
for token in ("segment_digit(","pixel_digit(","grid_digits[10][7]",
              "basic_digits_draw(","lv_draw_rect(layer,&d,&a)"):
    assert token in basic,token
for font in ("Modern Bold","Digital 7","Retro 8","Vintage","Classic Sans"):
    assert font in html,font
print("PASS: persistent clock placements/sizes, dynamic battery icon and original font families")
