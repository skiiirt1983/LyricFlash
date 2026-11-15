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


def pack(shapes: dict, gap=6.0, bed=BED, keep_order=False):
    """Shelf-pack bounding boxes on the bed. Returns {name: shape placed}.
    keep_order: place in the given order (keeps matching halves next to each other)."""
    items = list(shapes.items()) if keep_order else sorted(shapes.items(), key=lambda kv: -(kv[1].BoundingBox().ylen))
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


MATERIALS = {"gold": ("Iridium Gold PLA Metal", "#C99E4C"), "black": ("Black PETG", "#151517")}
FILAMENT_SLOT = {"gold": 1, "black": 2}     # Bambu filament slot per material


def _xyz_mat(rot_deg_x=0.0):
    """3x3 rotation about X (only flips are used for print orientation)."""
    import math
    c, s_ = math.cos(math.radians(rot_deg_x)), math.sin(math.radians(rot_deg_x))
    return np.array([[1, 0, 0], [0, c, -s_], [0, s_, c]])


def write_3mf_project(parts: list, path: Path, gap: float = 6.0, keep_order: bool = True):
    """One 3MF with every part laid flat on the plate (print orientation) AND an assembled
    pose for Bambu Studio's Assembly view (Metadata/model_settings.config <assemble>).

    parts: [dict(name, shape_assembled (board/world frame), flip_x_deg or R (3x3, print orientation), material)]
    Colours/filament slots come from 3MF base materials, one entry per material.
    """
    import math
    import tempfile

    import trimesh
    mats = list(dict.fromkeys(p["material"] for p in parts))
    # assembled poses: centre the assembly over the plate, lowest point on z = 0
    bbs = [p["shape_assembled"].BoundingBox() for p in parts]
    a_shift = np.array([BED[0] / 2 - (min(b.xmin for b in bbs) + max(b.xmax for b in bbs)) / 2,
                        BED[1] / 2 - (min(b.ymin for b in bbs) + max(b.ymax for b in bbs)) / 2,
                        -min(b.zmin for b in bbs)])
    locals_, mats_asm, tf = {}, {}, {}
    for p in parts:
        sh = p["shape_assembled"]
        if p.get("R") is not None:                      # general print orientation (3x3 rotation)
            R = np.asarray(p["R"], float)
            sh = p["rotate"](sh, R)
        else:
            R = _xyz_mat(p.get("flip_x_deg", 0.0))
            if p.get("flip_x_deg", 0.0):
                sh = sh.rotate(cq.Vector(0, 0, 0), cq.Vector(1, 0, 0), p["flip_x_deg"])
        bb = sh.BoundingBox()
        # local origin = bounding-box centre in X, Y AND Z. Bambu Studio re-centres every object on
        # its bbox centre at import; if we already are, its move is zero and the assemble pose holds.
        d = np.array([-(bb.xmin + bb.xmax) / 2, -(bb.ymin + bb.ymax) / 2, -(bb.zmin + bb.zmax) / 2])
        local = sh.moved(cq.Location(cq.Vector(*d)))
        locals_[p["name"]] = local
        # local = R*S + d  ->  S = R^T (local - d); assembled world = S + a_shift
        Rt = R.T
        mats_asm[p["name"]] = (Rt, Rt @ (-d) + a_shift)
        tf[p["name"]] = (R, d)
    placed = pack(dict(locals_), gap=gap, keep_order=keep_order)   # plate positions
    objs, items, asm, slots = [], [], [], []
    for i, p in enumerate(parts, 2):
        n = p["name"]
        with tempfile.TemporaryDirectory() as td:
            f = Path(td) / "p.stl"
            write_stl(locals_[n], f)
            m = trimesh.load(f)
        m.merge_vertices()
        if p.get("mesh_post") is not None:                # e.g. the floor logo (mesh boolean), same transform as the part
            m = p["mesh_post"](m, *tf[n])
        if not m.is_watertight:
            raise RuntimeError(f"{n}: not watertight")
        V = np.round(m.vertices, 4)
        vs = "".join(f'<vertex x="{a:.4f}" y="{b:.4f}" z="{c:.4f}"/>' for a, b, c in V)
        ts = "".join(f'<triangle v1="{a}" v2="{b}" v3="{c}"/>' for a, b, c in m.faces)
        # Bambu merges a non-Bambu multi-object 3MF into ONE object (losing the assemble poses)
        # when the objects' lowest points differ by > 1e-4 mm. Lift each object by exactly
        # -(lowest written z), so every object's lowest point is exactly 0.
        z_lift = -float(V[:, 2].min())
        # No colour tags: with a model_settings.config Bambu treats the file as a project, and its
        # colour-preview path would erase the filament assignment. Slots are set per object below.
        objs.append(f'<object id="{i}" name="{n}" type="model">'
                    f'<mesh><vertices>{vs}</vertices><triangles>{ts}</triangles></mesh></object>')
        pb, lb = placed[n].BoundingBox(), locals_[n].BoundingBox()
        tx, ty = pb.xmin - lb.xmin, pb.ymin - lb.ymin
        items.append(f'<item objectid="{i}" transform="1 0 0 0 1 0 0 0 1 {tx:.4f} {ty:.4f} {z_lift:.4f}"/>')
        Rt, t = mats_asm[n]
        vals = [Rt[r, c] for c in range(3) for r in range(3)] + list(t)
        slots.append(f'  <object id="{i}">\n    <metadata key="name" value="{n}"/>\n'
                     f'    <metadata key="extruder" value="{FILAMENT_SLOT[p["material"]]}"/>\n  </object>')
        asm.append(f'   <assemble_item object_id="{i}" instance_id="0" transform="{" ".join(f"{v:.6g}" for v in vals)}" offset="0 0 0" />')
    base = "".join(f'<base name="{MATERIALS[k][0]}" displaycolor="{MATERIALS[k][1]}FF"/>' for k in mats)
    model = ('<?xml version="1.0" encoding="UTF-8"?>\n'
             '<model unit="millimeter" xml:lang="en-US" xmlns="http://schemas.microsoft.com/3dmanufacturing/core/2015/02">'
             '<metadata name="Title">LyricFlash</metadata>'
             f'<resources>{"".join(objs)}</resources>'
             f'<build>{"".join(items)}</build></model>')
    config = ('<?xml version="1.0" encoding="UTF-8"?>\n<config>\n' + "\n".join(slots) +
              '\n  <assemble>\n' + "\n".join(asm) + '\n  </assemble>\n</config>\n')
    ct = ('<?xml version="1.0" encoding="UTF-8"?>\n<Types xmlns="http://schemas.openxmlformats.org/package/2006/content-types">'
          '<Default Extension="rels" ContentType="application/vnd.openxmlformats-package.relationships+xml"/>'
          '<Default Extension="model" ContentType="application/vnd.ms-package.3dmanufacturing-3dmodel+xml"/>'
          '<Default Extension="config" ContentType="text/xml"/></Types>')
    rels = ('<?xml version="1.0" encoding="UTF-8"?>\n<Relationships xmlns="http://schemas.openxmlformats.org/package/2006/relationships">'
            '<Relationship Target="/3D/3dmodel.model" Id="rel0" Type="http://schemas.microsoft.com/3dmanufacturing/2013/01/3dmodel"/></Relationships>')
    with zipfile.ZipFile(path, "w", zipfile.ZIP_DEFLATED) as z:
        z.writestr("[Content_Types].xml", ct)
        z.writestr("_rels/.rels", rels)
        z.writestr("3D/3dmodel.model", model)
        z.writestr("Metadata/model_settings.config", config)
    return placed


def write_3mf_plates(plates: list, path: Path, bed=BED, bambu_app=True):
    """Bambu Studio project with several plates. plates: [[dict(name, mesh (trimesh, print orientation),
    material)], ...] one list per plate. Parts on a plate are laid out side by side, centred on it.
    Plate positions follow Bambu's grid (stride = bed * 1.2, columns = ceil-ish sqrt(n), rows go -Y), and
    every object's lowest point is exactly z = 0 (Bambu merges objects otherwise).
    bambu_app: tag the file as written by Bambu Studio; its importer only reads plate assignments from
    model_settings.config for such files (Paul's test: an untagged file opened as one plate)."""
    import math
    n = len(plates)
    v = math.sqrt(n)
    cols = int(round(v)) + (1 if v > round(v) else 0)
    sx, sy = bed[0] * 1.2, bed[1] * 1.2
    objs, items, cfg_obj, cfg_plate = [], [], [], []
    oid = 2
    for pi, plate in enumerate(plates):
        row, col = divmod(pi, cols)
        ox, oy = col * sx, -row * sy
        gap = 8.0
        widths = [p["mesh"].extents[0] for p in plate]
        x = bed[0] / 2 - (sum(widths) + gap * (len(plate) - 1)) / 2
        inst = []
        for p in plate:
            m = p["mesh"].copy()
            m.merge_vertices()
            if not m.is_watertight:
                raise RuntimeError(f"{p['name']}: not watertight")
            c = (m.bounds[0] + m.bounds[1]) / 2
            V = np.round(m.vertices - c, 4)                     # stored centred (Bambu re-centres on load)
            z_lift = -float(V[:, 2].min())
            tx = ox + x + m.extents[0] / 2
            ty = oy + bed[1] / 2
            x += m.extents[0] + gap
            vs = "".join(f'<vertex x="{a:.4f}" y="{b:.4f}" z="{c_:.4f}"/>' for a, b, c_ in V)
            ts = "".join(f'<triangle v1="{a}" v2="{b}" v3="{c_}"/>' for a, b, c_ in m.faces)
            objs.append(f'<object id="{oid}" name="{p["name"]}" type="model"><mesh><vertices>{vs}</vertices>'
                        f'<triangles>{ts}</triangles></mesh></object>')
            items.append(f'<item objectid="{oid}" transform="1 0 0 0 1 0 0 0 1 {tx:.4f} {ty:.4f} {z_lift:.4f}" printable="1"/>')
            cfg_obj.append(f'  <object id="{oid}">\n    <metadata key="name" value="{p["name"]}"/>\n'
                           f'    <metadata key="extruder" value="{FILAMENT_SLOT[p["material"]]}"/>\n  </object>')
            inst.append(f'    <model_instance>\n      <metadata key="object_id" value="{oid}"/>\n'
                        f'      <metadata key="instance_id" value="0"/>\n    </model_instance>')
            oid += 1
        cfg_plate.append(f'  <plate>\n    <metadata key="plater_id" value="{pi + 1}"/>\n'
                         f'    <metadata key="plater_name" value="{plate[0].get("plate_name", "")}"/>\n'
                         f'    <metadata key="locked" value="false"/>\n' + "\n".join(inst) + '\n  </plate>')
    model = ('<?xml version="1.0" encoding="UTF-8"?>\n'
             '<model unit="millimeter" xml:lang="en-US" xmlns="http://schemas.microsoft.com/3dmanufacturing/core/2015/02">'
             '<metadata name="Title">LyricFlash print pack</metadata>'
             + ('<metadata name="Application">BambuStudio-01.10.00.00</metadata>'
                '<metadata name="BambuStudio:3mfVersion">1</metadata>' if bambu_app else '') +
             f'<resources>{"".join(objs)}</resources><build>{"".join(items)}</build></model>')
    config = '<?xml version="1.0" encoding="UTF-8"?>\n<config>\n' + "\n".join(cfg_plate + cfg_obj) + '\n</config>\n'
    ct = ('<?xml version="1.0" encoding="UTF-8"?>\n<Types xmlns="http://schemas.openxmlformats.org/package/2006/content-types">'
          '<Default Extension="rels" ContentType="application/vnd.openxmlformats-package.relationships+xml"/>'
          '<Default Extension="model" ContentType="application/vnd.ms-package.3dmanufacturing-3dmodel+xml"/>'
          '<Default Extension="config" ContentType="text/xml"/></Types>')
    rels = ('<?xml version="1.0" encoding="UTF-8"?>\n<Relationships xmlns="http://schemas.openxmlformats.org/package/2006/relationships">'
            '<Relationship Target="/3D/3dmodel.model" Id="rel0" Type="http://schemas.microsoft.com/3dmanufacturing/2013/01/3dmodel"/></Relationships>')
    with zipfile.ZipFile(path, "w", zipfile.ZIP_DEFLATED) as z:
        z.writestr("[Content_Types].xml", ct)
        z.writestr("_rels/.rels", rels)
        z.writestr("3D/3dmodel.model", model)
        z.writestr("Metadata/model_settings.config", config)
