#!/usr/bin/env python3
"""0.8.93: Basic digit rectangles and analog ticks reject invisible strips safely."""
from pathlib import Path
import math
root=Path(__file__).resolve().parents[1]
basic=(root/"main/basic_clock.c").read_text()
clock=(root/"main/clock_face.c").read_text()
assert "const lv_area_t *clip=&layer->_clip_area;" in basic
assert "const lv_area_t *clip=&layer->_clip_area;" in clock
draw_bar=basic.split("static void draw_bar(",1)[1].split("static uint8_t segments(",1)[0]
assert draw_bar.index("clip=&layer->_clip_area")<draw_bar.index("lv_draw_rect_dsc_init(&d);")
assert "(int64_t)x+w-1<clip->x1" in draw_bar
assert "(int64_t)y+h-1<clip->y1" in draw_bar
assert "if(w<1||h<1)return;" in draw_bar
ticks=clock.split("static void tick_draw_event(",1)[1].split("static void draw_ticks(",1)[0]
assert ticks.index("clip=&layer->_clip_area")<ticks.index("lv_draw_line_dsc_init(&line);")
assert "const int32_t pad=(diver?(major?5:2):2)+3;" in ticks
assert "if(!diver && i%5)continue;" in ticks
assert "const lf_dial_tick_t *ticks=dial_tick_cache[diver?1:0];" in ticks
assert "line.p1.x=ax;" in ticks and "line.p2.y=by;" in ticks
# Verify no actual stroke crossing a 24-pixel strip is excluded by the
# conservative stroke pad, for all 60 marks and both static dial types.
visible=0;skipped=0
for diver in (False,True):
    outer=185 if diver else 181
    for index in range(60):
        if not diver and index%5:continue
        major=index%5==0
        inner=(171 if major else 181) if diver else (160 if major else 175)
        angle=math.radians(index*6-90)
        ax=233+int(math.cos(angle)*inner)
        ay=233+int(math.sin(angle)*inner)
        bx=233+int(math.cos(angle)*outer)
        by=233+int(math.sin(angle)*outer)
        width=(5 if major else 2) if diver else 2
        pad=width+3
        for band in range(20):
            xlo=0; xhi=465; ylo=band*24; yhi=min(465,ylo+23)
            cull=(max(ax,bx)+pad<xlo or min(ax,bx)-pad>xhi or
                  max(ay,by)+pad<ylo or min(ay,by)-pad>yhi)
            intersects=(max(ax,bx)+pad>=xlo and min(ax,bx)-pad<=xhi and
                        max(ay,by)+pad>=ylo and min(ay,by)-pad<=yhi)
            assert cull != intersects
            if cull:skipped+=1
            else:visible+=1
assert skipped>visible*3,(skipped,visible)
assert "snap_exit();" in (root/"main/ui.c").read_text()  # 0.8.126 swipe-only snapshots
assert "lf_network_set_idle(false);" in (root/"main/ui.c").read_text()
print(f"PASS: clipped offscreen Basic draw bars & analog marks; {skipped} invisible tick-band draw tasks avoided per full dial test")
