"""C11 magnet coupon (Paul, 2026-11-11): test the 4 x 2 magnet pocket fit before reprinting the case.

Two small bars, each printed the way its pockets print in the real parts:
  * PETG bar = the base's corner pockets: they follow the seam, so in print orientation they lean 20 deg.
    The bar's top is sloped 20 deg and the pockets are square to it.
  * PLA bar = the shell's corner pockets: the shell prints face-down, so its pockets print straight up.
Each bar has three pockets, from the cut-off corner: 1 = tight (dia 4.00), 2 = v10.1 (dia 4.10), 3 = loose (dia 4.20).
(The cut-off corner marks the pocket-1 end.)
All are MAGNET_H + MAGNET_DEPTH_EXTRA deep (2.1), as in the case.

Writes out/coupons/C11_magnet_petg.stl, C11_magnet_pla.stl and C11_magnet_coupon.3mf (one plate, filaments set).
"""
from __future__ import annotations

import math
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))

import cadquery as cq  # noqa: E402
import trimesh  # noqa: E402

import params as P  # noqa: E402
from lfcase.export import write_3mf_plates, write_stl  # noqa: E402

OUT = HERE / "out" / "coupons"
FITS = (0.0, 0.05, 0.10)                 # radius over the 4.0 magnet: tight / v10.1 / loose
PITCH = 11.0
L, Wd = 36.0, 10.0
DEPTH = P.MAGNET_H + P.MAGNET_DEPTH_EXTRA
TILT = P.TILT_DEG


def _marker(bar, z_top):
    """Corner cut off (5 mm legs, full height) at the X-, Y- corner: pocket 1 is next to it."""
    w = cq.Wire.makePolygon([cq.Vector(-L / 2 - 0.01, -Wd / 2 - 0.01, -1), cq.Vector(-L / 2 + 5, -Wd / 2 - 0.01, -1),
                             cq.Vector(-L / 2 - 0.01, -Wd / 2 + 5, -1)], close=True)
    return bar.cut(cq.Solid.extrudeLinear(cq.Face.makeFromWires(w), cq.Vector(0, 0, 30)))


def petg_bar():
    """Sloped top (20 deg about X, rising toward +Y), pockets square to it."""
    h0 = 4.5
    rise = Wd * math.tan(math.radians(TILT))
    prof = [(-Wd / 2, 0), (Wd / 2, 0), (Wd / 2, h0 + rise), (-Wd / 2, h0)]
    w = cq.Wire.makePolygon([cq.Vector(-L / 2, y, z) for y, z in prof], close=True)
    bar = cq.Solid.extrudeLinear(cq.Face.makeFromWires(w), cq.Vector(L, 0, 0))
    zc = h0 + rise / 2                                     # top surface height at y = 0
    for i, f in enumerate(FITS):
        x = (i - 1) * PITCH
        pocket = cq.Solid.makeCylinder(P.MAGNET_D / 2 + f, DEPTH + 5, cq.Vector(x, 0, -DEPTH))
        pocket = pocket.rotate(cq.Vector(0, 0, 0), cq.Vector(1, 0, 0), TILT).translate(cq.Vector(0, 0, zc))
        bar = bar.cut(pocket)
    return _marker(bar, h0)


def pla_bar():
    h = 4.5
    bar = cq.Solid.makeBox(L, Wd, h, cq.Vector(-L / 2, -Wd / 2, 0))
    for i, f in enumerate(FITS):
        x = (i - 1) * PITCH
        bar = bar.cut(cq.Solid.makeCylinder(P.MAGNET_D / 2 + f, DEPTH + 1, cq.Vector(x, 0, h - DEPTH)))
    return _marker(bar, h)


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    parts = {"C11_magnet_petg": (petg_bar(), "black"), "C11_magnet_pla": (pla_bar(), "gold")}
    plate = []
    for name, (shape, mat) in parts.items():
        f = OUT / f"{name}.stl"
        write_stl(shape, f)
        m = trimesh.load(f)
        m.apply_translation([0, 0, -m.bounds[0][2]])
        m.export(f)
        print(name, "watertight", m.is_watertight, "size", [round(v, 1) for v in m.extents])
        plate.append(dict(name=name, mesh=m, material=mat, plate_name="C11 magnet coupon"))
    write_3mf_plates([plate], OUT / "C11_magnet_coupon.3mf", bambu_app=False)
    print("wrote", OUT / "C11_magnet_coupon.3mf")


if __name__ == "__main__":
    main()
