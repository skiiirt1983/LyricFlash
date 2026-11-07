#!/usr/bin/env python3
"""Build compact, embedded LVGL numerals from upstream OFL Google Fonts.

This generates C source for the ESP32-S3, not embedded TTF archives. Only the
clock's digits, colon, space, hyphen, and AM/PM letters are included. Requires
the lv_font_conv CLI, installed on the build runner (see workflow).
"""
from __future__ import annotations
import hashlib
import json
import os
from pathlib import Path
import subprocess
import urllib.parse
import urllib.request

ROOT = Path(__file__).resolve().parents[1]
OUTPUT = ROOT / "main" / "generated_clock_fonts"
FONT_SOURCES = (
    ("kablammo", "Kablammo", "ofl/kablammo/Kablammo[MORF].ttf"),
    ("mystery_quest", "Mystery Quest", "ofl/mysteryquest/MysteryQuest-Regular.ttf"),
    ("moirai_one", "Moirai One", "ofl/moiraione/MoiraiOne-Regular.ttf"),
    ("rampart_one", "Rampart One", "ofl/rampartone/RampartOne-Regular.ttf"),
    ("hanalei", "Hanalei", "ofl/hanalei/Hanalei-Regular.ttf"),
    ("bitcount_grid_double", "Bitcount Grid Double", "ofl/bitcountgriddouble/BitcountGridDouble[CRSV,ELSH,ELXP,slnt,wght].ttf"),
    ("iceberg", "Iceberg", "ofl/iceberg/Iceberg-Regular.ttf"),
    ("montserrat", "Montserrat", "ofl/montserrat/Montserrat[wght].ttf"),
    ("sancreek", "Sancreek", "ofl/sancreek/Sancreek-Regular.ttf"),
    ("modak", "Modak", "ofl/modak/Modak-Regular.ttf"),
    ("alumni_sans_collegiate_one", "Alumni Sans Collegiate One", "ofl/alumnisanscollegiateone/AlumniSansCollegiateOne-Regular.ttf"),
    ("honk", "Honk", "ofl/honk/Honk[MORF,SHLN].ttf"),
    ("nabla", "Nabla", "ofl/nabla/Nabla[EDPT,EHLT].ttf"),
    ("carlito", "Carlito (Calibri-like)", "ofl/carlito/Carlito-Regular.ttf"),
    ("tinos", "Tinos (Times New Roman-like)", "ofl/tinos/Tinos-Regular.ttf"),
    ("asap", "Asap (Trebuchet-like)", "ofl/asap/Asap[wdth,wght].ttf"),
)
# A clock does not need the entire Unicode corpus. Keep OTA dual-slot safe.
GLYPHS = "0123456789: AMP-"
SIZE = 72
# Extra native sizes so the clock never needs LVGL transform_scale layers
# (a scaled widget is re-rendered into an offscreen ARGB buffer every frame).
EXTRA_SIZES = (40, 56, 96, 120)
BPP = 2


def download(relative_path: str, destination: Path) -> str:
    quoted = urllib.parse.quote(relative_path, safe="/")
    url = "https://raw.githubusercontent.com/google/fonts/main/" + quoted
    request = urllib.request.Request(url, headers={"User-Agent": "LyricFlash-Font-Builder/1"})
    with urllib.request.urlopen(request, timeout=90) as response:
        data = response.read()
    if not data:
        raise RuntimeError("Empty Google Fonts source: " + relative_path)
    destination.parent.mkdir(parents=True, exist_ok=True)
    destination.write_bytes(data)
    return hashlib.sha256(data).hexdigest()


def main() -> None:
    OUTPUT.mkdir(parents=True, exist_ok=True)
    source_dir = ROOT / "_font_sources"  # build-only, never packaged into OTA
    source_dir.mkdir(exist_ok=True)
    manifest = []
    for slug, family, source in FONT_SOURCES:
        temp = source_dir / Path(source).name
        sha = download(source, temp)
        font_name = "lf_clock_font_" + slug
        output = OUTPUT / (font_name + ".c")
        cmd = [
            "lv_font_conv", "--font", str(temp),
            "--symbols", GLYPHS, "--size", str(SIZE),
            "--bpp", str(BPP), "--format", "lvgl",
            "--lv-font-name", font_name, "--lv-include", "lvgl.h", "--no-compress",
            "-o", str(output),
        ]
        # Color fonts (Honk/Nabla) are flattened to grayscale alpha masks
        # by this converter; the original layered color effects cannot be
        # faithfully reproduced by a single 2-bpp LVGL glyph.
        if slug in ("honk", "nabla"):
            cmd.insert(-2, "--use-color-info")
        subprocess.run(cmd, check=True, cwd=ROOT)
        if not output.exists() or output.stat().st_size < 1000:
            raise RuntimeError("No usable embedded LVGL font output: " + family)
        extra = []
        for px in EXTRA_SIZES:
            sized_name = f"{font_name}_{px}"
            sized_out = OUTPUT / (sized_name + ".c")
            sized_cmd = [c for c in cmd]
            sized_cmd[sized_cmd.index("--size") + 1] = str(px)
            sized_cmd[sized_cmd.index("--lv-font-name") + 1] = sized_name
            sized_cmd[sized_cmd.index("-o") + 1] = str(sized_out)
            subprocess.run(sized_cmd, check=True, cwd=ROOT)
            if not sized_out.exists() or sized_out.stat().st_size < 1000:
                raise RuntimeError(f"No usable {px}px LVGL font output: {family}")
            extra.append(px)
        manifest.append({"family": family, "source": source, "extra_sizes": extra,
                         "sha256": sha, "size_bytes": output.stat().st_size,
                         "lvgl_name": font_name,
                         "license": "https://github.com/google/fonts/blob/main/" +
                                    source.rsplit("/", 1)[0] + "/OFL.txt"})
        print(f"FONT PASS: {family}: {output.stat().st_size} bytes source C", flush=True)
    (OUTPUT / "manifest.json").write_text(json.dumps(manifest, indent=2) + "\n")
    print(f"PASS: converted {len(manifest)} OFL Google Fonts for Clock Studio")


if __name__ == "__main__":
    main()
