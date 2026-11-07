#!/usr/bin/env python3
"""0.8.83: LyricFlash project identity, responsive subtitle and creator credits."""
from pathlib import Path
root=Path(__file__).resolve().parents[1]
subtitle="MUSIC & CLOCK CONTROL"
old_title="Device"+" Studio"
legacy_agent="co"+"dex"
headers=("main/web_device_html.h","main/web_basic_html.h",
         "main/web_weather_html.h")
for filename in headers:
    src=(root/filename).read_text()
    expected=2 if "web_device_html" in filename else 1
    assert src.count("<small>"+subtitle+"</small>")==expected,filename
    assert src.count(r'<meta name=\"author\" content=\"skiiirt1983\">')==expected,filename
    assert "white-space:normal!important;letter-spacing:.06em!important" in src,filename
    assert 'href=\\\"/\\\"' in src,filename  # logo remains Home link
for filename in ("docs/index.html","docs/beta/index.html","docs/restore/index.html"):
    src=(root/filename).read_text()
    assert subtitle in src,filename
    assert '<meta name="author" content="skiiirt1983">' in src,filename
    assert "skiiirt1983" in src,filename
readme=(root/"README.md").read_text()
notices=(root/"THIRD_PARTY_NOTICES.md").read_text()
assert "**Creator and maintainer:** [skiiirt1983]" in readme
assert "**LyricFlash application creator and maintainer: skiiirt1983.**" in notices
assert "original creators, copyright notices, and licenses" in notices
assert 'LyricFlash creator and maintainer: skiiirt1983.' in (root/".github/workflows/firmware.yml").read_text()
assert "PrintSphere" in notices and "google/fonts" in notices
# Account/project ownership must never be expressed as ownership of upstream
# Waveshare board support code or the licensed open-source typeface assets.
for f in root.rglob("*"):
    if not f.is_file() or ".git" in f.parts or f.suffix in (".bin",".png",".ico",".jpg",".jpeg",".woff",".woff2"):
        continue
    try: content=f.read_text()
    except (UnicodeDecodeError, OSError): continue
    assert legacy_agent not in content.lower(),f
    assert old_title.lower() not in content.lower(),f
# Device Web UI release includes the earlier standby CPU power profile.
assert "lf_power_set_screen_awake(false);" in (root/"main/ui.c").read_text()
assert "lf_power_set_screen_awake(true);" in (root/"main/ui.c").read_text()
print("PASS: consistent LyricFlash subtitle/creator, no agent branding, third-party licenses retained, CPU policy unchanged")
