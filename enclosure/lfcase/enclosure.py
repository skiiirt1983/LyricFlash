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
    s = s.cut(hinge_relief(zb))
    return A.solidify(s)


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
    blk_w = blk_w.cut(_box(*battery_box()))                       # stays above the battery (support under it)
    # tie the port wall into the case: a floor under the bay and a cheek either side, wall-thick
    t = P.CASE_WALL
    frame = _box((x0, pc[1] - hw - t, zbot - P.CASE_FLOOR), (fx_w + P.WALL_MIN, pc[1] + hw + t, zbot + 0.01))
    for s in (-1, 1):
        y_a = pc[1] + s * hw
        frame = frame.fuse(_box((x0, min(y_a, y_a + s * t), zbot - P.CASE_FLOOR), (fx_w + P.WALL_MIN, max(y_a, y_a + s * t), 100)))
    frame = frame.intersect(outer_body(zb)).intersect(halfspace_bf(zb, P.SEAM_Z, above=False)).cut(_box(*battery_box()))
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
    """Open mic ducts (Paul, Phase 2: no collar, no crush rib).

    A solid bar runs from the USB bay's back wall to under each mic. Its top is the board-parallel plane
    MIC_CAVITY_H below the mic lid (the swing only lifts the mic, so it never comes lower than that).
    A vertical hole under the mic's sound port meets a horizontal teardrop duct that exits in the bay."""
    fx_w = P.BOARD_X + A.usb_geometry()["face_x"]
    d = P.MIC_DUCT_D
    cav = _inner_cavity().fuse(_box((fx_w - 0.01, -100, P.CASE_FLOOR - 0.01), (fx_w + 5, 100, 100)))
    info = {}
    for mic in ("MIC1", "MIC2"):
        cx, cy = getattr(P, f"{mic}_CENTER")
        z_top = getattr(P, f"{mic}_Z")[0] - P.MIC_CAVITY_H                     # bar top, board frame
        port = bf_point_to_wf((cx, cy, z_top), zb)
        c = bf_point_to_wf((cx, cy, z_top - d / 2 - P.MIC_DUCT_COVER), zb)     # duct centre under the port
        hw = d / 2 + P.MIC_BAR_SIDE
        bar = (_box((fx_w - 0.01, c[1] - hw, P.CASE_FLOOR - 0.01), (c[0] + hw, c[1] + hw, 100))
               .intersect(halfspace_bf(zb, z_top, above=False)).intersect(cav)
               .cut(_box(*battery_box())))                                      # hangs over the battery: support
        x_out, x_in = fx_w - 1.0, c[0]
        L = x_in - x_out
        q = d / 2 * math.sqrt(2)
        tear = (cq.Workplane("YZ").workplane(offset=x_out).center(c[1], c[2]).circle(d / 2).extrude(L).val()
                .fuse(cq.Solid.makeBox(L, q, q, pnt=cq.Vector(x_out, c[1] - q / 2, c[2] - q / 2))
                      .rotate(cq.Vector(0, c[1], c[2]), cq.Vector(1, c[1], c[2]), 45)))
        riser = A.cyl(d / 2, c[2], port[2] + 2.0, x=c[0], y=c[1])
        b = b.fuse(bar).cut(tear).cut(riser)
        info[mic] = dict(duct_len=round(x_in - fx_w + port[2] - c[2], 2), exit=(round(x_out, 2), round(c[1], 2), round(c[2], 2)))
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
    fin_w = fin_w.intersect(keep).intersect(_inner_cavity().fuse(outer_body(zb)))
    shell = outer_body(zb)
    add = tube_w.fuse(fin_w).intersect(shell).intersect(halfspace_bf(zb, P.SEAM_Z, above=False))
    b = b.fuse(add).cut(bore_w).cut(room_w)
    # finger dimple: shallow cone round the exit
    lc, zc = (d["l0"] + d["l1"]) / 2, (d["z0"] + d["z1"]) / 2
    ex = bf_point_to_wf((tip[0] + s_x * math.cos(a) - lc * math.sin(a), tip[1] + s_x * math.sin(a) + lc * math.cos(a), zc), zb)
    cone = cq.Solid.makeCone(P.PWR_DIMPLE_D / 2 - P.PWR_DIMPLE_DEPTH, P.PWR_DIMPLE_D / 2 + 2, P.PWR_DIMPLE_DEPTH + 2,
                             pnt=cq.Vector(*(ex - uh * (P.PWR_DIMPLE_DEPTH + 0.0))), dir=cq.Vector(*uh))
    b = b.cut(cone)
    return A.solidify(b), dict(exit=tuple(round(float(v), 2) for v in ex), s_exit=round(s_x, 2))


# ------------------------------------------------------------------ speaker (C8 numbers, standing, fires out the right wall)
def _teardrop_x(x0, x1, y, z, d):
    """Horizontal hole along X, teardrop point up (prints without support)."""
    q = d / 2 * math.sqrt(2)
    return (cq.Workplane("YZ").workplane(offset=x0).center(y, z).circle(d / 2).extrude(x1 - x0).val()
            .fuse(cq.Solid.makeBox(x1 - x0, q, q, pnt=cq.Vector(x0, y - q / 2, z - q / 2))
                  .rotate(cq.Vector(0, y, z), cq.Vector(1, y, z), 45)))


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
    holes = [_teardrop_x(sx1 - rh - 0.1, x_out + 1, y, z, P.SPK_GRILLE_HOLE_D) for y, z in pts]
    b = b.cut(cq.Compound.makeCompound(holes))
    # grip ribs on the two end walls (the speaker's 20 mm ends), C8 interference, centred on its thickness
    gr = P.SPK_CLEAR + P.SPK_GRIP_RIB
    xm = sx1 - rh - P.SPK_T / 2
    for y0_, y1_ in ((sy0 - 0.01, sy0 + gr), (sy1 - gr, sy1 + 0.01)):
        b = b.fuse(_box((xm - 0.4, y0_, sz0 + 2.0), (xm + 0.4, y1_, sz1 - 1.0)))
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
    nb_lo, nb_hi = nb0 - 0.7, nb0 + tb + 1.6
    a_b0 = a_root - P.LATCH_CLIP_BAR_H
    bar = [(nb_lo, a_b0), (nb_hi, a_b0), (nb_hi, a_root - (nb_hi - nb0 - tb)), (nb0 + tb, a_root), (nb0, a_root),
           (nb_lo, a_root - (nb0 - nb_lo))]
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
    for name, y_in, nrm in (("front", y0 + P.CASE_WALL, (0, 1, 0)), ("back", y1 - P.CASE_WALL, (0, -1, 0))):
        x = P.LATCH_CLIP_X
        # seam height on the wall face at this X: BF z = SEAM_Z
        # solve world z for (x, y_in) on the plane BF z = SEAM_Z
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
        out.append((name, S, n, a, t))
    return out


def _frame_loc(S, n, a, t):
    """Location mapping local (X=n, Y=a, Z=t) to the world."""
    pl = cq.Plane(origin=cq.Vector(*S), xDir=cq.Vector(*n), normal=cq.Vector(*t))
    return cq.Location(pl)


def _poly_solid(pts, z0, z1):
    w = cq.Wire.makePolygon([cq.Vector(x, y, 0) for x, y in pts], close=True)
    return cq.Solid.extrudeLinear(cq.Face.makeFromWires(w), cq.Vector(0, 0, z1 - z0)).moved(cq.Location(cq.Vector(0, 0, z0)))


def clip_local():
    """The clip in its local frame (X=n, Y=a, Z=t); this is also its print orientation (flat, Z up)."""
    pr = _clip_profile()
    E_ = P.LATCH_CLIP_E
    return _poly_solid(pr["beam"], 0, E_).fuse(_poly_solid(pr["bar"], 0, E_)).clean().moved(cq.Location(cq.Vector(0, 0, -E_ / 2)))


def latch_clips(zb):
    return {f"clip_{name}": clip_local().moved(_frame_loc(S, n, a, t)) for name, S, n, a, t in _clip_frames(zb)}


def latch_features(b, top, zb):
    """Base: boss with a channel the clip slides into along the wall (open at +t). Top shell: pocket with the catch."""
    from shapely.geometry import Polygon
    from shapely.ops import unary_union
    pr = _clip_profile()
    E_, c = P.LATCH_CLIP_E, P.LATCH_CLIP_FIT
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
    boss = [(0.0 - 1.0, a_bot - 1.0), (nB, a_bot + nB), (nB, 0.5 - ch_n - 2.0), (nB - ch_n, 0.5 - 2.0), (nB - ch_n, 0.5), (-1.0, 0.5)]
    hk = pr["hk"]
    n_far = pr["nb0"] + pr["tb"] + hk + 0.3
    n_win = pr["nb0"] - hk - 0.2
    a_top = pr["a_tip"] + 0.3
    pocket = [(pr["n_lip"], -3.0), (n_far, -3.0), (n_far, a_top), (n_win, a_top), (n_win, pr["lip_a"](n_win)),
              (pr["n_lip"], pr["lip_a"](pr["n_lip"]))]
    ct = P.LATCH_POCKET_T_CLEAR
    below = halfspace_bf(zb, P.SEAM_Z, above=False)
    for name, S, n, a, t in _clip_frames(zb):
        loc = _frame_loc(S, n, a, t)
        bs = _poly_solid(boss, -E_ / 2 - 1.6, E_ / 2 + 1.6).moved(loc).intersect(below).intersect(outer_body(zb))
        (bx0, by0, bz0), (bx1, by1, bz1) = battery_box()
        bs = bs.cut(_box((bx0 - 0.5, by0 - 0.5, bz0 - 1), (bx1 + 0.5, by1 + 0.5, bz1 + 0.5)))   # keep off the battery
        ch = _poly_solid(list(chan.exterior.coords)[:-1], -E_ / 2 - c, E_ / 2 + 5.0).moved(loc)
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
    log("PWR pin tube"); b, pwr = pwr_features(b, zb)
    log("speaker"); b, spk = speaker_features(b, zb)
    log("switch"); b, sw = switch_features(b, zb)
    log("BOOT pinhole"); b, boot = boot_features(b, zb)
    log("cable rail"); b = cable_rail_features(b, zb)
    log("top shell"); top = top_shell(zb)
    log("latches"); b, top, lat = latch_features(b, top, zb)
    parts = {"top_shell": top, "base": b, "pwr_pin": pwr_pin(zb), "cable_clip": cable_clip(zb)}
    parts.update(latch_clips(zb))
    return parts, dict(mic=mic, pwr=pwr, speaker=spk, switch=sw, boot=boot, latch=lat, zb=zb)


# ------------------------------------------------------------------ slide switch: C4 K3 cell in the floor
def switch_features(b, zb):
    """The C4 pocket (K3) set into the floor, outer face = case bottom, open side toward the battery.
    Same numbers and print orientation as the coupon Paul tested."""
    bl, bw, bh = P.SWITCH_BODY
    pl, pw = P.SWITCH_POCKET
    sl, sw = P.SWITCH_KNOB_SLOT
    wall = P.WALL_MIN
    cav_h = bh + P.SWITCH_POCKET_H_CLEAR
    pin = P.SWITCH_PIN_SPACE
    plate = P.SWITCH_KNOB_HEIGHT + P.SWITCH_KNOB_TIP_RECESS
    H = plate + cav_h + pin
    (bx0, by0, bz0), (bx1, by1, bz1) = battery_box()
    cx = P.SWITCH_CELL_X
    y_open = by1 + P.SWITCH_OPEN_GAP
    y_closed = y_open + pw
    yc = y_open + pw / 2
    cell = _box((cx - pl / 2 - wall, y_open, 0), (cx + pl / 2 + wall, y_closed + wall, H))
    b = b.fuse(cell.intersect(outer_body(zb)))
    # gap in the battery's low back wall over the pocket (the battery itself closes the pocket)
    b = b.cut(_box((cx - pl / 2 - wall, by1 - 0.1, bz0), (cx + pl / 2 + wall, y_open + 0.01, bz0 + P.BATTERY_POCKET_WALL_H + 1)))
    # knob slot through the plate
    b = b.cut(_box((cx - sl / 2, yc - sw / 2, -1), (cx + sl / 2, yc + sw / 2, plate)))
    # knob entry channel from the slot to the open side, under a skin on the outer face
    cw = P.SWITCH_KNOB_CHANNEL_W
    b = b.cut(_box((cx - cw / 2, y_open - 0.5, P.SWITCH_CHANNEL_SKIN), (cx + cw / 2, yc, plate + 0.01)))
    # body cavity, open toward the battery and upward (pins)
    b = b.cut(_box((cx - pl / 2, y_open - 0.5, plate), (cx + pl / 2, y_closed, H + 5)))
    # lips over both body ends, 45 deg wedge on the underside (as C4)
    z0 = plate + cav_h - P.SWITCH_LIP
    for s in (-1, 1):
        xw = cx + s * pl / 2
        xi = xw - s * P.SWITCH_LIP
        prof = [(xw, z0), (xi, z0 + P.SWITCH_LIP), (xi, H), (xw, H)]
        w = cq.Wire.makePolygon([cq.Vector(x, y_open, z) for x, z in prof], close=True)
        lip = cq.Solid.extrudeLinear(cq.Face.makeFromWires(w), cq.Vector(0, y_closed - y_open, 0))
        b = b.fuse(lip)
    return A.solidify(b), dict(centre=(cx, round(yc, 2)), knob_slot=(sl, sw), plate=plate)


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
    add = (tube_w.fuse(fin_w).intersect(body).intersect(halfspace_bf(zb, P.SEAM_Z, above=False)).cut(_box(*speaker_box()))
           .cut(_box((bx0 - 0.5, by0 - 0.5, bz0 - 1), (bx1 + 0.5, by1 + 0.5, bz1 + 0.5))))
    b = b.fuse(add).cut(bore_w)
    ex = bf_point_to_wf((tip[0] + s_x * math.cos(a), tip[1] + s_x * math.sin(a), zc), zb)
    return A.solidify(b), dict(exit=tuple(round(float(v), 2) for v in ex), length=round(s_x - s0, 1))


# ------------------------------------------------------------------ cable clip (separate part) on a rail, left face, low
def _clip_rail_profile():
    """Dovetail rail (X out from the wall face = 0, Z up), 45 deg flanks."""
    zc, wb, pr = P.CABLE_RAIL_Z, P.CABLE_RAIL_W, P.CABLE_RAIL_PROUD
    return [(0.01, zc - wb / 2), (-pr, zc - wb / 2 - pr), (-pr, zc + wb / 2 + pr), (0.01, zc + wb / 2)]


def cable_rail_features(b, zb):
    x0 = footprint()[0]
    y_a, y_b = P.CABLE_RAIL_Y
    prof = _clip_rail_profile()
    w = cq.Wire.makePolygon([cq.Vector(x0 + x, y_a, z) for x, z in prof], close=True)
    rail = cq.Solid.extrudeLinear(cq.Face.makeFromWires(w), cq.Vector(0, y_b - y_a, 0))
    # stop at the back end: the clip slides on from the front, cable pulls push it onto the stop
    zc = P.CABLE_RAIL_Z
    stop = _box((x0 - P.CABLE_RAIL_PROUD, y_b, zc - P.CABLE_RAIL_W / 2 - P.CABLE_RAIL_PROUD),
                (x0 + 0.01, y_b + 2.0, zc + P.CABLE_RAIL_W / 2 + P.CABLE_RAIL_PROUD))
    return A.solidify(b.fuse(rail).fuse(stop))


def cable_clip(zb):
    """Separate part: dovetail slot that slides onto the rail + a C-channel for the cable (along Y),
    opening outward so the cable pops in and out by hand. Profile in XZ, length along Y."""
    x0 = footprint()[0]
    y_a, y_b = P.CABLE_RAIL_Y
    c = P.CABLE_CLIP_FIT
    zc, wb, pr = P.CABLE_RAIL_Z, P.CABLE_RAIL_W, P.CABLE_RAIL_PROUD
    od = P.CABLE_OD
    t = P.CABLE_CLIP_WALL
    x_body0 = -pr - c - t                                   # body outer face (beyond the rail)
    z_lo, z_hi = max(P.CABLE_CLIP_FLOOR_GAP, zc - wb / 2 - pr - c - t), zc + wb / 2 + pr + c + t
    rc = od / 2 + 0.15                                      # channel radius (0.15 slack)
    cxc = x_body0 - rc - 0.01 + 0.0                         # channel centre: just outside the body
    body = _box((x0 + x_body0 - 2 * rc - t, y_a + 0.3, z_lo), (x0 - c, y_b - 0.3, z_hi))
    # dovetail slot = rail profile grown by c
    prof = [(0.5, zc - wb / 2 - c * 0.4), (-pr - c, zc - wb / 2 - pr - c * 0.4 - c), (-pr - c, zc + wb / 2 + pr + c * 0.4 + c), (0.5, zc + wb / 2 + c * 0.4)]
    wv = cq.Wire.makePolygon([cq.Vector(x0 + x, y_a - 1, z) for x, z in prof], close=True)
    slot = cq.Solid.extrudeLinear(cq.Face.makeFromWires(wv), cq.Vector(0, y_b - y_a + 2, 0))
    body = body.cut(slot)
    # cable channel (cylinder along Y) with an outward mouth of CABLE_CLIP_MOUTH x OD
    chan = cq.Solid.makeCylinder(rc, y_b - y_a + 4, pnt=cq.Vector(x0 + cxc, y_a - 2, zc), dir=cq.Vector(0, 1, 0))
    mouth = P.CABLE_CLIP_MOUTH * od
    mo = _box((x0 + cxc - rc - t - 1, y_a - 2, zc - mouth / 2), (x0 + cxc, y_b + 2, zc + mouth / 2))
    ring = cq.Solid.makeCylinder(rc + t, y_b - y_a - 0.6, pnt=cq.Vector(x0 + cxc, y_a + 0.3, zc), dir=cq.Vector(0, 1, 0))
    clip = body.fuse(ring).cut(chan).cut(mo)
    clip = clip.intersect(_box((x0 - 50, y_a - 5, max(0.3, z_lo)), (x0 + 5, y_b + 5, 50)))
    return A.solidify(clip)
