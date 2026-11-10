"""Print pack: STLs in a folder, one Bambu 3MF with one plate per print, the checklist; zipped.

Run after phase2.py (uses out/phase2/stl/).
"""
import shutil
import sys
import zipfile
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))

import trimesh  # noqa: E402

from lfcase.export import write_3mf_plates  # noqa: E402

STL = HERE / "out" / "phase2" / "stl"
OUT = HERE / "out" / "print_pack"

FILES = {  # file in the pack: (source stl, plate name, material)
    "top_shell.stl": ("top_shell.stl", "Top shell (gold)", "gold"),
    "base_ribbed.stl": ("base.stl", "Base - ribbed", "black"),
    "base_panels.stl": ("base_panels.stl", "Base - panels", "black"),
    "base_smooth.stl": ("base_smooth.stl", "Base - smooth", "black"),
    "latch_clip_front.stl": ("clip_front.stl", "Latch clips (PETG)", "black"),
    "latch_clip_back.stl": ("clip_back.stl", "Latch clips (PETG)", "black"),
}


def main():
    if OUT.exists():
        shutil.rmtree(OUT)
    (OUT / "stl").mkdir(parents=True)
    meshes = {}
    for name, (src, plate, mat) in FILES.items():
        shutil.copy(STL / src, OUT / "stl" / name)
        meshes[name] = dict(name=name[:-4], mesh=trimesh.load(STL / src), material=mat, plate_name=plate)
    plates = [[meshes["top_shell.stl"]], [meshes["base_ribbed.stl"]], [meshes["base_panels.stl"]],
              [meshes["base_smooth.stl"]], [meshes["latch_clip_front.stl"], meshes["latch_clip_back.stl"]]]
    write_3mf_plates(plates, OUT / "lyricflash_print_plates.3mf")
    # fallback: one 3MF per plate (works whatever Bambu does with multi-plate files)
    (OUT / "3mf_per_plate").mkdir()
    for i, pl in enumerate(plates, 1):
        nm = pl[0]["plate_name"].split(" (")[0].lower().replace(" - ", "_").replace(" ", "_")
        write_3mf_plates([pl], OUT / "3mf_per_plate" / f"plate{i}_{nm}.3mf", bambu_app=False)   # the proven format
    shutil.copy(HERE / "PHASE2_CHECKLIST.md", OUT / "PHASE2_CHECKLIST.md")
    (OUT / "README.txt").write_text(
        "LyricFlash case - print pack\n\n"
        "lyricflash_print_plates.3mf: one Bambu Studio project, one plate per print\n"
        "(if it opens as a single plate, use the files in 3mf_per_plate/ instead):\n"
        "  1 top shell (filament 1, Iridium Gold PLA Metal), face-down\n"
        "  2 base - ribbed, 3 base - panels, 4 base - smooth (filament 2, black PETG): print ONE of the three\n"
        "  5 the two latch clips (filament 2, PETG: they flex)\n"
        "stl/: the same parts as separate STLs, already in print orientation.\n\n"
        "Base: supports off, except paint a support enforcer on the top edge of the USB-C opening.\n"
        "Everything else: supports off. 0.2 mm layers, 4 walls, 15 % gyroid.\n"
        "Assembly steps and what to check: PHASE2_CHECKLIST.md.\n")
    zp = HERE / "out" / "lyricflash_print_pack.zip"
    with zipfile.ZipFile(zp, "w", zipfile.ZIP_DEFLATED) as z:
        for f in sorted(OUT.rglob("*")):
            if f.is_file():
                z.write(f, Path("lyricflash_print_pack") / f.relative_to(OUT))
    print(zp, zp.stat().st_size // 1024, "KB")


if __name__ == "__main__":
    main()
