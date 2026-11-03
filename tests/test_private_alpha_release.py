#!/usr/bin/env python3
"""Protect the explicit private 0.8.46 OTA release gate from regression."""
from pathlib import Path
p=Path(__file__).resolve().parents[1]
wf=(p/'.github/workflows/firmware.yml').read_text()
preparer=(p/'tools/prepare_site.py').read_text()
publisher=(p/'tools/publish_alpha_ota.py').read_text()
assert "feature/private-0.8.46-recording-visuals" in wf
assert "Run all host and native source tests before compiling" in wf
assert "contains(github.event.head_commit.message, '[compile-once]')" in wf
assert "needs: preflight" in wf
assert "contents: write" in wf
ordered=[
    "Compile LyricFlash for ESP32-S3",
    "Merge complete USB recovery image",
    "Validate OTA app and merged USB image",
    "Package and verify mandatory USB, OTA and checksummed ZIP",
    "Upload complete validated firmware artifacts",
    "Publish and verify direct HTTPS private alpha OTA image",
    "Publish verified private alpha as a GitHub prerelease",
]
positions=[wf.index(token) for token in ordered]
assert positions==sorted(positions), "Firmware validation/release order regressed"
for requirement in (
    "tools/check_build.py build/lyricflash.bin",
    "tools/validate_full_image.py build/lyricflash-full.bin build/lyricflash.bin",
    "tools/create_preserve_usb.py _site",
    "tools/package_release.py --verify _site",
    "tools/publish_alpha_ota.py _site/beta/firmware/lyricflash-ota.bin",
    "LyricFlash-0.8.46-alpha-USB-and-OTA.zip",
    "gh release create",
    "--prerelease --latest=false",
    "gh release view",
    "0.8.46-alpha",
):
    assert requirement in wf, requirement
assert "image_version!=version" in preparer, "The manifest must match the binary"
assert "lyricflash" in preparer and "LF_VERSION" in preparer
assert "hashlib.sha256" in publisher and "Direct OTA download verification failed" in publisher
print("PASS: private-alpha compile-once, complete USB/OTA release and publication gates")
