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
    uint32_t clock_repaint_recoveries;
} lf_ui_display_stats_t;
void lf_ui_display_stats(lf_ui_display_stats_t *out);
