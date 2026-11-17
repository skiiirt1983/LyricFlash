"""Load Waveshare's ESP32-S3-Touch-AMOLED-1.75 STEP model with component names.

Board frame used everywhere in this project (the "board frame"):
  * origin  : centre of the round PCB, on the PCB *back* face
  * +Z      : out of the display glass (towards the viewer)
  * X       : STEP x axis (USB-C points to -X)
  * Y       : STEP -z axis, so X, Y, Z is right-handed
Looking at the glass from the front, USB-C is on the left, the two side buttons
are on the right, the 8-pin header is at the bottom.

The STEP is read once (about 12 s) and every component group is cached as BREP
in enclosure/cache/<step-hash>/ so later runs load in about a second.
"""
from __future__ import annotations

import hashlib
import os
from pathlib import Path

import cadquery as cq

ENCLOSURE_DIR = Path(__file__).resolve().parent.parent
DEFAULT_STEP = ENCLOSURE_DIR / "board" / "ESP32-S3-Touch-AMOLED-1_75.stp"
CACHE_DIR = ENCLOSURE_DIR / "cache"

# STEP component name (leaf product, or its parent assembly) -> group name.
# Only components the enclosure cares about get their own group; all other
# parts (passives, ICs, crystals...) go to "misc_back" / "misc_front".
GROUPS = {
    "BOARD": "pcb",
    "H0175Y003AMT003": "display",          # AMOLED + touch cover glass
    "ESP32-S3-TOUCH-AMOLED-1_75-PMMA": "pmma_frame",
    "FPC": "display_fpc",
    "698B116-264-211_ASM": "usb_c",         # H1, USB-C receptacle
    "KEY1_ASM": "key1",
    "KEY2_ASM": "key2",
    "MIC1_ASM": "mic1",
    "MIC2_ASM": "mic2",
    "J1_ASM": "j1",                         # MX1.25 2P vertical
    "H3_ASM": "h3",                         # MX1.25 2P vertical
    "MICRO_SD_SOCKET_PU": "microsd",
    "FEMALE-2_54MM-H6-1X8P-V": "header",
    "SMTSO-M2-3_5ET": "standoffs",          # 3x M2 SMT standoff
}


def _step_hash(path: Path) -> str:
    h = hashlib.sha1()
    with open(path, "rb") as f:
        for chunk in iter(lambda: f.read(1 << 20), b""):
            h.update(chunk)
    return h.hexdigest()[:12]


def step_to_board_frame(pcb_back_y: float) -> cq.Location:
    """STEP coords -> board frame: rotate +90 deg about X, then lift PCB back to Z=0."""
    return cq.Location(cq.Vector(0, 0, -pcb_back_y)) * cq.Location(
        cq.Vector(0, 0, 0), cq.Vector(1, 0, 0), 90
    )


def _read_step_raw(step_path: Path) -> dict[str, list[cq.Shape]]:
    """Return {group: [shapes in STEP coordinates]} using XCAF so names survive."""
    from OCP.STEPCAFControl import STEPCAFControl_Reader
    from OCP.TCollection import TCollection_ExtendedString
    from OCP.TDataStd import TDataStd_Name
    from OCP.TDF import TDF_Label, TDF_LabelSequence
    from OCP.TDocStd import TDocStd_Document
    from OCP.TopLoc import TopLoc_Location
    from OCP.XCAFDoc import XCAFDoc_DocumentTool

    doc = TDocStd_Document(TCollection_ExtendedString("doc"))
    reader = STEPCAFControl_Reader()
    reader.SetNameMode(True)
    if reader.ReadFile(str(step_path)) != 1:
        raise RuntimeError(f"Could not read STEP file {step_path}")
    reader.Transfer(doc)
    st = XCAFDoc_DocumentTool.ShapeTool_s(doc.Main())

    def name(label) -> str:
        attr = TDataStd_Name()
        if label.FindAttribute(TDataStd_Name.GetID_s(), attr):
            return attr.Get().ToExtString()
        return ""

    out: dict[str, list[cq.Shape]] = {}

    def walk(label, loc, path):
        if st.IsReference_s(label):
            ref = TDF_Label()
            st.GetReferredShape_s(label, ref)
            loc = loc.Multiplied(st.GetLocation_s(label))
            label = ref
        nm = name(label)
        kids = TDF_LabelSequence()
        if st.IsAssembly_s(label):
            st.GetComponents_s(label, kids)
            for i in range(1, kids.Length() + 1):
                walk(kids.Value(i), loc, path + [nm])
            return
        shape = cq.Shape.cast(st.GetShape_s(label).Moved(loc))
        if nm == "SHELL" and path and path[-1] == "PCB_ASM":
            return  # zero-thickness copy of the PCB top face, not a solid
        group = None
        for key in [nm] + path[::-1]:
            if key in GROUPS:
                group = GROUPS[key]
                break
        if group is None:
            bb = shape.BoundingBox()
            group = "misc_front" if bb.ymin > -3.0 else "misc_back"
        out.setdefault(group, []).append(shape)

    roots = TDF_LabelSequence()
    st.GetFreeShapes(roots)
    for i in range(1, roots.Length() + 1):
        walk(roots.Value(i), TopLoc_Location(), [])
    return out


def load_board(step_path: Path | str | None = None, verbose: bool = True) -> dict[str, cq.Shape]:
    """Return {group: compound} in the board frame, using the BREP cache."""
    step_path = Path(step_path or os.environ.get("LF_BOARD_STEP", DEFAULT_STEP))
    if not step_path.exists():
        raise FileNotFoundError(
            f"Board STEP not found at {step_path}.\n"
            "Download it as described in enclosure/README.md (Board model)."
        )
    cache = CACHE_DIR / _step_hash(step_path)
    marker = cache / "groups.txt"
    if marker.exists():
        groups = {}
        for g in marker.read_text().split():
            groups[g] = cq.Shape.importBrep(str(cache / f"{g}.brep"))
        return groups

    if verbose:
        print(f"Reading {step_path.name} (first run only, ~15 s)...")
    raw = _read_step_raw(step_path)
    pcb_back_y = min(s.BoundingBox().ymin for s in raw["pcb"])
    loc = step_to_board_frame(pcb_back_y)
    cache.mkdir(parents=True, exist_ok=True)
    groups = {}
    for g, shapes in raw.items():
        comp = cq.Compound.makeCompound(shapes).moved(loc)
        comp.exportBrep(str(cache / f"{g}.brep"))
        groups[g] = comp
    (cache / "pcb_back_y.txt").write_text(f"{pcb_back_y!r}\n")
    marker.write_text("\n".join(sorted(groups)))
    return groups


def board_solid(groups: dict[str, cq.Shape], include=None, exclude=()) -> cq.Compound:
    """All (or selected) board groups as one compound, e.g. for interference checks."""
    names = include or [g for g in groups if g not in exclude]
    return cq.Compound.makeCompound([groups[n] for n in names])
