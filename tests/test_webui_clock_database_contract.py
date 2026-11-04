#!/usr/bin/env python3
"""0.8.61 WebUI, clock customization and read-only song history contracts."""
import ast
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
def text(name):return (ROOT/name).read_text()
def page(name,which=0):
    ls=text(name).splitlines()
    starts=[i+1 for i,s in enumerate(ls) if 'static const char lf_' in s and 'html[] =' in s]
    assert len(starts)>which,(name,which)
    parts=[]
    for l in ls[starts[which]:]:
        if l.strip()==';':break
        if l.strip():parts.append(ast.literal_eval(l.strip()))
    return ''.join(parts)
basic=page("main/web_basic_html.h")
device=page("main/web_device_html.h")
firmware=page("main/web_device_html.h",1)
weather=page("main/web_weather_html.h")
for name,p in [("clock",basic),("device",device),("firmware",firmware),("weather",weather)]:
    assert '<a class="lf-wordmark" href="/"' in p,(name,"logo")
    assert 'class="lf-wifi-icon"' in p,(name,"wifi")
    assert 'href="/#maintenance"' in p,(name,"maintenance nav")
assert basic.index('id="face_show_battery"')<basic.index('id="clock-preview"')
assert basic.index('id="face_show_date"')<basic.index('id="clock-preview"')
assert basic.index('id="face_24h"')<basic.index('id="clock-preview"')
assert basic.index('id="face_show_seconds"')<basic.index('id="clock-preview"')
for name in ('face_show_percent','face_battery_size','face_battery_x','face_battery_y',
             'face_date_size','face_date_x','face_date_y'):
    assert f'id="{name}"' in basic,name
for name in ('Cabin Bold','Comic Neue','Noto Serif Display','Go Mono'):
    assert name in basic,name
assert 'families!==7' in basic
assert "'date',profile.date_x,profile.date_y" in basic
assert "if(f.show_percent)" in basic
assert "faceProfiles[activeFace][part+'_x']" in basic
assert "readProfile();updateFaceControls();syncBasicFromFace();render();" in basic
assert 'id="song-start"' not in device
assert 'id="song-stop"' not in device
assert 'id="song-result"' not in device
assert 'Open complete scan diagnostics' not in device
assert 'id="song-token"' in device and 'id="song-save"' in device
assert 'id="maintenance-view"' in device
assert 'id="maintenance-diagnostics" hidden' in device
assert 'id="maintenance-database" hidden' in device
assert 'id="database-results"' in device
assert '/api/song-database?offset=' in device
assert "name.textContent=song.title" in device
assert "detail.textContent=song.artist" in device
assert "el('maintenance-diagnostics').hidden=mode!=='diagnostics'" in device
assert "el('maintenance-database').hidden=mode!=='database'" in device
network=text("main/network.c")
settings=text("main/settings.c")
hdr=text("main/settings.h")
clock=text("main/clock_face.c")
basic_clock=text("main/basic_clock.c")
history=text("main/song_history.c")
worker=text("main/recognition.c")
assert 'LF_CONFIG_SCHEMA 14U' in hdr
for prop in ("face_battery_size[6]","face_show_percent[6]",
             "face_date_x[6]","face_date_y[6]","face_date_size[6]"):
    assert prop in hdr
for key in ('"f%ubs"','"f%upc"','"f%udx"','"f%udy"','"f%udz"'):
    assert key in settings,key
assert 'number_in_range(root,"date_size",0,200' in network
assert 'number_in_range(root,"battery_size",0,200' in network
assert 'handle_song_database(httpd_req_t *req)' in network
assert '{.uri="/api/song-database",.method=HTTP_GET,.handler=handle_song_database}' in network
assert "nvs_entry_find(" in history
assert "'L'" in history and "'H'" in history
assert "nvs_erase" not in history and "nvs_flash_erase" not in history
assert "lf_song_history_remember(known.title,known.artist);" in text("main/ui.c")
assert 'LF_RECOG_MATCH' in worker
assert 'face_show_percent[current_style]' in clock
assert 'face_date_size[current_style]' in clock
assert 'face_battery_size[current_style]' in clock
assert "case 20:return 3;" in basic_clock
assert "case 23:return 6;" in basic_clock
print("PASS: responsive clock controls, 7 families, private manual-ID setup and read-only song database")
