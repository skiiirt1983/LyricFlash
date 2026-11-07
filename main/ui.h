#include <stdbool.h>
#include <stdint.h>
#pragma once
#include "settings.h"
#include "esp_err.h"
bool lf_ui_start(lf_settings_t *settings);

esp_err_t lf_ui_apply_basic_settings(const lf_settings_t *basic);

uint32_t lf_ui_max_tick_gap_ms(void);

/* Apply browser-editable global device preferences without losing clock profiles. */
esp_err_t lf_ui_apply_device_settings(const lf_settings_t *requested);

/* Read-only rendering and touch diagnostics for the local Web UI.
   FLUSH_FINISH counts LVGL driver calls, not hardware DMA completion. */
typedef struct {
    uint32_t touch_presses;
    uint32_t navigation_commits;
    uint32_t full_redraws;
    uint32_t render_count;
    uint32_t flush_starts;
    uint32_t flush_finishes;
    uint32_t refresh_ready;
    uint32_t clock_scene_builds;
    uint32_t clock_cache_hits;
    uint32_t clock_build_last_ms;
    uint32_t clock_build_max_ms;
    uint32_t clock_present_last_ms;
    uint32_t clock_present_max_ms;
    uint32_t nav_reveal_completions;
    uint32_t nav_reveal_last_ms;
    uint32_t pager_forced_snaps;
    uint32_t pager_snap_last_ms;
    uint32_t pager_snap_max_ms;
    uint32_t pager_final_frame_acks; /* presented destination scroll frame */
    uint32_t pager_final_frame_repaint_requests; /* one-shot retry */
    uint32_t pager_acknowledged_settles_no_full_repaint;
    uint32_t render_cycle_last_ms;
    uint32_t render_cycle_max_ms;
    uint32_t clock_repaint_recoveries;
    /* Safe cross-thread evidence if the OLED stops while LyricFlash Web UI lives. */
    uint32_t ui_tick_age_ms;
    uint32_t last_record_cancel_age_ms; /* UINT32_MAX until a double-tap Stop */
    uint32_t record_cancel_phase; /* read-only LVGL cancellation progress */
    uint32_t last_screen_off_ms;
    uint32_t wake_count;
    uint32_t last_wake_frame_ready_ms;
    uint32_t last_wake_overlay_clear_ms;
    bool screen_awake; /* screen state at last atomic snapshot */
    bool wake_overlay_pending; /* current wake has not cleared input cover */
    uint32_t last_wake_age_ms; /* ms since last wake, UINT32_MAX before first */
uint32_t ui_tick_work_last_us;
uint32_t ui_tick_work_max_us;
uint32_t pager_frame_gap_last_ms;
uint32_t pager_frame_gap_max_ms;
uint32_t pager_clock_home_mid_draw_last_us;
uint32_t pager_clock_home_mid_draw_max_us;
uint32_t pager_clock_home_mid_draw_samples;
uint32_t pager_home_settings_mid_draw_last_us;
uint32_t pager_home_settings_mid_draw_max_us;
uint32_t pager_home_settings_mid_draw_samples;
    uint32_t render_draw_last_us;
    uint32_t render_draw_max_us;
    uint32_t flush_wait_refresh_last_us;
    uint32_t flush_wait_refresh_max_us;
    uint32_t flush_calls_refresh_last;
    uint32_t flush_calls_refresh_max;
    uint32_t render_dma_stage_last_us;
    uint32_t render_dma_stage_max_us;
    uint32_t ui_page;
    uint32_t ui_navigation_phase;
    uint32_t lyrics_auto_open_queued;
    uint32_t lyrics_auto_open_completed;
    uint32_t lyrics_auto_open_recoveries;
    uint32_t lyrics_scene_phase; /* 0 idle, 1..5 building, 6 committed */
} lf_ui_display_stats_t;
void lf_ui_display_stats(lf_ui_display_stats_t *out);
/* 0.8.118 boot benchmark: isolates CPU clock vs LVGL drawing vs panel transfer. */
typedef struct {
    uint32_t cpu_mhz_measured;   /* cycle counter vs esp_timer at boot */
    uint32_t cpu_loop_us;        /* fixed integer workload */
    uint32_t blank_frame_ms;     /* full refresh, pager hidden (pipeline baseline) */
    uint32_t page_frame_ms[3];   /* full refresh: Clock, Home, Settings */
    uint32_t mid_frame_ms[2];    /* half-way swipe frames: Clock/Home, Home/Settings */
    uint32_t render_cpu_mhz_min; /* lowest CPU clock seen at any frame start since boot */
    uint32_t render_cpu_mhz_last;
} lf_ui_bench_t;
void lf_ui_bench(lf_ui_bench_t *out);
typedef struct {
    uint32_t count;          /* screen-off events since boot */
    uint32_t sleep_seconds;  /* timeout setting when the last one fired */
    uint32_t idle_ms;        /* firmware idle time at that moment */
    uint32_t lvgl_idle_ms;   /* LVGL input-device idle time at that moment */
    uint32_t page;
    uint32_t uptime_s;
} lf_ui_sleep_info_t;
void lf_ui_sleep_info(lf_ui_sleep_info_t *out);
/* 0.8.118: per-swipe frame statistics, newest first. */
#define LF_SWIPE_LOG 8
typedef struct {
    uint8_t from_page, to_page;   /* pager index: 0 Clock, 1 Home, 2 Settings */
    uint16_t frames;
    uint32_t duration_ms;
    uint32_t avg_gap_ms, max_gap_ms;   /* time between displayed frames */
    uint32_t avg_draw_ms, max_draw_ms; /* LVGL render time per frame */
    uint32_t avg_wait_ms;              /* waiting for panel transfer per frame */
    uint16_t over50, over100;          /* frames slower than 50 / 100 ms */
} lf_swipe_stat_t;
typedef struct {
    uint32_t total;
    uint8_t count;
    lf_swipe_stat_t s[LF_SWIPE_LOG];
} lf_swipe_log_t;
void lf_ui_swipe_log(lf_swipe_log_t *out);
void lf_ui_snapshot_stats(uint32_t *taken,uint32_t *last_ms,uint32_t *max_ms,uint32_t *swipes);
