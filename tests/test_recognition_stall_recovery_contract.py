#!/usr/bin/env python3
"""0.8.61: no silent forever-IDENTIFYING overlay, with independent diagnostics.
Physical AMOLED and real AudD outcomes must still be validated on hardware.
"""
from pathlib import Path
root=Path(__file__).resolve().parents[1]
worker=(root/"main/recognition.c").read_text()
state=(root/"main/recognition.h").read_text()
ui=(root/"main/ui.c").read_text()
header=(root/"main/ui.h").read_text()
network=(root/"main/network.c").read_text()
version=(root/"main/version.h").read_text()
assert 'LF_VERSION "0.8.73-alpha"' in version
assert "LF_RECOG_UPLOAD_BUDGET_MS 60000ULL" in worker
assert "if(lf_recog_now_ms()>=deadline_ms)" in worker
assert "set_transport_error(ESP_ERR_TIMEOUT);" in worker
assert "post_write(client,wav,wav_bytes,deadline_ms)" in worker
assert "result.upload_bytes_sent+=(uint32_t)written;" in worker
assert "result.upload_progress_ms=lf_recog_now_ms();" in worker
assert "upload_progress_ms" in state
assert '"upload_progress_age_ms"' in network
assert "result.upload_bytes_total=(uint32_t)body_len;" in worker
assert "result.stage_started_ms=lf_recog_now_ms();" in worker
for name in ("stage_started_ms","upload_bytes_sent","upload_bytes_total"):
    assert name in state and name in network, name
for name in ("stage_age_ms","ui_tick_age_ms","ui_page","ui_navigation_phase",
             "lyrics_auto_open_queued","lyrics_auto_open_completed",
             "lyrics_auto_open_recoveries","upload_bytes_sent","upload_bytes_total"):
    assert f'"{name}"' in network, name
assert "lf_ui_display_stats(&live)" in network
assert "atomic_store(&ui_last_tick_ms,(uint32_t)current)" in ui
assert "atomic_store(&ui_last_page,(uint32_t)page)" in ui
assert "atomic_store(&ui_nav_phase,1U)" in ui
assert "atomic_store(&ui_nav_phase,3U)" in ui
assert "current-auto_open_requested_ms>=4000" in ui
assert "build_timed_lyrics_now(&ready);" in ui
assert "auto_open_fallback_attempt=ready.attempts;" in ui
assert "if(page==1 && timed_lyrics_mode && timed_line_count>0)" in ui
assert "auto_open_recovery_count" in header or "lyrics_auto_open_recoveries" in header
assert '"UPLOADING %u%%"' in ui
assert '"AWAITING SONG ID %us"' in ui
assert 'set_transport_phase("awaiting_headers");' in worker
assert 'set_transport_phase("reading_body");' in worker
assert '"transport_phase"' in network and '"firmware_version"' in network
assert "transport_phase" in state
assert "current-song->upload_progress_ms>30000U" in ui
assert "response_stalled" in ui
assert '"Recognition upload timed out. Tap Play to retry."' in worker
print("PASS: bounded AudD HTTP upload, progress and lock-free UI heartbeat, one-time lyrics rescue")
