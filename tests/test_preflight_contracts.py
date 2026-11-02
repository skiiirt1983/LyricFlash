#!/usr/bin/env python3
"""Fast source-only release blockers. No ESP-IDF build or firmware generation.

Checks actual embedded browser JavaScript, links/routes, and source contracts.
This CANNOT replace the final ESP-IDF compile or real Waveshare testing.
"""
import ast
from html.parser import HTMLParser
from pathlib import Path
import re
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
def read(name):
    return (ROOT / name).read_text(encoding="utf-8")

network = read("main/network.c")
ui = read("main/ui.c")
weather = read("main/weather.c")
settings = read("main/settings.c")
web = read("main/web_device_html.h")
studio = read("main/web_basic_html.h")
cmake = read("main/CMakeLists.txt")
tests_passed = 0

def require(condition, message):
    global tests_passed
    if not condition:
        raise AssertionError("Preflight FAILED: " + message)
    tests_passed += 1

def embedded_html(source, symbol):
    lines = source.splitlines()
    line = next(i for i, s in enumerate(lines)
                if "static const char " + symbol + "[]" in s) + 1
    parts = []
    for value in lines[line:]:
        value = value.strip()
        if not value:
            continue
        if value == ";":
            return "".join(parts)
        require(value.startswith('"') and value.endswith('"'),
                "Malformed embedded C string in " + symbol)
        parts.append(ast.literal_eval(value))
    raise AssertionError("Missing C-string terminator: " + symbol)

class Inspector(HTMLParser):
    def __init__(self):
        super().__init__()
        self.ids = set()
        self.links = []
    def handle_starttag(self, tag, attrs):
        attrs = dict(attrs)
        if "id" in attrs:
            require(attrs["id"] not in self.ids,
                    "Duplicate HTML id " + attrs["id"])
            self.ids.add(attrs["id"])
        if tag == "a":
            self.links.append(attrs.get("href", ""))

pages = {
    "settings": embedded_html(web, "lf_device_settings_html"),
    "firmware": embedded_html(web, "lf_device_firmware_html"),
    "studio": embedded_html(studio, "lf_basic_settings_html"),
}
for name, html in pages.items():
    require("<html" in html and "</html>" in html,
            name + " HTML missing page wrapper")
    parser = Inspector()
    parser.feed(html)
    script = html.split("<script>", 1)[-1].split("</script>", 1)[0]
    require("<script>" in html and "</script>" in html, name + " missing JavaScript")
    ids_used = set(re.findall(
        r"(?:document\.getElementById|\bel)\(['\"]([^'\"]+)['\"]\)", script))
    require(not (ids_used - parser.ids),
            name + " JS references missing HTML IDs: " + str(ids_used-parser.ids))
    with tempfile.TemporaryDirectory() as directory:
        js_path = Path(directory) / (name + ".js")
        js_path.write_text(script, encoding="utf-8")
        result = subprocess.run(["node", "--check", str(js_path)],
                                capture_output=True, text=True)
        require(result.returncode == 0,
                name + " JavaScript syntax: " + result.stderr)
    if name != "studio":
        require("/" in parser.links and "/firmware" in parser.links,
                name + " missing Settings/Firmware navigation")

require("pin_matches" not in network and "X-LyricFlash-PIN" not in network,
        "PIN gate remains in firmware")
require("config_pin" not in studio and "lf_network_arm_ota" not in ui,
        "PIN/ARM control remains in UI")
require("HTTPD_409_CONFLICT" not in network,
        "Undefined ESP-IDF HTTP constant (use httpd_resp_set_status)")
# These are the HTTP server symbols verified against ESP-IDF v5.5.4.
allowed_httpd = {
    "HTTPD_RESP_USE_STRLEN", "HTTPD_500_INTERNAL_SERVER_ERROR",
    "HTTPD_400_BAD_REQUEST", "HTTPD_SOCK_ERR_TIMEOUT",
    "HTTPD_408_REQ_TIMEOUT", "HTTPD_403_FORBIDDEN", "HTTPD_DEFAULT_CONFIG",
}
used_httpd = set(re.findall(r"\bHTTPD_[A-Z0-9_]+\b", network))
require(not (used_httpd - allowed_httpd),
        "Unreviewed HTTPD symbols: " + str(used_httpd-allowed_httpd))
require("esp_https_ota" in cmake and "esp_https_ota_get_img_desc" in network
        and "esp_https_ota_finish" in network, "OTA API/dependency missing")
require("allowed_github_ota_url" in network and "handle_github_install" in network,
        "GitHub OTA URL authorization missing")
require('"/firmware"' in network and '"/api/install"' in network
        and '"/settings"' in network, "Expected device Web UI routes missing")
require('"/api/install"' in pages["firmware"]
        and "api.github.com/repos/skiiirt1983/LyricFlash/releases" in pages["firmware"],
        "GitHub build selector not linked to install endpoint")
require("lf_weather_save_config" in network and "xTaskNotifyGive(weather_task)" in weather,
        "Weather refresh on save missing")
require("if (schema < 6)" in settings and
        "0.003f + prefs->scroll_speed * 0.000605f" in ui,
        "Saved lyric speed migration or cap missing")
old_top = 18.0 + 1.3 * 35
new_top = 3.0 + 0.605 * 100
require(abs(old_top-new_top) < 0.00001,
        "New 100% lyric scroll cap does not equal old 35%")
require("lv_label_set_long_mode(s_text,LV_LABEL_LONG_CLIP)" in ui,
        "Clock settings dialog long label not protected")
require("page == 2" in ui and "case 7:" in ui,
        "Swipe back-navigation omits settings screens")
require("lf-mark" in pages["settings"] and "lf-mark" in pages["firmware"],
        "LyricFlash logo/header branding missing")
# This requirement is intentionally a release blocker until implemented.
require(bool(re.search(r'id=["\']clock-preview["\']|<canvas\b',
                       pages["studio"], flags=re.I)),
        "Clock Studio still lacks the agreed live preview")
print(f"PASS: {tests_passed} source-only LyricFlash preflight assertions")
