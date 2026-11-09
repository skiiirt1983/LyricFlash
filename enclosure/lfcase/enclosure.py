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
