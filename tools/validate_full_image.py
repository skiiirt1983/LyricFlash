#!/usr/bin/env python3
"""Reject incorrect or incomplete merged USB images before publishing."""
from pathlib import Path
import sys

def validate(full: bytes, app: bytes):
    if not 0x120000 < len(full) <= 16 * 1024 * 1024:
        raise ValueError('Invalid merged image size')
    if not 4096 < len(app) <= 5 * 1024 * 1024:
        raise ValueError('Application does not fit OTA slot')
    if full[0] != 0xE9:
        raise ValueError('No bootloader at 0x0')
    if full[0x8000:0x8002] != b'\xaa\x50':
        raise ValueError('No partition table at 0x8000')
    if full[0x110000:0x110000+len(app)] != app:
        raise ValueError('App mismatch at OTA0 0x110000')
    if b'lyricflash' not in app[:512]:
        raise ValueError('Incorrect application identity')

if __name__ == '__main__':
    if len(sys.argv) != 3: raise SystemExit('usage: validate_full_image.py full.bin app.bin')
    validate(Path(sys.argv[1]).read_bytes(),Path(sys.argv[2]).read_bytes())
    print('Merged USB image checks passed')
