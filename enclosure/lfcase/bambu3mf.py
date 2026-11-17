"""Multi-plate Bambu Studio project (.3mf) that opens with its plates, not as one plate and a ring of loose objects.

Why this exists: lfcase/export.py's write_3mf_plates wrote a plain 3MF with <plate> entries in model_settings.config. Bambu
Studio put every object at its plate's grid position but made only ONE plate (Paul's screenshot, 2026-11-17). Paul fixed an
old file by hand in Bambu Studio 02.09.01.70 and gave it back as the reference; this writer copies that file's structure:

  * "production extension" objects: 3D/3dmodel.model holds, per part, an <object> with a <component p:path=...> that points
    at its own 3D/Objects/object_N.model (the mesh), every object/component/item with a p:UUID, requiredextensions="p";
  * Metadata/model_settings.config: per object an <object> with a <part> (uuid, matrix, source_*, mesh_stat), per plate a
    <plate> (plater_id, plater_name, locked, filament_map_*, <model_instance> with object_id / instance_id / identify_id),
    and an <assemble> list;
  * Metadata/project_settings.config + filament_settings_1.config + slice_info.config + cut_information.xml +
    filament_sequence.json, copied or generated like the reference (the profile and the project info text come from
    assets/bambu_template/, taken from Paul's file: printer Bambu Lab P2S 0.4, Textured PEI, PETG Matte);
  * plate layout exactly as Bambu: columns = ceil(sqrt(n)), pitch = bed * 1.2, plate (row, col) is centred at
    (bed/2 + col * pitch, bed/2 - row * pitch), objects stored centred on their bounding box with the lowest point at z = 0.

Not written: thumbnails (Bambu draws its own). Not tested against Bambu Studio itself from here: check_against_reference()
compares the result with the reference file's structure.
"""
from __future__ import annotations

import json
import math
import uuid
import zipfile
from pathlib import Path

import numpy as np

HERE = Path(__file__).resolve().parent.parent
TEMPLATE = HERE / "assets" / "bambu_template"
BED = (256.0, 256.0)
GAP = 1.2                                   # Bambu: plate pitch = bed * 1.2
FILAMENT_SLOT = {"gold": 1, "black": 2}

NS = ('xmlns="http://schemas.microsoft.com/3dmanufacturing/core/2015/02" xmlns:BambuStudio="http://schemas.bambulab.com/package/2021" '
      'xmlns:p="http://schemas.microsoft.com/3dmanufacturing/production/2015/06" requiredextensions="p"')


def _esc(s: str) -> str:
    return s.replace("&", "&amp;").replace("<", "&lt;").replace(">", "&gt;").replace('"', "&quot;")


def _mesh_xml(m, inner_id: int, n_file: int) -> str:
    c = (m.bounds[0] + m.bounds[1]) / 2
    V = m.vertices - c
    vs = "\n".join(f'     <vertex x="{a:.7g}" y="{b:.7g}" z="{c_:.7g}"/>' for a, b, c_ in V)
    ts = "\n".join(f'     <triangle v1="{a}" v2="{b}" v3="{c_}"/>' for a, b, c_ in m.faces)
    return ('<?xml version="1.0" encoding="UTF-8"?>\n'
            f'<model unit="millimeter" xml:lang="en-US" {NS}>\n'
            ' <metadata name="BambuStudio:3mfVersion">1</metadata>\n <resources>\n'
            f'  <object id="{inner_id}" p:UUID="{n_file:04x}0000-81cb-4c03-9d28-80fed5dfa1dc" type="model">\n   <mesh>\n    <vertices>\n'
            f'{vs}\n    </vertices>\n    <triangles>\n{ts}\n    </triangles>\n   </mesh>\n  </object>\n </resources>\n <build/>\n</model>\n')


def write_plates(plates: list, path: Path, bed=BED, today: str = "") -> dict:
    """plates: [[dict(name, mesh (trimesh, print orientation), material 'gold'|'black', plate_name)], ...], one list per plate.
    Returns a summary dict (plate origins, object ids)."""
    import datetime
    today = today or datetime.date.today().isoformat()
    info = json.loads((TEMPLATE / "project_info.json").read_text())
    n = len(plates)
    v = math.sqrt(n)
    cols = int(round(v)) + (1 if v > round(v) else 0)
    sx, sy = bed[0] * GAP, bed[1] * GAP
    objects, items, files = [], [], {}
    cfg_objects, cfg_plates, assemble = [], [], []
    oid, n_file = 2, 9
    summary = dict(cols=cols, plates=[])
    ax = 183.75
    for pi, plate in enumerate(plates):
        row, col = divmod(pi, cols)
        cx, cy = bed[0] / 2 + col * sx, bed[1] / 2 - row * sy         # plate centre
        gap = 8.0
        widths = [p["mesh"].extents[0] for p in plate]
        x = cx - (sum(widths) + gap * (len(plate) - 1)) / 2
        inst = []
        for p in plate:
            m = p["mesh"].copy()
            m.merge_vertices()
            if not m.is_watertight:
                raise RuntimeError(f"{p['name']}: not watertight")
            c = (m.bounds[0] + m.bounds[1]) / 2
            z_lift = float(m.extents[2] / 2)                            # centred mesh: lowest point at z = 0
            tx = x + m.extents[0] / 2
            x += m.extents[0] + gap
            inner = oid - 1
            files[f"3D/Objects/object_{n_file}.model"] = _mesh_xml(m, inner, n_file)
            objects.append(
                f'  <object id="{oid}" p:UUID="{n_file:08x}-61cb-4c03-9d28-80fed5dfa1dc" type="model">\n   <components>\n'
                f'    <component p:path="/3D/Objects/object_{n_file}.model" objectid="{inner}" '
                f'p:UUID="{n_file:04x}0000-b206-40ff-9872-83e8017abed1" transform="1 0 0 0 1 0 0 0 1 0 0 0"/>\n   </components>\n  </object>')
            items.append(f'  <item objectid="{oid}" p:UUID="{oid:08x}-b1ec-4553-aec9-835e5b724bb4" '
                         f'transform="1 0 0 0 1 0 0 0 1 {tx:.7g} {cy:.7g} {z_lift:.7g}" printable="1"/>')
            slot = FILAMENT_SLOT[p["material"]]
            cfg_objects.append(
                f'  <object id="{oid}">\n    <metadata key="name" value="{_esc(p["name"])}"/>\n'
                f'    <metadata key="extruder" value="{slot}"/>\n    <metadata face_count="{len(m.faces)}"/>\n'
                f'    <part id="{inner}" subtype="normal_part" uuid="{uuid.uuid5(uuid.NAMESPACE_URL, p["name"] + str(oid))}">\n'
                f'      <metadata key="name" value="{_esc(p["name"])}"/>\n'
                '      <metadata key="matrix" value="1 0 0 0 0 1 0 0 0 0 1 0 0 0 0 1"/>\n'
                f'      <metadata key="source_file" value="{_esc(p["name"])}.stl"/>\n'
                '      <metadata key="source_object_id" value="0"/>\n      <metadata key="source_volume_id" value="0"/>\n'
                f'      <metadata key="source_offset_x" value="{c[0]:.7g}"/>\n      <metadata key="source_offset_y" value="{c[1]:.7g}"/>\n'
                f'      <metadata key="source_offset_z" value="{c[2]:.7g}"/>\n'
                f'      <mesh_stat face_count="{len(m.faces)}" edges_fixed="0" degenerate_facets="0" facets_removed="0" facets_reversed="0" backwards_edges="0"/>\n'
                '    </part>\n  </object>')
            inst.append(f'    <model_instance>\n      <metadata key="object_id" value="{oid}"/>\n'
                        f'      <metadata key="instance_id" value="0"/>\n      <metadata key="identify_id" value="{1000 + 11 * oid}"/>\n'
                        '    </model_instance>')
            assemble.append(f'   <assemble_item object_id="{oid}" instance_id="0" transform="1 0 0 0 1 0 0 0 1 {ax:.7g} 130.0479 {z_lift:.7g}" offset="0 0 0" />\n'
                            f'   <assemble_item object_id="{oid}" volume_id="0" transform="1 0 0 0 1 0 0 0 1 0 0 0" />')
            ax += max(64.0, m.extents[0] + 8.0)
            summary["plates"].append(dict(plate=pi + 1, object_id=oid, centre=(round(tx, 2), round(cy, 2))))
            oid += 2
            n_file += 1
        cfg_plates.append(
            f'  <plate>\n    <metadata key="plater_id" value="{pi + 1}"/>\n    <metadata key="plater_name" value="{_esc(plate[0].get("plate_name", ""))}"/>\n'
            '    <metadata key="locked" value="false"/>\n    <metadata key="filament_map_mode" value="Auto For Flush"/>\n'
            '    <metadata key="filament_maps" value="1 1 1"/>\n    <metadata key="filament_volume_maps" value="0 0 0"/>\n'
            + "\n".join(inst) + '\n  </plate>')
    meta = "".join(f' <metadata name="{k}">{info[k]}</metadata>\n' for k in
                   ("Application", "BambuStudio:3mfVersion", "Copyright"))
    meta += f' <metadata name="CreationDate">{today}</metadata>\n <metadata name="Description">{info["Description"]}</metadata>\n'
    for k in ("Designer", "DesignerUserId", "License", "Origin"):
        meta += f' <metadata name="{k}">{info[k]}</metadata>\n'
    meta += (f' <metadata name="ModificationDate">{today}</metadata>\n <metadata name="ProfileDescription">{info["ProfileDescription"]}</metadata>\n'
             f' <metadata name="ProfileTitle">{info["ProfileTitle"]}</metadata>\n <metadata name="Title">{info["Title"]}</metadata>\n')
    model = ('<?xml version="1.0" encoding="UTF-8"?>\n'
             f'<model unit="millimeter" xml:lang="en-US" {NS}>\n{meta} <resources>\n' + "\n".join(objects) + '\n </resources>\n'
             f' <build p:UUID="2c7c17d8-22b5-4d84-8835-1976022ea369">\n' + "\n".join(items) + '\n </build>\n</model>\n')
    config = ('<?xml version="1.0" encoding="UTF-8"?>\n<config>\n' + "\n".join(cfg_objects) + "\n" + "\n".join(cfg_plates)
              + "\n  <assemble>\n" + "\n".join(assemble) + "\n  </assemble>\n</config>\n")
    n_obj = len(objects)
    cut = ('<?xml version="1.0" encoding="utf-8"?>\n<objects>\n' + "".join(
        f' <object id="{i}">\n  <cut_id id="0" check_sum="1" connectors_cnt="0"/>\n </object>\n' for i in range(1, n_obj + 1)) + '</objects>\n')
    seq = json.dumps({f"plate_{i}": {"nozzle_sequence": [], "optimal_assignment": [], "sequence": []} for i in range(1, n + 1)},
                     separators=(",", ":"))
    ct = ('<?xml version="1.0" encoding="UTF-8"?>\n<Types xmlns="http://schemas.openxmlformats.org/package/2006/content-types">\n'
          ' <Default Extension="rels" ContentType="application/vnd.openxmlformats-package.relationships+xml"/>\n'
          ' <Default Extension="model" ContentType="application/vnd.ms-package.3dmanufacturing-3dmodel+xml"/>\n'
          ' <Default Extension="png" ContentType="image/png"/>\n <Default Extension="gcode" ContentType="text/x.gcode"/>\n</Types>\n')
    rels = ('<?xml version="1.0" encoding="UTF-8"?>\n<Relationships xmlns="http://schemas.openxmlformats.org/package/2006/relationships">\n'
            ' <Relationship Target="/3D/3dmodel.model" Id="rel-1" Type="http://schemas.microsoft.com/3dmanufacturing/2013/01/3dmodel"/>\n</Relationships>\n')
    mrels = ('<?xml version="1.0" encoding="UTF-8"?>\n<Relationships xmlns="http://schemas.openxmlformats.org/package/2006/relationships">\n'
             + "".join(f' <Relationship Target="/{k}" Id="rel-{i + 1}" Type="http://schemas.microsoft.com/3dmanufacturing/2013/01/3dmodel"/>\n'
                       for i, k in enumerate(sorted(files, key=lambda s: int(s.split("_")[-1].split(".")[0])))) + '</Relationships>\n')
    with zipfile.ZipFile(path, "w", zipfile.ZIP_DEFLATED) as z:
        z.writestr("[Content_Types].xml", ct)
        z.writestr("_rels/.rels", rels)
        for k in sorted(files, key=lambda s: int(s.split("_")[-1].split(".")[0])):
            z.writestr(k, files[k])
        z.writestr("3D/3dmodel.model", model)
        z.writestr("3D/_rels/3dmodel.model.rels", mrels)
        z.writestr("Metadata/model_settings.config", config)
        z.writestr("Metadata/project_settings.config", (TEMPLATE / "project_settings.config").read_text())
        z.writestr("Metadata/filament_settings_1.config", (TEMPLATE / "filament_settings_1.config").read_text())
        z.writestr("Metadata/slice_info.config", (TEMPLATE / "slice_info.config").read_text())
        z.writestr("Metadata/cut_information.xml", cut)
        z.writestr("Metadata/filament_sequence.json", seq)
    return summary


def check_plates(path: Path, bed=BED) -> list:
    """Structural check of a multi-plate file written here: every part is well-formed XML; every plate has its model_instance;
    every build item sits on its plate (Bambu's grid); every object's mesh is centred with the lowest point at z = 0 once the
    item's z is applied. Returns a list of problems (empty = fine)."""
    import xml.etree.ElementTree as ET
    ns = {"c": "http://schemas.microsoft.com/3dmanufacturing/core/2015/02", "p": "http://schemas.microsoft.com/3dmanufacturing/production/2015/06"}
    z = zipfile.ZipFile(path)
    bad = []
    for name in z.namelist():
        if name.endswith((".xml", ".model", ".rels")) or name in ("Metadata/model_settings.config", "Metadata/slice_info.config"):
            try:
                ET.fromstring(z.read(name))
            except ET.ParseError as e:
                bad.append(f"{name}: {e}")
    ms = ET.fromstring(z.read("Metadata/model_settings.config"))
    m3 = ET.fromstring(z.read("3D/3dmodel.model"))
    plates = ms.findall("plate")
    n = len(plates)
    v = math.sqrt(n)
    cols = int(round(v)) + (1 if v > round(v) else 0)
    items = {i.get("objectid"): [float(x) for x in i.get("transform").split()[9:]] for i in m3.find("c:build", ns)}
    for k, pl in enumerate(plates):
        if pl.find('metadata[@key="plater_id"]').get("value") != str(k + 1):
            bad.append(f"plate {k + 1}: plater_id out of order")
        inst = pl.findall("model_instance")
        if not inst:
            bad.append(f"plate {k + 1}: no model_instance")
        row, col = divmod(k, cols)
        cx, cy = bed[0] / 2 + col * bed[0] * GAP, bed[1] / 2 - row * bed[1] * GAP
        for i in inst:
            oid = i.find('metadata[@key="object_id"]').get("value")
            if oid not in items:
                bad.append(f"plate {k + 1}: object {oid} not in <build>")
                continue
            x, y, zz = items[oid]
            if abs(y - cy) > bed[1] / 2 or abs(x - cx) > bed[0] / 2:
                bad.append(f"plate {k + 1}: object {oid} at ({x:.1f}, {y:.1f}) is off its plate centred at ({cx:.1f}, {cy:.1f})")
    return bad
