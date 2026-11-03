#!/usr/bin/env python3
"""Dedicated Weather settings route, validated search selection and preservation."""
import ast
import subprocess
import tempfile
from pathlib import Path
root=Path(__file__).resolve().parents[1]
net=(root/"main/network.c").read_text()
weather=(root/"main/weather.c").read_text()
header=(root/"main/weather.h").read_text()
studio=(root/"main/web_device_html.h").read_text()
clock=(root/"main/web_basic_html.h").read_text()
page=(root/"main/web_weather_html.h").read_text()
assert 'lf_weather_settings_html' in net
for endpoint in ('"/weather"','"/api/weather"'):
    assert endpoint in net, endpoint
assert "cfg.max_uri_handlers=26" in net
assert 'handle_weather_post(httpd_req_t *req)' in net
assert 'lf_weather_save_config(&cfg)' in net
assert 'latitude_e5' in net and 'longitude_e5' in net
assert 'lat_e5' in weather and 'lon_e5' in weather and 'geofixed' in weather
assert 'selected->has_coordinates' in weather
assert 'fetch_weather(&c,' in weather
assert 'has_coordinates' in header
assert 'href=\"/weather\"' in studio.replace(r'\"','"')
assert 'href=\"/weather\"' in clock.replace(r'\"','"')
start=next(i for i,line in enumerate(page.splitlines()) if 'lf_weather_settings_html[] =' in line)+1
parts=[]
for line in page.splitlines()[start:]:
    v=line.strip()
    if not v: continue
    if v==";": break
    parts.append(ast.literal_eval(v))
html="".join(parts)
for token in ('aria-current="page"','geocoding-api.open-meteo.com',
              "encodeURIComponent(term)","Big White","latitude_e5",
              "longitude_e5","Save Weather","'/api/weather'"):
    assert token in html,token
assert "fetch('/api/recognize/start'" not in html
for selector in ("header .toolbar{display:flex!important",
                 "header .lf-wordmark{display:flex!important",
                 "header .lf-wordmark small{display:block!important",
                 "header .toolbar-links{display:flex!important",
                 "header .toolbar-links a{display:inline-flex!important",
                 "header .lf-mark{display:flex!important"):
    assert selector in html,selector
assert 'class="toolbar-links"' in html
assert 'class="lf-header-power"' in html

js=html.split("<script>",1)[1].split("</script>",1)[0]
with tempfile.TemporaryDirectory() as folder:
    target=Path(folder)/"weather.js"
    target.write_text(js)
    result=subprocess.run(["node","--check",str(target)],capture_output=True,text=True)
    assert result.returncode==0,result.stderr
print("PASS: dedicated weather page, exact geocoding, independent save and JS syntax")
