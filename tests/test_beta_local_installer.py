#!/usr/bin/env python3
"""Guard the local-file USB installer against OTA-image selection and unsafe offsets."""
from pathlib import Path

root=Path(__file__).resolve().parents[1]
html=(root/'docs/beta/index.html').read_text()
script=(root/'docs/beta/local-flash.js').read_text()
assert 'local-flash.js' in html
assert 'id="localBin"' in html
assert 'id="local-install"' in html
assert "customElements.whenDefined" in script
assert "URL.createObjectURL(file)" in script
assert "URL.createObjectURL(new Blob(" in script
assert 'file.slice(' in script
assert 'partInfo(table)' in script
assert 'parts.ota_0' in script and 'parts.ota_1' in script
assert 'project !== expectedProject' in script
assert 'offset: APP_OFFSET' in script and 'offset: OTA_DATA_OFFSET' in script
assert 'file.slice(APP_OFFSET)' in script
assert 'new Uint8Array(OTA_DATA_SIZE).fill(0xff)' in script
assert 'new_install_prompt_erase: true' in script
assert 'offset: 0' not in script
assert 'KEEP DATA' in html and 'KEEP DATA' in script
assert 'chipFamily: "ESP32-S3"' in script
assert "No flash addresses" not in script
assert 'http://' not in script and 'https://' not in script
print("PASS: local .bin selection, partition/application preflight, and USB installer integration")
