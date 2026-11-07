#!/usr/bin/env python3
"""Publish firmware manifests only for the validated current image version."""
import json
import pathlib
import re
import sys

root=pathlib.Path(__file__).resolve().parents[1]
site=pathlib.Path(sys.argv[1])
full=site/'beta/firmware/lyricflash-full.bin'
ota=site/'beta/firmware/lyricflash-ota.bin'
if not full.is_file() or not ota.is_file():
    raise SystemExit('Missing mandatory full USB and OTA firmware artifacts')
header=(root/'main/version.h').read_text()
match=re.search(r'^#define LF_VERSION "([^"]+)"', header, flags=re.MULTILINE)
if not match:
    raise SystemExit('Missing LF_VERSION in firmware header')
version=match.group(1)
data=ota.read_bytes()
if len(data)<0x120 or data[0]!=0xE9 or data[12:14]!=bytes([9,0]):
    raise SystemExit('Invalid ESP32-S3 OTA application image')
image_version=data[0x30:0x50].split(b'\0',1)[0].decode('ascii','strict')
project=data[0x50:0x70].split(b'\0',1)[0].decode('ascii','strict')
if image_version!=version or project!='lyricflash':
    raise SystemExit('Refusing manifest with wrong app version or project')
m={'name':'LyricFlash','version':version,'builds':[{'chipFamily':'ESP32-S3','parts':[{'path':'firmware/lyricflash-full.bin','offset':0}]}]}
(site/'beta/manifest.json').write_text(json.dumps(m,indent=2)+'\n')
