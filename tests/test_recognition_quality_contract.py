#!/usr/bin/env python3
"""Protect measured AudD capture quality diagnostics against regressions.

The metrics must describe the real 10-second upload, not a separate mic
probe. No raw audio samples or credential material belong in the status API.
"""
from pathlib import Path
p=Path(__file__).resolve().parents[1]
audio=(p/"main/recognition_audio.c").read_text()
header=(p/"main/recognition_audio.h").read_text()
recog=(p/"main/recognition.c").read_text()
snapshot=(p/"main/recognition.h").read_text()
net=(p/"main/network.c").read_text()
page=(p/"main/web_device_html.h").read_text()
for needle in ('const int32_t v1=tdm[i],v2=tdm[i+2]',
               'LF_SCAN_NEAR_RAIL 32760U',
               'm->mic1_sum_sq+=(uint64_t)a1*a1',
               'm->mic2_sum_sq+=(uint64_t)a2*a2',
               'm->mic1_rail_bursts++','m->mic2_rail_bursts++',
               'm->both_near_rail++'):
    assert needle in header+audio,needle
assert 'lf_scan_audio_metrics_add(&metrics,tdm,LF_SCAN_TDM_SAMPLES);' in recog
assert recog.index('lf_scan_tdm_to_stereo(wav+at') < recog.index(
    'lf_scan_audio_metrics_add(&metrics,tdm') < recog.index('frames+=LF_SCAN_TDM_FRAMES')
assert 'publish_capture_metrics(&metrics);metrics_published=true;' in recog
assert 'if(!metrics_published)publish_capture_metrics(&metrics);' in recog
assert 'lf_scan_audio_rms(m->mic1_sum_sq,m->frames)' in recog
assert 'lf_scan_audio_rms(m->mic2_sum_sq,m->frames)' in recog
for field in ('audio_measured_frames','mic1_rms','mic2_rms',
              'mic1_rms_without_rail','mic2_rms_without_rail',
              'mic1_peak','mic2_peak','mic1_near_rail','mic2_near_rail',
              'mic1_half_scale','mic2_half_scale',
              'mic1_rail_bursts','mic2_rail_bursts',
              'mic1_longest_rail_burst','mic2_longest_rail_burst',
              'both_near_rail_frames','audd_http_status',
              'audd_api_error_code','audd_transport_error_code'):
    assert 's.'+field in net or (field=='audd_http_status' and 's.http_status' in net),field
for field in ('mic1_rms','mic2_rms','audio_measured_frames',
              'audd_api_error_code','audd_transport_error_code'):
    assert field in snapshot and field in net,field
assert 'esp_codec_dev_set_in_channel_gain(codec,0x03,27.0f)' in recog
assert 'https://api.audd.io/' in recog and 'lf_recog_start(void)' in recog
assert 'lf_recog_has_token()' in recog
assert 'id=\\"maintenance-view\\"' in page
assert 'id=\\"maintenance-diagnostics\\" hidden' in page
assert 'id=\\"diagnostics\\"' in page and 'mic-pcm-result' in page
assert '/api/diagnostics' in page and 'AudD' in page
assert 'cJSON_AddStringToObject(reply,"api_token",' not in net
assert 'cJSON_AddStringToObject(reply,"wav",' not in net
print("PASS: 0.8.41 measured stereo AudD scan levels, HTTP status, privacy and Web UI")
