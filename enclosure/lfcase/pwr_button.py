"""Phase 2 v11 (Paul, 2026-11-15): a power button in the right wall that presses the board's PWR key (KEY2).
The slide switch is gone; the battery stays plugged in and this button turns the board on and off.

It is the round-2 C6 flexure (tested: clear click, responsive; beam 0.9 thick, nub 0.2 from the plunger, pad
recessed 1.5, window 0.5 round the pad) moved into the full case. KEY2 sits ~6 mm inside the right wall, so the
nub is on the blade and the pad reaches out to the wall on a rod, both on the plunger's own axis:

  - blade: a thin vertical sheet (printed on its edge, perpendicular to the PCB), tangent to the board at the
    plunger and anchored in the wall, bends sideways along the layers. Pressing the pad pushes it toward the board.
  - nub: 0.2 mm off the plunger tip at rest (FLEX_REST_GAP), 45 deg underside.
  - pad: grows off the blade's outside face toward the wall, 45 deg underside, through a window in the wall.
    Its end sits FLEX_PAD_RECESS (1.5) below the outer surface, in a shallow finger dimple.

The C6 coupon's blade stood on the floor and was 7.6 tall. Here the plunger is 30 mm above the floor, so the
blade is cut as a trapezoid with a 45 deg underside rising out of the wall: no support, no floating layers. A
taller blade is stiffer, so its thickness is PWR_BLADE_THK (see stiffness()), chosen to land near the coupon's force.
"""
from __future__ import annotations

import math

import cadquery as cq
import numpy as np

import params as P
from lfcase import coupons_a as A
from lfcase import enclosure as E


# ------------------------------------------------------------------ frame
def _frame():
    a = math.radians(P.KEY2_PRESS_ANGLE)
    tip = P.KEY2_TIP_CENTER
    zc = sum(P.KEY2_TIP_Z) / 2
    return a, tip, zc


def _place(shape, zb):
    """Local solid (x = s outward along the press axis from the plunger tip face, y = l along the tangent, z = BF z)
    -> world."""
    a, tip, _ = _frame()
    return E.to_wf(shape.rotate(cq.Vector(0, 0, 0), cq.Vector(0, 0, 1), math.degrees(a))
                   .moved(cq.Location(cq.Vector(tip[0], tip[1], 0))), zb)


def _sz(pts, l0, l1):
    """Polygon in the (s, z) plane, extruded over l0..l1."""
    w = cq.Wire.makePolygon([cq.Vector(s, l0, z) for s, z in pts], close=True)
    return cq.Solid.extrudeLinear(cq.Face.makeFromWires(w), cq.Vector(0, l1 - l0, 0))


def _lz(pts, s0, s1):
    """Polygon in the (l, z) plane, extruded over s0..s1."""
    w = cq.Wire.makePolygon([cq.Vector(s0, l, z) for l, z in pts], close=True)
    return cq.Solid.extrudeLinear(cq.Face.makeFromWires(w), cq.Vector(s1 - s0, 0, 0))


# ------------------------------------------------------------------ dimensions
def dims():
    a, tip, zc = _frame()
    x0, x1, _, _ = E.footprint()
    xw = x1 - P.CASE_WALL                                         # inner face of the right wall
    s_b0 = A.BASE_IN - P.KEY2_TIP_DIST                            # blade inner face (the C6 coupon's r_in)
    t = P.PWR_BLADE_THK
    s_b1 = s_b0 + t
    s_x = (x1 - tip[0]) / math.cos(a)                             # axis meets the outer wall surface
    s_pe = s_x - P.FLEX_PAD_RECESS                                # pad end
    s_p0 = s_b1 - 0.05
    pl = s_pe - s_p0                                              # pad length: its 45 deg underside climbs this much
    hp = P.FLEX_PAD_W / 2
    z_pad_bot = zc - 1.0 - pl                                     # pad underside at the blade face
    z_top = P.FLEX_BEAM_TOP_Z
    mz = P.PWR_BLADE_SLOPE
    z_bot_edge = z_pad_bot - 0.1                                  # blade underside at the pad's free edge (l = +hp)
    l_c = (tip[0] + (s_b0 + t / 2) * math.cos(a) - xw) / math.sin(a)   # where the blade's mid-plane meets the wall face
    l_root = l_c - P.PWR_BLADE_EMBED
    l_free = hp + 0.0                                             # blade ends flush with the pad's free edge
    z_root = z_bot_edge + (l_root - hp) * mz
    return dict(a=a, tip=tip, zc=zc, xw=xw, x1=x1, s_b0=s_b0, s_b1=s_b1, t=t, s_x=s_x, s_pe=s_pe, s_p0=s_p0, pl=pl,
                hp=hp, z_pad_bot=z_pad_bot, z_top=z_top, mz=mz, z_bot_edge=z_bot_edge, l_c=l_c, l_root=l_root,
                l_free=l_free, z_root=z_root)


def stiffness(d=None, E_mpa=1500.0, travel=None):
    """Euler-Bernoulli estimate of the blade (tapered height, clamped at the wall) at the nub, N/mm and N at the
    needed deflection (rest gap + switch travel). Compared with the coupon's uniform 7.6 mm blade."""
    d = d or dims()
    a_len = -d["l_c"]                                             # clamp (wall face) to the load point (l = 0)
    n = 4001
    x = np.linspace(0.0, a_len, n)                                # from the clamp
    l = d["l_c"] + x
    h = d["z_top"] - (d["z_bot_edge"] + (l - d["hp"]) * d["mz"])
    k = 1.0 / np.trapezoid((a_len - x) ** 2 / (E_mpa * h * d["t"] ** 3 / 12.0), x)
    defl = P.FLEX_REST_GAP + P.KEY_TRAVEL if travel is None else travel
    hc = d["z_top"] - A.FLOOR_BOT                                  # coupon blade height (floor bottom to top)
    a_c = P.FLEXURE_LEN - 1.5
    kc = 3 * E_mpa * (hc * P.FLEXURE_THK ** 3 / 12.0) / a_c ** 3
    return dict(k=k, force=k * defl, h_at_load=float(h[-1]), a=a_len, k_coupon=kc, force_coupon=kc * defl)


# ------------------------------------------------------------------ solids
def parts_bf(d=None):
    """(window cut, flexure to fuse) in the local frame, before placing."""
    d = d or dims()
    zc, hp = d["zc"], d["hp"]
    nz0, nz1 = P.PWR_NUB_Z
    blade = _lz([(d["l_root"], d["z_top"]), (d["l_free"], d["z_top"]),
                 (d["l_free"], d["z_bot_edge"]), (d["l_root"], d["z_root"])], d["s_b0"], d["s_b1"])
    g = P.PWR_REST_GAP
    s_n = d["s_b0"] + 0.2                                          # nub reaches 0.2 into the blade
    nub = _sz([(g, nz0), (s_n, nz0 - (s_n - g)), (s_n, nz1), (g, nz1)], -P.FLEX_NUB_W / 2, P.FLEX_NUB_W / 2)
    pad = _sz([(d["s_p0"], d["z_pad_bot"]), (d["s_pe"], zc - 1.0), (d["s_pe"], zc + 1.0), (d["s_p0"], zc + 1.0)], -hp, hp)
    flex = blade.fuse(nub).fuse(pad)
    # window through the wall round the pad: FLEX_WINDOW_CLEAR each side; 45 deg floor up to the pad end, then flat
    c = P.FLEX_WINDOW_CLEAR
    s0 = d["s_b1"] + P.FLEX_GAP
    zf = zc - 1.0 - c * math.sqrt(2)                               # floor 0.5 (normal) under the pad underside
    s_end = d["s_x"] + 4.0
    zt = zc + 1.0 + c + 3.0                                        # trimmed to a world-horizontal ceiling in pwr_button()
    pts = [(s0, zf - (d["s_pe"] - s0)), (d["s_pe"], zf), (s_end, zf), (s_end, zt), (s0, zt)]
    window = _sz(pts, -(hp + c), hp + c)
    return window, flex


def exit_point(zb, d=None):
    """World point where the plunger axis leaves the outer wall (pad height)."""
    d = d or dims()
    a, tip, zc = d["a"], d["tip"], d["zc"]
    return E.bf_point_to_wf((tip[0] + d["s_x"] * math.cos(a), tip[1] + d["s_x"] * math.sin(a), zc), zb)


def pwr_button(b, zb):
    """Cut the window and the finger dimple, then fuse the blade, nub and pad onto the base."""
    d = dims()
    window, flex = parts_bf(d)
    # ceiling horizontal in the world (the board's tilt would make it a shallow sloping overhang): level with the
    # pad's top at its outer end + the window clearance, which is the highest point of the pad
    s_end = d["s_x"]
    a_, tip_, zc_ = d["a"], d["tip"], d["zc"]
    zt_w = E.bf_point_to_wf((tip_[0] + s_end * math.cos(a_), tip_[1] + s_end * math.sin(a_), zc_ + 1.0 + P.FLEX_WINDOW_CLEAR), zb)[2]
    win_w = _place(window, zb).intersect(E._box((-200, -200, -50), (200, 200, zt_w)))
    b = b.cut(win_w)
    body = E.base_body(zb)
    flex_w = _place(flex, zb)
    # trim the root (it was run into the wall) to the case outline
    flex_w = flex_w.intersect(body.fuse(E._inner_cavity()))
    b = b.fuse(flex_w)
    ex = exit_point(zb, d)
    n = np.array([1.0, 0.0, 0.0])
    dep = min(P.PWR_DIMPLE_DEPTH, P.CASE_WALL - P.PWR_DIMPLE_MIN_WALL)
    cone = cq.Solid.makeCone(P.PWR_DIMPLE_D / 2 - dep, P.PWR_DIMPLE_D / 2 + 2, dep + 2,
                             pnt=cq.Vector(*(ex - n * dep)), dir=cq.Vector(*n))
    b = b.cut(cone)
    info = dict(exit=tuple(round(float(v), 2) for v in ex), stiffness=stiffness(d),
                blade=dict(h_at_load=round(stiffness(d)["h_at_load"], 2), t=d["t"], l_c=round(d["l_c"], 2)),
                pad=dict(len=round(d["pl"], 2), end_s=round(d["s_pe"], 2)))
    return A.solidify(b), info


def keepout(zb):
    """Outside patch of the right wall kept free of ribs / panels round the window and dimple; 45 deg roof outward so
    the texture above it starts on a printable slope."""
    d = dims()
    ex = exit_point(zb, d)
    half = P.PWR_PATCH_HALF
    x1 = d["x1"]
    zlo, top = ex[2] - P.PWR_PATCH_BELOW, ex[2] + P.PWR_PATCH_ABOVE
    prof = [(x1 - 0.3, zlo), (x1 - 0.3, top), (x1 + 6.0, top + 6.3), (x1 + 6.0, zlo)]
    w = cq.Wire.makePolygon([cq.Vector(x, ex[1] - half, z) for x, z in prof], close=True)
    return cq.Solid.extrudeLinear(cq.Face.makeFromWires(w), cq.Vector(0, 2 * half, 0))
