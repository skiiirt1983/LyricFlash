"""Layer-by-layer support check (what Bambu Studio calls 'floating regions').

For every layer, the part of the layer that has nothing under it in the previous layer
(after allowing a 45 deg self-supporting step) is 'unsupported'. Each unsupported patch is
then classified:
  * bridge  - its long axis is bounded by supported material at both ends
  * FLOATING - anything else (cantilevered into air, or an island with nothing below)
"""
from __future__ import annotations

import numpy as np
import trimesh
from shapely.geometry import MultiPolygon, Polygon
from shapely.ops import unary_union


def _layer_polys(mesh, z):
    sec = mesh.section(plane_origin=[0, 0, z], plane_normal=[0, 0, 1])
    if sec is None:
        return Polygon()
    planar, to3d = sec.to_2D()
    polys = planar.polygons_full
    # back to world XY (to_2D may shift/rotate)
    out = []
    for p in polys:
        ext = np.c_[np.asarray(p.exterior.coords), np.zeros(len(p.exterior.coords)), np.ones(len(p.exterior.coords))]
        ext = (to3d @ ext.T).T[:, :2]
        holes = []
        for h in p.interiors:
            hh = np.c_[np.asarray(h.coords), np.zeros(len(h.coords)), np.ones(len(h.coords))]
            holes.append((to3d @ hh.T).T[:, :2])
        out.append(Polygon(ext, holes))
    return unary_union(out) if out else Polygon()


SHORT = 0.3   # mm beyond the 45 deg allowance that still prints cleanly


def check(stl_path, layer=0.2, first=0.2, step45=None, min_area=0.05, max_bridge=10.0):
    m = trimesh.load(stl_path)
    m.apply_translation([0, 0, -m.bounds[0][2]])
    step45 = layer if step45 is None else step45
    zs = np.arange(first / 2, m.bounds[1][2], layer)
    prev = None
    issues = []
    for z in zs:
        cur = _layer_polys(m, z)
        if prev is not None and not cur.is_empty:
            unsup = cur.difference(prev.buffer(step45 + 0.02))
            geoms = list(unsup.geoms) if isinstance(unsup, MultiPolygon) else [unsup]
            for gpoly in geoms:
                if gpoly.is_empty or gpoly.area < min_area:
                    continue
                # bridge test: shrink the supported area check along the patch's long axis
                mrr = gpoly.minimum_rotated_rectangle
                xs, ys = mrr.exterior.coords.xy
                e1 = np.hypot(xs[1] - xs[0], ys[1] - ys[0])
                e2 = np.hypot(xs[2] - xs[1], ys[2] - ys[1])
                long_len = max(e1, e2)
                # supported material touching the patch boundary
                touch = gpoly.buffer(0.15).intersection(prev.buffer(step45 + 0.02))
                parts = list(touch.geoms) if hasattr(touch, "geoms") else ([touch] if not touch.is_empty else [])
                from shapely.geometry import Point
                sup = prev.buffer(step45 + 0.02)
                reach = max(Point(c).distance(sup) for c in gpoly.exterior.coords)
                if parts and reach <= SHORT:
                    kind = "short overhang"      # sticks out <= SHORT beyond the 45 deg allowance: prints fine
                elif len(parts) >= 2 and long_len <= max_bridge + 0.5:
                    kind = "bridge"
                else:
                    kind = "FLOATING"
                if kind == "bridge" and long_len > max_bridge:
                    kind = "LONG BRIDGE"
                b = gpoly.bounds
                issues.append((round(float(z), 2), kind, round(gpoly.area, 2), round(long_len, 2), tuple(round(x, 1) for x in b), round(reach, 2)))
            # true islands: a polygon of this layer with nothing at all under it
            isl = list(cur.geoms) if isinstance(cur, MultiPolygon) else [cur]
            for ip in isl:
                if ip.area > min_area and ip.intersection(prev).area < 1e-4:
                    b = ip.bounds
                    issues.append((round(float(z), 2), "FLOATING", round(ip.area, 2), 0.0, tuple(round(x, 1) for x in b), 9.9))
        prev = cur
    return issues
