#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
python3 tests/test_partitions.py
python3 tests/test_static_safety.py
python3 tests/test_ota_health_gate_contract.py
cc -std=c11 -Wall -Wextra -Werror -pedantic -Imain main/ota_health_logic.c tests/test_ota_health_logic.c -o /tmp/lyricflash_test_ota_health
/tmp/lyricflash_test_ota_health
rm -f /tmp/lyricflash_test_ota_health
python3 tests/test_preflight_contracts.py
python3 tests/test_touch_release_blackout_contract.py
python3 tests/test_wake_release_lifecycle_contract.py
python3 tests/test_wake_recovery_watchdog_contract.py
python3 tests/test_wake_timer_power_contract.py
python3 tests/test_screen_off_cpu_power_contract.py
python3 tests/test_wifi_sleep_grace_contract.py
python3 tests/test_display_pipeline_profile_contract.py
python3 tests/test_clock_raster_contract.py
python3 tests/test_clock_draw_clip_contract.py
python3 tests/test_device_settings_wake_while_scrolling_contract.py
python3 tests/test_pager_edge_timeout_contract.py
python3 tests/test_safe_id_baseline_contract.py
python3 tests/test_home_song_contract.py
python3 tests/test_recording_visual_contract.py
python3 tests/test_waveform_amplitude_contract.py
python3 tests/test_ui_render_budget_contract.py
python3 tests/test_frame_latched_navigation_contract.py
python3 tests/test_clock_back_origin_contract.py
python3 tests/test_pager_midpoint_render_contract.py
python3 tests/test_recognition_stall_recovery_contract.py
python3 tests/test_ui_freeze_failsafe_contract.py
python3 tests/test_lyrics_layout_contract.py
python3 tests/test_lyric_timeline_contract.py
python3 tests/test_song_cache_contract.py
python3 tests/test_touch_safe_lyrics_panel.py
python3 tests/test_cancel_recovery_contract.py
python3 tests/test_timing_touch_feedback_contract.py
python3 tests/test_lyrics_overlay_contract.py
python3 tests/test_upload_brand_navigation_contract.py
python3 tests/test_weather_studio_contract.py
python3 tests/test_clock_layout_contract.py
python3 tests/test_open_font_contract.py
python3 tests/test_mic_probe_contract.py
python3 tests/test_mic_pcm_contract.py
python3 tests/test_recognition_contract.py
python3 tests/test_recognition_quality_contract.py
python3 tests/test_lrclib_contract.py
python3 tests/test_ota_release_contract.py
python3 tests/test_private_alpha_release.py
python3 tests/test_build_catalog.py
python3 tests/test_release_package.py
python3 tests/test_alpha_0_8.py
python3 tests/test_web_clock_studio.py
python3 tests/test_webui_clock_database_contract.py
python3 tests/test_diagnostic_log_viewer_contract.py
python3 tests/test_project_identity_contract.py
node tests/test_clock_preview_runtime.js
node tests/test_firmware_picker_runtime.js
python3 tests/test_beta_local_installer.py
node --check docs/beta/local-flash.js
cc -std=c11 -Wall -Wextra -Werror -pedantic -Imain main/lyric_sync.c tests/test_lyric_sync.c -o /tmp/lyricflash_test_lyric_sync
/tmp/lyricflash_test_lyric_sync
rm -f /tmp/lyricflash_test_lyric_sync
cc -std=c11 -Wall -Wextra -Werror -pedantic -Imain main/lyric_clock.c tests/test_lyric_clock.c -o /tmp/lyricflash_test_lyric_clock
/tmp/lyricflash_test_lyric_clock
cc -std=c11 -Wall -Wextra -Werror -pedantic -Imain main/lyrics.c tests/test_lyrics.c -o /tmp/lyricflash_test_lyrics
/tmp/lyricflash_test_lyrics
cc -std=c11 -Wall -Wextra -Werror -pedantic -Imain main/image_policy.c tests/test_image_policy.c -o /tmp/lyricflash_test_image
/tmp/lyricflash_test_image
cc -std=c11 -Wall -Wextra -Werror -pedantic -Imain main/service_errors.c tests/test_service_errors.c -o /tmp/lyricflash_test_api
/tmp/lyricflash_test_api
cc -std=c11 -Wall -Wextra -Werror -pedantic -Itests/mock -Imain main/settings.c tests/mock/mock_nvs.c tests/test_settings.c -o /tmp/lyricflash_test_settings
/tmp/lyricflash_test_settings
cc -std=c11 -Wall -Wextra -Werror -pedantic -Imain main/power_logic.c tests/test_power.c -o /tmp/lyricflash_test_power
/tmp/lyricflash_test_power
rm -f /tmp/lyricflash_test_lyric_clock /tmp/lyricflash_test_power
rm -f /tmp/lyricflash_test_settings /tmp/lyricflash_test_image /tmp/lyricflash_test_api /tmp/lyricflash_test_lyrics
