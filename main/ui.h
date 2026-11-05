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
uint32_t ui_tick_work_last_us;
uint32_t ui_tick_work_max_us;
uint32_t pager_frame_gap_last_ms;
uint32_t pager_frame_gap_max_ms;
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
