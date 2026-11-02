#!/usr/bin/env python3
"""Mandatory LyricFlash release gate: USB + OTA + manifest + digest + ZIP.

CI MUST run this *after* ESP-IDF's app and merged-image validation and BEFORE
publishing any alpha, beta or stable firmware. An incomplete release fails.
"""
import hashlib
import json
import pathlib
import sys
import zipfile

APP_OFFSET = 0x110000
CHIP_ID = bytes([9, 0])
APP_DESC_MAGIC = bytes.fromhex("3254cdab")
ZIP_MEMBERS = [
    "firmware/lyricflash-full.bin",
    "firmware/lyricflash-ota.bin",
    "firmware/lyricflash-otadata-reset.bin",
    "manifest-preserve.json",
    "firmware/SHA256SUMS.txt",
    "firmware/USB-INSTALL.txt",
    "manifest.json",
]


def validate(site):
    site = pathlib.Path(site)
    root = site / "beta"
    full_path = root / ZIP_MEMBERS[0]
    ota_path = root / ZIP_MEMBERS[1]
    manifest_path = root / "manifest.json"

    for path in (full_path, ota_path, manifest_path):
        if not path.is_file() or path.stat().st_size == 0:
            raise ValueError(f"Release BLOCKED: required asset missing/empty: {path}")

    full, ota = full_path.read_bytes(), ota_path.read_bytes()
    if not (0x120000 < len(full) <= 16 * 1024 * 1024):
        raise ValueError("Release BLOCKED: bad full USB image length")
    if not (0x120 < len(ota) <= 0x500000):
        raise ValueError("Release BLOCKED: OTA does not fit inactive 5MB slot")
    if (full[0] != 0xE9 or full[0x8000:0x8002] != b"\xaa\x50"
            or full[APP_OFFSET:APP_OFFSET + len(ota)] != ota):
        raise ValueError("Release BLOCKED: full USB image is missing bootloader, partitions or matching OTA app")
    if ota[0] != 0xE9 or ota[12:14] != CHIP_ID or ota[0x20:0x24] != APP_DESC_MAGIC:
        raise ValueError("Release BLOCKED: invalid application-only ESP32-S3 OTA image")
    if ota[0x50:0x70].split(b"\x00", 1)[0] != b"lyricflash":
        raise ValueError("Release BLOCKED: wrong application project")
    embedded_version = ota[0x30:0x50].split(b"\x00", 1)[0].decode("ascii", "strict")
    m = json.loads(manifest_path.read_text())
    if m.get("version") != embedded_version:
        raise ValueError(f"Release BLOCKED: manifest version != binary version ({embedded_version})")
    if not any(
        build.get("chipFamily") == "ESP32-S3" and
        any(part.get("offset") == 0 and part.get("path") == "firmware/lyricflash-full.bin"
            for part in build.get("parts", []))
        for build in m.get("builds", [])
    ):
        raise ValueError("Release BLOCKED: no complete USB image mapped to offset 0x0")

    preserve_path = root / "manifest-preserve.json"
    reset_path = root / "firmware/lyricflash-otadata-reset.bin"
    if not preserve_path.is_file() or not reset_path.is_file():
        raise ValueError("Release BLOCKED: Wi-Fi-preserving USB installation assets missing")
    reset_bytes = reset_path.read_bytes()
    if len(reset_bytes) != 0x2000 or any(v != 0xff for v in reset_bytes):
        raise ValueError("Release BLOCKED: OTA selection reset image malformed")
    preserve = json.loads(preserve_path.read_text())
    if preserve.get("new_install_prompt_erase") is not True:
        raise ValueError("Release BLOCKED: USB updater must offer explicit erase-or-keep choice")
    if preserve.get("version") != embedded_version:
        raise ValueError("Release BLOCKED: preserve-NVS manifest version mismatch")
    builds = preserve.get("builds", [])
    required = {(0x110000, "firmware/lyricflash-ota.bin"),
                (0x109000, "firmware/lyricflash-otadata-reset.bin")}
    selected = []
    for build in builds:
        if build.get("chipFamily") == "ESP32-S3":
            selected.extend((part.get("offset"), part.get("path"))
                            for part in build.get("parts", []))
    if set(selected) != required or len(selected) != len(required):
        raise ValueError("Release BLOCKED: Wi-Fi-preserving installer must write only app + OTA selection")
    for start, name in selected:
        length = len(ota) if "ota.bin" in name else len(reset_bytes)
        if start < 0x109000 or start + length > 0x610000:
            raise ValueError("Release BLOCKED: preserve-NVS update writes outside OTA partitions")
        if start < 0x109000 and start + length > 0x9000:
            raise ValueError("Release BLOCKED: preserve-NVS update overlaps saved Wi-Fi")
    return embedded_version, full_path, ota_path


def package(site):
    site = pathlib.Path(site)
    version, full, ota = validate(site)
    folder = site / "beta/firmware"
    sums_path = folder / "SHA256SUMS.txt"
    sums_path.write_text(
        f"{hashlib.sha256(full.read_bytes()).hexdigest()}  lyricflash-full.bin\n"
        f"{hashlib.sha256(ota.read_bytes()).hexdigest()}  lyricflash-ota.bin\n"
    )
    (folder / "USB-INSTALL.txt").write_text(
        f"LyricFlash {version} — experimental firmware\n"
        "USB: flash firmware/lyricflash-full.bin at address 0x0 to the "
        "Waveshare ESP32-S3 16 MB device.\n"
        "OTA: use firmware/lyricflash-ota.bin only in an already authorized LyricFlash updater.\n"
        "Do NOT flash the OTA-only image at 0x0.\n"
        "Use manifest-preserve.json for an existing LyricFlash installation.\n"\n        "IMPORTANT: Choose KEEP DATA if the browser offers an erase prompt.\n"\n        "If KEEP is unavailable, cancel USB installation; it may otherwise wipe NVS.\n"
        "This writes the app at 0x110000 and OTA-data reset at 0x109000, not the NVS region.\n"
        "Use the full recovery image only if the preserve installation fails.\n"
        "The complete USB image overwrites existing NVS and OTA slot metadata, "
        "resetting saved Wi-Fi/device settings. No extra full-chip erase is needed.\n"
    )
    archive = folder / f"LyricFlash-{version}-USB-and-OTA.zip"
    with zipfile.ZipFile(archive, "w", compression=zipfile.ZIP_DEFLATED,
                         compresslevel=6) as z:
        for name in ZIP_MEMBERS:
            path = site / "beta" / name
            if not path.is_file():
                raise ValueError(f"Release BLOCKED: missing ZIP member: {name}")
            z.write(path, name)
    verify(site, archive)
    print(f"PASS: mandatory USB+OTA release package {archive} "
          f"(version={version}, {archive.stat().st_size} bytes)")


def verify(site, archive=None):
    site = pathlib.Path(site)
    version, full, ota = validate(site)
    folder = site / "beta/firmware"
    sums_path = folder / "SHA256SUMS.txt"
    if not sums_path.is_file():
        raise ValueError("Release BLOCKED: missing SHA256SUMS.txt")
    expected = {
        "lyricflash-full.bin": hashlib.sha256(full.read_bytes()).hexdigest(),
        "lyricflash-ota.bin": hashlib.sha256(ota.read_bytes()).hexdigest(),
    }
    hashes = {}
    for line in sums_path.read_text().splitlines():
        digest, name = line.split(maxsplit=1)
        hashes[name.strip()] = digest
    if hashes != expected:
        raise ValueError("Release BLOCKED: ZIP checksum manifest does not match firmware")
    if archive is None:
        archive = folder / f"LyricFlash-{version}-USB-and-OTA.zip"
    if not archive.is_file():
        raise ValueError("Release BLOCKED: complete USB+OTA download ZIP missing")
    with zipfile.ZipFile(archive) as z:
        if sorted(z.namelist()) != sorted(ZIP_MEMBERS) or z.testzip() is not None:
            raise ValueError("Release BLOCKED: ZIP missing a required file or has bad CRC")
        for name in ZIP_MEMBERS:
            data = (site / "beta" / name).read_bytes()
            if z.read(name) != data:
                raise ValueError(f"Release BLOCKED: ZIP member differs: {name}")
    print(f"PASS: checksums, full USB image, OTA image, manifest and ZIP verified for {version}")


if __name__ == "__main__":
    if len(sys.argv) not in (2, 3) or (len(sys.argv) == 3 and sys.argv[1] != "--verify"):
        raise SystemExit("Usage: package_release.py [--verify] SITE_DIRECTORY")
    if len(sys.argv) == 3:
        verify(sys.argv[2])
    else:
        package(sys.argv[1])
