"""Paul's logo + text, engraved 0.5 mm into the top of the base floor (inside, under the battery).

Taken from his Bambu edit of base_panels.stl (2026-11-10) as the cut volume, stored in world coordinates in
assets/logo_floor_cut.stl. It is a triangle mesh, so it is applied with an exact mesh boolean (manifold3d) on
the exported base meshes, not in the CAD model (OCC booleans with faceted solids fail).
"""
from __future__ import annotations

from pathlib import Path

import numpy as np
import trimesh

import params as P

_CACHE = {}


def logo_mesh():
    if "m" not in _CACHE:
        _CACHE["m"] = trimesh.load(Path(__file__).resolve().parent.parent / P.FLOOR_LOGO)
    return _CACHE["m"]


def cut(mesh: trimesh.Trimesh, R=None, offset=(0.0, 0.0, 0.0)) -> trimesh.Trimesh:
    """mesh minus the logo. R/offset: the same rotation + translation that took the world part to `mesh`."""
    if not P.FLOOR_LOGO:
        return mesh
    import manifold3d as mf
    lg = logo_mesh().copy()
    v = lg.vertices if R is None else lg.vertices @ np.asarray(R).T
    lg = trimesh.Trimesh(v + np.asarray(offset), lg.faces, process=False)

    def M(m):
        return mf.Manifold(mf.Mesh(vert_properties=np.asarray(m.vertices, np.float32), tri_verts=np.asarray(m.faces, np.uint32)))
    out = (M(mesh) - M(lg)).to_mesh()
    res = trimesh.Trimesh(np.asarray(out.vert_properties)[:, :3], np.asarray(out.tri_verts))
    if not res.is_watertight or abs(mesh.volume - res.volume - lg.volume) > 0.05 * lg.volume:
        raise RuntimeError("logo cut failed")
    return res


def cut_stl(path, R=None, offset=(0.0, 0.0, 0.0)):
    """Cut the logo into an exported base STL, then set it exactly on z = 0 (CAD bounding boxes are a little
    loose, so the export can float a few hundredths above the plate)."""
    m = cut(trimesh.load(path), R, offset)
    m.apply_translation([0, 0, -m.bounds[0][2]])
    m.export(path)
