"""Assembly-path simulation: move a part along its planned path in small steps and check it
never collides with the board (STEP) or the other printed parts.

Answers Paul's "don't let me print it and find out it can't go together".
"""
from __future__ import annotations

import math

import cadquery as cq

from lfcase.interference import check as interference


def hinge_pose(shape: cq.Shape, angle_deg: float, axis_pt, axis_dir=(0, 1, 0)):
    """Rotate about a hinge line (point + direction)."""
    return shape.rotate(cq.Vector(*axis_pt), cq.Vector(*(a + b for a, b in zip(axis_pt, axis_dir))), angle_deg)


def sweep(moving: cq.Shape, obstacles: dict, poses, ignore=lambda name, step: False, min_vol=1e-3):
    """poses: list of (label, transform function shape->shape). Returns [(label, [(obstacle, vol)])]."""
    out = []
    for label, fn in poses:
        s = fn(moving)
        hits = [(g, round(v, 3)) for g, v, _ in interference(s, obstacles, min_vol=min_vol) if not ignore(g, label)]
        out.append((label, hits))
    return out
