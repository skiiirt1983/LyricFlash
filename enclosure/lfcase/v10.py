"""Phase 2 v10 (Paul, 2026-11-11). See DESIGN_RULES.md.

- Carrier plate: the board screws to a printed plate through its own three M2 standoffs (M2 x 3 flat heads from
  below). The plate's three tabs, past the board's edge, screw down onto posts in the base (M2.5 x 6 pan head into
  an M2.5 nut slid into a side slot in the post).
- Shell to base: an alignment rim hangs from the shell inside the base's walls, and four 4 x 2 magnet pairs sit
  in the corners. The shell comes on last, straight down along the board's normal.
- Switch: SS-12D00 pressed into a pocket on the inside of the left wall at the bottom, under the USB-C. The knob
  goes out through a 1 mm skin.
"""
from __future__ import annotations

import math

import cadquery as cq
import numpy as np
from shapely.geometry import Point, Polygon, box
from shapely.ops import nearest_points, unary_union

import params as P
from lfcase import coupons_a as A
from lfcase import enclosure as E

T = math.radians(P.TILT_DEG)
S = P.SEAM_Z


# ------------------------------------------------------------------ helpers
def _wf_to_bf_xy(x, y, z_bf):
    """BF (x, y) of the point on the plane BF z = z_bf above WF (x, y)."""
    return x - P.BOARD_X, (y + z_bf * math.sin(T)) / math.cos(T)


def _poly_bf(poly_wf, z_bf):
    """WF outline -> BF outline on the plane BF z = z_bf (points straight above/below)."""
    def m(c):
        return [_wf_to_bf_xy(x, y, z_bf) for x, y in c]
    return Polygon(m(poly_wf.exterior.coords), [m(h.coords) for h in poly_wf.interiors])


def _extrude_bf(poly_bf, z0, z1):
    """Prism of a BF polygon from BF z0 to z1, in the world frame."""
    polys = list(poly_bf.geoms) if hasattr(poly_bf, "geoms") else [poly_bf]
    out = None
    for p in polys:
        if p.is_empty or p.area < 1e-3:
            continue
        wp = cq.Workplane("XY").workplane(offset=z0).polyline(list(p.exterior.coords)[:-1]).close()
        for h in p.interiors:
            wp = wp.polyline(list(h.coords)[:-1]).close()
        s = wp.extrude(z1 - z0).val()
        out = s if out is None else out.fuse(s)
    return out


def _cavity_poly(off=0.0):
    """Inner wall outline (WF), offset `off` outward (into the wall); negative = into the cavity."""
    x0, x1, y0, y1 = E.footprint()
    R, w = P.CASE_CORNER_R, P.CASE_WALL
    return box(x0 + R, y0 + R, x1 - R, y1 - R).buffer(R - w + off, resolution=48)


# ------------------------------------------------------------------ carrier plate
Z_TOP = P.STANDOFF_Z[0]                       # plate top = the standoffs' screw faces (BF)


def _z_under():
    return Z_TOP - P.CARRIER_T


def carrier_outline():
    bosses = [Point(x, y).buffer(2.6) for x, y in P.MOUNT_HOLES_XY]
    core = unary_union(bosses).convex_hull
    arms = []
    for tx, ty in P.CARRIER_TABS:
        q = nearest_points(core, Point(tx, ty))[0]
        arms.append(unary_union([Point(q.x, q.y).buffer(3.0), Point(tx, ty).buffer(P.CARRIER_TAB_R)]).convex_hull)
    shp = unary_union([core] + arms)
    keep = [box(-10.93, -20.4, 10.92, -17.2),                       # pin header (down to BF z -5.8)
            box(-11.63, 14.92, -2.6, 21.6), box(2.3, 14.92, 11.32, 21.6),   # J1 / H3 plugs + wires
            box(-30, -8.5, -15.6, 8.5)]                             # USB-C + the base's USB insert
    o = shp.difference(unary_union(keep))
    return max(o.geoms, key=lambda g: g.area) if hasattr(o, "geoms") else o


def carrier_bf():
    pl = _extrude_bf(carrier_outline(), _z_under(), Z_TOP)
    for x, y in P.MOUNT_HOLES_XY:                                   # M2 clearance + 90 deg countersink below
        pl = pl.cut(cq.Solid.makeCylinder(1.15, 10, cq.Vector(x, y, Z_TOP - 5)))
        pl = pl.cut(cq.Solid.makeCone(P.M2_HEAD_D / 2 + 0.15, 1.15, P.M2_HEAD_D / 2 + 0.15 - 1.15,
                                      cq.Vector(x, y, _z_under() - 0.01)))
    for x, y in P.CARRIER_TABS:                                     # M2.5 clearance
        pl = pl.cut(cq.Solid.makeCylinder(1.45, 10, cq.Vector(x, y, Z_TOP - 5)))
    return pl


def carrier(zb):
    return E.to_wf(carrier_bf(), zb)


def carrier_lowest_over(zb, xy_box):
    """Lowest WF z of the plate's underside over a WF (x0, y0, x1, y1) box (sampled)."""
    o = carrier_outline()
    zmin = None
    for x in np.arange(o.bounds[0], o.bounds[2], 0.5):
        for y in np.arange(o.bounds[1], o.bounds[3], 0.5):
            if not o.contains(Point(x, y)):
                continue
            p = E.bf_point_to_wf((x, y, _z_under()), zb)
            if xy_box[0] <= p[0] <= xy_box[2] and xy_box[1] <= p[1] <= xy_box[3]:
                zmin = p[2] if zmin is None else min(zmin, p[2])
    return zmin


def _hex_bf(cx, cy, z0, z1, af, ang):
    r = af / 2 / math.cos(math.radians(30))
    pts = [(cx + r * math.cos(ang + k * math.pi / 3), cy + r * math.sin(ang + k * math.pi / 3)) for k in range(6)]
    return cq.Workplane("XY").workplane(offset=z0).polyline(pts).close().extrude(z1 - z0).val()


def posts(b, zb):
    """A post under each tab, tied into the nearest wall by a fin. Top = the tab's underside plane. Pilot hole along
    the board normal for the M2.5 x 6 to self-tap into."""
    x0, x1, y0, y1 = E.footprint()
    w = P.CASE_WALL
    zu = _z_under()
    body = E.base_body(zb)
    below = E.halfspace_bf(zb, zu, above=False)
    info = []
    for tx, ty in P.CARRIER_TABS:
        p = E.bf_point_to_wf((tx, ty, zu), zb)
        walls = {"x-": (abs(p[0] - (x0 + w)), (x0 - 1, p[1])), "y-": (abs(p[1] - (y0 + w)), (p[0], y0 - 1)),
                 "y+": (abs(p[1] - (y1 - w)), (p[0], y1 + 1))}
        side, (d, (wx, wy)) = min(walls.items(), key=lambda kv: kv[1][0])
        post = E._box((p[0] - P.POST_HALF, p[1] - P.POST_HALF, 0), (p[0] + P.POST_HALF, p[1] + P.POST_HALF, 200))
        fw = P.POST_FIN_W / 2
        fin = E._box((min(p[0], wx) - (fw if side != "x-" else 0), min(p[1], wy) - (fw if side == "x-" else 0), 0),
                     (max(p[0], wx) + (fw if side != "x-" else 0), max(p[1], wy) + (fw if side == "x-" else 0), 200))
        b = b.fuse(post.fuse(fin).intersect(below).intersect(body))
        # v10.1 (Paul): the M2.5 x 6 self-taps into a pilot hole along the board normal; no nut, no slot
        hole = cq.Solid.makeCylinder(P.POST_PILOT_D / 2, P.POST_HOLE_DEPTH + 1, cq.Vector(tx, ty, zu - P.POST_HOLE_DEPTH))
        b = b.cut(E.to_wf(hole, zb))
        info.append(dict(tab=(tx, ty), wf=tuple(round(float(v), 2) for v in p), wall=side))
    return b, info


# ------------------------------------------------------------------ magnets + alignment rim
def magnet_points():
    """WF (x, y) of the four corner magnets: the inner corner arcs' centres."""
    x0, x1, y0, y1 = E.footprint()
    R = P.CASE_CORNER_R
    return [(x0 + R, y0 + R), (x0 + R, y1 - R), (x1 - R, y0 + R), (x1 - R, y1 - R)]


def magnet_bosses(b, zb):
    """Corner bosses in the base: top flush with the seam, 45 deg underside, magnet pocket in the top."""
    below = E.halfspace_bf(zb, S, above=False)
    cav = E._inner_cavity()
    pockets = []
    for mx, my in magnet_points():
        mb = _wf_to_bf_xy(mx, my, S)
        top = E.bf_point_to_wf((mb[0], mb[1], S), zb)[2]
        # v10.1: a solid corner block. Straight sides down past the pocket (MAGNET_WALL of plastic round the magnet,
        # plus the seam's tilt across the block), then a 45 deg underside back into the walls. (v10's pure cone
        # narrowed so fast that the pocket broke out through its side.)
        sx, sy = np.sign(mx), np.sign(my)
        r = P.MAGNET_D / 2 + P.MAGNET_WALL
        x0_, x1_ = sorted((mx - r * sx, mx + 12 * sx))
        ry = r + (P.MAGNET_H + P.MAGNET_DEPTH_EXTRA + P.MAGNET_WALL) * math.sin(T) + 0.4   # the pocket leans with the tilt
        y0_, y1_ = sorted((my - ry * sy, my + 12 * sy))
        z_str = top - (P.MAGNET_H + P.MAGNET_DEPTH_EXTRA + P.MAGNET_WALL + 2 * r * math.tan(T))
        straight = E._box((x0_, y0_, z_str), (x1_, y1_, top + 4.0))
        pl = cq.Plane(origin=(0, 0, z_str + 0.01), xDir=(1, 0, 0), normal=(0, 0, -1))
        pts = [(x0_, -y0_), (x1_, -y0_), (x1_, -y1_), (x0_, -y1_)]
        taper = cq.Workplane(pl).polyline(pts).close().extrude(30.0, taper=45).val()
        boss = straight.fuse(taper)
        boss = boss.intersect(cav).intersect(below)
        b = b.fuse(boss)
        pockets.append(E.to_wf(cq.Solid.makeCylinder(P.MAGNET_D / 2 + P.MAGNET_FIT, P.MAGNET_H + P.MAGNET_DEPTH_EXTRA + 0.5,
                                                     cq.Vector(mb[0], mb[1], S - P.MAGNET_H - P.MAGNET_DEPTH_EXTRA)), zb))
    for pk in pockets:
        b = b.cut(pk)
    return b


def shell_features(top, zb):
    """Alignment rim under the shell (inside the base's walls) and the shell's four magnet pockets."""
    c, tt, D = P.RIM_CLEAR, P.RIM_T, P.RIM_DEPTH
    band_wf = _cavity_poly(-c).difference(_cavity_poly(-c - tt))
    band = _poly_bf(band_wf, S)                                     # leans with the normal: clear at the front
    band = band.difference(Point(0, 0).buffer(P.RIM_BOARD_R, resolution=96))   # away from the board's edge
    for mx, my in magnet_points():                                  # the corner bosses locate there
        band = band.difference(Point(*_wf_to_bf_xy(mx, my, S)).buffer(P.MAGNET_D / 2 + P.MAGNET_RIM + 6.0))
    band = band.buffer(-0.45, join_style=2).buffer(0.45, join_style=2)          # no slivers
    rim = E.to_wf(_extrude_bf(band, S - D, S + 1.0), zb)
    rim = rim.intersect(_extrude_wf_cavity(-c))                     # and vertical where that's tighter (back)
    recess, _ = E._usb_recess(zb)
    rb = recess.BoundingBox()                                       # the whole USB-C bay: recess, cheeks, insert
    rim = rim.cut(E._box((-200, rb.ymin - P.CASE_WALL - 0.6, -50), (E.footprint()[0] + P.CASE_WALL + 4.0,
                                                                  rb.ymax + P.CASE_WALL + 0.6, 300)))
    rim = cq.Compound.makeCompound([s for s in rim.Solids() if s.Volume() > 5.0])   # no scraps
    top = top.fuse(rim)
    for mx, my in magnet_points():
        mb = _wf_to_bf_xy(mx, my, S)
        top = top.cut(E.to_wf(cq.Solid.makeCylinder(P.MAGNET_D / 2 + P.MAGNET_FIT, P.MAGNET_H + P.MAGNET_DEPTH_EXTRA + 0.5,
                                                    cq.Vector(mb[0], mb[1], S - 0.5)), zb))
    return top


def _extrude_wf_cavity(off):
    x0, x1, y0, y1 = E.footprint()
    R, w = P.CASE_CORNER_R, P.CASE_WALL
    return (cq.Workplane("XY").workplane(offset=-50).center((x0 + x1) / 2, (y0 + y1) / 2)
            .sketch().rect(x1 - x0 - 2 * (w - off), y1 - y0 - 2 * (w - off)).vertices().fillet(R - w + off)
            .finalize().extrude(400).val())


# ------------------------------------------------------------------ switch in the left wall
def switch_wall(b, zb):
    """SS-12D00 in a press-fit pocket on the inside of the left wall, knob out through a SWITCH_SKIN skin."""
    x0 = E.footprint()[0]
    L, W, H = P.SWITCH_MEAS_L, P.SWITCH_MEAS_W, P.SWITCH_MEAS_H
    f = P.SWITCH_PRESS
    yc = P.SWITCH_WALL_Y
    zc = P.CASE_FLOOR + P.SWITCH_FLOOR_GAP + W / 2
    xs = x0 + P.SWITCH_SKIN                                         # inner face of the skin = knob face of the body
    fw = P.SWITCH_FRAME_T
    frame = E._box((x0 + 0.5, yc - L / 2 - f - fw, 0), (xs + H - P.SWITCH_BACK_FREE, yc + L / 2 + f + fw, zc + W / 2 + f + fw))
    b = b.fuse(frame.intersect(E.base_body(zb)))
    b = b.cut(E._box((xs, yc - L / 2 - f, zc - W / 2 - f), (xs + H + 5, yc + L / 2 + f, zc + W / 2 + f)))
    sl, sw = P.SWITCH_KNOB_SLOT
    b = b.cut(E._box((x0 - 5, yc - sl / 2, zc - sw / 2), (xs + 0.01, yc + sl / 2, zc + sw / 2)))
    return b, dict(centre=(round(xs, 2), yc, round(zc, 2)), knob_out=round(P.SWITCH_MEAS_KNOB - P.SWITCH_SKIN, 2))


def switch_keepout(zb):
    """Outside patch kept free of ribs / panels round the knob."""
    x0 = E.footprint()[0]
    zc = P.CASE_FLOOR + P.SWITCH_FLOOR_GAP + P.SWITCH_MEAS_W / 2
    yc = P.SWITCH_WALL_Y
    top = zc + P.SWITCH_PATCH_HALF
    # 45 deg roof outward, so the ribs / panels above the patch end in a printable slope, not a flat overhang
    prof = [(x0 + 0.3, -1.0), (x0 + 0.3, top), (x0 - 6.0, top + 6.3), (x0 - 6.0, -1.0)]
    w = cq.Wire.makePolygon([cq.Vector(x, yc - P.SWITCH_PATCH_HALF, z) for x, z in prof], close=True)
    return cq.Solid.extrudeLinear(cq.Face.makeFromWires(w), cq.Vector(0, 2 * P.SWITCH_PATCH_HALF, 0))
