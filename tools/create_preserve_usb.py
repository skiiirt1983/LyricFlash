#!/usr/bin/env python3
"""Build a USB update manifest that preserves the existing LyricFlash NVS.

This is ONLY for a device already running LyricFlash with the unchanged 16MB
layout. Flash the application to OTA_0 and reset OTA selection metadata.
Unlike the merged recovery image, neither write touches the NVS partition
(0x9000..0x108FFF). The standalone FULL image remains mandatory for recovery. ESP Web Tools\nMUST present the erase-choice prompt if the device is treated as a new install;\nthe user must choose not to erase. This cannot be silently assumed.
"""
import json
import pathlib
import sys

NVS_BEGIN, NVS_END = 0x9000, 0x109000
OTA_DATA_BEGIN, OTA_DATA_END = 0x109000, 0x10B000
APP_BEGIN, APP_END = 0x110000, 0x610000

def create(site):
    site = pathlib.Path(site)
    app = site / "beta/firmware/lyricflash-ota.bin"
    if not app.is_file():
        raise ValueError("Cannot make Wi-Fi-preserving USB installer without OTA image")
    data = app.read_bytes()
    if not 512 <= len(data) <= APP_END-APP_BEGIN:
        raise ValueError("Invalid application size")
    if data[0] != 0xE9 or data[12:14] != bytes([9,0]) or data[0x50:0x60].split(b'\0',1)[0] != b'lyricflash':
        raise ValueError("Invalid LyricFlash ESP32-S3 application")
    ota_data = site / "beta/firmware/lyricflash-otadata-reset.bin"
    ota_data.write_bytes(b'\xff' * (OTA_DATA_END-OTA_DATA_BEGIN))
    version = data[0x30:0x50].split(b'\0',1)[0].decode("ascii")
    m = {
        "name": "LyricFlash Existing-Device Update (preserves Wi-Fi NVS)",
        "version": version,
        "new_install_prompt_erase": True,
        "builds": [{"chipFamily": "ESP32-S3", "parts": [
            {"path": "firmware/lyricflash-ota.bin", "offset": APP_BEGIN},
            {"path": "firmware/lyricflash-otadata-reset.bin", "offset": OTA_DATA_BEGIN},
        ]}]
    }
    (site/"beta/manifest-preserve.json").write_text(json.dumps(m,indent=2)+"\n")
    print("Wi-Fi-preserving LyricFlash USB manifest generated: does not write NVS")

if __name__=="__main__":
    if len(sys.argv)!=2: raise SystemExit("Usage: create_preserve_usb.py SITE")
    create(sys.argv[1])
