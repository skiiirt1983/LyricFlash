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
for source in (settings,api):
    assert "basic_font_size" in source and "200" in source
assert "transform_scale" not in basic  # 1.0.3: native font sizes, no scaled layers
assert "lf_clock_custom_font_px(family," in basic
assert "transform_scale" not in clock
assert "basic_digit_height(" in basic and "basic_digit_cell(" in basic
assert "0..200" in config
assert r'max=\"200\"' in html

assert "LF_CONFIG_SCHEMA 16U" in config
for source in (settings,config,clock,api,ui):
    assert "face_battery_x" in source and "face_battery_y" in source
assert "styled_digit(" in basic and "p->basic_font_style>=5" in basic
assert "p->basic_font_style<5?p->basic_font_style:0" in basic
assert "return \"Brolex Diver\"" in clock
assert 'face_text(parent,"XII"' in clock and 'face_text(parent,"VI"' in clock
assert 'lv_obj_align(battery_label,LV_ALIGN_CENTER' in clock
for name in ("Deco Engraved","Bauhaus Geometry","Broadway Lights","Stout Display",
             "Ice Crystals","Playful","Brush Motion","Roadster","Relief Engraved",
             "TXT Terminal","Neon Outline","Stencil","Gothic Block","Dot Matrix","Thin Line"):
    assert name in html,name

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
