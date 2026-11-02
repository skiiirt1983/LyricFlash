#!/usr/bin/env python3
"""Alpha 0.8.13 OTA and USB release contract, host-side checks."""
from pathlib import Path

root = Path(__file__).resolve().parents[1]
net = (root / "main/network.c").read_text()
ui = (root / "main/web_device_html.h").read_text()
workflow = (root / ".github/workflows/pages-and-firmware.yml").read_text()
preparer = (root / "tools/prepare_site.py").read_text()
page = (root / "docs/beta/index.html").read_text()
main = (root / "main/main.c").read_text()

for token in (
    "ota_result", "ota_received",
    "esp_ota_begin(part,(size_t)req->content_len,&handle)",
    "esp_ota_abort(handle)", "esp_ota_end(handle)",
    "esp_ota_set_boot_partition(part)",
):
    assert token in net, f"Missing OTA regression guard: {token}"

for token in ("XMLHttpRequest", "xhr.upload.onprogress", "xhr.onerror",
              "xhr.ontimeout", "xhr.onload", "/api/install",
              "https://skiiirt1983.github.io/LyricFlash/beta/"):
    assert token in ui, f"Missing Web UI firmware update flow: {token}"
assert "pin_matches" not in net and "X-LyricFlash-PIN" not in net
assert "esp_https_ota_begin" in net and "esp_https_ota_finish" in net
assert "OTA_SIZE_UNKNOWN" not in net
assert "LyricFlash-Alpha-0.8.13-FULL-USB-and-OTA-UNTESTED" in workflow
assert "tools/publish_build_catalog.py _site" in workflow
assert "_site/beta/firmware/" in workflow
assert "lyricflash-full.bin" in workflow and "lyricflash-ota.bin" in workflow
assert "tools/validate_full_image.py" in workflow
assert "0.8.13-alpha" in preparer
assert "Alpha 0.8.13" in page
assert "LF_VERSION" in main
print("PASS: Alpha 0.8.13 OTA and full-USB packaging contract")

assert "tools/publish_alpha_ota.py" in workflow
assert "LyricFlash-OTA-Direct:" in workflow
assert "raw.githubusercontent.com/skiiirt1983/LyricFlash/alpha-ota/releases/" in ui
print("PASS: direct OTA host and release picker contract")

# Broken 0.8.11 firmware remains on-device with working Web UI.
# The recovery build must be DISCOVERABLE as a direct OTA update, not only an
# Actions artifact; do not accidentally skip alpha publication again.
assert "fix/0.8.13-touch-screen-lifecycle" in workflow, "Recovery branch not listed in build rules"
assert "Publish and verify direct HTTPS alpha OTA image" in workflow
assert "Publish validated development OTA as an alpha prerelease" in workflow
assert "github.ref == 'refs/heads/fix/0.8.13-touch-screen-lifecycle'" in workflow
assert "--prerelease --latest=false" in workflow
assert "tools/publish_alpha_ota.py _site/beta/firmware/lyricflash-ota.bin" in workflow
assert "gh release create" in workflow
assert "0.8.13-alpha" in (root / "main/version.h").read_text()
print("PASS: 0.8.13 recovery OTA selector will receive a published alpha release")
