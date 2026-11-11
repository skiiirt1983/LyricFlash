"""Concept: printed carrier plate under the Waveshare (Paul, 2026-11-11). Visual only, not in the build yet.

Board frame (BF): z = 0 at the PCB back. The plate sits under the three M2 SMT standoffs (screw faces at
BF z -3.5), avoids the pin header and the two plug zones, and has three tabs past the board's edge that screw
down onto posts in the base (M2.5 x 6 pan head into a pressed-in M2.5 nut).
"""
from __future__ import annotations

import math
import sys
from pathlib import Path

import cadquery as cq
from shapely.geometry import Point, Polygon, box
from shapely.ops import unary_union

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))
import params as P  # noqa: E402
from lfcase import enclosure as E  # noqa: E402

T_PLATE = 2.4          # plate thickness
Z_TOP = P.STANDOFF_Z[0]  # -3.5: plate top touches the standoff screw faces
SO = P.MOUNT_HOLES_XY
TABS = [(-17.5, -18.5), (0.0, 24.0), (-19.5, 12.3)]   # BF, past the PCB edge (r 23)


def outline():
    bosses = [Point(x, y).buffer(2.6) for x, y in SO]
    core = unary_union(bosses).convex_hull
    from shapely.ops import nearest_points
    arms = []
    for (tx, ty) in TABS:
        q = nearest_points(core, Point(tx, ty))[0]           # arm from the nearest point of the plate body
        arms.append(unary_union([Point(q.x, q.y).buffer(3.0), Point(tx, ty).buffer(4.0)]).convex_hull)
    shp = unary_union([core] + arms)
    keep = [box(-10.93, -20.4, 10.92, -17.2),                 # pin header (to BF z -5.8)
            box(-11.63, 14.92, -2.6, 21.6), box(2.3, 14.92, 11.32, 21.6),   # J1 / H3 plugs + wires
            box(-30, -8.5, -15.6, 8.5)]                       # USB-C receptacle + the base's USB insert
    return shp.difference(unary_union(keep))


def plate_bf():
    o = outline()
    if o.geom_type != "Polygon":
        o = max(o.geoms, key=lambda g: g.area)
    wp = cq.Workplane("XY").workplane(offset=Z_TOP - T_PLATE).polyline(list(o.exterior.coords)[:-1]).close()
    pl = wp.extrude(T_PLATE).val()
    for x, y in SO:                                          # M2 clearance + countersink from below
        pl = pl.cut(cq.Solid.makeCylinder(1.15, 10, cq.Vector(x, y, Z_TOP - 5)))
        pl = pl.cut(cq.Solid.makeCone(2.1, 0.9, 1.2, cq.Vector(x, y, Z_TOP - T_PLATE - 0.01)))
    for x, y in TABS:                                        # M2.5 clearance
        pl = pl.cut(cq.Solid.makeCylinder(1.45, 10, cq.Vector(x, y, Z_TOP - 5)))
    return pl


def screws_bf():
    out = []
    for x, y in SO:                                          # M2 x 3 flat head, from below
        out.append(cq.Solid.makeCone(1.9, 1.0, 1.2, cq.Vector(x, y, Z_TOP - T_PLATE)).fuse(
            cq.Solid.makeCylinder(1.0, 3.0, cq.Vector(x, y, Z_TOP - T_PLATE))))
    for x, y in TABS:                                        # M2.5 x 6 pan head, from above
        out.append(cq.Solid.makeCylinder(2.25, 1.6, cq.Vector(x, y, Z_TOP)).fuse(
            cq.Solid.makeCylinder(1.25, 6.0, cq.Vector(x, y, Z_TOP - 6.0))))
    return out


def posts_wf(zb):
    """Base posts under the tabs: from the floor up to the tab's underside, nut pocket at the top."""
    out = []
    for x, y in TABS:
        p = E.bf_point_to_wf((x, y, Z_TOP - T_PLATE), zb)
        h = p[2] - P.CASE_FLOOR
        post = cq.Solid.makeCylinder(4.2, h, cq.Vector(p[0], p[1], P.CASE_FLOOR))
        out.append(post)
    return out


if __name__ == "__main__":
    from lfcase.board import load_board
    from lfcase.render import Scene, save_labelled
    out = Path(__file__).resolve().parent.parent / "out" / "concepts"
    out.mkdir(parents=True, exist_ok=True)
    zb = E.board_z() + 3.0                                   # board raised so the plate clears the battery
    g = load_board(verbose=False)
    plate = plate_bf()
    scr = screws_bf()
    ORANGE, BLUE, GREEN, GREY, STEEL = (0.95, 0.55, 0.15), (0.35, 0.45, 0.8), (0.1, 0.42, 0.22), (0.7, 0.7, 0.75), (0.55, 0.55, 0.6)
    # 1: bench step, board face-down, plate on its back, seen from below-ish
    s = Scene(size=(1600, 1200))
    for k in ("pcb", "header", "j1", "h3", "usb_c", "standoffs", "key1", "key2", "misc_back", "mic1", "mic2"):
        s.add(g[k], GREEN if k == "pcb" else GREY)
    s.add(plate, ORANGE)
    for c in scr[:3]:
        s.add(c, STEEL)
    s.camera(((0.35, -0.55, -1.0), (0, 1, 0)), target=(0, 0, -3), zoom=1.3)
    save_labelled(s, str(out / "carrier_1_bench.png"),
                  title="1. On the bench: board's back, orange plate, 3x M2x3 flat heads into its own standoffs")
    # 2: in the case, from above, board hidden
    s = Scene(size=(1600, 1200))
    s.add(E._box(*E.battery_box()), BLUE)
    s.add(E.to_wf(plate, zb), ORANGE)
    for c in scr[3:]:
        s.add(E.to_wf(c, zb), STEEL)
    for p in posts_wf(zb):
        s.add(p, (0.25, 0.25, 0.28))
    x0, x1, y0, y1 = E.footprint()
    s.add(E._box((x0, y0, 0), (x1, y1, P.CASE_FLOOR)), (0.2, 0.2, 0.22))
    s.camera(((0.0, -0.25, 1.0), (0, 1, 0)), target=(0, 0, 20), zoom=1.05)
    save_labelled(s, str(out / "carrier_2_top.png"),
                  title="2. In the base, from above (board hidden): plate on 3 posts, M2.5x6 + nut each; battery (blue)")
    # 3: assembled, side view
    s = Scene(size=(1600, 1200))
    s.add(E._box(*E.battery_box()), BLUE)
    for k in ("pcb", "display", "pmma_frame", "header", "j1", "h3", "usb_c", "standoffs"):
        s.add(E.to_wf(g[k], zb), GREEN if k == "pcb" else GREY)
    s.add(E.to_wf(plate, zb), ORANGE)
    for c in scr:
        s.add(E.to_wf(c, zb), STEEL)
    for p in posts_wf(zb):
        s.add(p, (0.25, 0.25, 0.28))
    s.add(E._box((x0, y0, 0), (x1, y1, P.CASE_FLOOR)), (0.2, 0.2, 0.22))
    s.camera(((-0.9, -1.0, 0.35), (0, 0, 1)), target=(0, 0, 25), zoom=0.95)
    save_labelled(s, str(out / "carrier_3_assembled.png"),
                  title="3. Assembled (walls hidden): board on the plate, plate on its posts, battery standing on the right")
    print("plate area", round(outline().area, 1), "zb", zb)
