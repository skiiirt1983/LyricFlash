"""Phase 2: the full LyricFlash case.

Frames
  board frame (BF): as everywhere else (origin PCB-back centre, +Z out of the glass)
  world frame (WF): desk at Z=0, +Y back, +X right. WF = T(BOARD_X, 0, board_z) * Rx(TILT)
Parts
  top shell (gold PLA Metal): everything above the seam plane (BF Z >= SEAM_Z); printed face-down
  base (black PETG): everything below the seam plane; printed floor-down
"""
from __future__ import annotations

import math

import cadquery as cq
import numpy as np

import params as P
from lfcase import coupons_a as A
from lfcase.coupons import deboss

T = math.radians(P.TILT_DEG)


# ------------------------------------------------------------------ placement
_ZB = {}


def board_z():
    """Lowest board height that keeps CASE_AIR between the board stack (every back-side part as its box,
    the plug + wire zones, the top shell around the glass) and the battery, the speaker and the floor."""
    if "zb" in _ZB:
        return _ZB["zb"]
    import sys
    from pathlib import Path
    sys.path.insert(0, str(Path(__file__).resolve().parent.parent / "concepts"))
    import layout_study as L
    from lfcase.board import load_board
    boxes = L.board_boxes(load_board(verbose=False))
    obst = [battery_box(), speaker_box()]
    air = P.CASE_AIR

    def ok(zb):
        for b in boxes:
            c = L.to_world(L.corners(b), P.TILT_DEG, (P.BOARD_X, 0, zb))
            mn, mx = c.min(0), c.max(0)
            if mn[2] < P.CASE_FLOOR + air:
                return False
            for lo, hi in obst:
                if (mn[0] < hi[0] + air and mx[0] > lo[0] - air and mn[1] < hi[1] + air and mx[1] > lo[1] - air
                        and mn[2] < hi[2] + air):
                    return False
        return True
    lo, hi = 0.0, 80.0
    for _ in range(40):
        mid = (lo + hi) / 2
        lo, hi = (lo, mid) if ok(mid) else (mid, hi)
    _ZB["zb"] = round(hi + 0.005 + P.BOARD_Z_EXTRA, 2)
    return _ZB["zb"]


def bf_loc(zb):
    return cq.Location(cq.Vector(P.BOARD_X, 0, zb)) * cq.Location(cq.Vector(0, 0, 0), cq.Vector(1, 0, 0), P.TILT_DEG)


def to_wf(shape, zb):
    return shape.moved(bf_loc(zb))


def bf_point_to_wf(p, zb):
    x, y, z = p
    return np.array([P.BOARD_X + x, y * math.cos(T) - z * math.sin(T), zb + y * math.sin(T) + z * math.cos(T)])


def halfspace_bf(zb, z_bf, above=True, size=400):
    """World solid for BF Z >= z_bf (above) or <= z_bf."""
    box = cq.Solid.makeBox(size, size, size, pnt=cq.Vector(-size / 2, -size / 2, z_bf if above else z_bf - size))
    return to_wf(box, zb)


# ------------------------------------------------------------------ outer body
def footprint():
    """Layout B (Paul): the case is exactly as wide as the bezel ring, the board centred left-right;
    the battery runs front-to-back and sets the depth."""
    bez = A.BORE_R + 1.6                                      # bezel ring outer radius (BF)
    x0, x1 = P.BOARD_X - bez, P.BOARD_X + bez
    yr = bez * math.cos(T) + 1.0
    yb = P.BATTERY_POCKET[0] / 2 + P.CASE_WALL
    y_half = max(yr, yb)
    return x0, x1, -y_half, y_half


def speaker_box():
    """Speaker standing against the right wall (cone out through the grille), under the board's right side."""
    x0, x1, _, _ = footprint()
    t = P.SPK_T + 2 * P.SPK_CLEAR
    l = P.SPK_L + 2 * P.SPK_CLEAR
    h = P.SPK_W + P.SPK_DEPTH_CLEAR
    sx1 = x1 - P.CASE_WALL
    return (sx1 - t, -l / 2, P.CASE_FLOOR), (sx1, l / 2, P.CASE_FLOOR + h)


def battery_box():
    """Battery turned front-to-back (long side along Y), against the speaker divider."""
    L, W, H = P.BATTERY_POCKET
    (sx0, _, _), _ = speaker_box()
    bx1 = sx0 - P.SPK_DIVIDER
    return (bx1 - W, -L / 2, P.CASE_FLOOR), (bx1, L / 2, P.CASE_FLOOR + H)


def outer_body(zb):
    x0, x1, y0, y1 = footprint()
    h = 120
    body = (cq.Workplane("XY").center((x0 + x1) / 2, (y0 + y1) / 2)
            .sketch().rect(x1 - x0, y1 - y0).vertices().fillet(P.CASE_CORNER_R).finalize().extrude(h).val())
    top = halfspace_bf(zb, A.LIP_TOP, above=True)
    return body.cut(top)


def dims(zb):
    x0, x1, y0, y1 = footprint()
    lip_c = bf_point_to_wf((0, 0, A.LIP_TOP), zb)
    top = lambda y: lip_c[2] + (y - lip_c[1]) * math.tan(T)
    return dict(width=x1 - x0, depth=y1 - y0, h_front=top(y0), h_back=top(y1))


# ------------------------------------------------------------------ top shell
def top_shell(zb):
    s = outer_body(zb).intersect(halfspace_bf(zb, P.SEAM_Z, above=True))
    bore = A.cyl(A.BORE_R, P.SEAM_Z - 2, P.GLASS_TOP_Z)
    lip = A.cyl(A.LIP_R, P.SEAM_Z - 2, A.LIP_TOP + 2)
    s = s.cut(to_wf(bore.fuse(lip).fuse(A.fold_reliefs(P.SEAM_Z - 2, P.GLASS_BOTTOM_Z - 0.05)), zb))
    s = s.cut(hinge_relief(zb))
    s = _chamfer_face_outer(s, bf_point_to_wf((0, 0, 1), zb) - bf_point_to_wf((0, 0, 0), zb), P.TOP_EDGE_CHAMFER, top=True)
    return A.solidify(s)


def _chamfer_face_outer(shape, normal, d, top=True):
    """Chamfer the outer boundary of the planar face whose normal is `normal` (top=True: the highest such face)."""
    nv = cq.Vector(*normal).normalized()
    faces = [f for f in shape.Faces() if f.geomType() == "PLANE" and abs(f.normalAt().dot(nv) - (1 if top else -1)) < 1e-6]
    f = max(faces, key=lambda f: f.Area())
    sol = shape.Solids()[0] if shape.Solids() else shape
    return sol.chamfer(d, None, f.outerWire().Edges())


def hinge_axis(zb):
    H = bf_point_to_wf((P.TILT_HINGE[0], 0, P.TILT_HINGE[1]), zb)
    ax = bf_point_to_wf((P.TILT_HINGE[0], 1, P.TILT_HINGE[1]), zb) - H
    return H, ax / np.linalg.norm(ax)


def hinge_relief(zb, step=1.0):
    """The base's wall outboard of the hinge line rises as the base swings open. Sweep that strip
    (grown by HINGE_RELIEF_AIR) over the whole swing; the top shell is cut back by it."""
    x0, x1, y0, y1 = footprint()
    air = P.HINGE_RELIEF_AIR
    x0_bf = x0 - P.BOARD_X
    strip = to_wf(cq.Solid.makeBox(P.TILT_HINGE[0] + 0.5 - (x0_bf - air - 3), 140, 6 + air,
                                   pnt=cq.Vector(x0_bf - air - 3, -70, P.SEAM_Z - 6)), zb)
    H, ax = hinge_axis(zb)
    parts = [strip.rotate(cq.Vector(*H), cq.Vector(*(H + ax)), float(a))
             for a in np.arange(0.0, P.TILT_SWEEP_DEG + air * 10 + 1e-9, step)]
    out = parts[0]
    for p in parts[1:]:
        out = out.fuse(p)
    return out.clean()


# ------------------------------------------------------------------ base
def _inner_cavity():
    x0, x1, y0, y1 = footprint()
    w = P.CASE_WALL
    return (cq.Workplane("XY").workplane(offset=P.CASE_FLOOR).center((x0 + x1) / 2, (y0 + y1) / 2)
            .sketch().rect(x1 - x0 - 2 * w, y1 - y0 - 2 * w).vertices().fillet(P.CASE_CORNER_R - w).finalize().extrude(200).val())


def _wall_normal(p):
    """Outward horizontal normal of the case's side surface nearest to world point p."""
    x0, x1, y0, y1 = footprint()
    R = P.CASE_CORNER_R
    cx = min(max(p[0], x0 + R), x1 - R)
    cy = min(max(p[1], y0 + R), y1 - R)
    if abs(cx - p[0]) > 1e-6 and abs(cy - p[1]) > 1e-6:          # in a rounded corner: radial
        v = np.array([p[0] - cx, p[1] - cy, 0.0])
    else:                                                          # on a flat side
        d = {(1, 0): x1 - p[0], (-1, 0): p[0] - x0, (0, 1): y1 - p[1], (0, -1): p[1] - y0}
        k = min(d, key=lambda k: abs(d[k]))
        v = np.array([k[0], k[1], 0.0])
    return v / np.linalg.norm(v)


def _battery_keepout(gap=0.5):
    (bx0, by0, bz0), (bx1, by1, bz1) = battery_box()
    return _box((bx0 - gap, by0 - gap, bz0 - 1), (bx1 + gap, by1 + gap, bz1 + gap))


def battery_roof_y(gap=0.5):
    """The battery's front/back 45 deg roof, extended across the whole case width."""
    (bx0, by0, bz0), (bx1, by1, bz1) = battery_box()
    y0, y1, zt = by0 - gap, by1 + gap, bz1 + gap
    w = (y1 - y0) / 2
    pts = [(y0, 0.0), (y1, 0.0), (y1, zt), (y1 - w, zt + w), (y0, zt)]
    wire = cq.Wire.makePolygon([cq.Vector(-100, y, z) for y, z in pts], close=True)
    return cq.Solid.extrudeLinear(cq.Face.makeFromWires(wire), cq.Vector(200, 0, 0))


def battery_cut45(gap=0.5):
    """Battery envelope (+gap) plus a 45 deg roof over it, so anything trimmed by it keeps a printable
    45 deg underside instead of a flat ceiling over the battery."""
    (bx0, by0, bz0), (bx1, by1, bz1) = battery_box()
    x0, x1, y0, y1 = bx0 - gap, bx1 + gap, by0 - gap, by1 + gap
    zt = bz1 + gap
    w = (y1 - y0) / 2
    pts = [(y0, bz0 - 1), (y1, bz0 - 1), (y1, zt), (y1 - w, zt + w), (y0, zt)]
    wire = cq.Wire.makePolygon([cq.Vector(x0, y, z) for y, z in pts], close=True)
    roof_y = cq.Solid.extrudeLinear(cq.Face.makeFromWires(wire), cq.Vector(x1 - x0, 0, 0))
    h = (x1 - x0) / 2
    pts2 = [(x0, bz0 - 1), (x1, bz0 - 1), (x1, zt), (x1 - h, zt + h), (x0, zt)]
    wire2 = cq.Wire.makePolygon([cq.Vector(x, y0, z) for x, z in pts2], close=True)
    roof_x = cq.Solid.extrudeLinear(cq.Face.makeFromWires(wire2), cq.Vector(0, y1 - y0, 0))
    return roof_y.intersect(roof_x)


def _box(lo, hi):
    return cq.Solid.makeBox(hi[0] - lo[0], hi[1] - lo[1], hi[2] - lo[2], pnt=cq.Vector(*lo))


_BASE_BODY = {}


def base_body(zb):
    """Outer body with the bottom edge chamfered (everything added to the base is trimmed to it)."""
    if zb not in _BASE_BODY:
        _BASE_BODY[zb] = _chamfer_face_outer(outer_body(zb), (0, 0, 1), P.BOTTOM_EDGE_CHAMFER, top=False)
    return _BASE_BODY[zb]


def base_shell(zb):
    body = base_body(zb)
    b = body.intersect(halfspace_bf(zb, P.SEAM_Z, above=False)).cut(_inner_cavity())
    (bx0, by0, bz0), (bx1, by1, bz1) = battery_box()
    # low wall along the battery's left long side (front and back are the case walls, right is the
    # speaker divider); lead gap at the back-left corner, where the cell's leads leave (Paul, C3)
    b = b.fuse(_box((bx0 - 1.2, by0 - 0.01, bz0 - 0.01), (bx0, by1 + 0.01, bz0 + P.BATTERY_POCKET_WALL_H)))
    if P.BATTERY_LEAD_GAP > 0:
        b = b.cut(_box((bx0 - 1.3, by1 - P.BATTERY_LEAD_GAP, bz0), (bx0 + 0.1, by1 + 1, bz0 + P.BATTERY_POCKET_WALL_H + 1)))
    # speaker pocket: divider wall + end walls (speaker stands, fires out the right wall)
    (sx0, sy0, sz0), (sx1, sy1, sz1) = speaker_box()
    b = b.fuse(_box((bx1, sy0 - 1.2, sz0 - 0.01), (sx0, sy1 + 1.2, sz0 + P.SPK_DIVIDER_H)))
    # low wall continuing the divider along the battery's right side (front and back of the speaker)
    b = b.fuse(_box((bx1, by0 - 0.01, bz0 - 0.01), (bx1 + 1.2, by1 + 0.01, bz0 + P.BATTERY_POCKET_WALL_H)))
    for y in (sy0 - 1.2, sy1):
        b = b.fuse(_box((sx0 - 0.01, y, sz0 - 0.01), (sx1 + 0.01, y + 1.2, sz1)))
    return A.solidify(b)


_LEDGE_REACH = {}


def _ledge(zb, a):
    x0, x1, y0, y1 = footprint()
    r_pcb = P.PCB_D / 2
    ca, sa = math.cos(math.radians(a)), math.sin(math.radians(a))
    inner = bf_point_to_wf((ca * (r_pcb - P.BOARD_LEDGE_REACH), sa * (r_pcb - P.BOARD_LEDGE_REACH), 0.0), zb)
    wall_y = (y1 - P.CASE_WALL + 0.5) if sa > 0 else (y0 + P.CASE_WALL - 0.5)
    slope = math.tan(T)                                    # PCB back plane rises toward +Y
    ztop = lambda y: inner[2] + (y - inner[1]) * slope - 0.02
    L = abs(wall_y - inner[1])
    prof = [(inner[1], ztop(inner[1])), (wall_y, ztop(wall_y)), (wall_y, ztop(inner[1]) - 2.0 - L), (inner[1], ztop(inner[1]) - 2.0)]
    plate = (cq.Workplane("YZ", origin=(inner[0] - P.BOARD_LEDGE_W / 2, 0, 0)).polyline(prof).close()
             .extrude(P.BOARD_LEDGE_W).val())
    if not plate.intersect(_battery_keepout()).Volume() > 1e-3:
        return plate
    # v5.2: over the battery the ledge's own 45 deg underside and the battery's 45 deg roof run parallel and
    # leave a sliver. Make it a solid wedge down to the roof instead: thick at the wall, 45 deg underside.
    deep = [(inner[1], ztop(inner[1])), (wall_y, ztop(wall_y)), (wall_y, P.CASE_FLOOR), (inner[1], P.CASE_FLOOR)]
    wedge = (cq.Workplane("YZ", origin=(inner[0] - P.BOARD_LEDGE_W / 2, 0, 0)).polyline(deep).close()
             .extrude(P.BOARD_LEDGE_W).val())
    wedge = wedge.cut(battery_roof_y())
    # end it square where it gets thinner than LEDGE_MIN_T (a knife edge prints as a floating last layer)
    (bx0, by0, bz0), (bx1, by1, bz1) = battery_box()
    g_, zt = 0.5, bz1 + 0.5
    roof = lambda x, y: zt + min(y - (by0 - g_), (by1 + g_) - y)
    step = 0.05 if inner[1] > wall_y else -0.05
    yc = wall_y
    while (inner[1] - yc) * step > 0 and ztop(yc) - max(roof(inner[0] - P.BOARD_LEDGE_W / 2, yc),
                                                        roof(inner[0] + P.BOARD_LEDGE_W / 2, yc)) >= P.LEDGE_MIN_T:
        yc += step
    lo_y, hi_y = (yc, 200) if step > 0 else (-200, yc)
    _LEDGE_REACH[a] = abs(yc - wall_y)
    return wedge.cut(_box((inner[0] - 10, lo_y, 0), (inner[0] + 10, hi_y, 100)))


def pick_ledge_angles(zb, board_wf, targets=(-50.0, -110.0, 135.0)):  # noqa: B006
    """Two front and two back ledge positions that touch nothing but the PCB back."""
    from lfcase.interference import check
    obst = {k: v for k, v in board_wf.items() if k != "pcb"}
    # plugs + bent wires (Paul: 7 mm below the PCB) around J1 / H3
    for nm, bb in (("plug_j1", P.J1_BBOX), ("plug_h3", P.H3_BBOX)):
        (px0, py0, _), (px1, py1, _) = bb
        g = P.PLUG_KEEPOUT_GROW
        obst[nm] = to_wf(cq.Solid.makeBox(px1 - px0 + 2 * g, py1 - py0 + 2 * g, P.MIN_DEPTH_BELOW_PCB,
                                          pnt=cq.Vector(px0 - g, py0 - g, -P.MIN_DEPTH_BELOW_PCB)), zb)
    H, ax = hinge_axis(zb)
    from lfcase.interference import check as _chk

    def clear(led):
        for ang in (0.0, 1.0, 2.0, 3.0, 4.0, 6.0, 8.0, 10.0, 13.0, 16.0, 20.0, 24.0):
            l = led.rotate(cq.Vector(*H), cq.Vector(*(H + ax)), ang)
            lb = l.BoundingBox()
            for o in obst.values():
                for so in (o.Solids() or [o]):
                    ob = so.BoundingBox()
                    if (ob.xmin > lb.xmax + 1 or lb.xmin > ob.xmax + 1 or ob.ymin > lb.ymax + 1 or lb.ymin > ob.ymax + 1
                            or ob.zmin > lb.zmax + 1 or lb.zmin > ob.zmax + 1):
                        continue
                    if l.distance(so) < P.LEDGE_AIR:
                        return False
        return True
    chosen = []
    for target in targets:
        for d in (0, 5, -5, 10, -10, 15, -15, 20, -20, 25, -25, 30, -30):
            a = target + d
            if not _chk(_ledge(zb, a), obst) and clear(_ledge(zb, a)):
                chosen.append(a)
                break
    return chosen


def board_ledges(zb, angles=None):
    out = None
    for a in (angles or P.BOARD_LEDGE_ANGLES):
        sol = _ledge(zb, a)
        out = sol if out is None else out.fuse(sol)
    return out.cut(_battery_keepout())                # (wedge ledges are already clear of it)


def base(zb, ledge_angles=None):
    b = base_shell(zb).fuse(board_ledges(zb, ledge_angles))
    return A.solidify(b)


# ------------------------------------------------------------------ USB-C port (C10 geometry)
def _bf_inverse(zb):
    return bf_loc(zb).inverse


def port_features(b, zb):
    """Bay in the left wall + the proven C10 port wall/opening/pad, built in the board frame."""
    g = A.usb_geometry()
    fx_bf = g["face_x"]
    fx_w = P.BOARD_X + fx_bf
    pc = bf_point_to_wf((P.USB_MOUTH_X, g["yc"], g["zc"]), zb)
    x0, x1, y0, y1 = footprint()
    hw = P.PORT_BAY_HALF_W
    zbot = pc[2] - P.PORT_BAY_BELOW
    bay = _box((x0 - 5, pc[1] - hw, zbot), (fx_w, pc[1] + hw, 100))
    b = b.cut(bay)
    # seed in the board frame: a slab at the port face, the C10 block builder does the rest
    seed_w = _box((fx_w - 0.01, pc[1] - hw, zbot), (fx_w + P.WALL_MIN, pc[1] + hw, 100)).intersect(
        halfspace_bf(zb, P.SEAM_Z, above=False)).intersect(_inner_cavity().fuse(_box((fx_w - 1, -100, 0), (fx_w + 3, 100, 100))))
    seed_bf = seed_w.moved(_bf_inverse(zb))
    blk = A._usb_port_block(seed_bf)
    blk_w = blk.moved(bf_loc(zb)).intersect(halfspace_bf(zb, P.SEAM_Z, above=False))
    # keep the PCB/PMMA outline clear: the web touches it, nothing may enter it
    blk_w = blk_w.cut(to_wf(A.cyl(P.USB_PCB_CONTACT_R, -0.05, P.SEAM_Z + 2), zb))
    (bx0_, by0_, bz0_), (bx1_, by1_, bz1_) = battery_box()
    bat = _box((bx0_ - 0.5, by0_ - 0.5, bz0_ - 1), (bx1_ + 0.5, by1_ + 0.5, bz1_ + 0.5))
    blk_w = blk_w.cut(bat)                                              # keep off the battery
    # v3.1 (Paul): behind the wall keep only the receptacle guide; the C10 insert spanned the whole
    # +/-14 mm recess and left two blocks reaching into the case behind the mics
    gh = g["W"] / 2 + P.USB_GUIDE_CLEAR + P.USB_INSERT_SIDE_WALL
    for s_ in (-1, 1):
        y_a = pc[1] + s_ * gh
        blk_w = blk_w.cut(_box((fx_w + P.WALL_MIN + 0.01, min(y_a, y_a + s_ * 50), -10),
                               (fx_w + 40, max(y_a, y_a + s_ * 50), 100)))
    # v3 (Paul: no support towers): fill the 45 deg "shadow" under the part of the port insert that reaches
    # in past the wall, back down toward the wall, so every underside is >= 45 deg and it prints on its own
    inner = (blk_w.intersect(_box((fx_w + P.WALL_MIN - 0.01, -100, 0), (fx_w + 40, 100, 100)))
             .intersect(halfspace_bf(zb, g["z_bot"] - P.USB_UNDER_GAP - 0.01, above=False)))   # only what's under the receptacle
    if inner.Volume() > 1e-3:
        # exact sweep (v3.2): a stack of 0.4 mm-stepped copies left a sawtooth edge and creases (Paul's slicer view)
        # = the solid plus every face extruded along the sweep direction
        vec = cq.Vector(-12.0, 0.0, -12.0)
        prisms = []
        for f in inner.Faces():
            if abs(f.normalAt().dot(vec.normalized())) < 1e-6:
                continue                                           # parallel to the sweep: adds nothing
            try:
                pr = cq.Solid.extrudeLinear(f, vec)
                if pr.isValid() and pr.Volume() > 1e-6:
                    prisms.append(pr)
            except Exception:
                pass
        sh = inner.fuse(*prisms).clean() if prisms else inner
        sh = (sh.intersect(_box((x0, -100, P.CASE_FLOOR), (fx_w + 40, 100, zbot - 0.01))).fuse(
                  sh.intersect(_box((fx_w - 0.01, -100, zbot - 0.01), (fx_w + 40, 100, 100))))
              .intersect(base_body(zb))
              .intersect(halfspace_bf(zb, P.SEAM_Z, above=False)).cut(bat))
        blk_w = blk_w.fuse(sh.clean())
    # tie the port wall into the case: a floor under the bay and a cheek either side, wall-thick
    t = P.CASE_WALL
    # bay floor: wall-thick at the wall, then a 45 deg underside where it reaches in over the cavity
    x_in = x0 + t
    xe = fx_w + P.WALL_MIN
    zf = zbot - max(P.CASE_FLOOR, xe - x_in)          # deep enough that the 45 deg underside reaches under the port wall
    prof = [(x0, zf), (x_in, zf), (xe, zbot + 0.01), (x0, zbot + 0.01)]
    wv = cq.Wire.makePolygon([cq.Vector(x, pc[1] - hw - t, z) for x, z in prof], close=True)
    frame = cq.Solid.extrudeLinear(cq.Face.makeFromWires(wv), cq.Vector(0, 2 * (hw + t), 0))
    cheek = [(x0, zf), (x_in, zf), (xe, zf + (xe - x_in)), (xe, 100), (x0, 100)]     # 45 deg underside inside
    for s in (-1, 1):
        y_a = pc[1] + s * hw
        wv = cq.Wire.makePolygon([cq.Vector(x, min(y_a, y_a + s * t), z) for x, z in cheek], close=True)
        frame = frame.fuse(cq.Solid.extrudeLinear(cq.Face.makeFromWires(wv), cq.Vector(0, t, 0)))
    frame = frame.intersect(base_body(zb)).intersect(halfspace_bf(zb, P.SEAM_Z, above=False)).cut(_box(*battery_box()))
    return A.solidify(b.fuse(blk_w).fuse(frame))


# ------------------------------------------------------------------ mics (C5 geometry, looser collar for tilt-in)
def _swept_outline(pts, air=0.2, top=None, step=0.25, a_max=30.0):
    """XY outline (board frame) of a board part as the base swings closed about the tilt hinge.

    pts: (N,3) points on the part, board frame. In the base's frame the part moves along the hinge arc;
    for each step the footprint of the points below `top` is taken, the steps are unioned, then grown by
    `air`. Returns (shapely polygon, lowest Z reached)."""
    import numpy as np
    from shapely.geometry import MultiPoint
    from shapely.ops import unary_union
    top = P.MIC_CRUSH if top is None else top
    hx, hz = P.TILT_HINGE
    keep, zmin = [], pts[:, 2].min()
    for ang in np.arange(0.0, a_max + 1e-9, step):
        t = math.radians(-ang)                      # base opened by +ang == board rotated by -ang in the base frame
        dx, dz = pts[:, 0] - hx, pts[:, 2] - hz
        # right-handed rotation about +Y: x' = x cos + z sin, z' = -x sin + z cos
        X = hx + dx * math.cos(t) + dz * math.sin(t)
        Z = hz - dx * math.sin(t) + dz * math.cos(t)
        m = Z <= top
        if m.sum() >= 3:
            keep.append(MultiPoint(np.c_[X[m], pts[m, 1]]).convex_hull)
            zmin = min(zmin, Z[m].min())
    return unary_union(keep).buffer(air, join_style=2), zmin


def _mic_points(mic):
    import numpy as np
    cx, cy = getattr(P, f"{mic}_CENTER")
    a, bb, _ = getattr(P, f"{mic}_SIZE")
    z0 = getattr(P, f"{mic}_Z")[0]
    rot = math.radians(A._mic_edge_angle(mic))
    u = np.linspace(-0.5, 0.5, 9)
    p = np.array([(x * bb, y * a, z) for x in u for y in u for z in np.linspace(z0, 0.0, 6)])
    c, s_ = math.cos(rot), math.sin(rot)
    return np.c_[cx + p[:, 0] * c - p[:, 1] * s_, cy + p[:, 0] * s_ + p[:, 1] * c, p[:, 2]]


def _mic_swept_outline(mic, air=0.2):
    return _swept_outline(_mic_points(mic), air)


def mic_features(b, zb):
    """Mic holes (Paul, v3): a plain D MIC_HOLE_D_CASE teardrop straight through the USB recess's back wall,
    level with each mic, opening into the case. No bar or duct inside: those hung over the battery and needed
    a wall of supports. The bigger hole makes up for the open path."""
    fx_w = P.BOARD_X + A.usb_geometry()["face_x"]
    d = P.MIC_HOLE_D_CASE
    info = {}
    for mic in ("MIC1", "MIC2"):
        cx, cy = getattr(P, f"{mic}_CENTER")
        # centre the hole just under the mic package (top-port: the sound port faces away from the PCB),
        # top of the hole MIC_HOLE_PCB_GAP below the PCB back
        c = bf_point_to_wf((cx, cy, -P.MIC_HOLE_PCB_GAP - d / 2), zb)
        x_out, x_in = fx_w - 1.0, fx_w + P.WALL_MIN + 1.0
        b = b.cut(_teardrop_x(x_out, x_in, c[1], c[2], d))
        info[mic] = dict(hole_d=d, centre=(round(fx_w, 2), round(float(c[1]), 2), round(float(c[2]), 2)),
                         to_mic=round(float(cx + P.BOARD_X - fx_w), 1))
    return A.solidify(b), info


# ------------------------------------------------------------------ PWR push pin (KEY2)
def _pin_frame(key=None):
    key = key or P.PWR_IS
    a = math.radians(getattr(P, f"{key}_PRESS_ANGLE"))
    tip = getattr(P, f"{key}_TIP_CENTER")
    tz = getattr(P, f"{key}_TIP_Z")
    return a, tip, (tz[0] + tz[1]) / 2


def _local_box(s0, s1, l0, l1, z0, z1, key=None):
    """Box in the pin frame (s outward along the press axis from the plunger tip face, l across, BF z)."""
    a, tip, _ = _pin_frame(key)
    b = cq.Solid.makeBox(s1 - s0, l1 - l0, z1 - z0, pnt=cq.Vector(s0, l0, z0))
    return b.rotate(cq.Vector(0, 0, 0), cq.Vector(0, 0, 1), math.degrees(a)).moved(cq.Location(cq.Vector(tip[0], tip[1], 0)))


def _pin_dims():
    nw, nh, nl = P.PWR_NUB
    _, _, zc = _pin_frame()
    l0 = -nw / 2                                       # face A (print bed side) flush with the nub
    l1 = l0 + P.PWR_PIN_W
    z1 = P.PWR_PIN_TOP
    z0 = z1 - P.PWR_PIN_H
    g0 = P.PWR_PIN_REST_GAP
    s_col0 = g0 + nl
    s_col1 = s_col0 + P.PWR_COLLAR_LEN
    return dict(nw=nw, nh=nh, zc=zc, l0=l0, l1=l1, z0=z0, z1=z1, g0=g0, s_col0=s_col0, s_col1=s_col1)


def _pin_exit_s(zb):
    """Distance along the axis (shaft centre line) where it leaves the case outer surface."""
    d = _pin_dims()
    a, tip, _ = _pin_frame()
    lc, zc = (d["l0"] + d["l1"]) / 2, (d["z0"] + d["z1"]) / 2
    body = outer_body(zb)
    s = d["s_col1"]
    while s < 80:
        p = (tip[0] + s * math.cos(a) - lc * math.sin(a), tip[1] + s * math.sin(a) + lc * math.cos(a), zc)
        if not body.isInside(cq.Vector(*bf_point_to_wf(p, zb)), 1e-4):
            return s
        s += 0.05
    raise RuntimeError("pin axis never leaves the case")


def pwr_pin(zb):
    """The separate pin, assembled position (world). Rest: collar against the tube's inner end."""
    d = _pin_dims()
    s_out = _pin_exit_s(zb) - P.PWR_PIN_RECESS - P.PWR_DIMPLE_DEPTH
    nub = _local_box(d["g0"], d["s_col0"] + 0.01, d["l0"], d["l0"] + d["nw"], d["zc"] - d["nh"] / 2, d["zc"] + d["nh"] / 2)
    col = _local_box(d["s_col0"], d["s_col1"], d["l0"], d["l1"] + P.PWR_COLLAR[0], d["z0"] - P.PWR_COLLAR[1], d["z1"])
    shaft = _local_box(d["s_col1"] - 0.01, s_out, d["l0"], d["l1"], d["z0"], d["z1"])
    return to_wf(nub.fuse(col).fuse(shaft).clean(), zb)


def pwr_features(b, zb):
    """Guide tube from the collar stop to the outer wall, a fin under it (45 deg underside), finger dimple."""
    d = _pin_dims()
    c, w = P.PWR_PIN_CLEAR, P.PWR_TUBE_WALL
    s_t0 = d["s_col1"] + 0.0
    s_x = _pin_exit_s(zb)
    tube = _local_box(s_t0, s_x + 5, d["l0"] - c - w, d["l1"] + c + w, d["z0"] - c - w, d["z1"] + c + w)
    bore = _local_box(s_t0 - 1, s_x + 6, d["l0"] - c, d["l1"] + c, d["z0"] - c, d["z1"] + c)
    # room for the collar to move in when pressed
    col_room = _local_box(d["s_col0"] - 1.5, s_t0, d["l0"] - c, d["l1"] + P.PWR_COLLAR[0] + c, d["z0"] - P.PWR_COLLAR[1] - c, d["z1"] + c)
    fin = _local_box(s_t0, s_x + 5, d["l0"] - c - w, d["l1"] + c + w, -60, d["z0"] - c - w + 0.01)
    tube_w, bore_w, fin_w, room_w = (to_wf(x, zb) for x in (tube, bore, fin, col_room))
    # fin underside: 45 deg in the world, rising toward the inner end of the tube
    a, tip, _ = _pin_frame()
    p_in = bf_point_to_wf((tip[0] + s_t0 * math.cos(a), tip[1] + s_t0 * math.sin(a), d["z0"] - c - w), zb)
    u = bf_point_to_wf((tip[0] + math.cos(a), tip[1] + math.sin(a), 0), zb) - bf_point_to_wf((tip[0], tip[1], 0), zb)
    uh = np.array([u[0], u[1], 0.0]); uh /= np.linalg.norm(uh)
    # half-space z >= p_in.z - (x - p_in).uh  ->  normal (uh_x, uh_y, 1)/sqrt2 pointing up-outward
    nrm = cq.Vector(uh[0], uh[1], 1.0).normalized()
    big = cq.Solid.makeBox(400, 400, 400, pnt=cq.Vector(-200, -200, 0))
    pl = cq.Plane(origin=cq.Vector(*p_in), xDir=cq.Vector(-uh[1], uh[0], 0), normal=nrm)
    keep = big.moved(cq.Location(pl))
    fin_w = fin_w.intersect(keep).intersect(_inner_cavity().fuse(base_body(zb)))
    shell = base_body(zb)
    fin_w = fin_w.cut(battery_cut45()).cut(_box(*speaker_box()))
    add = tube_w.fuse(fin_w).intersect(shell).intersect(halfspace_bf(zb, P.SEAM_Z, above=False))
    b = b.fuse(add).cut(bore_w).cut(room_w)
    # finger dimple: shallow cone round the exit
    lc, zc = (d["l0"] + d["l1"]) / 2, (d["z0"] + d["z1"]) / 2
    ex = bf_point_to_wf((tip[0] + s_x * math.cos(a) - lc * math.sin(a), tip[1] + s_x * math.sin(a) + lc * math.cos(a), zc), zb)
    # axis = the wall's outward normal (not the pin axis: tilted 34 deg to the wall, a cone along the pin
    # cut right through the 2 mm wall beside the tube). Depth capped so >= DIMPLE_MIN_WALL stays behind it.
    nw = _wall_normal(ex)
    dep = min(P.PWR_DIMPLE_DEPTH, P.CASE_WALL - P.PWR_DIMPLE_MIN_WALL)
    cone = cq.Solid.makeCone(P.PWR_DIMPLE_D / 2 - dep, P.PWR_DIMPLE_D / 2 + 2, dep + 2,
                             pnt=cq.Vector(*(ex - nw * dep)), dir=cq.Vector(*nw))
    b = b.cut(cone)
    return A.solidify(b), dict(exit=tuple(round(float(v), 2) for v in ex), s_exit=round(s_x, 2))


# ------------------------------------------------------------------ speaker (C8 numbers, standing, fires out the right wall)
def _teardrop_x(x0, x1, y, z, d):
    """Horizontal hole along X, teardrop point up (prints without support)."""
    q = d / 2 * math.sqrt(2)
    return (cq.Workplane("YZ").workplane(offset=x0).center(y, z).circle(d / 2).extrude(x1 - x0).val()
            .fuse(cq.Solid.makeBox(x1 - x0, q, q, pnt=cq.Vector(x0, y - q / 2, z - q / 2))
                  .rotate(cq.Vector(0, y, z), cq.Vector(1, y, z), 45)))


_GRILLE = {}


def speaker_features(b, zb):
    """Grille through the right wall, seal rib on its inner face, grip ribs on the end walls, lead slot (+Y end, toward H3)."""
    (sx0, sy0, sz0), (sx1, sy1, sz1) = speaker_box()
    L, W = P.SPK_L, P.SPK_W
    yc, zc = 0.0, sz0 + W / 2                      # speaker stands on the floor
    x_out = footprint()[1]
    # seal rib on the wall face (speaker front frame presses on it)
    si, rw, rh = P.SPK_SEAL_INSET, P.SPK_SEAL_RIB_W, P.SPK_SEAL_RIB_H
    a0, b0 = L / 2 - si + rw / 2, W / 2 - si + rw / 2
    a1, b1 = L / 2 - si - rw / 2, W / 2 - si - rw / 2
    rib = _box((sx1 - rh, yc - a0, zc - b0), (sx1 + 0.01, yc + a0, zc + b0)).cut(
        _box((sx1 - rh - 1, yc - a1, zc - b1), (sx1 + 1, yc + a1, zc + b1)))
    b = b.fuse(rib)
    # grille: same hex / stadium pattern as C8, now in the YZ plane
    gm, p = P.SPK_GRILLE_MARGIN, P.SPK_GRILLE_PITCH
    gx, gy = L / 2 - gm, W / 2 - gm
    pts, j, v = [], 0, -gy
    while v <= gy + 1e-6:
        u = -gx + (p / 2 if j % 2 else 0)
        while u <= gx + 1e-6:
            if abs(u) <= gx - gy or math.hypot(abs(u) - (gx - gy), v) <= gy:
                pts.append((yc + u, zc + v))
            u += p
        v += p * math.sqrt(3) / 2
        j += 1
    holes = [_teardrop_x(sx1 - rh - 0.1, x_out + 1 + P.RIB_PROUD + 1, y, z, P.SPK_GRILLE_HOLE_D) for y, z in pts]
    _GRILLE["holes"] = holes                            # recut after the ribs go on
    b = b.cut(cq.Compound.makeCompound(holes))
    # grip ribs on the two end walls (the speaker's 20 mm ends), C8 interference, centred on its thickness
    gr = P.SPK_CLEAR + P.SPK_GRIP_RIB
    xm = sx1 - rh - P.SPK_T / 2
    # v5.3 (Paul): only on the -Y end. On the lead end the slot left the rib's top as a free-standing fin;
    # one rib still pushes the speaker against the lead-end wall with the same 0.15 grip.
    for y0_, y1_ in ((sy0 - 0.01, sy0 + gr),):
        b = b.fuse(_box((xm - 0.4, y0_, sz0 - 0.01), (xm + 0.4, y1_, sz1 - 1.0)))
    # lead slot: +Y end wall (H3 is on the +Y side of the board), from mid-height to the top
    b = b.cut(_box((xm - P.SPK_LEAD_SLOT_W / 2, sy1 - 0.1, zc - P.SPK_LEAD_SLOT_W / 2), (xm + P.SPK_LEAD_SLOT_W / 2, sy1 + 1.4, sz1 + 1)))
    return A.solidify(b), dict(holes=len(pts), centre=(round(xm, 2), yc, round(zc, 2)))


def speaker_dummy():
    (sx0, sy0, sz0), (sx1, sy1, sz1) = speaker_box()
    return _box((sx1 - P.SPK_SEAL_RIB_H - P.SPK_T, -P.SPK_L / 2, sz0), (sx1 - P.SPK_SEAL_RIB_H, P.SPK_L / 2, sz0 + P.SPK_W))


# ------------------------------------------------------------------ latch clips (C7 v5 notch 1, separate PETG parts)
def _clip_profile():
    """(n, a) polygons of the clip: n inward from the wall face, a along the closing motion, 0 at the seam."""
    hk, tb, L = P.LATCH_HOOK, P.SNAP_BEAM_T, P.SNAP_BEAM_L
    lead = hk / math.tan(math.radians(P.SNAP_LEADIN_DEG))
    ret = hk / math.tan(math.radians(P.LATCH_CATCH_DEG))
    nb0 = P.LATCH_CLIP_NB0
    c_ax = P.LATCH_AXIAL_PLAY
    n_lip = nb0 - P.LATCH_GUIDE_CLEAR
    # place the hook so the lid keeps LATCH_CLIP_LIP_MIN under the catch at its thinnest point
    # retention face: (nb0-hk, a_ro) -> (nb0, a_ro - ret); lid catch = that face shifted by -c_ax
    a_ro = P.LATCH_CLIP_LIP_MIN + c_ax + (hk - (nb0 - n_lip)) * ret / hk
    a_tip = a_ro + lead + 0.3
    a_root = a_tip - L
    beam = [(nb0 + tb, a_root - 0.01), (nb0 + tb, a_tip), (nb0, a_tip), (nb0 - hk, a_tip - lead), (nb0 - hk, a_ro),
            (nb0, a_ro - ret), (nb0, a_root - 0.01)]
    nb_lo, nb_hi = nb0 - 0.7, nb0 + tb + P.LATCH_CLIP_BAR_IN
    a_b0 = a_root - P.LATCH_CLIP_BAR_H
    k = P.LATCH_CLIP_SHOULDER                     # shoulder rise per unit width: steep enough that the base
    bar = [(nb_lo, a_b0), (nb_hi, a_b0), (nb_hi, a_root - k * (nb_hi - nb0 - tb)), (nb0 + tb, a_root), (nb0, a_root),
           (nb_lo, a_root - k * (nb0 - nb_lo))]       # channel ceiling stays >= 45 deg even with the 12 deg lean
    lip_a = lambda n: a_ro - c_ax - (n - (nb0 - hk)) * ret / hk
    return dict(beam=beam, bar=bar, nb0=nb0, tb=tb, hk=hk, a_tip=a_tip, a_root=a_root, a_b0=a_b0,
                nb_lo=nb_lo, nb_hi=nb_hi, n_lip=n_lip, lip_a=lip_a)


def _clip_frames(zb):
    """[(name, origin, n, a, t)] in the world, one per clip. a = closing direction at that point."""
    H = bf_point_to_wf((P.TILT_HINGE[0], 0, P.TILT_HINGE[1]), zb)
    ax = bf_point_to_wf((P.TILT_HINGE[0], 1, P.TILT_HINGE[1]), zb) - H
    ax /= np.linalg.norm(ax)
    up_bf = bf_point_to_wf((0, 0, 1), zb) - bf_point_to_wf((0, 0, 0), zb)
    x0, x1, y0, y1 = footprint()
    out = []
    walls = {"y-": ((0, 1, 0), lambda u: (u, y0 + P.CASE_WALL)), "y+": ((0, -1, 0), lambda u: (u, y1 - P.CASE_WALL)),
             "x+": ((-1, 0, 0), lambda u: (x1 - P.CASE_WALL, u)), "x-": ((1, 0, 0), lambda u: (x0 + P.CASE_WALL, u))}
    for name, wall, u, E_, side in P.LATCH_CLIPS:
        nrm, xy = walls[wall]
        x, y_in = xy(u)
        # seam height on the wall face here: BF z = SEAM_Z
        # world point = BOARD_X + bx, by cos - bz sin, zb + by sin + bz cos  with bz = SEAM_Z
        by = (y_in + P.SEAM_Z * math.sin(T)) / math.cos(T)
        S = np.array([x, y_in, zb + by * math.sin(T) + P.SEAM_Z * math.cos(T)])
        v = np.cross(ax, S - H)
        a = v / np.linalg.norm(v)
        if np.dot(a, up_bf) < 0:
            a = -a
        n = np.array(nrm, float)
        n = n - np.dot(n, a) * a
        n /= np.linalg.norm(n)
        t = np.cross(n, a)                    # so the plane's local Y comes out as +a
        # the clip leans with the closing motion; slide it inward until every point of the clip (and
        # its base channel) stays >= 0.3 inside the wall face
        nw = np.array(nrm, float)
        pr = _clip_profile()
        pts = pr["beam"] + pr["bar"] + [(pr["nb_lo"] - P.LATCH_CLIP_FIT, pr["a_b0"] - P.LATCH_CLIP_FIT)]
        dmin = min(nn * np.dot(n, nw) + aa * np.dot(a, nw) for nn, aa in pts if aa < 0)
        if dmin < 0.3:
            # leaning would take it into the wall (back wall): stand it parallel to the wall instead
            a = a - np.dot(a, nw) * nw
            a /= np.linalg.norm(a)
            n = nw.copy()
            t = np.cross(n, a)
        out.append((name, S, n, a, t, E_, side))
    return out


def _frame_loc(S, n, a, t):
    """Location mapping local (X=n, Y=a, Z=t) to the world."""
    pl = cq.Plane(origin=cq.Vector(*S), xDir=cq.Vector(*n), normal=cq.Vector(*t))
    return cq.Location(pl)


def _poly_solid(pts, z0, z1):
    w = cq.Wire.makePolygon([cq.Vector(x, y, 0) for x, y in pts], close=True)
    return cq.Solid.extrudeLinear(cq.Face.makeFromWires(w), cq.Vector(0, 0, z1 - z0)).moved(cq.Location(cq.Vector(0, 0, z0)))


def clip_local(E_=None):
    """The clip in its local frame (X=n, Y=a, Z=t); this is also its print orientation (flat, Z up)."""
    pr = _clip_profile()
    E_ = E_ or P.LATCH_CLIP_E
    return _poly_solid(pr["beam"], 0, E_).fuse(_poly_solid(pr["bar"], 0, E_)).clean().moved(cq.Location(cq.Vector(0, 0, -E_ / 2)))


def latch_clips(zb):
    return {f"clip_{name}": clip_local(E_).moved(_frame_loc(S, n, a, t)) for name, S, n, a, t, E_, _ in _clip_frames(zb)}


def latch_features(b, top, zb):
    """Base: boss with a channel the clip slides into along the wall (open at +t). Top shell: pocket with the catch."""
    from shapely.geometry import Polygon
    from shapely.ops import unary_union
    pr = _clip_profile()
    c = P.LATCH_CLIP_FIT
    clip_poly = unary_union([Polygon(pr["beam"]), Polygon(pr["bar"])])
    chan = clip_poly.buffer(c, join_style=2)
    # beam bending room below the seam (inward), plus the channel continues up through the seam
    defl = Polygon([(pr["nb0"] - c, pr["a_root"] + 0.5), (pr["nb0"] + pr["tb"] + 0.9, pr["a_root"] + 0.5),
                    (pr["nb0"] + pr["tb"] + 0.9, 3.0), (pr["nb0"] - c, 3.0)])
    chan = unary_union([chan, defl])
    nB = pr["nb_hi"] + c + 1.2
    a_bot = pr["a_b0"] - c - 1.2 - nB
    # 45 deg underside into the wall; inner top corner chamfered so it keeps clear of the PCB edge in the swing
    ch_n = nB - (pr["nb0"] + pr["tb"] + 0.9 + 1.2)
    a_bot -= 30.0                                     # v5.2: deep; the battery roof below sets the 45 deg underside
    boss = [(-8.0, a_bot - 8.0), (nB, a_bot + nB), (nB, 0.5 - ch_n - 2.0), (nB - ch_n, 0.5 - 2.0), (nB - ch_n, 0.5), (-8.0, 0.5)]
    hk = pr["hk"]
    n_far = pr["nb0"] + pr["tb"] + hk + 0.3
    n_win = pr["nb0"] - hk - 0.2
    a_top = pr["a_tip"] + 0.3
    pocket = [(pr["n_lip"], -3.0), (n_far, -3.0), (n_far, a_top), (n_win, a_top), (n_win, pr["lip_a"](n_win)),
              (pr["n_lip"], pr["lip_a"](pr["n_lip"]))]
    ct = P.LATCH_POCKET_T_CLEAR
    below = halfspace_bf(zb, P.SEAM_Z, above=False)
    walls_of = {c_[0]: c_[1] for c_ in P.LATCH_CLIPS}
    for name, S, n, a, t, E_, side in _clip_frames(zb):
        wall = walls_of[name]
        loc = _frame_loc(S, n, a, t)
        bs = _poly_solid(boss, -E_ / 2 - side, E_ / 2 + side).moved(loc).intersect(below).intersect(base_body(zb))
        # 45 deg underside, over its full width: the battery roof alone left the boss's end past the battery
        # edge running to the floor as a thin leaning spike (Paul's slicer view)
        if wall in ("y-", "y+"):                       # front/back wall: over the battery, 45 deg front-to-back roof
            bs = bs.cut(battery_cut45()).cut(battery_roof_y())
        else:                                         # side wall: nothing under it but the speaker pocket
            (sx0, sy0, sz0), (sx1, sy1, sz1) = speaker_box()
            bs = bs.cut(_battery_keepout()).cut(_box((sx0 - 0.3, sy0 - 1.5, 0), (sx1 + 5, sy1 + 1.5, sz1 + 0.5)))
        ch = _poly_solid(list(chan.exterior.coords)[:-1], -E_ / 2 - c, E_ / 2 + side + 0.5).moved(loc)   # open at +t
        b = b.fuse(bs).cut(ch)
        top = top.cut(_poly_solid(pocket, -E_ / 2 - ct, E_ / 2 + ct).moved(loc))
    return A.solidify(b), A.solidify(top), dict(a_tip=round(pr["a_tip"], 2), a_root=round(pr["a_root"], 2))


# ------------------------------------------------------------------ everything
def build_all(zb=None, log=print):
    """All Phase 2 parts in the world (assembled) frame."""
    zb = zb or board_z()
    log("base shell + ledges")
    b = base(zb)
    log("USB-C bay"); b = port_features(b, zb)
    log("mic ducts"); b, mic = mic_features(b, zb)
    # v5 (Paul): no button access at all. PWR and BOOT aren't used by his firmware; walls stay plain.
    pwr = boot = None
    if P.BUTTON_ACCESS:
        log("PWR pin tube"); b, pwr = pwr_features(b, zb)
    log("speaker"); b, spk = speaker_features(b, zb)
    log("switch"); b, sw = switch_features(b, zb)
    if P.BUTTON_ACCESS:
        log("BOOT pinhole"); b, boot = boot_features(b, zb)
    log("cable rail"); b = cable_rail_features(b, zb)
    if P.RIBS:
        log("ribs"); b = rib_texture(b, zb)
    log("top shell"); top = top_shell(zb)
    log("latches"); b, top, lat = latch_features(b, top, zb)
    parts = {"top_shell": top, "base": b, "cable_clip": cable_clip(zb)}
    if P.BUTTON_ACCESS:
        parts["pwr_pin"] = pwr_pin(zb)
    parts.update(latch_clips(zb))
    return parts, dict(mic=mic, pwr=pwr, speaker=spk, switch=sw, boot=boot, latch=lat, zb=zb)


# ------------------------------------------------------------------ slide switch: C4 K3 cell in the floor
def switch_features(b, zb):
    """The C4 pocket (K3) set into the floor, outer face = case bottom, open side toward the battery.
    Same numbers and print orientation as the coupon Paul tested. Layout B: it sits behind the speaker
    (back-right), long axis front-to-back, open toward the battery's right side, which keeps it in."""
    bl, bw, bh = P.SWITCH_BODY
    pl, pw = P.SWITCH_POCKET
    sl, sw = P.SWITCH_KNOB_SLOT
    wall = P.WALL_MIN
    cav_h = bh + P.SWITCH_POCKET_H_CLEAR
    pin = P.SWITCH_PIN_SPACE
    plate = P.SWITCH_KNOB_HEIGHT + P.SWITCH_KNOB_TIP_RECESS
    H = plate + cav_h + pin
    (bx0, by0, bz0), (bx1, by1, bz1) = battery_box()
    # build in a local frame: open side at u = 0 facing -u, length along v; then u -> +X, v -> +Y
    add, cut = [], []
    add.append(_box((0, -pl / 2 - wall, 0), (pw + wall, pl / 2 + wall, H)))
    cut.append(_box((pw / 2 - sw / 2, -sl / 2, -1), (pw / 2 + sw / 2, sl / 2, plate)))                 # knob slot
    cw = P.SWITCH_KNOB_CHANNEL_W
    cut.append(_box((-0.5, -cw / 2, P.SWITCH_CHANNEL_SKIN), (pw / 2, cw / 2, plate + 0.01)))            # knob entry channel
    cut.append(_box((-0.5, -pl / 2, plate), (pw, pl / 2, H + 5)))                                    # body cavity
    cut.append(_box((-1.5, -pl / 2 - wall, bz0), (0.01, pl / 2 + wall, bz0 + P.BATTERY_POCKET_WALL_H + 1)))  # low-wall gap
    z0 = plate + cav_h - P.SWITCH_LIP
    for sgn in (-1, 1):
        vw = sgn * pl / 2
        vi = vw - sgn * P.SWITCH_LIP
        prof = [(vw, z0), (vi, z0 + P.SWITCH_LIP), (vi, H), (vw, H)]
        w = cq.Wire.makePolygon([cq.Vector(0, v, z) for v, z in prof], close=True)
        add.append(cq.Solid.extrudeLinear(cq.Face.makeFromWires(w), cq.Vector(pw, 0, 0)))
    loc = cq.Location(cq.Vector(bx1 + 1.2 * 0, P.SWITCH_CELL_Y, 0))
    body = base_body(zb)
    for a_ in add[:1]:
        b = b.fuse(a_.moved(loc).intersect(body))
    for c_ in cut:
        b = b.cut(c_.moved(loc))
    for a_ in add[1:]:
        b = b.fuse(a_.moved(loc))
    return A.solidify(b), dict(centre=(round(bx1 + pw / 2, 2), P.SWITCH_CELL_Y), knob_slot=(sl, sw), plate=plate)


# ------------------------------------------------------------------ BOOT pinhole (KEY1), PLAN 3.5
def boot_features(b, zb):
    """Paperclip guide: a tube on KEY1's press axis from just past the PCB edge out through the
    front-right corner, D BOOT_HOLE_D at the surface. Fin under it like the PWR tube."""
    key = P.BOOT_IS
    a, tip, zc = _pin_frame(key)
    d, w = P.BOOT_HOLE_D, P.PWR_TUBE_WALL
    s0 = P.BOOT_TUBE_START
    body = outer_body(zb)
    s = s0
    while s < 80:
        p = (tip[0] + s * math.cos(a), tip[1] + s * math.sin(a), zc)
        if not body.isInside(cq.Vector(*bf_point_to_wf(p, zb)), 1e-4):
            break
        s += 0.05
    s_x = s
    s0 = max(s0, s_x - P.BOOT_TUBE_LEN)          # v3: short guide, mostly in the wall (inner end sat over the speaker)
    h = d / 2
    top_z = min(zc + h + w, P.PWR_PIN_TOP + P.PWR_PIN_CLEAR + w)
    tube = _local_box(s0, s_x + 5, -h - w, h + w, zc - h - w, top_z, key)
    bore = _local_box(s0 - 3, s_x + 6, -h, h, zc - h, zc + h, key)
    fin = _local_box(s0, s_x + 5, -h - w, h + w, -60, zc - h - w + 0.01, key)
    tube_w, bore_w, fin_w = (to_wf(x, zb) for x in (tube, bore, fin))
    p_in = bf_point_to_wf((tip[0] + s0 * math.cos(a), tip[1] + s0 * math.sin(a), zc - h - w), zb)
    u = bf_point_to_wf((tip[0] + math.cos(a), tip[1] + math.sin(a), 0), zb) - bf_point_to_wf((tip[0], tip[1], 0), zb)
    uh = np.array([u[0], u[1], 0.0]); uh /= np.linalg.norm(uh)
    nrm = cq.Vector(uh[0], uh[1], 1.0).normalized()
    keep = cq.Solid.makeBox(400, 400, 400, pnt=cq.Vector(-200, -200, 0)).moved(
        cq.Location(cq.Plane(origin=cq.Vector(*p_in), xDir=cq.Vector(-uh[1], uh[0], 0), normal=nrm)))
    fin_w = fin_w.intersect(keep)
    (bx0, by0, bz0), (bx1, by1, bz1) = battery_box()
    tube_w = tube_w.cut(_box((bx0 - 0.5, by0 - 0.5, bz0 - 1), (bx1 + 0.5, by1 + 0.5, bz1 + 0.5)))
    fin_w = fin_w.cut(battery_cut45())
    add = (tube_w.fuse(fin_w).intersect(base_body(zb)).intersect(halfspace_bf(zb, P.SEAM_Z, above=False))
           .cut(_box(*speaker_box())))
    b = b.fuse(add).cut(bore_w)
    ex = bf_point_to_wf((tip[0] + s_x * math.cos(a), tip[1] + s_x * math.sin(a), zc), zb)
    return A.solidify(b), dict(exit=tuple(round(float(v), 2) for v in ex), length=round(s_x - s0, 1))


# ------------------------------------------------------------------ cable clip (separate part) on a vertical dovetail, left face
def _rail_xy():
    """Dovetail section in the XY plane (X = out from the left face, Y along it); vertical faces only."""
    wb, pr = P.CABLE_RAIL_W, P.CABLE_RAIL_PROUD
    return [(0.01, -wb / 2), (-pr, -wb / 2 - pr), (-pr, wb / 2 + pr), (0.01, wb / 2)]


def cable_rail_features(b, zb):
    """Vertical dovetail on the left face (prints standing: no overhangs) + a stop at the bottom.
    The clip drops onto it from above and rests on the stop."""
    x0 = footprint()[0]
    yc = P.CABLE_RAIL_YC
    z0, z1 = P.CABLE_STOP_H, P.CABLE_STOP_H + P.CABLE_CLIP_H
    w = cq.Wire.makePolygon([cq.Vector(x0 + x, yc + y, z0 - 0.01) for x, y in _rail_xy()], close=True)
    rail = cq.Solid.extrudeLinear(cq.Face.makeFromWires(w), cq.Vector(0, 0, z1 - z0 + 0.01))
    hw = P.CABLE_RAIL_W / 2 + P.CABLE_RAIL_PROUD + P.CABLE_CLIP_FIT + 0.8
    stop = _box((x0 - P.CABLE_RAIL_PROUD - P.CABLE_CLIP_FIT - 0.8, yc - hw, 0), (x0 + 1.0, yc + hw, z0))
    return A.solidify(b.fuse(rail).fuse(stop.intersect(_box((x0 - 20, -100, 0), (x0 + 1.0, 100, 50)))))


def cable_clip(zb):
    """Separate part, printed as installed (Z up): dovetail slot (through, vertical) + a U-channel for the
    cable running along Y, open at the top so the cable presses in and lifts out by hand."""
    x0 = footprint()[0]
    yc = P.CABLE_RAIL_YC
    c, pr, wb = P.CABLE_CLIP_FIT, P.CABLE_RAIL_PROUD, P.CABLE_RAIL_W
    t = P.CABLE_CLIP_WALL
    z0, H = P.CABLE_STOP_H, P.CABLE_CLIP_H
    r = P.CABLE_OD / 2 + 0.15
    L = P.CABLE_CLIP_LEN
    back = -pr - c - t                                       # back plate outer face (X, from the wall face)
    xc = back - r                                            # cable centre
    zc = z0 + t + r
    body = _box((x0 + xc - r - t, yc - L / 2, z0), (x0 - c, yc + L / 2, z0 + H))
    # dovetail slot through the full height
    sl = [(0.5, -wb / 2 - c), (-pr - c, -wb / 2 - pr - c), (-pr - c, wb / 2 + pr + c), (0.5, wb / 2 + c)]
    w = cq.Wire.makePolygon([cq.Vector(x0 + x, yc + y, z0 - 1) for x, y in sl], close=True)
    body = body.cut(cq.Solid.extrudeLinear(cq.Face.makeFromWires(w), cq.Vector(0, 0, H + 2)))
    # U channel along Y: cylinder + mouth slot to the top (mouth CABLE_CLIP_MOUTH x OD)
    chan = cq.Solid.makeCylinder(r, L + 2, pnt=cq.Vector(x0 + xc, yc - L / 2 - 1, zc), dir=cq.Vector(0, 1, 0))
    m = P.CABLE_CLIP_MOUTH * P.CABLE_OD
    mouth = _box((x0 + xc - m / 2, yc - L / 2 - 1, zc), (x0 + xc + m / 2, yc + L / 2 + 1, z0 + H + 1))
    clip = body.cut(chan).cut(mouth)
    return A.solidify(clip)


# ------------------------------------------------------------------ outside texture (v6, Paul: ribbed / japandi)
def rib_texture(b, zb):
    """Vertical half-round flutes round the base's outside: cylinders R RIB_R whose axes sit RIB_R - RIB_PROUD
    inside the wall face, every RIB_PITCH along the perimeter (they follow the rounded corners). They run from
    the desk to RIB_TOP_BAND below the seam, and skip the USB-C recess and the cable rail. The speaker grille is
    drilled through them."""
    x0, x1, y0, y1 = footprint()
    R, proud, pitch = P.RIB_R, P.RIB_PROUD, P.RIB_PITCH
    inset = R - proud                                  # axis inside the wall face
    Rc = P.CASE_CORNER_R
    # perimeter path at the axis line: rounded rectangle offset inward by `inset`
    import shapely.geometry as sg
    rect = sg.box(x0 + Rc, y0 + Rc, x1 - Rc, y1 - Rc).buffer(Rc - inset, resolution=64)
    ring = rect.exterior
    n = int(round(ring.length / pitch))
    step = ring.length / n
    g = A.usb_geometry()
    pc = bf_point_to_wf((P.USB_MOUTH_X, g["yc"], g["zc"]), zb)
    bay_y = (pc[1] - P.PORT_BAY_HALF_W - P.CASE_WALL - R, pc[1] + P.PORT_BAY_HALF_W + P.CASE_WALL + R)
    bay_z = pc[2] - P.PORT_BAY_BELOW - max(P.CASE_FLOOR, 3.0) - 0.5
    rail_y = (P.CABLE_RAIL_YC - P.CABLE_CLIP_LEN / 2 - R - 0.5, P.CABLE_RAIL_YC + P.CABLE_CLIP_LEN / 2 + R + 0.5)
    rail_z = P.CABLE_STOP_H + P.CABLE_CLIP_H + 1.0
    ribs = []
    for k in range(n):
        p_ = ring.interpolate(k * step)
        x, y = p_.x, p_.y
        z0, z1 = 0.0, 200.0
        on_left = x < x0 + Rc - 1e-6 and abs(x - (x0 + inset)) < 0.5
        if on_left and bay_y[0] < y < bay_y[1]:
            z1 = bay_z                                 # below the USB-C recess only
        if on_left and rail_y[0] < y < rail_y[1]:
            z0 = rail_z                                # above the cable clip only
        if z1 - z0 < 3.0:
            continue
        r_ = cq.Solid.makeCylinder(R, z1 - z0 + R + 1, pnt=cq.Vector(x, y, z0 - R - 1), dir=cq.Vector(0, 0, 1))
        if z0 > 0:
            # starts above the desk: 45 deg underside growing out of the wall (left wall: outward = -X)
            pl = cq.Plane(origin=cq.Vector(x0, 0, z0), xDir=cq.Vector(0, 1, 0), normal=cq.Vector(1, 0, 1).normalized())
            r_ = r_.intersect(cq.Solid.makeBox(400, 400, 400, pnt=cq.Vector(-200, -200, 0)).moved(cq.Location(pl)))
        else:
            r_ = r_.intersect(_box((-200, -200, 0), (200, 200, 300)))
        ribs.append(r_)
    rib = cq.Compound.makeCompound(ribs)
    rib = rib.intersect(halfspace_bf(zb, P.SEAM_Z - P.RIB_TOP_BAND, above=False)).cut(outer_body(zb))
    b = b.fuse(rib)
    if _GRILLE.get("holes"):
        b = b.cut(cq.Compound.makeCompound(_GRILLE["holes"]))
    return A.solidify(b)
