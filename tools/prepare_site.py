#!/usr/bin/env python3
"""Publish installer + verified OTA catalog from actual CI build outputs."""
import hashlib
import json
import pathlib
import re
import sys

site = pathlib.Path(sys.argv[1])
full = site / "beta/firmware/lyricflash-full.bin"
ota = site / "beta/firmware/lyricflash-ota.bin"
if not full.is_file() or not ota.is_file():
    raise SystemExit("Missing firmware artifacts")
version_header = pathlib.Path("main/version.h").read_text()
match = re.search(r'^#define LF_VERSION "([^"]+)"$', version_header, re.MULTILINE)
if not match:
    raise SystemExit("Missing firmware version")
version = match.group(1)
m = {
    "name": "LyricFlash Alpha",
    "version": version,
    "builds": [{"chipFamily": "ESP32-S3", "parts": [
        {"path": "firmware/lyricflash-full.bin", "offset": 0}
    ]}]
}
(site / "beta/manifest.json").write_text(json.dumps(m, indent=2) + "\n")

PREFIX = "https://skiiirt1983.github.io/LyricFlash/beta/firmware/"
NAME = re.compile(r"^0\.[0-9]{1,3}(?:\.[0-9]{1,3})?-alpha$")


def item(ident, binary, path, label):
    data = binary.read_bytes()
    if len(data) < 512 or len(data) > 0x500000 or data[0] != 0xE9 or data[12:14] != bytes([9, 0]):
        raise ValueError("Invalid ESP32-S3 OTA image: " + str(binary))
    if data[0x20:0x24] != bytes.fromhex("3254cdab") or not data[0x50:0x60].startswith(b"lyricflash"):
        raise ValueError("Invalid LyricFlash firmware: " + str(binary))
    return {
        "id": ident, "label": label, "ota": True,
        "url": PREFIX + path,
        "bytes": len(data),
        "sha256": hashlib.sha256(data).hexdigest()
    }


releases = [item(version, ota, "lyricflash-ota.bin", "Alpha " + version.split("-")[0] + " -- NEW")]
archive = site / "beta/firmware/archive"
if archive.is_dir():
    for old in sorted(archive.glob("*.bin"), reverse=True):
        ident = old.stem
        if ident != version and NAME.fullmatch(ident):
            releases.append(item(ident, old, "archive/" + old.name,
                                 "Alpha " + ident.split("-")[0] + " (older build)"))
        if len(releases) >= 7:
            break

releases.append({
    "id": "factory-original",
    "label": "Original factory firmware (USB recovery only)",
    "ota": False,
    "restore_url": "https://skiiirt1983.github.io/LyricFlash/restore/"
})
catalog = {"schema": 1, "channel": "private-beta", "latest": version, "versions": releases}
(site / "beta/updates.json").write_text(json.dumps(catalog, indent=2) + "\n")
print("OTA catalog:", len(releases) - 1, "installable versions; factory recovery shown last")
