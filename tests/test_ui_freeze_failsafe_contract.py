#!/usr/bin/env python3
"""0.8.61: a failed song lookup must never leave the OLED permanently frozen."""
from pathlib import Path
p=Path(__file__).resolve().parents[1]
ui=(p/"main/ui.c").read_text()
main=(p/"main/main.c").read_text()
header=(p/"main/ui.h").read_text()
worker=(p/"main/recognition.c").read_text()
version=(p/"main/version.h").read_text()
assert 'LF_VERSION "0.8.125-alpha"' in version
# One spatially linked Home logo + white/blue wordmark, no off-screen label.
assert "brand_row=lv_obj_create(parent)" in ui
assert "lv_obj_set_size(brand_row,218,88)" in ui
assert "lv_obj_set_pos(mark,4,2)" in ui
assert "lv_obj_set_pos(wordmark_col,73,2)" in ui
assert 'lv_label_set_text(lyric_mark,"LYRIC")' in ui
assert 'lv_label_set_text(flash_mark,"FLASH")' in ui
assert "lv_obj_align(mark,LV_ALIGN_TOP_MID,-111,68)" not in ui
# Keep normal UI responsive on slow AudD / LRCLIB connections.
assert "static uint32_t home_stalled_attempt;" in ui
for condition in ("current-song->stage_started_ms>30000U",
                  "current-song->stage_started_ms>65000U",
                  "current-song->stage_started_ms>40000U"):
    assert condition in ui,condition
assert 'home_warning=upload_stalled?"Upload stalled - retry":' in ui
assert "lf_recog_stop(); /* cooperative cancel" in ui
assert "home_stalled_attempt==song->attempts" in ui
assert "lv_obj_add_flag(home_record_layer,LV_OBJ_FLAG_HIDDEN)" in ui
# Independent watchdog is *only* for a stalled LVGL task in song workflow,
# never during ordinary idle or OTA updates, and preserves the NVS partitions.
assert 'static void lyricflash_display_recovery_watchdog(void *arg)' in main
assert 'lf_ui_display_stats(&ui);' in main
assert 'lf_recog_snapshot(&song);' in main
for guard in ("ui.ui_tick_age_ms>25000U",
              "uptime_ms>60000",
              "song.attempts>0",
              "song_page",
              "scan_related"):
    assert guard in main,guard
assert "if(uptime_ms>60000 && scan_related && song_page &&" in main
assert "esp_restart();" in main
assert "last_record_cancel_age_ms" in header and "last_record_cancel_age_ms" in ui
assert "ui.last_record_cancel_age_ms<90000U" in main
assert "song.state==LF_RECOG_MATCH || recent_cancel" in main
assert "ui.ui_tick_age_ms>25000U" in main
assert "nvs_flash_erase(" not in main
assert "lf_ota_guard_start_health_check()" in main
assert "lf_ota_guard_confirm_startup();" not in main
assert "xTaskCreate(lyricflash_display_recovery_watchdog" in main
assert "ui_tick_age_ms" in header
# A real song scan must stay visible past normal 15/30/60s screen timeout.
tick=ui.split("static void on_tick_body(lv_timer_t *timer)",1)[1]
assert "if (page == 0 && lf_recog_busy())" in tick
assert "active_scan.attempts != home_stalled_attempt" in tick
assert "last_activity_ms = current;" in tick
assert tick.index("last_activity_ms = current;") < tick.index("sleep_display();")
assert '"AWAITING SONG ID %us"' in ui
assert "upload_stalled" in ui
assert "current-song->upload_progress_ms>18000U" in ui
assert 'home_warning=upload_stalled?"Upload stalled - retry":' in ui
assert "LF_RECOG_UPLOAD_BUDGET_MS 60000ULL" in worker
print("PASS: grouped circular-safe Home branding and independent bounded UI/reboot fail-safes")
