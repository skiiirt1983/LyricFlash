#!/usr/bin/env python3
"""Regression checks: every release ships independently verifiable USB recovery."""
import importlib.util
from pathlib import Path
from tempfile import TemporaryDirectory
import json

root = Path(__file__).resolve().parents[1]
workflow = (root / ".github/workflows/pages-and-firmware.yml").read_text()
assert "python3 tools/package_release.py _site" in workflow
assert "python3 tools/package_release.py --verify _site" in workflow
assert "LyricFlash-*.zip" in workflow
assert "if-no-files-found: error" in workflow

spec = importlib.util.spec_from_file_location("release_packager", root / "tools/package_release.py")
mod = importlib.util.module_from_spec(spec)
spec.loader.exec_module(mod)
with TemporaryDirectory() as temporary:
    site = Path(temporary)
    folder = site / "beta/firmware"
    folder.mkdir(parents=True)
    app = bytearray(0x220)
    app[0] = 0xE9
    app[12:14] = bytes([9, 0])
    app[0x20:0x24] = bytes.fromhex("3254cdab")
    app[0x30:0x30 + len(b"0.8.1-alpha")] = b"0.8.1-alpha"
    app[0x50:0x5a] = b"lyricflash"
    full = bytearray(0x120100)
    full[0] = 0xE9
    full[0x8000:0x8002] = b"\xaa\x50"
    full[mod.APP_OFFSET:] = app
    (folder/"lyricflash-full.bin").write_bytes(full)
    (folder/"lyricflash-ota.bin").write_bytes(app)
    (site/"beta/manifest.json").write_text(json.dumps({
        "version": "0.8.1-alpha", "builds": [{"chipFamily":"ESP32-S3",
        "parts":[{"path":"firmware/lyricflash-full.bin","offset":0}]}]
    }))
    mod.package(site)
    mod.verify(site)
    (folder/"lyricflash-full.bin").unlink()
    try:
        mod.verify(site)
        raise AssertionError("Missing USB firmware should have blocked release")
    except ValueError:
        pass
print("PASS: mandatory USB+OTA+ZIP release gating, including missing-file rejection")
