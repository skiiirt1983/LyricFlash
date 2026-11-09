"""STL / STEP per part and a single 3MF with every part laid out on the plate."""
from __future__ import annotations

import zipfile
from pathlib import Path

import cadquery as cq
import numpy as np

BED = (256.0, 256.0)   # Bambu Lab P2S build plate


def to_mesh(shape: cq.Shape, tol=0.01, ang=0.15):
    verts, tris = shape.tessellate(tol, ang)
    v = np.array([(p.x, p.y, p.z) for p in verts], float)
    t = np.array(tris, int)
    return v, t


def write_stl(shape, path: Path):
    cq.exporters.export(shape, str(path), tolerance=0.01, angularTolerance=0.15)


def write_step(shape, path: Path):
    cq.exporters.export(shape, str(path))


def drop_to_bed(shape: cq.Shape) -> cq.Shape:
    bb = shape.BoundingBox()
    return shape.moved(cq.Location(cq.Vector(-bb.xmin, -bb.ymin, -bb.zmin)))


def pack(shapes: dict, gap=6.0, bed=BED):
    """Shelf-pack bounding boxes on the bed. Returns {name: shape placed}."""
    items = sorted(shapes.items(), key=lambda kv: -(kv[1].BoundingBox().ylen))
    placed, x, y, row_h = {}, gap, gap, 0.0
    for name, s in items:
        s = drop_to_bed(s)
        bb = s.BoundingBox()
        if x + bb.xlen + gap > bed[0]:
            x, y, row_h = gap, y + row_h + gap, 0.0
        if y + bb.ylen + gap > bed[1]:
            raise RuntimeError("Coupons do not fit on one P2S plate")
        placed[name] = s.moved(cq.Location(cq.Vector(x, y, 0)))
        x += bb.xlen + gap
        row_h = max(row_h, bb.ylen)
    return placed


def write_3mf(shapes: dict, path: Path):
    """Minimal 3MF core spec: one object per part, each with a build item."""
    import tempfile

    import trimesh
    objs, items = [], []
    for i, (name, s) in enumerate(shapes.items(), 1):
        with tempfile.TemporaryDirectory() as td:
            f = Path(td) / "p.stl"
            write_stl(s, f)
            m = trimesh.load(f)            # merges coincident vertices -> one connected surface
        m.merge_vertices()
        if not m.is_watertight:
            raise RuntimeError(f"{name}: mesh is not watertight / manifold")
        v, t = m.vertices, m.faces
        vs = "".join(f'<vertex x="{a:.4f}" y="{b:.4f}" z="{c:.4f}"/>' for a, b, c in v)
        ts = "".join(f'<triangle v1="{a}" v2="{b}" v3="{c}"/>' for a, b, c in t)
        objs.append(f'<object id="{i}" name="{name}" type="model"><mesh><vertices>{vs}</vertices>'
                    f'<triangles>{ts}</triangles></mesh></object>')
        items.append(f'<item objectid="{i}"/>')
    model = ('<?xml version="1.0" encoding="UTF-8"?>\n'
             '<model unit="millimeter" xml:lang="en-US" '
             'xmlns="http://schemas.microsoft.com/3dmanufacturing/core/2015/02">'
             '<metadata name="Title">LyricFlash Phase 1 coupons</metadata>'
             f'<resources>{"".join(objs)}</resources><build>{"".join(items)}</build></model>')
    ct = ('<?xml version="1.0" encoding="UTF-8"?>\n<Types xmlns="http://schemas.openxmlformats.org/package/2006/content-types">'
          '<Default Extension="rels" ContentType="application/vnd.openxmlformats-package.relationships+xml"/>'
          '<Default Extension="model" ContentType="application/vnd.ms-package.3dmanufacturing-3dmodel+xml"/></Types>')
    rels = ('<?xml version="1.0" encoding="UTF-8"?>\n<Relationships xmlns="http://schemas.openxmlformats.org/package/2006/relationships">'
            '<Relationship Target="/3D/3dmodel.model" Id="rel0" '
            'Type="http://schemas.microsoft.com/3dmanufacturing/2013/01/3dmodel"/></Relationships>')
    with zipfile.ZipFile(path, "w", zipfile.ZIP_DEFLATED) as z:
        z.writestr("[Content_Types].xml", ct)
        z.writestr("_rels/.rels", rels)
        z.writestr("3D/3dmodel.model", model)


def mesh_report(shape: cq.Shape):
    """Watertightness check of the exported tessellation (every edge used twice)."""
    v, t = to_mesh(shape)
    from collections import Counter
    # weld vertices by position (tessellation duplicates them per face)
    key = {tuple(np.round(p, 4)): i for i, p in enumerate(v)}
    idx = np.array([key[tuple(np.round(p, 4))] for p in v])
    tt = idx[t]
    e = Counter()
    for a, b, c in tt:
        for x, y in ((a, b), (b, c), (c, a)):
            e[(min(x, y), max(x, y))] += 1
    bad = sum(1 for n in e.values() if n != 2)
    return len(tt), bad


def write_3mf_assembled(parts: dict, path: Path, colors: dict | None = None):
    """Parts in their ASSEMBLED positions (not laid flat), one named object each, so a slicer
    shows the finished product and each piece can be given its own filament/colour.
    colors: {part name: '#RRGGBB'} written as 3MF base materials (display hint).
    Lift everything so the lowest point sits on the plate."""
    import tempfile

    import trimesh
    zmin = min(s.BoundingBox().zmin for s in parts.values())
    xs = [s.BoundingBox() for s in parts.values()]
    cx = (min(b.xmin for b in xs) + max(b.xmax for b in xs)) / 2
    cy = (min(b.ymin for b in xs) + max(b.ymax for b in xs)) / 2
    shift = cq.Location(cq.Vector(BED[0] / 2 - cx, BED[1] / 2 - cy, -zmin))
    names = list(parts)
    colors = colors or {}
    mats = "".join(f'<base name="{n}" displaycolor="{colors.get(n, "#B0B0B0")}FF"/>' for n in names)
    objs, items = [], []
    for i, n in enumerate(names, 2):
        with tempfile.TemporaryDirectory() as td:
            f = Path(td) / "p.stl"
            write_stl(parts[n].moved(shift), f)
            m = trimesh.load(f)
        m.merge_vertices()
        if not m.is_watertight:
            raise RuntimeError(f"{n}: not watertight")
        vs = "".join(f'<vertex x="{a:.4f}" y="{b:.4f}" z="{c:.4f}"/>' for a, b, c in m.vertices)
        ts = "".join(f'<triangle v1="{a}" v2="{b}" v3="{c}"/>' for a, b, c in m.faces)
        objs.append(f'<object id="{i}" name="{n}" type="model" pid="1" pindex="{i - 2}"><mesh><vertices>{vs}</vertices>'
                    f'<triangles>{ts}</triangles></mesh></object>')
        items.append(f'<item objectid="{i}"/>')
    model = ('<?xml version="1.0" encoding="UTF-8"?>\n'
             '<model unit="millimeter" xml:lang="en-US" xmlns="http://schemas.microsoft.com/3dmanufacturing/core/2015/02">'
             '<metadata name="Title">LyricFlash assembled preview</metadata>'
             f'<resources><basematerials id="1">{mats}</basematerials>{"".join(objs)}</resources>'
             f'<build>{"".join(items)}</build></model>')
    ct = ('<?xml version="1.0" encoding="UTF-8"?>\n<Types xmlns="http://schemas.openxmlformats.org/package/2006/content-types">'
          '<Default Extension="rels" ContentType="application/vnd.openxmlformats-package.relationships+xml"/>'
          '<Default Extension="model" ContentType="application/vnd.ms-package.3dmanufacturing-3dmodel+xml"/></Types>')
    rels = ('<?xml version="1.0" encoding="UTF-8"?>\n<Relationships xmlns="http://schemas.openxmlformats.org/package/2006/relationships">'
            '<Relationship Target="/3D/3dmodel.model" Id="rel0" Type="http://schemas.microsoft.com/3dmanufacturing/2013/01/3dmodel"/></Relationships>')
    with zipfile.ZipFile(path, "w", zipfile.ZIP_DEFLATED) as z:
        z.writestr("[Content_Types].xml", ct)
        z.writestr("_rels/.rels", rels)
        z.writestr("3D/3dmodel.model", model)
