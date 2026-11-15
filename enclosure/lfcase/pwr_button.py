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

T = math.radians(P.TILT_DEG)


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
    hp = P.PWR_FACE_W / 2
    x_face = x1 + P.PWR_FACE_PROUD                                # button face: a plane parallel to the wall
    sa, ca = math.sin(a), math.cos(a)
    s_face = lambda l: (x_face - tip[0] + l * sa) / ca            # where the face plane meets the axis line at offset l
    s_fm = s_face(hp)                                             # farthest point of the face (the +l edge)
    z_top = zc + 1.0                                              # neck top inside the case: 0.4 under the shell rim (BF z 0.5)
    z_fb = P.PWR_FACE_BOT                                         # face bottom (at the +l edge); the head's top is set in the world
    z_bt = P.FLEX_BEAM_TOP_Z
    mz = P.PWR_BLADE_SLOPE
    t = pwr_blade_thk(s_b0=s_b0, hp=hp, s_fm=s_fm, z_fb=z_fb, xw=xw, tip=tip, a=a, z_bt=z_bt, mz=mz)
    s_b1 = s_b0 + t
    s_p0 = s_b1 - 0.05
    z_ramp0 = z_fb - (s_fm - s_p0)                                # pad underside (45 deg) at the blade face
    z_bot_edge = z_ramp0 - 0.1                                    # blade underside at l = +hp
    l_c = (tip[0] + (s_b0 + t / 2) * ca - xw) / sa                # where the blade's mid-plane meets the wall face
    l_root = l_c - P.PWR_BLADE_EMBED
    z_root = z_bot_edge + (l_root - hp) * mz
    return dict(a=a, tip=tip, zc=zc, xw=xw, x1=x1, x_face=x_face, s_b0=s_b0, s_b1=s_b1, t=t, s_fm=s_fm, s_p0=s_p0,
                hp=hp, z_top=z_top, z_fb=z_fb, z_ramp0=z_ramp0, z_bt=z_bt, mz=mz, z_bot_edge=z_bot_edge, l_c=l_c,
                l_root=l_root, l_free=hp, z_root=z_root, s_face=s_face)


def _k(t, a_len, h_of_x, E_mpa=1500.0, n=4001):
    x = np.linspace(0.0, a_len, n)
    h = h_of_x(x)
    return 1.0 / np.trapezoid((a_len - x) ** 2 / (E_mpa * h * t ** 3 / 12.0), x)


def pwr_blade_thk(s_b0, hp, s_fm, z_fb, xw, tip, a, z_bt, mz):
    """PWR_BLADE_THK, or (when None) the thickness that gives the beam force PWR_TARGET_FORCE for this geometry: the
    taller the blade the stiffer it is, and stiffness goes with t^3."""
    if getattr(P, "PWR_BLADE_THK", None):
        return P.PWR_BLADE_THK
    t0 = 0.7
    s_p0 = s_b0 + t0 - 0.05
    z_bot_edge = z_fb - (s_fm - s_p0) - 0.1
    l_c = (tip[0] + (s_b0 + t0 / 2) * math.cos(a) - xw) / math.sin(a)
    a_len = -l_c
    h = lambda x: z_bt - (z_bot_edge + ((l_c + x) - hp) * mz)
    f0 = _k(t0, a_len, h) * (P.FLEX_REST_GAP + P.KEY_TRAVEL)
    return round(t0 * (P.PWR_TARGET_FORCE / f0) ** (1.0 / 3.0), 2)


def stiffness(d=None, E_mpa=1500.0, travel=None):
    """Euler-Bernoulli estimate of the blade (tapered height, clamped at the wall) at the nub, N/mm and N at the
    needed deflection (rest gap + switch travel). Compared with the coupon's uniform 7.6 mm blade."""
    d = d or dims()
    a_len = -d["l_c"]                                             # clamp (wall face) to the load point (l = 0)
    n = 4001
    x = np.linspace(0.0, a_len, n)                                # from the clamp
    l = d["l_c"] + x
    h = d["z_bt"] - (d["z_bot_edge"] + (l - d["hp"]) * d["mz"])
    k = 1.0 / np.trapezoid((a_len - x) ** 2 / (E_mpa * h * d["t"] ** 3 / 12.0), x)
    defl = P.FLEX_REST_GAP + P.KEY_TRAVEL if travel is None else travel
    hc = d["z_bt"] - A.FLOOR_BOT                                   # coupon blade height (floor bottom to top)
    a_c = P.FLEXURE_LEN - 1.5
    kc = 3 * E_mpa * (hc * P.FLEXURE_THK ** 3 / 12.0) / a_c ** 3
    return dict(k=k, force=k * defl, h_at_load=float(h[-1]), a=a_len, k_coupon=kc, force_coupon=kc * defl)


# ------------------------------------------------------------------ solids
Z_HI = 3.4          # bar prism top (BF), trimmed to the head's world-horizontal top and the neck's BF top


def parts_bf(d=None):
    """(window cut, blade + nub, bar prism) in the local frame, before placing; trimmed in pwr_button()."""
    d = d or dims()
    zc, hp = d["zc"], d["hp"]
    nz0, nz1 = P.PWR_NUB_Z
    blade = _lz([(d["l_root"], d["z_bt"]), (d["l_free"], d["z_bt"]),
                 (d["l_free"], d["z_bot_edge"]), (d["l_root"], d["z_root"])], d["s_b0"], d["s_b1"])
    g = P.PWR_REST_GAP
    s_n = d["s_b0"] + 0.2                                          # nub reaches 0.2 into the blade
    nub = _sz([(g, nz0), (s_n, nz0 - (s_n - g)), (s_n, nz1), (g, nz1)], -P.FLEX_NUB_W / 2, P.FLEX_NUB_W / 2)
    # bar: 45 deg underside from the blade face up to the face's bottom; runs on past the face plane, trimmed later
    s_ext = d["s_fm"] + 1.5
    pad = _sz([(d["s_p0"], d["z_ramp0"]), (s_ext, d["z_ramp0"] + (s_ext - d["s_p0"])), (s_ext, Z_HI), (d["s_p0"], Z_HI)], -hp, hp)
    # tunnel through the wall round the bar: FLEX_WINDOW_CLEAR each side; its floor follows the underside, 0.5 below
    c = P.FLEX_WINDOW_CLEAR
    s0 = d["s_b1"] + P.FLEX_GAP
    off = c * math.sqrt(2)
    s_end = s_ext + 3.0
    fl = lambda s_: d["z_ramp0"] + (s_ - d["s_p0"]) - off
    window = _sz([(s0, fl(s0)), (s_end, fl(s_end)), (s_end, Z_HI + c + 2.0), (s0, Z_HI + c + 2.0)], -(hp + c), hp + c)
    return window, blade.fuse(nub), pad


_G = {}


def _seam_z(y_w, zb):
    """World height of the seam plane (BF z = SEAM_Z) above world y."""
    S = P.SEAM_Z
    return zb + (y_w + S * math.sin(T)) * math.tan(T) + S * math.cos(T)


def geometry(zb):
    """Head top (world), the bar's head and the scoop's extent, from the case's actual size. Cached."""
    if zb in _G:
        return _G[zb]
    d = dims()
    a, tip = d["a"], d["tip"]
    c = P.FLEX_WINDOW_CLEAR
    ys = []
    for l in np.linspace(-(d["hp"] + c), d["hp"] + c, 15):
        s_ = d["s_face"](l)
        p = E.bf_point_to_wf((tip[0] + s_ * math.cos(a) - l * math.sin(a), tip[1] + s_ * math.sin(a) + l * math.cos(a), 0.0), zb)
        ys.append(p[1])
    z_ht = _seam_z(min(ys), zb) - P.PWR_FACE_TOP_GAP            # head top: this far under the seam at its lowest end
    window, flex, pad = parts_bf(d)
    big = (-200, 200)
    pad_w = _place(pad, zb).intersect(E._box((-200, -200, -50), (d["x_face"], 200, z_ht)))
    neck = pad_w.intersect(E.halfspace_bf(zb, d["z_top"], above=False))
    head = pad_w.intersect(E._box((d["xw"] - 0.02, -200, -50), (300, 200, 300)))
    bar = neck.fuse(head)
    hv = head.intersect(E._box((d["x1"] - 0.5, -200, -50), (300, 200, 300))).BoundingBox()
    g = dict(z_ht=z_ht, bar=bar, y0=hv.ymin, y1=hv.ymax, z0=hv.zmin, z1=hv.zmax)
    dep = min(P.PWR_SCOOP_DEPTH, P.CASE_WALL - P.PWR_DIMPLE_MIN_WALL)
    g["dep"] = dep
    g["sy"] = (g["y1"] - g["y0"]) + 2 * P.PWR_SCOOP_MW           # scoop floor: width (Y) ...
    g["sz0"] = g["z0"] - P.PWR_SCOOP_MH_B
    g["sz1"] = g["z1"] + P.PWR_SCOOP_MH_T
    g["yc"] = (g["y0"] + g["y1"]) / 2
    _G[zb] = g
    return g


def face_centre(zb, d=None):
    """World point at the middle of the button face (on the outer surface)."""
    g = geometry(zb)
    return np.array([(d or dims())["x1"], g["yc"], (g["z0"] + g["z1"]) / 2])


def exit_point(zb, d=None):
    return face_centre(zb, d)


def _scoop(zb):
    """Finger scoop: a vertical stadium dish, floor `dep` under the outer surface, 45 deg walls out to the surface."""
    d, g = dims(), geometry(zb)
    dep = g["dep"]
    x_floor = d["x1"] - dep
    L, Wd = g["sz1"] - g["sz0"], g["sy"]
    zc_s = (g["sz0"] + g["sz1"]) / 2
    ext = dep + 1.5
    from shapely.geometry import LineString
    half = max(L - Wd, 0.0) / 2
    seg = LineString([(g["yc"], zc_s - half), (g["yc"], zc_s + half)])
    p0 = list(seg.buffer(Wd / 2, resolution=16).exterior.coords)[:-1]
    p1 = list(seg.buffer(Wd / 2 + ext, resolution=16).exterior.coords)[:-1]
    assert len(p0) == len(p1)
    sc = (cq.Workplane("YZ", origin=(x_floor, 0, 0)).polyline(p0).close()
          .workplane(offset=ext).polyline(p1).close().loft(ruled=True).val())
    if sc.Volume() < 0:
        sc = cq.Solid(sc.wrapped.Reversed())
    return sc


def pwr_button(b, zb):
    """Cut the tunnel and the finger scoop, then fuse the blade, nub and bar onto the base."""
    d = dims()
    g = geometry(zb)
    window, flex, pad = parts_bf(d)
    c = P.FLEX_WINDOW_CLEAR
    # world-horizontal tunnel ceiling (a ceiling tilted with the board is a shallow overhang in the layer check)
    win_w = _place(window, zb).intersect(E._box((-200, -200, -50), (200, 200, g["z_ht"] + c)))
    b = b.cut(win_w)
    b = b.cut(_scoop(zb))
    body = E.base_body(zb)
    flex_w = _place(flex, zb).intersect(body.fuse(E._inner_cavity()))     # the blade's root runs into the wall: trim to the case
    b = b.fuse(flex_w).fuse(g["bar"])
    st = stiffness(d)
    info = dict(exit=tuple(round(float(v), 2) for v in face_centre(zb, d)), stiffness=st,
                blade=dict(h_at_load=round(st["h_at_load"], 2), t=d["t"], l_c=round(d["l_c"], 2)),
                pad=dict(face=(round(g["y1"] - g["y0"], 1), round(g["z1"] - g["z0"], 1)), proud=P.PWR_FACE_PROUD,
                         scoop=(round(g["sy"], 1), round(g["sz1"] - g["sz0"], 1))))
    return A.solidify(b), info


def keepout(zb):
    """Outside patch of the right wall kept free of ribs / panels round the scoop; 45 deg roof outward so the texture
    above it starts on a printable slope."""
    d, g = dims(), geometry(zb)
    half = g["sy"] / 2 + g["dep"] + 1.5 + P.PWR_PATCH_PAD
    x1 = d["x1"]
    zlo, top = g["sz0"] - g["dep"] - 1.5 - P.PWR_PATCH_PAD, g["sz1"] + g["dep"] + 1.5
    prof = [(x1 - 0.3, zlo), (x1 - 0.3, top), (x1 + 6.0, top + 6.3), (x1 + 6.0, zlo)]
    w = cq.Wire.makePolygon([cq.Vector(x, g["yc"] - half, z) for x, z in prof], close=True)
    return cq.Solid.extrudeLinear(cq.Face.makeFromWires(w), cq.Vector(0, 2 * half, 0))
