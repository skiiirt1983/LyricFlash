#!/usr/bin/env python3
"""Check LRCLIB is lookup-only, safe, bounded, and tied to a manual AudD scan."""
from pathlib import Path
import subprocess
import tempfile
p=Path(__file__).resolve().parents[1]
main=(p/"main/recognition.c").read_text()
client=(p/"main/lrclib_client.c").read_text()
net=(p/"main/network.c").read_text()
ui=(p/"main/web_device_html.h").read_text()
cm=(p/"main/CMakeLists.txt").read_text()
h=(p/"main/recognition.h").read_text()
for s in ('"lrclib_client.c"','"lrclib_match.c"','"recognition.c"'):
 assert s in cm,s
for s in ('https://lrclib.net/api/get?track_name=','&artist_name=',
          '.crt_bundle_attach=esp_crt_bundle_attach',
          '.user_agent=client_user_agent','LF_LRCLIB_JSON_MAX (65536U)',
          'if(status==429){result=LF_LRCLIB_RATE_LIMIT;',
          'if(status==404){result=LF_LRCLIB_NOT_FOUND;',
          'vTaskDelay(pdMS_TO_TICKS(300))',
          'lf_parse_lrc(synced->valuestring,out)',
          'LF_LRCLIB_INSTRUMENTAL','LF_LRCLIB_PLAIN',
          'lf_lrclib_search_title','lf_lrclib_url_encode',
          'heap_caps_calloc','esp_http_client_cleanup(client)'):
 assert s in client,s
assert 'http://lrclib.net' not in client
assert 'esp_http_client_set_method(client,HTTP_METHOD_POST)' not in client
assert "LF_LRCLIB_RATE_LIMIT" in main
assert 'lf_lrclib_lookup(' in main
assert 'lf_recog_lyric_line' in h and 'lf_recog_lyric_line' in main
assert 'lf_recog_lyric_line(i,&line)' in net
assert 'cJSON_AddStringToObject(reply,"lyrics_state",' in net
assert 'cJSON_AddNumberToObject(reply,"lyric_lines",s.lyric_lines);' in net
assert 'lf_recog_busy(void)' in main and 'atomic_load(&worker_active)' in main
assert 'atomic_compare_exchange_strong(&worker_active' in main
assert 'MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT' in main
assert 'if(ok && !atomic_load(&stop_flag))lookup_lyrics_after_match();' in main
assert 'LF_RECOG_LYRICS' in h and 'finding_lyrics' in main
assert 'id=\\"song-token\\"' in ui and 'id=\\"maintenance-view\\"' in ui
assert 'lrclib' in client.lower() and 'lf_recog_lyric_line' in main
assert "Checked again. No newer validated version" not in ui
assert 'Some sources unavailable:' not in ui
assert 'checkedAtLeastOneSource' in ui
with tempfile.TemporaryDirectory(prefix="lrclib_native_") as td:
 binary=Path(td)/"test"
 subprocess.run(["cc","-std=c11","-Wall","-Wextra","-Werror",
                  "-pedantic","-Imain","main/lrclib_match.c",
                  "tests/test_lrclib_match.c","-o",str(binary)],cwd=p,check=True)
 subprocess.run([str(binary)],check=True)
print("PASS: LRCLIB manual follow-up, TLS, timestamp parser, bounded PSRAM, no quota retries and friendly OTA catalog status")
