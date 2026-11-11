"""Fit Paul's floor logo into the open floor of a base STL: as big as possible, centred in the space it's in.

Free floor = the cavity's cross-sections just above the floor (1.8 to 6 mm), intersected, shrunk by MARGIN.
Searches positions on a grid for the largest scale, then takes the best-scale position nearest the free
area's centre. Writes the scaled, moved logo (depth unchanged) to the asset path given.
Usage: python concepts/fit_logo.py <base.stl> <logo_src.stl> <out.stl>
"""
import sys

import numpy as np
import shapely.geometry as sg
import trimesh
from shapely import affinity
from shapely.ops import unary_union

MARGIN = 1.5


def free_floor(base):
    """Inside of the walls minus any material standing on the floor (1.8 to 6 mm up)."""
    sys.path.insert(0, str(__import__("pathlib").Path(__file__).resolve().parent.parent))
    from lfcase import v10
    free = v10._cavity_poly(-0.01)
    for z in (1.8, 2.5, 4.0, 6.0):
        sec = base.section(plane_origin=[0, 0, z], plane_normal=[0, 0, 1])
        p, to3 = sec.to_2D()
        mats = []
        for pg in p.polygons_full:
            def w(ring):
                c = np.asarray(ring.coords)
                return (to3 @ np.c_[c, np.zeros(len(c)), np.ones(len(c))].T).T[:, :2]
            mats.append(sg.Polygon(w(pg.exterior), [w(h) for h in pg.interiors]))
        free = free.difference(unary_union(mats))
    return free


def main(base_path, logo_path, out_path):
    base = trimesh.load(base_path)
    free = free_floor(base)
    # the logo goes in the biggest open piece
    if hasattr(free, "geoms"):
        free = max(free.geoms, key=lambda g: g.area)
    inner = free.buffer(-MARGIN)
    lg = trimesh.load(logo_path)
    tri = lg.vertices[lg.faces][:, :, :2]
    outline = unary_union([sg.Polygon(t).buffer(0.01) for t in tri if sg.Polygon(t).area > 1e-6])
    lc = np.array(outline.envelope.centroid.coords[0])

    def best_scale(c):
        lo, hi = 0.1, 4.0
        for _ in range(25):
            s = (lo + hi) / 2
            o = affinity.translate(affinity.scale(outline, s, s, origin=tuple(lc)), *(c - lc))
            lo, hi = (s, hi) if inner.contains(o) else (lo, s)
        return lo
    x0, y0, x1, y1 = inner.bounds
    cands = [np.array([x, y]) for x in np.arange(x0, x1, 0.5) for y in np.arange(y0, y1, 0.5)
             if inner.contains(sg.Point(x, y))]
    scored = [(best_scale(c), c) for c in cands]
    smax = max(s for s, _ in scored)
    cen = np.array(free.centroid.coords[0])
    s, c = min(((s, c) for s, c in scored if s >= smax - 0.01), key=lambda sc: np.linalg.norm(sc[1] - cen))
    v = lg.vertices.copy()
    v[:, 0] = (v[:, 0] - lc[0]) * s + c[0]
    v[:, 1] = (v[:, 1] - lc[1]) * s + c[1]
    trimesh.Trimesh(v, lg.faces, process=False).export(out_path)
    o = affinity.translate(affinity.scale(outline, s, s, origin=tuple(lc)), *(c - lc))
    print(f"free floor {np.round(free.bounds, 1)}; logo scale {s:.3f} (of Paul's original), centre {np.round(c, 2)}, "
          f"size {o.bounds[2] - o.bounds[0]:.1f} x {o.bounds[3] - o.bounds[1]:.1f} mm")
    return free, o


if __name__ == "__main__":
    main(*sys.argv[1:4])
