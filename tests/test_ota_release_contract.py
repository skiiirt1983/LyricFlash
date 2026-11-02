#!/usr/bin/env python3
"""Alpha 0.8.4 OTA and USB release contract, host-side checks."""
from pathlib import Path

root = Path(__file__).resolve().parents[1]
net = (root / "main/network.c").read_text()
workflow = (root / ".github/workflows/pages-and-firmware.yml").read_text()
preparer = (root / "tools/prepare_site.py").read_text()
page = (root / "docs/beta/index.html").read_text()
main = (root / "main/main.c").read_text()

for token in (
    "XMLHttpRequest", "xhr.upload.onprogress", "xhr.onerror",
    "xhr.ontimeout", "xhr.onload", "ota_result", "ota_received",
    "esp_ota_begin(part,(size_t)req->content_len,&handle)",
    "esp_ota_abort(handle)", "esp_ota_end(handle)",
    "esp_ota_set_boot_partition(part)",
):
    assert token in net, f"Missing OTA regression guard: {token}"

assert "OTA_SIZE_UNKNOWN" not in net
assert "LyricFlash-Alpha-0.8.4-FULL-USB-and-OTA-UNTESTED" in workflow
assert "_site/beta/firmware/" in workflow
assert "lyricflash-full.bin" in workflow and "lyricflash-ota.bin" in workflow
assert "tools/validate_full_image.py" in workflow
assert "0.8.4-alpha" in preparer
assert "Alpha 0.8.4" in page
assert "LF_VERSION" in main
print("PASS: Alpha 0.8.4 OTA and full-USB packaging contract")
