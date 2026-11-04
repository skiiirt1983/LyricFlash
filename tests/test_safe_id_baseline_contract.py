#!/usr/bin/env python3
"""Protect the user-verified 0.8.57 recognition baseline (2/2 songs identified).

Any unintentional modification to audio, ID, HTTP, LRCLIB, cache or sync
invalidates this gate. This 0.8.60 update is intentionally UI-only.
"""
from pathlib import Path
import hashlib
ROOT=Path(__file__).resolve().parents[1]
BASELINE={
    "main/lrclib_client.c": "0c55bd6ddcee6cb5e3b68e3b2ca6ce7bd1a18d80",
    "main/lrclib_client.h": "ce67f373d606660840bc8abc37516d7c9b9a279d",
    "main/lyric_sync.c": "1ae1e9fc20b14a4cad77054891204eb16b7a9bf5",
    "main/lyric_sync.h": "ad1b55f97731c5e81ea7003c78c746a64768159a",
    "main/mic_pcm.c": "275dd3324179e099a7a082563b291997848f53b5",
    "main/mic_pcm.h": "c184c32a9c58f14056f1a3b87d9926472e22507d",
    "main/network.c": "b960e73fcb4f800b5558d1e0ddb3ae1174883532",
    "main/recognition.c": "348c643f092ef70a9db0caa7ff926e3eb528607f",
    "main/recognition.h": "d228fa95ed6f3b266905d3db74d91148c535edad",
    "main/recognition_audio.c": "ee45dc0e2da70216a56812ca84a3ee17499e6f53",
    "main/recognition_audio.h": "8a757e7762ad8b76d3942f3b821f72fae7c36f79",
    "main/service_errors.c": "38506b900b7d199828bb9151dce2b3cb5f14a082",
    "main/service_errors.h": "0a444232b3c2310f236dc417a617fb725a59433d",
    "main/song_cache.c": "9fbc47f2b8311eb6a10d466638c18f1c9fe60bf4",
    "main/song_cache.h": "f933a4fce8cb0e08c0b71d6dee24ffa987f92aa8"
}
for path,expected in BASELINE.items():
    payload=(ROOT/path).read_bytes()
    git_blob=b"blob "+str(len(payload)).encode()+b"\0"+payload
    assert hashlib.sha1(git_blob).hexdigest()==expected, (
        f"Verified 0.8.57 recognition baseline was changed: {path}")
print(f"PASS: safe 0.8.57 recognition baseline intact ({len(BASELINE)} files)")
