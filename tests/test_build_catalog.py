#!/usr/bin/env python3
"""Test public OTA catalog generation without a firmware compile or GitHub API."""
import importlib.util
import json
from pathlib import Path
from tempfile import TemporaryDirectory

root=Path(__file__).resolve().parents[1]
spec=importlib.util.spec_from_file_location("catalog",root/"tools/publish_build_catalog.py")
module=importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)
with TemporaryDirectory() as d:
    folder=Path(d)/"beta/firmware"
    folder.mkdir(parents=True)
    image=bytearray(0x240)
    image[0]=0xE9
    image[0x30:0x30+len(b"0.8.4-alpha")]=b"0.8.4-alpha"
    image[0x50:0x50+len(b"lyricflash")]=b"lyricflash"
    (folder/"lyricflash-ota.bin").write_bytes(image)
    (folder.parent/"manifest.json").write_text(json.dumps({"version":"0.8.4-alpha"}))
    catalog=module.publish(Path(d),"")
    assert catalog["chipFamily"]=="ESP32-S3"
    assert len(catalog["builds"])==1
    assert catalog["builds"][0]["version"]=="0.8.4-alpha"
    assert catalog["builds"][0]["url"].endswith("/firmware/lyricflash-ota.bin")
    assert json.loads((folder.parent/"catalog.json").read_text())==catalog
    (folder.parent/"manifest.json").write_text(json.dumps({"version":"wrong"}))
    try:
        module.publish(Path(d),"")
        raise AssertionError("Version mismatch was not blocked")
    except ValueError:
        pass
print("PASS: catalog generator enforces app descriptor and manifest compatibility")
