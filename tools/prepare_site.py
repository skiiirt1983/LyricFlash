#!/usr/bin/env python3
"""Only publish install manifests after verified build outputs exist."""
import json
import pathlib
import sys
site=pathlib.Path(sys.argv[1])
full=site/'beta/firmware/lyricflash-full.bin'
ota=site/'beta/firmware/lyricflash-ota.bin'
if not full.is_file() or not ota.is_file(): raise SystemExit('Missing firmware artifacts')
m={'name':'LyricFlash Alpha','version':'0.8.5-alpha','builds':[{'chipFamily':'ESP32-S3','parts':[{'path':'firmware/lyricflash-full.bin','offset':0}]}]}
(site/'beta/manifest.json').write_text(json.dumps(m,indent=2)+'\n')
