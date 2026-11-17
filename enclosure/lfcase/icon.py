"""Power symbol (assets/power_symbol.svg, Paul's file) as shapely polygons, for engraving on the PWR button face.
Only absolute M / C / L / H / V / Z path commands are supported (that is all the file uses)."""
from __future__ import annotations

import re
from pathlib import Path

import numpy as np
from shapely.geometry import Polygon
from shapely.ops import unary_union

SVG = Path(__file__).resolve().parent.parent / "assets" / "power_symbol.svg"


def _subpaths(d: str, n: int = 24):
    toks = re.findall(r"[MCLHVZ]|-?\d*\.?\d+(?:e-?\d+)?", d)
    i, cur, subs, pts, cmd = 0, (0.0, 0.0), [], [], None
    num = lambda: float(toks[i])
    while i < len(toks):
        if re.match(r"[MCLHVZ]", toks[i]):
            cmd = toks[i]
            i += 1
            if cmd == "Z":
                if pts:
                    subs.append(pts)
                pts = []
                continue
        if cmd == "M":
            if pts:
                subs.append(pts)
            cur = (num(), float(toks[i + 1])); i += 2
            pts = [cur]
            cmd = "L"
        elif cmd == "L":
            cur = (num(), float(toks[i + 1])); i += 2; pts.append(cur)
        elif cmd == "H":
            cur = (num(), cur[1]); i += 1; pts.append(cur)
        elif cmd == "V":
            cur = (cur[0], num()); i += 1; pts.append(cur)
        elif cmd == "C":
            p0 = np.array(cur)
            c1, c2, p3 = (np.array([float(toks[i + k]), float(toks[i + k + 1])]) for k in (0, 2, 4))
            i += 6
            for t in np.linspace(0, 1, n + 1)[1:]:
                p = (1 - t) ** 3 * p0 + 3 * (1 - t) ** 2 * t * c1 + 3 * (1 - t) * t ** 2 * c2 + t ** 3 * p3
                pts.append((float(p[0]), float(p[1])))
            cur = (float(p3[0]), float(p3[1]))
    if pts:
        subs.append(pts)
    return subs


def polygons(width_mm: float):
    """Upright power symbol, `width_mm` wide, centred on its bounding box. Returns shapely polygons in mm, x right, y UP."""
    s = SVG.read_text()
    d = " ".join(re.findall(r'\sd="([^"]+)"', s))
    polys = [Polygon(p).buffer(0) for p in _subpaths(d)]
    shape = unary_union(polys)                       # the bar and the open ring do not overlap: union == even-odd fill here
    x0, y0, x1, y1 = shape.bounds
    k = width_mm / (x1 - x0)
    cx, cy = (x0 + x1) / 2, (y0 + y1) / 2
    out = []
    for g in (shape.geoms if hasattr(shape, "geoms") else [shape]):
        out.append(Polygon([((x - cx) * k, -(y - cy) * k) for x, y in g.exterior.coords]))   # SVG y is down: flip
    return out
