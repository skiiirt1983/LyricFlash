"""C12 carrier coupon (Paul, v10.4): the carrier plate plus a slice of the base with just its two posts, so the
two M2.5 screws can be tried before printing a full base.

The posts are cut out of the real base (same positions, same 20 deg tops, same 2.2 mm pilot holes) and stand on a
3 mm bar at their real relative heights, lowered so the shorter post is 12 mm tall. They're shorter than in the
base (12 and ~26 mm instead of ~40 and ~55), so they drift a little less than the full print will.

Writes out/coupons/C12_carrier_posts.stl, C12_carrier_plate.stl and C12_carrier_coupon.3mf (one plate, PETG).
"""
from __future__ import annotations

import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))

import cadquery as cq  # noqa: E402
import numpy as np  # noqa: E402
import trimesh  # noqa: E402
from shapely.geometry import box as sbox  # noqa: E402

import params as P  # noqa: E402
import phase2 as F  # noqa: E402
from lfcase import enclosure as E  # noqa: E402
from lfcase import v10  # noqa: E402
from lfcase.export import write_3mf_plates, write_stl  # noqa: E402

OUT = HERE / "out" / "coupons"
BAR_T = 3.0
POST_MIN = 12.0


def build():
    zb = E.board_z()
    parts, info = E.build_all(zb, log=lambda m: None)
    base = parts["base"]
    zu = v10._z_under()
    tabs = [E.bf_point_to_wf((x, y, zu), zb) for x, y in P.CARRIER_TABS]
    z_cut = min(t[2] for t in tabs) - POST_MIN
    h = P.POST_HALF + 0.01
    keep = None
    for t in tabs:
        k = E._box((t[0] - h, t[1] - h, z_cut), (t[0] + h, t[1] + h, t[2] + 5))
        keep = k if keep is None else keep.fuse(k)
    posts = base.intersect(keep)
    hull = sbox(tabs[0][0] - h, tabs[0][1] - h, tabs[0][0] + h, tabs[0][1] + h).union(
        sbox(tabs[1][0] - h, tabs[1][1] - h, tabs[1][0] + h, tabs[1][1] + h)).convex_hull
    bar = (cq.Workplane("XY").workplane(offset=z_cut - BAR_T)
           .polyline(list(hull.exterior.coords)[:-1]).close().extrude(BAR_T + 0.5).val())
    posts = posts.fuse(bar).clean()
    posts = posts.moved(cq.Location(cq.Vector(0, 0, -(z_cut - BAR_T))))
    R = F.print_rotations(info)["carrier"]
    plate = F.rotate(parts["carrier"], R)
    bb = plate.BoundingBox()
    plate = plate.moved(cq.Location(cq.Vector(0, 0, -bb.zmin)))
    return posts, plate, z_cut


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    posts, plate, z_cut = build()
    plist = []
    for name, shape in (("C12_carrier_posts", posts), ("C12_carrier_plate", plate)):
        f = OUT / f"{name}.stl"
        write_stl(shape, f)
        m = trimesh.load(f)
        m.apply_translation([0, 0, -m.bounds[0][2]])
        m.export(f)
        print(name, "watertight", m.is_watertight, "bodies", len(m.split(only_watertight=False)),
              "size", [round(float(v), 1) for v in m.extents])
        plist.append(dict(name=name, mesh=m, material="black", plate_name="C12 carrier coupon"))
    write_3mf_plates([plist], OUT / "C12_carrier_coupon.3mf", bambu_app=False)
    print("wrote", OUT / "C12_carrier_coupon.3mf", "(posts cut at world z", round(z_cut, 2), ")")


if __name__ == "__main__":
    main()
