"""Paul's Bambu Studio profile and his hand-painted USB-C supports, applied to the multi-plate print projects.

  * assets/bambu_template/usb_support_paint.npz: the 190 triangles per base that Paul painted as support enforcers round the
    USB-C opening (taken from his own saved project, 2026-11-17: all six bases carry the same painting).
  * assets/bambu_template/project_settings_user.config: his process/filament settings from that project (manual tree supports,
    tree_slim, supports on build plate only, Arachne, 4 walls, gyroid, seam at the back, top Z distance 0.26, 3 interface layers,
    skirt 1: those last three are what make PETG supports come off easily).
  * 'petg' = his settings as they are. 'pla' = the same but with the PETG-only edits removed: support top Z distance and interface
    layers back to Bambu's defaults (0.2 mm, 2 layers) and no skirt.
Triangles are matched by position (centroid, 1e-3 mm), so the painting follows the mesh even though the file is rebuilt every time.
"""
from __future__ import annotations

import json
import re
import zipfile
from pathlib import Path

import numpy as np
from scipy.spatial import cKDTree

TEMPLATE = Path(__file__).resolve().parent.parent / "assets" / "bambu_template"
PLA_RESET = {"support_top_z_distance": "0.2", "support_interface_top_layers": "2", "skirt_loops": "0"}   # Bambu defaults
SEAM_BACK = ("base_smooth", "base_smooth_nocleat")           # Paul set the seam to 'back' on the smooth bases


def _settings(kind: str) -> str:
    d = json.loads((TEMPLATE / "project_settings_user.config").read_text())
    if kind == "pla":
        for k, v in PLA_RESET.items():
            d[k] = v
        diff = d["different_settings_to_system"][0].split(";")
        d["different_settings_to_system"][0] = ";".join(k for k in diff if k not in PLA_RESET)
    return json.dumps(d, indent=4)


def _paint(obj_xml: str, ref: np.ndarray) -> tuple[str, int]:
    V = np.array(re.findall(r'<vertex x="([^"]+)" y="([^"]+)" z="([^"]+)"', obj_xml), float)
    tri = re.findall(r'<triangle v1="(\d+)" v2="(\d+)" v3="(\d+)"/>', obj_xml)
    F = np.array(tri, int)
    cen = V[F].mean(1)
    d, j = cKDTree(ref.mean(1)).query(cen, distance_upper_bound=1e-3)
    hit = np.isfinite(d)
    n = int(hit.sum())
    it = iter(np.nonzero(hit)[0])
    nxt = next(it, -1)
    out, i = [], 0

    def sub(m):
        nonlocal i, nxt
        if i == nxt:
            nxt = next(it, -1)
            i += 1
            return m.group(0)[:-2] + ' paint_supports="4"/>'
        i += 1
        return m.group(0)
    return re.sub(r'<triangle v1="\d+" v2="\d+" v3="\d+"/>', sub, obj_xml), n


def apply(path: Path, kind: str) -> dict:
    """Rewrite the 3MF at `path` in place: painted USB supports on every base, Paul's settings (kind 'petg' or 'pla'), seam
    'back' on the smooth bases, two-filament plate maps. Returns {object name: painted triangles}."""
    ref = np.load(TEMPLATE / "usb_support_paint.npz")
    with zipfile.ZipFile(path) as z:
        files = {n: z.read(n) for n in z.namelist()}
    cfg = files["Metadata/model_settings.config"].decode()
    mdl = files["3D/3dmodel.model"].decode()
    names = {m.group(1): m.group(2) for m in re.finditer(r'<object id="(\d+)">\s*<metadata key="name" value="([^"]+)"', cfg)}
    comp = {m.group(1): m.group(2) for m in re.finditer(
        r'<object id="(\d+)"[^>]*>\s*<components>\s*<component p:path="/(3D/Objects/object_\d+\.model)"', mdl)}
    painted = {}
    for oid, nm in names.items():
        base = nm[:-4] if nm.endswith("_pla") else nm
        if base not in ref.files:
            continue
        xml, n = _paint(files[comp[oid]].decode(), ref[base])
        if n != len(ref[base]):
            raise RuntimeError(f"{nm}: matched {n} of {len(ref[base])} painted triangles; the USB-C area changed, repaint in Bambu Studio")
        files[comp[oid]] = xml.encode()
        painted[nm] = n
        if base in SEAM_BACK:
            cfg = re.sub(rf'(<object id="{oid}">\s*<metadata key="name" value="[^"]+"/>\s*<metadata key="extruder" value="\d"/>)',
                         r'\1\n    <metadata key="seam_position" value="back"/>', cfg, count=1)
    cfg = cfg.replace('filament_maps" value="1 1 1"', 'filament_maps" value="1 1"').replace('filament_volume_maps" value="0 0 0"', 'filament_volume_maps" value="0 0"')
    files["Metadata/model_settings.config"] = cfg.encode()
    files["Metadata/project_settings.config"] = _settings(kind).encode()
    with zipfile.ZipFile(path, "w", zipfile.ZIP_DEFLATED) as z:
        for n, b in files.items():
            z.writestr(n, b)
    return painted
