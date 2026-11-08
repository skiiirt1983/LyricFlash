#!/usr/bin/env python3
"""Protect the 1.0 build and release workflow from regression."""
from pathlib import Path
p=Path(__file__).resolve().parents[1]
wf=(p/'.github/workflows/firmware.yml').read_text()
preparer=(p/'tools/prepare_site.py').read_text()
publisher=(p/'tools/publish_alpha_ota.py').read_text()
assert "      - main\n" in wf
assert "Run all host and native source tests before compiling" in wf
assert "github.ref == 'refs/heads/main'" in wf
assert "needs: preflight" in wf
assert "contents: write" in wf
ordered=[
    "Generate all 16 open-source embedded fonts before ESP-IDF compilation",
    "Verify complete generated font manifest",
    "Compile LyricFlash for ESP32-S3",
    "Merge complete USB recovery image",
    "Validate OTA app and merged USB image",
    "Package and verify mandatory USB, OTA and checksummed ZIP",
    "Upload complete validated firmware artifacts",
    "Publish and verify direct HTTPS OTA image",
    "Publish the build for on-device updates",
    "Publish the stable release",
]
positions=[wf.index(token) for token in ordered]
assert positions==sorted(positions), "Firmware validation/release order regressed"
for requirement in (
    "python3 tools/generate_clock_fonts.py",
    "tools/check_build.py build/lyricflash.bin",
    "tools/validate_full_image.py build/lyricflash-full.bin build/lyricflash.bin",
    "tools/create_preserve_usb.py _site",
    "tools/package_release.py --verify _site",
    "tools/publish_alpha_ota.py _site/beta/firmware/lyricflash-ota.bin",
    "LyricFlash-1.0.4-USB-and-OTA.zip",
    "gh release create",
    "--prerelease --latest=false",
    "gh release view",
    "1.0.4",
    'gh release create "v$VERSION"',
    "--latest\n",
):
    assert requirement in wf, requirement
assert "image_version!=version" in preparer, "The manifest must match the binary"
assert "lyricflash" in preparer and "LF_VERSION" in preparer
assert "hashlib.sha256" in publisher and "Direct OTA download verification failed" in publisher
# Recovery safeguard: a build may add a new SHA-specific OTA binary but must
# never replace or delete an earlier release's image.
assert 'TAG="alpha-build-${GITHUB_SHA:0:12}"' in wf
assert 'BRANCH = "alpha-ota"' in publisher
assert 'if request("GET",key+"?ref="+BRANCH,token) is None:' in publisher
assert 'releases/"+tag+"/lyricflash-ota.bin' in publisher
print("PASS: main-branch build, complete USB/OTA release and publication gates")
