"""Fit Paul's floor logo into the open floor of a base STL: as big as possible, centred in the space it's in.

Free floor = the cavity's cross-sections just above the floor (1.8 to 6 mm), intersected, shrunk by MARGIN.
Searches positions on a grid for the largest scale (each rotation given), then takes the best-scale position
nearest the free area's centre. Thin strokes are offset outward to --min-stroke, and the cut is rebuilt from the
2D outline at --depth below the floor top (0.8 if the floor there would keep less than 0.6).
Usage: python concepts/fit_logo.py <base.stl> <logo_src.stl> <out.stl> [--rotations 0 90] [--depth 1.0]
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


def outline_2d(mesh):
    """Footprint of a logo cut mesh: union of its projected triangles (holes kept)."""
    tri = mesh.vertices[mesh.faces][:, :, :2]
    return unary_union([sg.Polygon(t).buffer(0.005) for t in tri if sg.Polygon(t).area > 1e-6])


def ring_width(poly):
    """Width of the thinnest ring stroke (the rounded square's outline): exterior to its own hole."""
    widths = [sg.Polygon(h).exterior.distance(g.exterior) for g in getattr(poly, "geoms", [poly]) for h in g.interiors]
    return min(widths)


def thin_loss(poly, w):
    """Area of the outline narrower than w (opening by w/2 removes it), ignoring rounded-corner trims < 0.05 mm2."""
    o = poly.buffer(-w / 2 + 1e-3, join_style=1).buffer(w / 2 - 1e-3, join_style=1)
    lost = poly.difference(o)
    return sum(g.area for g in getattr(lost, "geoms", [lost]) if g.area > 0.05)


def main(base_path, logo_path, out_path, rotations=(0.0, 90.0), depth=1.0, min_stroke=0.8, floor_top=1.6,
         min_floor=0.6, top_extra=0.05):
    base = trimesh.load(base_path)
    free = free_floor(base)
    if hasattr(free, "geoms"):
        free = max(free.geoms, key=lambda g: g.area)
    inner = free.buffer(-MARGIN)
    out0 = outline_2d(trimesh.load(logo_path))
    w0 = ring_width(out0)
    lc = np.array(out0.envelope.centroid.coords[0])
    x0, y0, x1, y1 = inner.bounds
    cands = [np.array([x, y]) for x in np.arange(x0, x1, 0.5) for y in np.arange(y0, y1, 0.5)
             if inner.contains(sg.Point(x, y))]
    cen = np.array(free.centroid.coords[0])

    def shape(s, rot, c):
        d = max(0.0, (min_stroke - w0 * s) / 2)                  # outward offset so the thinnest stroke >= min
        o = affinity.rotate(out0, rot, origin=tuple(lc))
        o = affinity.translate(affinity.scale(o, s, s, origin=tuple(lc)), *(c - lc))
        return (o.buffer(d, join_style=1) if d > 0 else o), d

    best = None
    for rot in rotations:
        def best_scale(c):
            lo, hi = 0.1, 4.0
            for _ in range(25):
                s = (lo + hi) / 2
                lo, hi = (s, hi) if inner.contains(shape(s, rot, c)[0]) else (lo, s)
            return lo
        scored = [(best_scale(c), c) for c in cands]
        smax = max(s for s, _ in scored)
        s, c = min(((s, c) for s, c in scored if s >= smax - 0.01), key=lambda sc: np.linalg.norm(sc[1] - cen))
        o, d = shape(s, rot, c)
        bx = o.bounds
        print(f"  rotation {rot:g} deg: scale {s:.3f}, {bx[2] - bx[0]:.1f} x {bx[3] - bx[1]:.1f} mm, stroke "
              f"{w0 * s:.2f} + {2 * d:.2f} = {w0 * s + 2 * d:.2f} mm")
        if best is None or o.area > best[0].area:
            best = (o, s, rot, c, d)
    o, s, rot, c, d = best
    # floor under the logo: rays up from below, bottom face to floor top
    pts = [p for p in (sg.Point(x, y) for x in np.arange(o.bounds[0], o.bounds[2], 0.7)
                       for y in np.arange(o.bounds[1], o.bounds[3], 0.7)) if o.contains(p)]
    org = np.array([[p.x, p.y, -1.0] for p in pts])
    loc, ri, _ = base.ray.intersects_location(org, np.tile([0, 0, 1.0], (len(org), 1)), multiple_hits=True)
    thick = []
    for i in range(len(org)):
        z = np.sort(loc[ri == i][:, 2])
        if len(z) >= 2:
            thick.append(z[1] - z[0])
    tmin = float(min(thick)) if thick else floor_top
    if tmin - depth < min_floor - 1e-6:
        depth = 0.8
        print(f"  floor under the logo only {tmin:.2f} mm: depth set to 0.8")
    polys = list(getattr(o, "geoms", [o]))
    meshes = [trimesh.creation.extrude_polygon(p, depth + top_extra) for p in polys]
    m = trimesh.util.concatenate(meshes)
    m.apply_translation([0, 0, floor_top - depth])
    m.export(out_path)
    bx = o.bounds
    loss = thin_loss(o, min_stroke - 0.02)
    print(f"  parts narrower than {min_stroke - 0.02:.2f} mm anywhere in the logo: {loss:.2f} mm2")
    res = dict(scale=s, rotation=rot, centre=c, size=(bx[2] - bx[0], bx[3] - bx[1]), depth=depth,
               stroke_scaled=w0 * s, offset=d, stroke=w0 * s + 2 * d, floor_min=tmin, watertight=m.is_watertight)
    print(f"logo: rotation {rot:g} deg, scale {s:.3f}, centre {np.round(c, 2)}, size {res['size'][0]:.1f} x "
          f"{res['size'][1]:.1f} mm, depth {depth} mm (floor {tmin:.2f} mm, {tmin - depth:.2f} left), thinnest stroke "
          f"{w0 * s:.2f} mm scaled, +{2 * d:.2f} thickening = {res['stroke']:.2f} mm, watertight {m.is_watertight}")
    return res


if __name__ == "__main__":
    import argparse
    ap = argparse.ArgumentParser()
    ap.add_argument("base"); ap.add_argument("src"); ap.add_argument("out")
    ap.add_argument("--rotations", type=float, nargs="+", default=[0.0, 90.0])
    ap.add_argument("--depth", type=float, default=1.0)
    ap.add_argument("--min-stroke", type=float, default=0.8)
    a = ap.parse_args()
    main(a.base, a.src, a.out, tuple(a.rotations), a.depth, a.min_stroke)
