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
def board_z():
    """Lowest board height that keeps CASE_AIR between the board stack and the battery."""
    import sys
    from pathlib import Path
    sys.path.insert(0, str(Path(__file__).resolve().parent.parent / "concepts"))
    import layout_study as L
    return L.study("phase2", P.TILT_DEG, (0.0, 0.0))["board_z"]


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
def battery_box():
    x0 = P.BOARD_X - (A.BORE_R + 1.6) + P.CASE_WALL
    L, W, H = P.BATTERY_POCKET
    return (x0, -W / 2, P.CASE_FLOOR), (x0 + L, W / 2, P.CASE_FLOOR + H)


def speaker_box():
    (_, _, _), (bx1, _, _) = battery_box()
    t = P.SPK_T + 2 * P.SPK_CLEAR
    l = P.SPK_L + 2 * P.SPK_CLEAR
    h = P.SPK_W + P.SPK_DEPTH_CLEAR
    x0 = bx1 + P.SPK_DIVIDER
    return (x0, -l / 2, P.CASE_FLOOR), (x0 + t, l / 2, P.CASE_FLOOR + h)


def footprint():
    bez = A.BORE_R + 1.6                                      # bezel ring outer radius (BF)
    x0 = P.BOARD_X - bez
    x1 = speaker_box()[1][0] + P.CASE_WALL
    yr = bez * math.cos(T) + 1.0
    yb = P.BATTERY_POCKET[1] / 2 + P.CASE_WALL
    y_half = max(yr, yb)
    return x0, x1, -y_half, y_half


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
    return A.solidify(s)


# ------------------------------------------------------------------ base
def _inner_cavity():
    x0, x1, y0, y1 = footprint()
    w = P.CASE_WALL
    return (cq.Workplane("XY").workplane(offset=P.CASE_FLOOR).center((x0 + x1) / 2, (y0 + y1) / 2)
            .sketch().rect(x1 - x0 - 2 * w, y1 - y0 - 2 * w).vertices().fillet(P.CASE_CORNER_R - w).finalize().extrude(200).val())


def _box(lo, hi):
    return cq.Solid.makeBox(hi[0] - lo[0], hi[1] - lo[1], hi[2] - lo[2], pnt=cq.Vector(*lo))


def base_shell(zb):
    b = outer_body(zb).intersect(halfspace_bf(zb, P.SEAM_Z, above=False)).cut(_inner_cavity())
    (bx0, by0, bz0), (bx1, by1, bz1) = battery_box()
    # low walls front/back of the battery (keep it from sliding); wire gap at the back-left corner
    for sy in (-1, 1):
        y = by1 if sy > 0 else by0 - 1.2
        b = b.fuse(_box((bx0, y, bz0 - 0.01), (bx1, y + 1.2, bz0 + P.BATTERY_POCKET_WALL_H)))
    b = b.cut(_box((bx0, by1 - 0.1, bz0), (bx0 + 9.0, by1 + 1.4, bz0 + P.BATTERY_POCKET_WALL_H + 1)))
    # speaker pocket: divider wall + end walls (speaker stands, fires out the right wall)
    (sx0, sy0, sz0), (sx1, sy1, sz1) = speaker_box()
    b = b.fuse(_box((bx1, sy0 - 1.2, sz0 - 0.01), (sx0, sy1 + 1.2, sz1)))
    for y in (sy0 - 1.2, sy1):
        b = b.fuse(_box((sx0 - 0.01, y, sz0 - 0.01), (sx1 + 0.01, y + 1.2, sz1)))
    return A.solidify(b)


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
    return (cq.Workplane("YZ", origin=(inner[0] - P.BOARD_LEDGE_W / 2, 0, 0)).polyline(prof).close()
            .extrude(P.BOARD_LEDGE_W).val())


def pick_ledge_angles(zb, board_wf):
    """Two front and two back ledge positions that touch nothing but the PCB back."""
    from lfcase.interference import check
    obst = {k: v for k, v in board_wf.items() if k != "pcb"}
    # plugs + bent wires (Paul: 7 mm below the PCB) around J1 / H3
    for nm, bb in (("plug_j1", P.J1_BBOX), ("plug_h3", P.H3_BBOX)):
        (px0, py0, _), (px1, py1, _) = bb
        g = P.PLUG_KEEPOUT_GROW
        obst[nm] = to_wf(cq.Solid.makeBox(px1 - px0 + 2 * g, py1 - py0 + 2 * g, P.MIN_DEPTH_BELOW_PCB,
                                          pnt=cq.Vector(px0 - g, py0 - g, -P.MIN_DEPTH_BELOW_PCB)), zb)
    chosen = []
    for target in (-60, -120, 60, 120):
        for d in (0, 5, -5, 10, -10, 15, -15, 20, -20, 25, -25):
            a = target + d
            if not check(_ledge(zb, a), obst):
                chosen.append(a)
                break
    return chosen


def board_ledges(zb, angles=None):
    out = None
    for a in (angles or P.BOARD_LEDGE_ANGLES):
        sol = _ledge(zb, a)
        out = sol if out is None else out.fuse(sol)
    return out


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
    return A.solidify(b.fuse(blk_w))


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


def _standoff_points(xy):
    import numpy as np
    r = P.STANDOFF_OD / 2
    return np.array([(xy[0] + r * math.cos(t), xy[1] + r * math.sin(t), z)
                     for t in np.linspace(0, 2 * math.pi, 48, endpoint=False) for z in np.linspace(P.STANDOFF_Z[0], 0, 8)])


def _mic_swept_outline(mic, air=0.2):
    return _swept_outline(_mic_points(mic), air)


def _prism(poly, z0, z1):
    pts = [cq.Vector(x, y, 0) for x, y in list(poly.exterior.coords)[:-1]]
    w = cq.Wire.makePolygon(pts, close=True)
    return cq.Solid.extrudeLinear(cq.Face.makeFromWires(w), cq.Vector(0, 0, z1 - z0)).moved(cq.Location(cq.Vector(0, 0, z0)))


def _mic_parts_bf(mic, air=None):
    """Collar (with crush rib) and its pocket, board frame. Pocket = package swept along the tilt-in arc + air.

    Same wall / rib / cavity numbers as C5. If a standoff is too close for a wall between them (MIC2), the
    collar wraps round the standoff too: its pocket also takes the standoff's swept footprint, deeper."""
    from shapely.geometry import Point
    air = P.MIC_COLLAR_CLEAR if air is None else air
    wl = P.MIC_COLLAR_WALL
    hole, zmin = _mic_swept_outline(mic, air)
    z_cav = min(getattr(P, f"{mic}_Z")[0], zmin) - P.MIC_CAVITY_H
    inner = _prism(hole, z_cav, 1.0)
    outline = hole
    import numpy as np
    from shapely.geometry import MultiPoint
    from lfcase.board import load_board
    cands = [_standoff_points(xy) for xy in P.MOUNT_HOLES_XY]
    for grp in ("misc_back", "microsd", "h3", "header", "j1", "key1", "key2", "usb_c"):
        for sol in load_board(verbose=False)[grp].Solids():
            bb = sol.BoundingBox()
            cands.append(np.array([(x, y, z) for x in (bb.xmin, bb.xmax) for y in (bb.ymin, bb.ymax)
                                   for z in (bb.zmin, min(bb.zmax, 0.0))]))
    taken = set()
    for _ in range(8):                                   # enclose neighbours until none sits under the wall
        grew = False
        for n, pts in enumerate(cands):
            if n in taken or pts[:, 2].min() > 0.05:
                continue
            fp = MultiPoint(pts[:, :2]).convex_hull
            if outline.distance(fp) < wl + 0.3:
                sh, sz = _swept_outline(pts, air)
                outline = outline.union(sh).convex_hull
                inner = inner.fuse(_prism(sh, sz - air, 1.0))
                taken.add(n); grew = True
        if not grew:
            break
    if outline is not hole:
        inner = inner.fuse(_prism(outline, z_cav, 1.0))
    js = dict(join_style=2, mitre_limit=2.0)
    outer = _prism(outline.buffer(wl, **js), -40.0, 0.0)
    k = wl / 2
    rib = _prism(outline.buffer(k + P.MIC_CRUSH_RIB_W / 2, **js), -0.01, P.MIC_CRUSH).cut(
        _prism(outline.buffer(k - P.MIC_CRUSH_RIB_W / 2, **js), -0.1, 1.0))
    return outer.fuse(rib), inner, z_cav


def mic_features(b, zb):
    """Collar + crush rib on each mic, duct straight out through the USB bay's back wall (world -X)."""
    fx_w = P.BOARD_X + A.usb_geometry()["face_x"]
    d = P.MIC_DUCT_D
    cav = _inner_cavity().fuse(_box((fx_w - 0.01, -100, P.CASE_FLOOR - 0.01), (fx_w + 5, 100, 100)))
    info = {}
    for mic in ("MIC1", "MIC2"):
        cx, cy = getattr(P, f"{mic}_CENTER")
        collar, inner, z_cav = _mic_parts_bf(mic)
        collar_w = to_wf(collar, zb).intersect(cav)
        inner_w = to_wf(inner, zb)
        # duct: horizontal in the world (prints as a teardrop, point up), from the pocket out through the bay wall
        c = bf_point_to_wf((cx, cy, z_cav - d / 2 + 0.05), zb)
        x_in, x_out = c[0], fx_w - 1.0
        L = x_in - x_out
        s = d / 2 * math.sqrt(2) / 2
        tear = (cq.Workplane("YZ").workplane(offset=x_out).center(c[1], c[2]).circle(d / 2).extrude(L).val()
                .fuse(cq.Solid.makeBox(L, d / 2 * math.sqrt(2), d / 2 * math.sqrt(2), pnt=cq.Vector(x_out, c[1] - s, c[2] - s))
                      .rotate(cq.Vector(0, c[1], c[2]), cq.Vector(1, c[1], c[2]), 45)))
        # solid sleeve from the floor to above the duct, between the collar and the bay wall
        sleeve = _box((fx_w - 0.01, c[1] - d / 2 - 0.8, P.CASE_FLOOR - 0.01), (x_in, c[1] + d / 2 + 0.8, c[2] + d / 2 + 0.8))
        b = b.fuse(collar_w).fuse(sleeve).cut(inner_w).cut(tear)
        info[mic] = dict(duct_len=round(x_in - fx_w, 2), exit=(round(x_out, 2), round(c[1], 2), round(c[2], 2)))
    return A.solidify(b), info
