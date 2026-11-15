"""C13: pilot-hole gauge. One bar, five tilted holes (the same 20 deg lean and the same funnel as the base posts), pilot
diameters 2.2 / 2.3 / 2.4 / 2.5 / 2.6, with 1..5 notches on the edge to tell them apart. Print it in the base's PETG, screw
an M2.5 x 6 into each hole by hand and report the smallest one the screw starts in and drives to the head without a
struggle. Writes out/coupons/C13_pilot_gauge.stl and .3mf (one plate, PETG)."""
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
DIAMS = (2.2, 2.3, 2.4, 2.5, 2.6)
PITCH, W, H = 10.0, 10.0, 11.0


def build():
    L = PITCH * len(DIAMS) + 4
    bar = cq.Workplane("XY").box(L, W, H, centered=(False, True, False)).val()
    tilt = P.TILT_DEG
    lead = P.POST_PILOT_LEAD
    for i, d in enumerate(DIAMS):
        x = 7.0 + i * PITCH
        # hole axis leans like the posts' pilots: along the board normal, TILT_DEG from vertical (about X)
        ax = cq.Vector(0, -math.sin(math.radians(tilt)), math.cos(math.radians(tilt)))
        top = cq.Vector(x, 0, H - 0.01)
        hole = cq.Solid.makeCylinder(d / 2, P.POST_HOLE_DEPTH + 2, top - ax * P.POST_HOLE_DEPTH, ax)
        cone = cq.Solid.makeCone(d / 2, d / 2 + lead + 0.4, lead + 0.4, top - ax * lead, ax)
        bar = bar.cut(hole.fuse(cone))
        for k in range(i + 1):                                   # i+1 notches on the front edge
            bar = bar.cut(cq.Solid.makeBox(1.0, 1.2, 1.4, pnt=cq.Vector(x - 3.0 + k * 2.0 - 0.5, -W / 2 - 0.01, 2.0)))
    return bar


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    f = OUT / "C13_pilot_gauge.stl"
    write_stl(build(), f)
    m = trimesh.load(f)
    m.apply_translation([0, 0, -m.bounds[0][2]])
    m.export(f)
    print("C13 watertight", m.is_watertight, "size", [round(float(v), 1) for v in m.extents])
    write_3mf_plates([[dict(name="C13_pilot_gauge", mesh=m, material="black", plate_name="C13 pilot gauge")]],
                     OUT / "C13_pilot_gauge.3mf", bambu_app=False)
    print("wrote", OUT / "C13_pilot_gauge.3mf", "pilots", DIAMS)


if __name__ == "__main__":
    main()
