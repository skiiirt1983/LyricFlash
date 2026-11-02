#!/usr/bin/env python3
"""Carry forward verified published OTA builds into next Beta Pages deployment.

Past images are HTTPS-fetched from our own published Pages site in CI, not from
a device or local user's files. Failures never invent availability.
"""
import hashlib
import json
import pathlib
import re
import sys
import urllib.request

BASE = "https://skiiirt1983.github.io/LyricFlash/beta/"
PREFIX = BASE + "firmware/"
MAX_SIZE = 0x500000
NAME = re.compile(r"^0\.[0-9]{1,3}(?:\.[0-9]{1,3})?-alpha$")


def get(url, limit=MAX_SIZE):
    req = urllib.request.Request(url, headers={"User-Agent": "LyricFlash-Beta-Pages-CI"})
    with urllib.request.urlopen(req, timeout=20) as response:
        result = response.read(limit + 1)
    if len(result) > limit:
        raise ValueError("Oversized remote firmware")
    return result


def is_app(data):
    return (512 <= len(data) <= MAX_SIZE and data[0] == 0xE9 and
            data[12:14] == bytes([9, 0]) and
            data[0x20:0x24] == bytes.fromhex("3254cdab") and
            data[0x50:0x60].split(bytes([0]), 1)[0] == b"lyricflash")


def retain(site):
    archive = site / "beta/firmware/archive"
    archive.mkdir(parents=True, exist_ok=True)
    items = []
    try:
        old_manifest = json.loads(get(BASE + "manifest.json", 4096))
        version = old_manifest.get("version")
        if isinstance(version, str) and NAME.fullmatch(version):
            items.append((version, PREFIX + "lyricflash-ota.bin", None))
    except (OSError, ValueError, KeyError, TimeoutError) as exc:
        print("No previous Beta manifest available:", exc)
    try:
        past = json.loads(get(BASE + "updates.json", 10000))
        if past.get("channel") == "private-beta":
            for item in past.get("versions", []):
                if isinstance(item, dict) and item.get("ota") is True:
                    items.append((item.get("id"), item.get("url"), item.get("sha256")))
    except (OSError, ValueError, KeyError, TimeoutError) as exc:
        print("No previous history catalog:", exc)
    seen = set()
    for version, url, sha in items[:12]:
        if (not isinstance(version, str) or not NAME.fullmatch(version) or
                version in seen or not isinstance(url, str) or
                not url.startswith(PREFIX) or "?" in url or "#" in url or ".." in url):
            continue
        seen.add(version)
        try:
            data = get(url)
            if not is_app(data):
                raise ValueError("Not a compatible application-only ESP32-S3 image")
            if sha and hashlib.sha256(data).hexdigest() != sha:
                raise ValueError("SHA-256 mismatch")
            (archive / (version + ".bin")).write_bytes(data)
            print("Retained OTA build:", version, len(data), "bytes")
        except (OSError, ValueError, TimeoutError) as exc:
            print("Archived OTA build not available:", version, exc)


if __name__ == "__main__":
    retain(pathlib.Path(sys.argv[1]))
