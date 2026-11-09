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
    blk_w = blk_w.cut(_box(*battery_box()))                       # stays above the battery (support under it)
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
def _pin_frame():
    key = P.PWR_IS
    a = math.radians(getattr(P, f"{key}_PRESS_ANGLE"))
    tip = getattr(P, f"{key}_TIP_CENTER")
    tz = getattr(P, f"{key}_TIP_Z")
    return a, tip, (tz[0] + tz[1]) / 2


def _local_box(s0, s1, l0, l1, z0, z1):
    """Box in the pin frame (s outward along the press axis from the plunger tip face, l across, BF z)."""
    a, tip, _ = _pin_frame()
    b = cq.Solid.makeBox(s1 - s0, l1 - l0, z1 - z0, pnt=cq.Vector(s0, l0, z0))
    return b.rotate(cq.Vector(0, 0, 0), cq.Vector(0, 0, 1), math.degrees(a)).moved(cq.Location(cq.Vector(tip[0], tip[1], 0)))


def _pin_dims():
    nw, nh, nl = P.PWR_NUB
    _, _, zc = _pin_frame()
    l0 = -nw / 2                                       # face A (print bed side) flush with the nub
    l1 = l0 + P.PWR_PIN_W
    z1 = P.PWR_PIN_TOP
    z0 = z1 - P.PWR_PIN_H
    g0 = P.FLEX_REST_GAP
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
