"""Fast clearance check for assembly paths, on surface point samples.

BRep booleans on the full base take minutes per pose, so the swing simulation samples every surface
densely and uses k-d trees for the smallest surface-to-surface distance. Two parts that overlap have
crossing surfaces, so an overlap shows up as a distance near 0; no inside/outside test is needed
(tessellated STEP normals aren't reliable). Accuracy is about half the sample spacing.
"""
from __future__ import annotations

import math

import numpy as np
import trimesh
from scipy.spatial import cKDTree


def to_mesh(shape, tol=0.03):
    v, f = shape.tessellate(tol, 0.2)
    m = trimesh.Trimesh(np.array([(p.x, p.y, p.z) for p in v]), np.array(f), process=True)
    return m


class Cloud:
    def __init__(self, shape, spacing=0.12, tol=0.03):
        m = to_mesh(shape, tol)
        n = max(2000, int(m.area / spacing ** 2))
        pts, fi = trimesh.sample.sample_surface(m, n, seed=1)
        self.p = np.asarray(pts)
        self.n = np.asarray(m.face_normals[fi])
        # include the vertices too, so sharp corners and edges are represented exactly
        self.p = np.vstack([self.p, m.vertices])
        vn = m.vertex_normals
        self.n = np.vstack([self.n, vn])
        self.tree = cKDTree(self.p)
        self.spacing = spacing
        self.lo, self.hi = self.p.min(0), self.p.max(0)


def rot_matrix(axis_pt, axis_dir, deg):
    k = np.asarray(axis_dir, float); k /= np.linalg.norm(k)
    a = math.radians(deg)
    K = np.array([[0, -k[2], k[1]], [k[2], 0, -k[0]], [-k[1], k[0], 0]])
    R = np.eye(3) + math.sin(a) * K + (1 - math.cos(a)) * K @ K
    o = np.asarray(axis_pt, float)
    return R, o - R @ o


def _nearest(points, cloud: Cloud, margin=1.0):
    m = np.all((points > cloud.lo - margin) & (points < cloud.hi + margin), axis=1)
    if not m.any():
        return np.inf, None
    q = points[m]
    d, _ = cloud.tree.query(q, distance_upper_bound=margin)
    k = int(np.argmin(d))
    return float(d[k]), q[k]


def clearance(moving: Cloud, R, t, fixed: Cloud):
    """(smallest surface distance, where on the moving part, posed) between moving (posed by R,t) and fixed."""
    return _nearest(moving.p @ R.T + t, fixed)
