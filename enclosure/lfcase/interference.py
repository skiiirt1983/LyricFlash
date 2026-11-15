"""Boolean interference check of coupon parts (board frame) against the STEP board."""
from __future__ import annotations

import cadquery as cq


def _overlap(a, b, pad=0.0):
    return not (a.xmax + pad < b.xmin or b.xmax + pad < a.xmin or a.ymax + pad < b.ymin or
                b.ymax + pad < a.ymin or a.zmax + pad < b.zmin or b.zmax + pad < a.zmin)


def check(part: cq.Shape, groups: dict, min_vol=1e-4):
    """Return [(group, volume mm3, bbox of the overlap)] for every board group the part intersects."""
    pb = part.BoundingBox()
    hits = []
    for g, comp in groups.items():
        solids = comp.Solids() or [comp]
        vol, boxes = 0.0, []
        for s in solids:
            if not _overlap(pb, s.BoundingBox()):
                continue
            i = part.intersect(s)
            v = i.Volume()
            if v > min_vol:
                vol += v
                boxes.append(i.BoundingBox())
        if vol > min_vol:
            b = boxes[0]
            for o in boxes[1:]:
                b = b.add(o)
            hits.append((g, vol, (b.xmin, b.ymin, b.zmin, b.xmax, b.ymax, b.zmax)))
    return hits


def min_gap(part: cq.Shape, groups: dict, names):
    """Smallest distance between the part and the named board groups."""
    out = {}
    for n in names:
        out[n] = part.distance(groups[n]) if hasattr(part, "distance") else None
    return out
