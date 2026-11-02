#!/usr/bin/env python3
"""Read-only ESP32-S3 application-only image preflight; never flashes hardware."""
import argparse
import hashlib
import json
import struct
from pathlib import Path


def expected_version():
    version_header = (Path(__file__).resolve().parents[1] / "main/version.h").read_text()
    for line in version_header.splitlines():
        if line.startswith('#define LF_VERSION "'):
            return line.split('"')[1]
    raise AssertionError("Missing firmware LF_VERSION")


def check(path):
    b = Path(path).read_bytes()
    assert 0x120 <= len(b) < 0x500000, f"Image exceeds 5 MB OTA slot or is too short: {len(b)}"
    assert b[0] == 0xE9, "Not an Espressif application image"
    chip = struct.unpack_from("<H", b, 12)[0]
    assert chip == 9, f"Wrong chip ID: {chip} (expected ESP32-S3=9)"
    assert b[0x20:0x24] == bytes.fromhex("3254cdab"), "Missing ESP-IDF app descriptor at 0x20"
    project = b[0x50:0x70].split(bytes([0]), 1)[0].decode("ascii", "strict")
    assert project == "lyricflash", f"Wrong project name: {project}"
    image_version = b[0x30:0x50].split(bytes([0]), 1)[0].decode("ascii", "strict")
    target_version = expected_version()
    assert image_version == target_version, (
        f"ESP-IDF image descriptor version {image_version} does not match "
        f"firmware version {target_version}"
    )
    return {
        "project": project,
        "version": image_version,
        "chip": "ESP32-S3",
        "bytes": len(b),
        "sha256": hashlib.sha256(b).hexdigest(),
        "file_type": "application-only OTA",
        "slot_limit_bytes": 0x500000,
        "cross_project_ota_verified": False,
        "hardware_tested": False,
    }


if __name__ == "__main__":
    ap = argparse.ArgumentParser()
    ap.add_argument("bin")
    ap.add_argument("--json", action="store_true")
    args = ap.parse_args()
    result = check(args.bin)
    if args.json:
        print(json.dumps(result, indent=2))
    else:
        print("PASS: build image header, identity and size checks:", json.dumps(result, indent=2))
