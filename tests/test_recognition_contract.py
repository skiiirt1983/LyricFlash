#!/usr/bin/env python3
"""Pure-audio WAV and manual AudD recognition privacy/build contract."""
from pathlib import Path
import subprocess
import tempfile
p=Path(__file__).resolve().parents[1]
src=(p/"main/recognition.c").read_text()
aud=(p/"main/recognition_audio.c").read_text()
net=(p/"main/network.c").read_text()
html=(p/"main/web_device_html.h").read_text()
cm=(p/"main/CMakeLists.txt").read_text()
assert '"recognition.c"' in cm and '"recognition_audio.c"' in cm
for s in ('bsp_audio_codec_microphone_init','esp_codec_dev_open(codec,&fs)',
          'esp_codec_dev_set_in_channel_gain(codec,0x03,24.0f)',
          'esp_codec_dev_close(codec)','LF_SCAN_SECONDS','LF_SCAN_TDM_FRAMES',
          'esp_codec_dev_read(codec,tdm,sizeof tdm)',
          'heap_caps_malloc(LF_SCAN_WAV_BYTES,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT)',
          'lf_scan_tdm_to_stereo','lf_scan_wav_header',
          'esp_http_client_open(client,(int)body_len)',
          'esp_http_client_write(client','esp_http_client_fetch_headers(client)',
          'esp_http_client_read_response','esp_http_client_cleanup(client)',
          'esp_crt_bundle_attach','lf_recog_stop(void)',
          'clear_secret(wav,LF_SCAN_WAV_BYTES)',
          'lf_pcm_snapshot(&pcm)'):
    assert s in src,s
assert '.url="https://api.audd.io/"' in src
assert 'http://api.audd.io/' not in src
assert 'MALLOC_CAP_SPIRAM' in src
assert '"api_token"' in src and 'nvs_set_str' in src and 'nvs_erase_key' in src
assert 'lf_scan_token_valid' in aud
assert 'printf("%s",token)' not in src and 'ESP_LOG_BUFFER_HEX' not in src
assert 'audio_uploaded' in net and 'audio_saved' in net
for route in ('/api/recognize','/api/recognize/token',
              '/api/recognize/start','/api/recognize/stop'):
    assert route in net and route in html,route
assert 'cJSON_AddStringToObject(reply,"title",s.title)' in net
assert 'cJSON_AddStringToObject(reply,"artist",s.artist)' in net
assert 'cJSON_AddBoolToObject(reply,"token_configured",lf_recog_has_token())' in net
assert 'cJSON_AddStringToObject(reply,"api_token",' not in net
assert "if(!manual_audio_header(req))" in net
assert 'cfg.max_uri_handlers=22' in net
assert 'lf_recog_busy()' in net
assert "lf_recog_start()" in net
assert "lf_recog_stop()" in net
assert 'id=\"song-token\"' in html.replace(r'\"','"')
assert 'song-start' in html and 'song-stop' in html
assert 'if(!confirm(' in html and 'AudD' in html
with tempfile.TemporaryDirectory(prefix="lf_song_wav_") as tmp:
    out=Path(tmp)/"wav"
    subprocess.run(["cc","-std=c11","-Wall","-Wextra","-Werror","-pedantic",
        "-Imain","main/recognition_audio.c","tests/test_recognition_audio.c",
        "-o",str(out)],cwd=p,check=True)
    subprocess.run([str(out)],check=True)
print("PASS: manual AudD service, local key, WAV privacy and device Web UI source contracts")
