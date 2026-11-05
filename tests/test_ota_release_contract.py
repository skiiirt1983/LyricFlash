#!/usr/bin/env python3
"""Alpha 0.8.41 OTA and USB release contract, host-side checks."""
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
assert "LyricFlash-Alpha-0.8.41-FULL-USB-and-OTA-UNTESTED" in workflow
assert "tools/publish_build_catalog.py _site" in workflow
assert "_site/beta/firmware/" in workflow
assert "lyricflash-full.bin" in workflow and "lyricflash-ota.bin" in workflow
assert "tools/validate_full_image.py" in workflow
assert "LF_VERSION" in preparer and "image_version" in preparer
assert "Alpha 0.8.41" in page
assert "LF_VERSION" in main
print("PASS: Alpha 0.8.41 OTA and full-USB packaging contract")

assert "tools/publish_alpha_ota.py" in workflow
assert "LyricFlash-OTA-Direct:" in workflow
assert "raw.githubusercontent.com/skiiirt1983/LyricFlash/alpha-ota/releases/" in ui
print("PASS: direct OTA host and release picker contract")

# An older version can be present on-device with working Web UI.
# The recovery build must be DISCOVERABLE as a direct OTA update, not only an
# Actions artifact; do not accidentally skip alpha publication again.
assert "feature/0.8.41-recognition-diagnostics" in workflow, "Recovery branch not listed in build rules"
assert "Publish and verify direct HTTPS alpha OTA image" in workflow
assert "Publish validated development OTA as an alpha prerelease" in workflow
assert "github.ref == 'refs/heads/feature/0.8.41-recognition-diagnostics'" in workflow
assert "--prerelease --latest=false" in workflow
assert "tools/publish_alpha_ota.py _site/beta/firmware/lyricflash-ota.bin" in workflow
assert "gh release create" in workflow
# The previously validated 0.8.39 release remains published.
# The 0.8.41 branch may publish a new OTA only after a validated compile-once gate.
active_version=(root / "main/version.h").read_text()
# The experimental private builds have separate ESP32-S3 release gates.
# A successful compile does not prove hardware operation or catalog publication.
assert any(f'"{ver}"' in active_version for ver in
           ("0.8.41-alpha","0.8.49-alpha","0.8.50-alpha","0.8.51-alpha","0.8.52-alpha","0.8.53-alpha","0.8.61-alpha","0.8.63-alpha","0.8.64-alpha","0.8.65-alpha","0.8.66-alpha","0.8.67-alpha","0.8.68-alpha","0.8.69-alpha","0.8.71-alpha","0.8.72-alpha","0.8.73-alpha","0.8.74-alpha","0.8.75-alpha","0.8.76-alpha","0.8.77-alpha","0.8.78-alpha","0.8.79-alpha","0.8.80-alpha","0.8.81-alpha","0.8.82-alpha","0.8.83-alpha","0.8.84-alpha","0.8.85-alpha")), "Unexpected alpha firmware version"
if any(f'"{v}-alpha"' in active_version for v in ("0.8.49","0.8.50","0.8.51","0.8.52","0.8.53","0.8.61","0.8.63","0.8.64","0.8.65","0.8.66","0.8.67","0.8.68","0.8.69","0.8.71","0.8.72","0.8.73","0.8.74","0.8.75","0.8.76","0.8.77","0.8.78","0.8.79","0.8.80","0.8.81","0.8.82","0.8.83","0.8.84","0.8.85")):
    private_workflow=(root / ".github/workflows/firmware.yml").read_text()
    is_85='"0.8.85-alpha"' in active_version
    is_84='"0.8.84-alpha"' in active_version
    is_83='"0.8.83-alpha"' in active_version
    is_82='"0.8.82-alpha"' in active_version
    is_81='"0.8.81-alpha"' in active_version
    is_80='"0.8.80-alpha"' in active_version
    is_79='"0.8.79-alpha"' in active_version
    is_78='"0.8.78-alpha"' in active_version
    is_77='"0.8.77-alpha"' in active_version
    is_76='"0.8.76-alpha"' in active_version
    is_75='"0.8.75-alpha"' in active_version
    is_74='"0.8.74-alpha"' in active_version
    is_73='"0.8.73-alpha"' in active_version
    is_72='"0.8.72-alpha"' in active_version
    is_71='"0.8.71-alpha"' in active_version
    is_69='"0.8.69-alpha"' in active_version
    is_68='"0.8.68-alpha"' in active_version
    is_67='"0.8.67-alpha"' in active_version
    is_66='"0.8.66-alpha"' in active_version
    is_65='"0.8.65-alpha"' in active_version
    is_64='"0.8.64-alpha"' in active_version
    is_63='"0.8.63-alpha"' in active_version
    is_61='"0.8.61-alpha"' in active_version
    is_53='"0.8.53-alpha"' in active_version
    is_52='"0.8.52-alpha"' in active_version
    is_51='"0.8.51-alpha"' in active_version
    is_50='"0.8.50-alpha"' in active_version
    version="0.8.85" if is_85 else "0.8.84" if is_84 else "0.8.83" if is_83 else "0.8.82" if is_82 else "0.8.81" if is_81 else "0.8.80" if is_80 else "0.8.79" if is_79 else "0.8.78" if is_78 else "0.8.77" if is_77 else "0.8.76" if is_76 else "0.8.75" if is_75 else "0.8.74" if is_74 else "0.8.73" if is_73 else "0.8.72" if is_72 else "0.8.71" if is_71 else "0.8.69" if is_69 else "0.8.68" if is_68 else "0.8.67" if is_67 else "0.8.66" if is_66 else "0.8.65" if is_65 else "0.8.64" if is_64 else "0.8.63" if is_63 else "0.8.61" if is_61 else "0.8.53" if is_53 else "0.8.52" if is_52 else "0.8.51" if is_51 else "0.8.50" if is_50 else "0.8.49"
    expected_branch=("feature/private-0.8.85-display-frame-profile" if is_85
                      else "feature/private-0.8.84-power-ui-audit" if is_84
                      else "feature/private-0.8.83-webui-project-identity" if is_83
                      else "feature/private-0.8.82-screen-off-cpu-power" if is_82
                      else "feature/private-0.8.81-frame-latched-page-motion" if is_81
                      else "feature/private-0.8.80-pager-waveform-performance" if is_80
                      else "feature/private-0.8.79-cancel-redraw-isolation" if is_79
                      else "feature/private-0.8.78-record-cancel-touch-guard" if is_78
                      else "feature/private-0.8.77-record-cancel-edge-background" if is_77
                      else "feature/private-0.8.76-restore-settings-home-swipe" if is_76
                      else "feature/private-0.8.75-corrected-pager-direction" if is_75
                      else "feature/private-0.8.74-pager-edge-wake-activity" if is_74
                      else "feature/private-0.8.73-device-settings-wake-guard" if is_73
                      else "feature/private-0.8.72-sleep-timer-with-wake-fix" if is_72
                      else "feature/private-0.8.71-wake-release-reliability" if is_71
                      else "feature/private-0.8.69-waveform-amplitude" if is_69
                      else "feature/private-0.8.68-ota-health-gate" if is_68
                      else "feature/private-0.8.67-touch-release-repaint" if is_67
                      else "feature/private-0.8.66-recovery-polish" if is_66
                      else "feature/private-0.8.63-device-clock-refinement" if (is_65 or is_64 or is_63)
                     else "feature/private-0.8.61-webui-clock-studio-database" if is_61
                     else "feature/private-0.8.53-lyric-timeline" if is_53
                     else "feature/private-0.8.52-upload-ui" if is_52
                     else "feature/private-0.8.51-lyrics-overlay" if is_51
                     else "feature/private-0.8.50-recognition-awake" if is_50
                     else "feature/private-0.8.49-watchdog-header")
    assert expected_branch in private_workflow
    assert f"LyricFlash-Private-Alpha-{version}-ESP32S3-UNTESTED" in private_workflow
    assert "tools/package_release.py --verify _site" in private_workflow
    assert "tools/publish_alpha_ota.py _site/beta/firmware/lyricflash-ota.bin" in private_workflow
    assert "gh release create" in private_workflow
    assert "--prerelease --latest=false" in private_workflow
    print(f"PASS: {version} private firmware artifacts and gated OTA publishing contract")
else:
    print("PASS: 0.8.41 manual alpha build is eligible for direct OTA publishing")
