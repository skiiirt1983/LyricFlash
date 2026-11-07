#include "ui.h"
#include "bsp/display.h"
#include "esp_lv_adapter.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_heap_caps.h"
#include "esp_cpu.h"
#include "esp_private/esp_clk.h"
#include "esp_sleep.h"
#include "driver/gpio.h"
#include "power.h"
#include "network.h"
#include "diagnostics.h"
#include "version.h"
#include "brand_icon.h"
#include "clock_face.h"
#include "recognition.h"
#include "lyric_clock.h"
#include "lyric_sync.h"
#include "song_cache.h"
#include "song_history.h"
#include "recognition_audio.h"
#include "weather.h"
#include "weather_visual.h"
#include "char_wheel.h"
#include "lvgl.h"
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdatomic.h>

static const char *TAG = "LyricFlashUI";
static lf_settings_t *prefs;
static uint64_t last_activity_ms;
static uint64_t last_scroll_ms;
static float scroll_y;
static bool paused;
static bool asleep;
static lv_obj_t *scroll_panel;
static lv_obj_t *lyrics_label;
/* A three-row lyric viewport avoids repainting huge scrolling text; all
 * LVGL mutations stay on its existing task. No additional display roots. */
static bool timed_lyrics_mode;
static lf_lyric_clock_t timed_clock;
static uint32_t timed_stamps[LF_MAX_LINES];
/* Premeasure every LRC line before its first frame with one fixed 24px
   font and identical wrapping width for all four lyric slots. */
#define LF_TIMED_TEXT_WIDTH 378
static uint16_t timed_line_heights[LF_MAX_LINES];
static size_t timed_line_count;
static int16_t timed_row_centers[4];
static uint8_t timed_visible_future_mask;
static uint32_t timed_scan_attempt;
static int timed_active_index=-999;
static uint32_t timed_last_second=UINT32_MAX;
static uint64_t timed_last_update_ms;
/* Four retained lyric rows: previous, active, next and approaching next.
 * Only their vertical positions update between timestamps. */
static lv_obj_t *timed_rows[4];
/* A single retained parent moves every lyric row together, avoiding four
 * separate QSPI invalidations on each animation frame. */
static lv_obj_t *timed_overlay; /* removable child of the one LVGL root */
static lv_obj_t *timed_viewport; /* fixed clipping boundary; never translated */
static lv_obj_t *timed_scroll_group;
static lv_timer_t *timed_scroll_timer;
/* Stop unused 16ms clock sweep while black screen-off cover is active. */
static lv_timer_t *clock_sweep_timer;
static int16_t timed_scroll_y=INT16_MIN;
static uint8_t timed_row_opa[4]={255,255,255,255};
static lv_obj_t *timed_status;
static lv_obj_t *timed_pause_button;
static lv_obj_t *timed_controls;
static lv_obj_t *timed_hint;
static lv_obj_t *timed_battery;
static uint64_t timed_power_update_ms;
static lv_obj_t *timed_save_note;
static uint64_t timed_hint_until_ms;
static uint64_t timed_last_short_tap_ms;
static uint64_t timed_controls_until_ms;
/* Song-local timing anchors. Rate changes are continuous, without jumps. */
static bool timed_estimated;
static uint16_t timed_rate_percent=100;
static uint32_t timed_rate_clock_anchor_ms;
static uint32_t timed_rate_song_anchor_ms;
static bool timed_song_custom;
/* Write only changed per-track timing when a user exits its controls. */
static bool timed_profile_dirty;
static lv_timer_t *timed_nudge_feedback_timer;
static lv_obj_t *timed_nudge_feedback_button;
static void timed_profile_save_if_dirty(void);
static lv_obj_t *timed_sync_slider;
static lv_obj_t *timed_rate_slider;
static lv_obj_t *timed_sync_value;
static lv_obj_t *timed_rate_value;
static uint32_t auto_open_attempt;
static bool auto_open_pending;
static bool auto_open_transition;
static uint64_t auto_open_poll_ms;
static uint64_t auto_open_requested_ms;
static uint32_t auto_open_fallback_attempt;
/* Counts are diagnostics only; a successful screen build acknowledges
 * a song attempt, not merely queuing an asynchronous navigation request. */
static atomic_uint auto_open_queued_count;
static atomic_uint auto_open_completed_count;
/* Home now reflects the same explicitly triggered recognition session and
 * manual LRC timeline as the Lyrics viewer. These pointers are never used
 * after make_base() destroys its child pager. */
static lv_obj_t *home_scan_button;
/* Arm on Play release; begin capture only after a silent 500ms settling gap. */
static bool home_scan_armed;
static uint64_t home_scan_released_ms;
#define LF_HOME_SCAN_SETTLE_MS 500U
/* Decorative rings and Play share exactly the same player center. */
#define LF_HOME_PLAYER_CENTER_Y 18
static lv_obj_t *home_scan_status;
static lv_obj_t *home_song_title;
static lv_obj_t *home_song_artist;
static lv_obj_t *home_lyric_line;
static uint64_t home_last_refresh_ms;
/* Never let a network request monopolize the round OLED indefinitely. */
static uint32_t home_stalled_attempt;
static uint32_t home_history_logged_attempt; /* once per recognized scan */
static lv_obj_t *home_play_symbol;
static lv_obj_t *home_play_hint;
/* Overlay is owned by the existing Home pager, not a second LVGL root. */
static lv_obj_t *home_record_layer;
static lv_obj_t *home_record_wave; /* first of four inexpensive drawing strips */
#define LF_RECORD_WAVE_SEGMENTS 4U
#define LF_RECORD_WAVE_STRIP_WIDTH 90U
#define LF_RECORD_WAVE_STRIP_POINTS 26U
#define LF_RECORD_WAVE_REFRESH_MS 140U
static lv_obj_t *home_record_wave_segments[LF_RECORD_WAVE_SEGMENTS];
static lv_obj_t *home_record_centerline;
static lv_obj_t *home_record_loading;
static lv_obj_t *home_record_caption;
static uint32_t home_record_attempt;
static uint64_t home_record_cancel_tap_ms; /* two short taps, never single */
static uint32_t home_record_cancel_pending_attempt; /* timer owns cancellation */
static atomic_uint last_record_cancel_ms; /* survives a wedged LVGL task for watchdog */
static atomic_uint ui_record_cancel_phase; /* 0 idle, 1 requested, 2 retire, 3 hidden, 4 refresh due, 5 refreshed */
static int32_t home_record_wave_bias_q8; /* UI-only DC center, 8-bit fixed point */
static uint32_t home_record_last_frames;
static uint64_t home_record_last_tick;
#define LF_RECORD_WAVE_MAX_POINTS (LF_RECORD_WAVE_SEGMENTS*LF_RECORD_WAVE_STRIP_POINTS)
static lv_point_precise_t home_record_wave_points[LF_RECORD_WAVE_SEGMENTS][LF_RECORD_WAVE_STRIP_POINTS];
static uint8_t home_record_wave_counts[LF_RECORD_WAVE_SEGMENTS];
static uint8_t home_record_wave_last_y;
static uint64_t home_warning_until_ms;
static const char *home_warning;
static int home_visual_busy=-1;
static lv_obj_t *speed_text;
static lv_obj_t *brightness_text;
static lv_obj_t *timeout_text;
static lv_obj_t *screen;
/* The original LVGL screen remains the ONLY active screen. This child pager
   holds the three permanently adjacent menu pages (Clock | Home | Settings).
   Weather and configuration subpages keep their proven deferred navigation. */
static lv_obj_t *primary_pager;
static lv_obj_t *primary_pager_pages[3];
static int primary_pager_current=1;
static int primary_pager_origin=1;
static int primary_pager_snap_to=1;
static bool primary_pager_snapping;
static bool primary_pager_initializing;
/* Only advance post-release movement after the previous display refresh.
   LVGL's millisecond scroll animation issued many positions while QSPI was
   still transferring an older frame, visually skipping frames and jerking. */
static lv_timer_t *primary_pager_frame_timer;
/* Do not declare a swipe complete until the destination was refreshed. */
static bool primary_pager_frame_pending;
static bool pager_final_frame_ready;
static int32_t pager_render_scroll_x=-1;
static uint64_t pager_arrived_at_ms;
static atomic_uint pager_final_frame_acks;
static atomic_uint pager_final_frame_repaint_requests;
static atomic_uint pager_acknowledged_settles_no_full_repaint;
#define LF_PAGER_STALL_GUARD_MS 1600U
/* 0.8.25: retain the 0.8.23 stuck-snap recovery, but always keep the
   actual clock scene visible. The optional off-screen RGB565 snapshot was
   hiding the live clock during page flips and could leave a black page
   until an unrelated touch caused a new display refresh. */
static uint64_t primary_pager_last_move_ms;
static uint64_t primary_pager_snap_started_ms;
static atomic_uint pager_forced_snap_count;
static atomic_uint pager_snap_last_ms;
static atomic_uint pager_snap_max_ms;
static atomic_uint clock_repaint_recoveries;
/* Keep the clock as a hidden child on the one safe LVGL root instead of
   reconstructing 30+ decorative widgets on every visit. Never create a
   second active LVGL screen or swap roots during touch dispatch. */
static lv_obj_t *clock_scene;
static bool clock_scene_stale = true;
static uint8_t clock_scene_style = UINT8_MAX;
static atomic_uint ui_clock_scene_builds;
static atomic_uint ui_clock_cache_hits;
static atomic_uint ui_clock_build_last_ms;
static atomic_uint ui_clock_build_max_ms;
static atomic_uint ui_clock_present_last_ms;
static atomic_uint ui_clock_present_max_ms;
/* LVGL thread only. Measures from entering Clock until first completed refresh. */
static int64_t clock_present_start_us;
static lv_obj_t *sleep_overlay;
/* Keep the waking touch target alive until the physical finger releases. */
static bool wake_cleanup_queued;
/* Display diagnostic counters must be readable from the HTTP task without
   taking the LVGL mutex (which could block during a display failure). */
static atomic_uint ui_touch_count;
static atomic_uint ui_nav_count;
static atomic_uint ui_full_redraw_count;
static atomic_uint ui_render_count;
static atomic_uint ui_flush_start_count;
static atomic_uint ui_flush_finish_count;
static atomic_uint ui_refresh_ready_count;
static atomic_uint render_cycle_last_ms;
static atomic_uint render_cycle_max_ms;
static int64_t render_cycle_start_us;
static atomic_uint nav_transition_completed_count;
static atomic_uint nav_transition_last_ms;
static atomic_uint ui_last_tick_ms;
static atomic_uint ui_last_page;
static atomic_uint ui_nav_phase;
static atomic_uint auto_open_recovery_count;
static atomic_uint ui_lyrics_scene_phase; /* 0 idle, 1..5 building, 6 committed */
/* Measure wake performance after different off durations without adding
   any screen-refresh work to the critical wake path. */
static atomic_uint screen_off_started_ms;
static atomic_uint last_screen_off_ms;
static atomic_uint wake_count;
static atomic_uint wake_started_ms;
static atomic_uint wake_frame_pending_ms;
static atomic_uint wake_frame_ready_ms;
static atomic_uint wake_overlay_clear_ms;
/* Cross-task wake status: no LVGL locking needed for recovery monitoring. */
static atomic_bool ui_screen_awake;
static atomic_bool wake_overlay_pending;
/* Allocation-free UI timing snapshots, readable without the LVGL lock. */
static atomic_uint ui_tick_work_last_us;
static atomic_uint ui_tick_work_max_us;
static atomic_uint pager_frame_gap_last_ms;
static atomic_uint pager_frame_gap_max_ms;
static atomic_uint pager_refresh_prev_ms;
/* Profile the overlapping-center raster workload, separated by page pair. */
/* 0.8.112: why did the screen turn off? */
static lf_ui_sleep_info_t sleep_info;
void lf_ui_sleep_info(lf_ui_sleep_info_t *out){ if(out)*out=sleep_info; }
/* 0.8.104 benchmark: CPU clock sampled at every frame start. */
static atomic_uint render_cpu_mhz_min=UINT32_MAX;
static atomic_uint render_cpu_mhz_last;
static uint8_t pager_mid_draw_pair; /* 0 edge, 1 Clock/Home, 2 Home/Settings */
static atomic_uint pager_mid_draw_last_us[2];
static atomic_uint pager_mid_draw_max_us[2];
static atomic_uint pager_mid_draw_samples[2];
/* 0.8.85: measure LVGL draw and driver wait without frame logging. */
static int64_t draw_started_us;
static int64_t flush_wait_started_us;
static uint32_t flush_wait_accum_us;
static uint32_t flush_calls_in_refresh;
static atomic_uint render_draw_last_us;
static atomic_uint render_draw_max_us;
static atomic_uint flush_wait_refresh_last_us;
static atomic_uint flush_wait_refresh_max_us;
static atomic_uint flush_calls_refresh_last;
static atomic_uint flush_calls_refresh_max;
static uint32_t render_dma_stage_at_start_us;
static atomic_uint render_dma_stage_last_us;
static atomic_uint render_dma_stage_max_us;

void lf_ui_display_stats(lf_ui_display_stats_t *out)
{
    if(!out)return;
    out->touch_presses=atomic_load(&ui_touch_count);
    out->navigation_commits=atomic_load(&ui_nav_count);
    out->full_redraws=atomic_load(&ui_full_redraw_count);
    out->render_count=atomic_load(&ui_render_count);
    out->flush_starts=atomic_load(&ui_flush_start_count);
    out->flush_finishes=atomic_load(&ui_flush_finish_count);
    out->refresh_ready=atomic_load(&ui_refresh_ready_count);
    out->clock_scene_builds=atomic_load(&ui_clock_scene_builds);
    out->clock_cache_hits=atomic_load(&ui_clock_cache_hits);
    out->clock_build_last_ms=atomic_load(&ui_clock_build_last_ms);
    out->clock_build_max_ms=atomic_load(&ui_clock_build_max_ms);
    out->clock_present_last_ms=atomic_load(&ui_clock_present_last_ms);
    out->clock_present_max_ms=atomic_load(&ui_clock_present_max_ms);
    out->nav_reveal_completions=atomic_load(&nav_transition_completed_count);
    out->nav_reveal_last_ms=atomic_load(&nav_transition_last_ms);
    out->pager_forced_snaps=atomic_load(&pager_forced_snap_count);
    out->pager_snap_last_ms=atomic_load(&pager_snap_last_ms);
    out->pager_snap_max_ms=atomic_load(&pager_snap_max_ms);
    out->pager_final_frame_acks=atomic_load(&pager_final_frame_acks);
    out->pager_final_frame_repaint_requests=atomic_load(&pager_final_frame_repaint_requests);
    out->pager_acknowledged_settles_no_full_repaint=
        atomic_load(&pager_acknowledged_settles_no_full_repaint);
    out->render_cycle_last_ms=atomic_load(&render_cycle_last_ms);
    out->render_cycle_max_ms=atomic_load(&render_cycle_max_ms);
    out->clock_repaint_recoveries=atomic_load(&clock_repaint_recoveries);
    const uint32_t current=(uint32_t)(esp_timer_get_time()/1000LL);
    const uint32_t tick=atomic_load(&ui_last_tick_ms);
    out->ui_tick_age_ms=tick?current-tick:UINT32_MAX;
    const uint32_t cancelled=atomic_load(&last_record_cancel_ms);
    out->last_record_cancel_age_ms=cancelled?current-cancelled:UINT32_MAX;
    out->record_cancel_phase=atomic_load(&ui_record_cancel_phase);
    out->ui_page=atomic_load(&ui_last_page);
    out->last_screen_off_ms=atomic_load(&last_screen_off_ms);
    out->wake_count=atomic_load(&wake_count);
    out->last_wake_frame_ready_ms=atomic_load(&wake_frame_ready_ms);
    out->last_wake_overlay_clear_ms=atomic_load(&wake_overlay_clear_ms);
    out->screen_awake=atomic_load(&ui_screen_awake);
    out->wake_overlay_pending=atomic_load(&wake_overlay_pending);
    const uint32_t wake_at=atomic_load(&wake_started_ms);
    out->last_wake_age_ms=wake_at?current-wake_at:UINT32_MAX;
    out->ui_tick_work_last_us=atomic_load(&ui_tick_work_last_us);
    out->ui_tick_work_max_us=atomic_load(&ui_tick_work_max_us);
    out->pager_frame_gap_last_ms=atomic_load(&pager_frame_gap_last_ms);
    out->pager_frame_gap_max_ms=atomic_load(&pager_frame_gap_max_ms);
    out->pager_clock_home_mid_draw_last_us=atomic_load(&pager_mid_draw_last_us[0]);
    out->pager_clock_home_mid_draw_max_us=atomic_load(&pager_mid_draw_max_us[0]);
    out->pager_clock_home_mid_draw_samples=atomic_load(&pager_mid_draw_samples[0]);
    out->pager_home_settings_mid_draw_last_us=atomic_load(&pager_mid_draw_last_us[1]);
    out->pager_home_settings_mid_draw_max_us=atomic_load(&pager_mid_draw_max_us[1]);
    out->pager_home_settings_mid_draw_samples=atomic_load(&pager_mid_draw_samples[1]);
    out->render_draw_last_us=atomic_load(&render_draw_last_us);
    out->render_draw_max_us=atomic_load(&render_draw_max_us);
    out->flush_wait_refresh_last_us=atomic_load(&flush_wait_refresh_last_us);
    out->flush_wait_refresh_max_us=atomic_load(&flush_wait_refresh_max_us);
    out->flush_calls_refresh_last=atomic_load(&flush_calls_refresh_last);
    out->flush_calls_refresh_max=atomic_load(&flush_calls_refresh_max);
    out->render_dma_stage_last_us=atomic_load(&render_dma_stage_last_us);
    out->render_dma_stage_max_us=atomic_load(&render_dma_stage_max_us);
    out->ui_navigation_phase=atomic_load(&ui_nav_phase);
    out->lyrics_auto_open_queued=atomic_load(&auto_open_queued_count);
    out->lyrics_auto_open_completed=atomic_load(&auto_open_completed_count);
    out->lyrics_auto_open_recoveries=atomic_load(&auto_open_recovery_count);
    out->lyrics_scene_phase=atomic_load(&ui_lyrics_scene_phase);
}
static void display_render_event(lv_event_t *e)
{
    (void)e;
    {
        const unsigned mhz=(unsigned)(esp_clk_cpu_freq()/1000000);
        atomic_store(&render_cpu_mhz_last,mhz);
        if(mhz<atomic_load(&render_cpu_mhz_min))atomic_store(&render_cpu_mhz_min,mhz);
    }
    atomic_fetch_add(&ui_render_count,1);
    render_cycle_start_us=esp_timer_get_time();
    draw_started_us=render_cycle_start_us;
    pager_render_scroll_x=(primary_pager && primary_pager_snapping) ?
        abs(lv_obj_get_scroll_x(primary_pager)) : -1;
    pager_mid_draw_pair=0;
    if(primary_pager && !asleep){
        const int32_t x=abs(lv_obj_get_scroll_x(primary_pager));
        const int32_t offset=x%466;
        /* The center of each transition exposes two fully populated pages. */
        if(offset>=150 && offset<=316 && x<932)
            pager_mid_draw_pair=(uint8_t)(x<466?1:2);
    }
    bsp_display_transfer_stats_t staged={0};
    bsp_display_transfer_stats(&staged);
    render_dma_stage_at_start_us=staged.stage_cumulative_us;
}
/* LVGL 9.5 RENDER_READY precedes the flush callback. */
static void display_render_ready_event(lv_event_t *e)
{
    (void)e;
    if(!draw_started_us)return;
    const int64_t us=esp_timer_get_time()-draw_started_us;
    draw_started_us=0;
    const uint32_t duration=(uint32_t)(us>0?us:0);
    if(pager_mid_draw_pair){
        const uint8_t pair=(uint8_t)(pager_mid_draw_pair-1);
        atomic_store(&pager_mid_draw_last_us[pair],duration);
        if(duration>atomic_load(&pager_mid_draw_max_us[pair]))
            atomic_store(&pager_mid_draw_max_us[pair],duration);
        atomic_fetch_add(&pager_mid_draw_samples[pair],1);
    }
    bsp_display_transfer_stats_t staged={0};
    bsp_display_transfer_stats(&staged);
    /* Unsigned subtraction handles 32-bit monotonic wraparound. */
    const uint32_t stage_us=staged.stage_cumulative_us-render_dma_stage_at_start_us;
    atomic_store(&render_dma_stage_last_us,stage_us);
    if(stage_us>atomic_load(&render_dma_stage_max_us))
        atomic_store(&render_dma_stage_max_us,stage_us);
    atomic_store(&render_draw_last_us,duration);
    if(duration>atomic_load(&render_draw_max_us))
        atomic_store(&render_draw_max_us,duration);
}
static void display_flush_wait_start_event(lv_event_t *e)
{
    (void)e;
    flush_wait_started_us=esp_timer_get_time();
}
static void display_flush_wait_finish_event(lv_event_t *e)
{
    (void)e;
    if(!flush_wait_started_us)return;
    const int64_t us=esp_timer_get_time()-flush_wait_started_us;
    flush_wait_started_us=0;
    if(us>0)flush_wait_accum_us+=(uint32_t)us;
}

static void display_flush_start_event(lv_event_t *e)
{
    (void)e;
    atomic_fetch_add(&ui_flush_start_count,1);
    flush_calls_in_refresh++;
}
static void display_flush_finish_event(lv_event_t *e)
{
    (void)e;
    atomic_fetch_add(&ui_flush_finish_count,1);
}
static void display_refresh_ready_event(lv_event_t *e)
{
    (void)e;
    atomic_fetch_add(&ui_refresh_ready_count,1);
    const uint32_t wait_us=flush_wait_accum_us;
    flush_wait_accum_us=0;
    atomic_store(&flush_wait_refresh_last_us,wait_us);
    if(wait_us>atomic_load(&flush_wait_refresh_max_us))
        atomic_store(&flush_wait_refresh_max_us,wait_us);
    atomic_store(&flush_calls_refresh_last,flush_calls_in_refresh);
    if(flush_calls_in_refresh>atomic_load(&flush_calls_refresh_max))
        atomic_store(&flush_calls_refresh_max,flush_calls_in_refresh);
    flush_calls_in_refresh=0;
    /* Only a completed destination-position render is a valid snap commit.
       A pointer/timer reporting X=target does not mean pixels are displayed. */
    if(primary_pager_snapping && primary_pager && pager_render_scroll_x>=0 &&
       abs(pager_render_scroll_x-primary_pager_snap_to*466)<=2 &&
       abs(abs(lv_obj_get_scroll_x(primary_pager))-primary_pager_snap_to*466)<=2){
        if(!pager_final_frame_ready)
            atomic_fetch_add(&pager_final_frame_acks,1);
        pager_final_frame_ready=true;
    }
    pager_render_scroll_x=-1;
    /* Measured completed-refresh cadence while the pager is snapping. */
    const uint32_t ready_now=(uint32_t)(esp_timer_get_time()/1000LL);
    if(primary_pager_snapping){
        const uint32_t prev=atomic_exchange(&pager_refresh_prev_ms,ready_now);
        if(prev){
            const uint32_t gap=ready_now-prev;
            atomic_store(&pager_frame_gap_last_ms,gap);
            if(gap>atomic_load(&pager_frame_gap_max_ms))
                atomic_store(&pager_frame_gap_max_ms,gap);
        }
    }else atomic_store(&pager_refresh_prev_ms,0);
    const uint32_t started=atomic_exchange(&wake_frame_pending_ms,0);
    if(started)atomic_store(&wake_frame_ready_ms,
                              (uint32_t)(esp_timer_get_time()/1000LL)-started);
    if(render_cycle_start_us) {
        int64_t elapsed=esp_timer_get_time()-render_cycle_start_us;
        render_cycle_start_us=0;
        uint32_t ms=(uint32_t)((elapsed>0?elapsed:0)/1000);
        atomic_store(&render_cycle_last_ms,ms);
        uint32_t previous=atomic_load(&render_cycle_max_ms);
        if(ms>previous)atomic_store(&render_cycle_max_ms,ms);
    }
    if (clock_present_start_us) {
        int64_t elapsed_us=esp_timer_get_time()-clock_present_start_us;
        clock_present_start_us=0;
        if (elapsed_us < 0) elapsed_us=0;
        uint32_t duration=(uint32_t)(elapsed_us/1000);
        atomic_store(&ui_clock_present_last_ms,duration);
        uint32_t maximum=atomic_load(&ui_clock_present_max_ms);
        if (duration > maximum) atomic_store(&ui_clock_present_max_ms,duration);
    }
}
static void invalidate_full_screen(void)
{
    if(!screen || asleep)return;
    lv_obj_invalidate(screen);
    atomic_fetch_add(&ui_full_redraw_count,1);
}
/* Every navigation request is executed from the LVGL async queue, after the
   originating input event has finished. No active touch target is destroyed
   inside its own PRESSED/CLICKED/RELEASED callback. */
static bool nav_queued;
static void (*nav_pending)(void);
/* Recovery mode: exactly one LVGL screen, no pager. */
static bool pager_scrolling; /* Always false for clock refresh guard. */
static lv_obj_t *battery_status;
static lv_obj_t *network_status;
static lv_obj_t *network_password;
static lv_obj_t *network_url;
static lv_obj_t *ota_status;
static uint64_t last_power_refresh_ms;
static uint64_t last_wifi_poll_ms;
static bool user_dragging;
static bool finger_was_scrolling;
static uint64_t scroll_at_end_ms;
/* Finger releases resume auto-scrolling after this time; explicit PAUSE stays paused. */
static uint64_t touch_resume_at_ms;
#define LF_TOUCH_RESUME_DELAY_MS 1500ULL
static lv_obj_t *pause_button;
static lv_obj_t *wifi_list;
static lv_obj_t *wifi_status;
static lv_obj_t *wifi_password_status;
static lf_wifi_ap_t wifi_aps[LF_WIFI_MAX_RESULTS];
static size_t wifi_ap_count;
static char selected_wifi[33];
static bool selected_wifi_secured;
static int wifi_state_previous;
static uint64_t clock_last_refresh;
static uint64_t weather_detail_refresh;
static lv_obj_t *basic_font_size_label;
static lv_obj_t *basic_colour_swatch;
static lv_obj_t *basic_hue_label;
static uint8_t selected_colour_target;
static lv_obj_t *debug_text;
static uint64_t last_debug_refresh;
static uint64_t last_ui_tick_ms;
static uint32_t max_ui_tick_gap_ms;
static int page; // 14 diagnostics, 0 home, 1 lyrics, 2 settings, 3 sleep, 4 wifi, 5 password, 6 clock, 7 clock settings, 8 city, 9 lyrics settings, 10 device, 11 detail, 12 Basic settings, 13 hue picker

static const uint32_t PALETTE[] = {0xFFFFFF, 0x86E3AE, 0x76C6FF, 0xFFD18B, 0xF6A1D2};
static const char *DEMO_TEXT =
    "Distant lights awake\n\n"
    "Above the quiet valley\n\n"
    "We follow winding roads\n\n"
    "As evening turns to silver\n\n"
    "And every window glows\n\n"
    "A thousand little stories\n\n"
    "Are floating in the air\n\n"
    "Tomorrow waits beyond us\n\n"
    "But we're still standing here\n\n"
    "The final note is fading\n\n"
    "A gentle end to song";

static uint64_t now_ms(void) { return (uint64_t)esp_timer_get_time() / 1000ULL; }
static void activity(void) { last_activity_ms = now_ms(); }
static const lv_font_t *system_font(void)
{
    return prefs->system_font_size == 0 ? &lv_font_montserrat_18 :
           prefs->system_font_size == 1 ? &lv_font_montserrat_20 : &lv_font_montserrat_24;
}
static const lv_font_t *font_for_size(uint8_t size)
{
    return size == 0 ? &lv_font_montserrat_18 :
           size == 2 ? &lv_font_montserrat_26 : &lv_font_montserrat_24;
}
/* A CO5300 panel at brightness=0 stops its TE pulse. The black AMOLED
   sleep overlay displays no light, while 1% keeps TE synchronization alive.
   Do not pause LVGL inside its own timer/gesture dispatch. */
static void apply_brightness(void) { (void)bsp_display_brightness_set(asleep ? 1 : prefs->brightness); }

static void save_settings(void)
{
    /* On-device Basic changes must not leave Web UI face 6 out of sync. */
    prefs->face_background[5]=prefs->basic_background;
    prefs->face_foreground[5]=prefs->basic_foreground;
    prefs->face_show_date[5]=prefs->basic_show_date;
    esp_err_t r = lf_settings_save(prefs);
    if (r != ESP_OK) ESP_LOGE(TAG, "Could not save settings: %s", esp_err_to_name(r));
    else clock_scene_stale = true; /* Only invalidate after a saved preference change. */
    activity();
}

static lv_obj_t *label(lv_obj_t *parent, const char *text, int y,
                       const lv_font_t *font, uint32_t rgb)
{
    lv_obj_t *o = lv_label_create(parent);
    lv_label_set_text(o, text);
    lv_obj_set_style_text_color(o, lv_color_hex(rgb), 0);
    lv_obj_set_style_text_font(o, (font == &lv_font_montserrat_12 || font == &lv_font_montserrat_16 || font == &lv_font_montserrat_18) ? system_font() : font, 0);
    lv_obj_set_width(o, 360);
    lv_obj_set_style_text_align(o, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_add_flag(o, LV_OBJ_FLAG_EVENT_BUBBLE);
    lv_obj_add_flag(o, LV_OBJ_FLAG_GESTURE_BUBBLE);
    lv_obj_align(o, LV_ALIGN_TOP_MID, 0, y);
    return o;
}

/* Shared touchscreen button treatment, including a 110 ms tactile press
   animation. Big hit areas and labels clamped to usable circle geometry. */
static void style_button(lv_obj_t *b, uint32_t color)
{
    /* A single visual treatment is applied from first boot through every
       page, not only after entering Device Settings. The custom Home Play
       control is styled separately and remains untouched. */
    (void)color;
    lv_obj_set_style_radius(b, 17, 0);
    lv_obj_set_style_bg_color(b, lv_color_hex(0x143553), 0);
    lv_obj_set_style_bg_grad_color(b, lv_color_hex(0x32658A), 0);
    lv_obj_set_style_bg_grad_dir(b, LV_GRAD_DIR_VER, 0);
    lv_obj_set_style_bg_opa(b, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(b, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_PRESSED);
    lv_obj_set_style_border_width(b, 2, 0);
    lv_obj_set_style_border_color(b, lv_color_hex(0x72C9FF), 0);
    lv_obj_set_style_shadow_width(b, 0, 0);
    lv_obj_set_style_bg_color(b, lv_color_hex(0x418ABE), LV_STATE_PRESSED);
    lv_obj_set_style_border_color(b, lv_color_hex(0xD6F3FF), LV_STATE_PRESSED);
    lv_obj_set_style_pad_hor(b, 8, 0);
}
/* Capture drags that start on the large Home button, not just empty glass. */
static void on_gesture_press(lv_event_t *e);
static void on_touch_pressing(lv_event_t *e);
static void on_touch_release(lv_event_t *e);
static void on_touch_cancel(lv_event_t *e);
static void on_lvgl_gesture(lv_event_t *e);

static lv_obj_t *button(lv_obj_t *parent, const char *text, int width, int height,
                        int x, int y, uint32_t bg, lv_event_cb_t cb, void *arg)
{
    lv_obj_t *b = lv_button_create(parent);
    /* Keep buttons fully inside the usable 466px circle. */
    if (width > 284) width = 284;
    if (height < 48) height = 48;
    if (height > 94) height = 94;
    lv_obj_set_size(b, width, height);
    lv_obj_align(b, LV_ALIGN_CENTER, x, y);
    style_button(b, bg);
    /* Let LVGL's horizontal pager see drag gestures starting on buttons. */
    lv_obj_add_flag(b, LV_OBJ_FLAG_EVENT_BUBBLE);
    lv_obj_add_flag(b, LV_OBJ_FLAG_GESTURE_BUBBLE);
    lv_obj_add_event_cb(b, cb, LV_EVENT_CLICKED, arg);
    lv_obj_add_event_cb(b, on_gesture_press, LV_EVENT_PRESSED, NULL);
    lv_obj_add_event_cb(b, on_touch_pressing, LV_EVENT_PRESSING, NULL);
    lv_obj_add_event_cb(b, on_touch_release, LV_EVENT_RELEASED, NULL);
    lv_obj_add_event_cb(b, on_touch_cancel, LV_EVENT_PRESS_LOST, NULL);
    lv_obj_add_event_cb(b, on_lvgl_gesture, LV_EVENT_GESTURE, NULL);
    lv_obj_t *l = lv_label_create(b);
    lv_label_set_text(l, text);
    lv_obj_set_width(l, width - 16);
    lv_label_set_long_mode(l, LV_LABEL_LONG_DOT);
    const lv_font_t *f = system_font();
    /* Long captions still fit comfortably without clipping. */
    if (strlen(text) > 19 && prefs->system_font_size == 2) f = &lv_font_montserrat_20;
    lv_obj_set_style_text_font(l, f, 0);
    lv_obj_set_style_text_align(l, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_color(l, lv_color_hex(0xEAF6FF), 0);
    lv_obj_center(l);
    return b;
}

static void build_home(void);
static void build_debug(void);
static void build_connection_help(void);
static void refresh_debug(void);
static void build_preview(void);
static void build_settings(void);
static void build_wifi(void);
static void build_wifi_password(void);
static void build_clock(void);
static void build_clock_settings(void);
static void build_basic_settings(void);
static void build_basic_colour(void);
static void build_weather_location(void);
static void build_lyrics_settings(void);
static void build_device_settings(void);
static void build_weather_details(void);
static void on_clock_options(lv_event_t *e);
static void on_clock_back(lv_event_t *e);
static void on_home(lv_event_t *e)
{
    (void)e; activity(); build_home();
}
static void on_preview(lv_event_t *e)
{
    (void)e; activity(); build_preview();
}
static void on_settings(lv_event_t *e)
{
    (void)e; activity(); build_settings();
}
/* Press lost does not mean the finger was lifted: LVGL sends it when a
   swipe exits a button. Track the pointer device itself until its reported
   input state is RELEASED; use LVGL's gesture detection as a second path.
   Screen construction remains deferred, AFTER physical finger release. */
#define LF_SWIPE_MIN_PX 38
#define LF_CLOCK_SETTINGS_HOLD_MS 1050ULL
#define LF_HOLD_MOVE_CANCEL_PX 20
static lv_point_t gesture_origin;
static lv_indev_t *gesture_indev;
static bool gesture_origin_valid;
static bool touch_held;
static uint64_t touch_pressed_at_ms;
static int pending_swipe_page = -1;
static lv_screen_load_anim_t pending_swipe_direction = LV_SCR_LOAD_ANIM_NONE;
/* One LVGL root; pace partial QSPI refreshes during directional page reveal. */
static lv_screen_load_anim_t nav_transition_hint = LV_SCR_LOAD_ANIM_NONE;
static lv_timer_t *nav_transition_timer;
static lv_obj_t *nav_transition_catcher;
static bool nav_transition_active;
static uint8_t nav_transition_step;
static int32_t nav_transition_cursor;
static lv_screen_load_anim_t nav_transition_dir;
static uint64_t nav_transition_started_ms;
static uint64_t nav_transition_last_frame_ms;
static bool nav_transition_frame_pending;
static uint32_t nav_transition_frame_render_at_issue;
static uint32_t nav_transition_frame_ready_at_issue;
/* Fewer reveal passes avoid flooding QSPI with redundant pending updates.
   The same complete page is transferred in twelve larger strips. */
#define LF_NAV_REVEAL_STEPS 12
#define LF_NAV_REVEAL_PERIOD_MS 28
#define LF_NAV_PANEL_SIZE 466

/* No dual-screen animation: 0.8.3 left old/new UI trees overlapping
   during navigation and made the touch target inconsistent. */
static uint64_t screen_animation_until_ms;
static uint64_t next_swipe_allowed_ms;
static lv_obj_t *clock_settings_popup;
/* Persist entry origin across nested Clock Settings menus. */
static uint8_t clock_settings_return_page=2; /* 2 Settings, 6 Clock */

/* One candidate per physical gesture; no screen swap until the finger lifts. */
static int swipe_candidate_page = -1;
static lv_screen_load_anim_t swipe_candidate_animation = LV_SCR_LOAD_ANIM_NONE;

static void build_home(void);
static void build_settings(void);
static void build_clock(void);
static void build_weather_details(void);
static void build_debug(void);
static void build_clock_settings(void);
static void render_home(void);
static void render_settings(void);
static void render_clock(void);
static void primary_pager_enter(int page_index);
static void primary_pager_watchdog(uint64_t now);

static bool is_swipe_page(void)
{
    return page == 0 || page == 1 || page == 2 || page == 4 ||
           page == 5 || page == 6 || page == 7 || page == 8 ||
           page == 9 || page == 10 || page == 11 || page == 12 ||
           page == 13 || page == 14 || page == 15;
}

/* Swap screens after gesture release, without animations. */
static void commit_swipe(void *arg)
{
    (void)arg;
    const int target = pending_swipe_page;
    nav_transition_hint = pending_swipe_direction;
    pending_swipe_page = -1;
    pending_swipe_direction = LV_SCR_LOAD_ANIM_NONE;
    switch(target) {
    case 0: build_home(); break;
    case 2: build_settings(); break;
    case 6: build_clock(); break;
    case 11: build_weather_details(); break;
    case 14: build_debug(); break;
    case 1: build_preview(); break;
    case 4: build_wifi(); break;
    case 5: build_wifi_password(); break;
    case 7: build_clock_settings(); break;
    case 8: build_weather_location(); break;
    case 9: build_lyrics_settings(); break;
    case 10: build_device_settings(); break;
    case 12: build_basic_settings(); break;
    case 13: build_basic_colour(); break;
    case 15: build_connection_help(); break;
    default: break;
    }
}
static void queue_swipe(int target, lv_screen_load_anim_t animation)
{
    uint64_t current = now_ms();
    if (clock_settings_popup || pending_swipe_page >= 0 ||
        current < next_swipe_allowed_ms) return;
    pending_swipe_page = target;
    pending_swipe_direction = animation;
    next_swipe_allowed_ms = current + 250;
    lv_async_call(commit_swipe, NULL);
}

/* The recording screen owns the glass until manual capture and network
   recognition finish. No pager navigation may race its completion. */
static bool recognition_touch_locked(void)
{
    if(page!=0)return false;
    lf_recog_snapshot_t scan={0};
    lf_recog_snapshot(&scan);
    /* A cancelled attempt must not keep swipe gestures locked while HTTPS
       is winding down in the unchanged recognition worker. */
    if(scan.attempts && scan.attempts==home_stalled_attempt)return false;
    return scan.state==LF_RECOG_CAPTURE ||
           scan.state==LF_RECOG_UPLOAD || scan.state==LF_RECOG_LYRICS;
}
static void swipe_intent(lv_dir_t direction)
{
    if(recognition_touch_locked())return;
    /* Moving the timing controls must never navigate away by accident. */
    if(page==1 && timed_controls &&
       !lv_obj_has_flag(timed_controls,LV_OBJ_FLAG_HIDDEN))return;
    if (clock_settings_popup || swipe_candidate_page >= 0) return;
    /* Horizontal Clock/Home/Settings navigation belongs exclusively to the
       scrolling pager, not the old on-release page-rebuild mechanism. */
    if(primary_pager && (page==0 || page==2 || page==6) &&
       (direction==LV_DIR_LEFT || direction==LV_DIR_RIGHT)) return;
    if (page == 0) {
        if (direction == LV_DIR_LEFT) {
            swipe_candidate_page = 2;
            swipe_candidate_animation = LV_SCR_LOAD_ANIM_OVER_LEFT;
        } else if (direction == LV_DIR_RIGHT) {
            swipe_candidate_page = 6;
            swipe_candidate_animation = LV_SCR_LOAD_ANIM_OVER_RIGHT;
        } else if (direction == LV_DIR_BOTTOM) {
            swipe_candidate_page = 1; /* Lyrics panel sits above Home. */
            swipe_candidate_animation = LV_SCR_LOAD_ANIM_OVER_BOTTOM;
        }
    } else if (page == 2) {
        /* Settings is a navigation destination, never a dead end.
           Return Home with either horizontal swipe; a short accidental
           gesture still cannot activate because the threshold is 38 px. */
        if (direction == LV_DIR_LEFT || direction == LV_DIR_RIGHT) {
            swipe_candidate_page = 0;
            swipe_candidate_animation = direction == LV_DIR_LEFT
                ? LV_SCR_LOAD_ANIM_OVER_LEFT : LV_SCR_LOAD_ANIM_OVER_RIGHT;
        }
    } else if (page == 6) {
        if (direction == LV_DIR_LEFT) {
            swipe_candidate_page = 0;
            swipe_candidate_animation = LV_SCR_LOAD_ANIM_OVER_LEFT;
        } else if (direction == LV_DIR_TOP) {
            swipe_candidate_page = 11;
            swipe_candidate_animation = LV_SCR_LOAD_ANIM_OVER_TOP;
        }
    } else if (page == 1 && direction == LV_DIR_TOP) {
        swipe_candidate_page = 0;
        swipe_candidate_animation = LV_SCR_LOAD_ANIM_OVER_TOP;
    } else if (page == 14 && (direction == LV_DIR_TOP || direction == LV_DIR_RIGHT)) {
        swipe_candidate_page = 0;
        swipe_candidate_animation = LV_SCR_LOAD_ANIM_OVER_RIGHT;
    } else if (page == 11 && (direction == LV_DIR_BOTTOM || direction == LV_DIR_RIGHT)) {
        swipe_candidate_page = 6;
        swipe_candidate_animation = LV_SCR_LOAD_ANIM_OVER_RIGHT;
    } else if (direction == LV_DIR_RIGHT || direction == LV_DIR_LEFT) {
        /* Consistent exit for all settings submenus, either horizontal
           direction. Clock and weather paths keep their parent structure. */
        switch(page) {
        case 1: case 2: case 15: swipe_candidate_page=0; break;
        case 4: case 9: case 10: swipe_candidate_page=2; break;
        case 5: swipe_candidate_page=4; break;
        case 7: swipe_candidate_page=6; break;
        case 8: case 12: swipe_candidate_page=7; break;
        case 13: swipe_candidate_page=12; break;
        default: break;
        }
        swipe_candidate_animation = LV_SCR_LOAD_ANIM_OVER_RIGHT;
    }
    if (swipe_candidate_page >= 0)
        touch_pressed_at_ms = 0; /* Never treat a swipe as a settings hold. */
}

static void observe_touch_position(void)
{
    if(recognition_touch_locked())return;
    if (!touch_held || !gesture_origin_valid || !gesture_indev) return;
    lv_point_t current={0};
    lv_indev_get_point(gesture_indev, &current);
    int dx=current.x-gesture_origin.x;
    int dy=current.y-gesture_origin.y;
    int distance_sq=dx*dx+dy*dy;
    if (page == 6 && distance_sq > LF_HOLD_MOVE_CANCEL_PX*LF_HOLD_MOVE_CANCEL_PX)
        touch_pressed_at_ms=0;
    if (distance_sq < LF_SWIPE_MIN_PX*LF_SWIPE_MIN_PX) return;
    if(abs(dx)>abs(dy))swipe_intent(dx<0?LV_DIR_LEFT:LV_DIR_RIGHT);
    else swipe_intent(dy<0?LV_DIR_TOP:LV_DIR_BOTTOM);

}

static void finish_touch(void)
{
    if (!touch_held) return;
    observe_touch_position();
    const int target=swipe_candidate_page;
    const lv_screen_load_anim_t animation=swipe_candidate_animation;
    touch_held=false;
    gesture_origin_valid=false;
    gesture_indev=NULL;
    touch_pressed_at_ms=0;
    swipe_candidate_page=-1;
    swipe_candidate_animation=LV_SCR_LOAD_ANIM_NONE;
    if (target >= 0 && !recognition_touch_locked())
        queue_swipe(target, animation);
    activity();
}

static void on_gesture_press(lv_event_t *e)
{
    (void)e;
    if (recognition_touch_locked() || !is_swipe_page() ||
        clock_settings_popup || touch_held) return;
    lv_indev_t *indev=lv_indev_active();
    if (!indev || lv_indev_get_type(indev)!=LV_INDEV_TYPE_POINTER) return;
    atomic_fetch_add(&ui_touch_count,1);
    gesture_indev=indev;
    gesture_origin_valid=true;
    lv_indev_get_point(indev, &gesture_origin);
    touch_held=true;
    touch_pressed_at_ms=now_ms();
    swipe_candidate_page=-1;
    swipe_candidate_animation=LV_SCR_LOAD_ANIM_NONE;
    activity();
}
static void on_touch_pressing(lv_event_t *e)
{
    (void)e;
    observe_touch_position();
}
static void on_lvgl_gesture(lv_event_t *e)
{
    (void)e;
    if (recognition_touch_locked() || !touch_held ||
        !gesture_origin_valid || clock_settings_popup) return;
    lv_indev_t *indev=lv_indev_active();
    if (!indev || indev!=gesture_indev) return;
    /* A button can generate LV_EVENT_GESTURE after just a small move.
       Require the physical 38px page-swipe threshold before accepting
       LVGL's gesture direction; otherwise a tap becomes navigation. */
    observe_touch_position();
    lv_point_t current={0};
    lv_indev_get_point(indev,&current);
    const int dx=current.x-gesture_origin.x;
    const int dy=current.y-gesture_origin.y;
    if(dx*dx+dy*dy < LF_SWIPE_MIN_PX*LF_SWIPE_MIN_PX) return;
    /* If direct position tracking selected the destination already, don't
       issue a second intent for the same physical gesture. */
    if(swipe_candidate_page<0)
        swipe_intent(lv_indev_get_gesture_dir(indev));
}
static void on_touch_release(lv_event_t *e)
{
    (void)e;
    finish_touch();
}
static void on_touch_cancel(lv_event_t *e)
{
    (void)e;
    /* LV_EVENT_PRESS_LOST means the finger left the original widget.
       Do NOT clear gesture state: release may be sent elsewhere or nowhere. */
    observe_touch_position();
}
static void on_popup_cancel(lv_event_t *e)
{
    (void)e;
    lv_obj_t *popup=clock_settings_popup;
    clock_settings_popup=NULL;
    if (popup) lv_obj_delete_async(popup);
}
static void on_popup_settings(lv_event_t *e)
{
    (void)e;
    clock_settings_popup=NULL;
    clock_settings_return_page=6;
    build_clock_settings();
}
static void show_clock_settings_popup(void)
{
    if (page!=6 || clock_settings_popup) return;
    touch_held=false;
    gesture_origin_valid=false;
    touch_pressed_at_ms=0;
    lv_obj_t *overlay=lv_obj_create(screen);
    clock_settings_popup=overlay;
    lv_obj_set_size(overlay, 466, 466);
    lv_obj_center(overlay);
    lv_obj_set_style_radius(overlay, 0, 0);
    lv_obj_set_style_bg_color(overlay, lv_color_hex(0x06111D), 0);
    lv_obj_set_style_bg_opa(overlay, LV_OPA_80, 0);
    lv_obj_set_style_border_width(overlay, 0, 0);
    lv_obj_remove_flag(overlay, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(overlay, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_t *panel=lv_obj_create(overlay);
    lv_obj_set_size(panel, 322, 220);
    lv_obj_center(panel);
    lv_obj_set_style_radius(panel, 23, 0);
    lv_obj_set_style_bg_color(panel,lv_color_hex(0x13283F),0);
    lv_obj_set_style_border_width(panel,1,0);
    lv_obj_set_style_border_color(panel,lv_color_hex(0x497DA7),0);
    lv_obj_remove_flag(panel,LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_t *heading=lv_label_create(panel);
    lv_label_set_text(heading,"CLOCK SETTINGS");
    lv_obj_set_style_text_font(heading,&lv_font_montserrat_24,0);
    lv_obj_set_style_text_color(heading,lv_color_hex(0xEAF6FF),0);
    lv_obj_align(heading,LV_ALIGN_TOP_MID,0,12);
    lv_obj_t *sub=lv_label_create(panel);
    lv_label_set_text(sub,"Open settings on this device?");
    lv_obj_set_style_text_font(sub,&lv_font_montserrat_16,0);
    lv_obj_set_style_text_color(sub,lv_color_hex(0xBDD4E8),0);
    lv_obj_align(sub,LV_ALIGN_TOP_MID,0,60);
    lv_obj_t *cancel = button(panel,"CANCEL",125,50,-72,68,0xB4C7DB,on_popup_cancel,NULL);
    lv_obj_t *settings = button(panel,"SETTINGS",125,50,72,68,0x82C8FF,on_popup_settings,NULL);
    /* Fixed action font so the longer SETTINGS label never wraps at 24pt. */
    lv_obj_t *c_text=lv_obj_get_child(cancel,0);
    lv_obj_t *s_text=lv_obj_get_child(settings,0);
    if(c_text) {
        lv_obj_set_style_text_font(c_text,&lv_font_montserrat_16,0);
        lv_label_set_long_mode(c_text,LV_LABEL_LONG_CLIP);
    }
    if(s_text) {
        lv_obj_set_style_text_font(s_text,&lv_font_montserrat_16,0);
        lv_label_set_long_mode(s_text,LV_LABEL_LONG_CLIP);
    }
}
/* Atomically swap a single full-screen root. Old screen cleanup is deferred
   until LVGL finishes processing the callback that initiated navigation. */
/* A successful song keeps the working Home/Clock/Settings pager cached.
   Close the lyrics child without tearing down that pager or its touch tree. */
static void lyrics_overlay_close(void)
{
    if(!timed_overlay)return;
    timed_profile_save_if_dirty(); /* preserve song settings before scene cleanup */
    /* Feedback timer cannot retain a pointer to the discarded lyric buttons. */
    if(timed_nudge_feedback_timer){
        lv_timer_delete(timed_nudge_feedback_timer);
        timed_nudge_feedback_timer=NULL;
    }
    timed_nudge_feedback_button=NULL;
    auto_open_pending=false;
    auto_open_transition=false;
    if(home_record_layer)lv_obj_add_flag(home_record_layer,LV_OBJ_FLAG_HIDDEN);
    if(timed_scroll_timer){
        lv_timer_delete(timed_scroll_timer);
        timed_scroll_timer=NULL;
    }
    lv_obj_delete(timed_overlay);
    timed_overlay=NULL;
    timed_lyrics_mode=false;
    timed_viewport=NULL;
    timed_scroll_group=NULL;
    timed_visible_future_mask=0xFF;
    for(size_t i=0;i<4;i++)timed_rows[i]=NULL;
    timed_status=NULL;
    timed_pause_button=NULL;
    timed_controls=NULL;
    timed_hint=NULL;
    timed_battery=NULL;
    timed_save_note=NULL;
    timed_sync_slider=NULL;timed_rate_slider=NULL;
    timed_sync_value=NULL;timed_rate_value=NULL;
    timed_line_count=0;
    timed_last_short_tap_ms=0;
    atomic_store(&ui_lyrics_scene_phase,0U);
    if(primary_pager)lv_obj_remove_flag(primary_pager,LV_OBJ_FLAG_HIDDEN);
}
static void make_base(void)
{
    if(timed_overlay)lyrics_overlay_close();
    if(timed_nudge_feedback_timer){
        lv_timer_delete(timed_nudge_feedback_timer);
        timed_nudge_feedback_timer=NULL;
    }
    timed_nudge_feedback_button=NULL;
    /* A timer that renders the retired lyric scene must NEVER outlive it. */
    if(timed_scroll_timer){
        lv_timer_delete(timed_scroll_timer);
        timed_scroll_timer=NULL;
    }
    /* This child is destroyed by the regular root cleanup below. */
    timed_overlay=NULL;
    /* One LVGL screen for the lifetime of the application. Rebuild its child
       tree ONLY in a deferred navigation callback. Unlike swapping screen
       roots, this cannot leave old framebuffer roots layered after touches. */
    lf_weather_visual_clear();
    /* All screen mutation happens after LVGL input dispatch via nav_drain.
       A stale clock is deleted only after its last active event has finished. */
    if (clock_scene && (clock_scene_stale || clock_scene_style != prefs->clock_face)) {
        lf_clock_clear();
        lv_obj_delete(clock_scene);
        clock_scene=NULL;
    }
    /* Preserve the cached clock across visits to Weather/settings subpages.
       Reparent before deleting the pager that previously owned it. */
    if(primary_pager) {
        /* Pager has a persistent frame timer. Delete it BEFORE removing
           the pager objects so no callback can reference freed children. */
        if(primary_pager_frame_timer){
            lv_timer_delete(primary_pager_frame_timer);
            primary_pager_frame_timer=NULL;
        }
        primary_pager_frame_pending=false;
        pager_final_frame_ready=false;
        pager_render_scroll_x=-1;
        pager_arrived_at_ms=0;
        if(clock_scene && lv_obj_get_parent(clock_scene)!=screen) {
            lv_obj_set_parent(clock_scene,screen);
            lv_obj_add_flag(clock_scene,LV_OBJ_FLAG_HIDDEN);
        }
        primary_pager=NULL;
        primary_pager_pages[0]=primary_pager_pages[1]=primary_pager_pages[2]=NULL;
        primary_pager_snapping=false;
        pager_scrolling=false;
    }
    if (!screen) {
        lv_obj_t *old_screen=lv_screen_active();
        screen=lv_obj_create(NULL);
        lv_screen_load(screen);
        if(old_screen && old_screen!=screen)lv_obj_delete_async(old_screen);
        lv_obj_add_event_cb(screen,on_gesture_press,LV_EVENT_PRESSED,NULL);
        lv_obj_add_event_cb(screen,on_touch_pressing,LV_EVENT_PRESSING,NULL);
        lv_obj_add_event_cb(screen,on_touch_release,LV_EVENT_RELEASED,NULL);
        lv_obj_add_event_cb(screen,on_touch_cancel,LV_EVENT_PRESS_LOST,NULL);
        lv_obj_add_event_cb(screen,on_lvgl_gesture,LV_EVENT_GESTURE,NULL);
    } else {
        /* Preserve the clock scene as one hidden child while other pages
           rebuild. This retains LVGL's safe single-root architecture. */
        uint32_t children=lv_obj_get_child_count(screen);
        while (children > 0) {
            lv_obj_t *child=lv_obj_get_child(screen,--children);
            if (child != clock_scene) lv_obj_delete(child);
        }
        if (clock_scene) lv_obj_add_flag(clock_scene,LV_OBJ_FLAG_HIDDEN);
    }
    sleep_overlay=NULL;
    lv_obj_set_style_bg_color(screen,lv_color_hex(prefs->background_color),0);
    lv_obj_set_style_bg_opa(screen,LV_OPA_COVER,0);
    lv_obj_set_style_border_width(screen,0,0);
    lv_obj_set_style_radius(screen,0,0);
    lv_obj_set_style_pad_all(screen,0,0);
    lv_obj_remove_flag(screen,LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(screen,LV_OBJ_FLAG_CLICKABLE);
    screen_animation_until_ms=0;
    touch_held=false;
    gesture_origin_valid=false;
    gesture_indev=NULL;
    swipe_candidate_page=-1;
    swipe_candidate_animation=LV_SCR_LOAD_ANIM_NONE;
    touch_pressed_at_ms=0;
    clock_settings_popup=NULL;
    debug_text=NULL;
    scroll_panel=NULL;
    lyrics_label=NULL;
    timed_lyrics_mode=false;
    for(size_t i=0;i<4;i++)timed_rows[i]=NULL;
    timed_viewport=NULL;
    timed_scroll_group=NULL;
    timed_visible_future_mask=0xFF;
    timed_scroll_y=INT16_MIN;
    for(size_t i=0;i<4;i++)timed_row_opa[i]=255;
    timed_status=NULL;
    timed_pause_button=NULL;
    timed_controls=NULL;
    timed_hint=NULL;
    timed_battery=NULL;
    timed_power_update_ms=0;
    timed_save_note=NULL;
    timed_sync_slider=NULL;timed_rate_slider=NULL;
    timed_sync_value=NULL;timed_rate_value=NULL;
    timed_hint_until_ms=0;
    timed_controls_until_ms=0;
    timed_last_short_tap_ms=0;
    home_scan_button=NULL;
    home_scan_status=NULL;
    home_song_title=NULL;
    home_song_artist=NULL;
    home_lyric_line=NULL;
    home_play_symbol=NULL;
    home_play_hint=NULL;
    home_record_layer=NULL;
    home_record_wave=NULL;
    for(unsigned i=0;i<LF_RECORD_WAVE_SEGMENTS;i++)home_record_wave_segments[i]=NULL;
    home_record_centerline=NULL;
    home_record_loading=NULL;
    home_record_caption=NULL;
    home_record_attempt=0;
    home_record_last_frames=0;
    home_record_last_tick=0;
    memset(home_record_wave_counts,0,sizeof(home_record_wave_counts));
    home_last_refresh_ms=0;
    home_visual_busy=-1;
    /* home_stalled_attempt is scan-id scoped and survives ordinary Home
       page rebuilds until lf_recog_start increments the session number. */
    battery_status=NULL;
    network_status=NULL;
    network_password=NULL;
    network_url=NULL;
    ota_status=NULL;
    wifi_list=NULL;
    wifi_status=NULL;
    wifi_password_status=NULL;
    pause_button=NULL;
}

static void on_wifi(lv_event_t *e)
{
    (void)e; activity(); build_wifi();
}
static void on_connection_help(lv_event_t *e)
{
    (void)e; activity(); build_connection_help();
}
static void build_connection_help_now(void)
{
    /* Local connection portal: no PIN or touch authorization required. */
    page = 15;
    make_base();
    label(screen, "CONNECT / UPDATE", 37, &lv_font_montserrat_24, prefs->text_color);
    network_status = label(screen, "Checking Wi-Fi...", 92, &lv_font_montserrat_16, 0xE4EDF3);
    network_password = label(screen, "", 126, &lv_font_montserrat_16, 0xDDE9FB);
    network_url = label(screen, "", 161, &lv_font_montserrat_16, 0x8BCCFF);
    ota_status = label(screen, "Web settings and OTA: no PIN required", 215,
                       &lv_font_montserrat_16, 0x8BCCFF);
    button(screen, "HOME", 154, 49, 0, 172, 0xB2C6D3, on_home, NULL);
    last_power_refresh_ms = 0; /* Refresh the network details immediately. */
}

/* A quiet, full-screen player instead of navigation buttons.
 * Home deliberately presents one large, circular PLAY control. The Clock
 * and Settings pages are still reached by swiping the proven LVGL pager.
 * A tap on song text opens the timed-lyrics viewer after recognition. */
static void home_set_text(lv_obj_t *obj,const char *value)
{
    if(!obj||!value)return;
    const char *shown=lv_label_get_text(obj);
    if(!shown||strcmp(shown,value)!=0)lv_label_set_text(obj,value);
}
static lv_obj_t *home_caption(lv_obj_t *parent,const char *value,int y,
                              int width,const lv_font_t *font,uint32_t color)
{
    lv_obj_t *item=lv_label_create(parent);
    lv_label_set_text(item,value);
    lv_obj_set_size(item,width,34);
    lv_obj_set_style_text_font(item,font,0);
    lv_obj_set_style_text_color(item,lv_color_hex(color),0);
    lv_obj_set_style_text_align(item,LV_TEXT_ALIGN_CENTER,0);
    lv_label_set_long_mode(item,LV_LABEL_LONG_DOT);
    lv_obj_align(item,LV_ALIGN_TOP_MID,0,y);
    lv_obj_add_flag(item,LV_OBJ_FLAG_EVENT_BUBBLE);
    lv_obj_add_flag(item,LV_OBJ_FLAG_GESTURE_BUBBLE);
    return item;
}
/* A stale touch_held flag is not evidence of a finger on glass.
 * Query the actual LVGL pointer before opening, then retry automatically. */
static bool ui_pointer_is_down(void)
{
    lv_indev_t *ptr=lv_indev_get_next(NULL);
    while(ptr){
        if(lv_indev_get_type(ptr)==LV_INDEV_TYPE_POINTER &&
           lv_indev_get_state(ptr)==LV_INDEV_STATE_PRESSED)
            return true;
        ptr=lv_indev_get_next(ptr);
    }
    return false;
}
static void auto_open_lyrics_deferred(void *unused)
{
    (void)unused;
    lf_recog_snapshot_t latest={0};
    lf_recog_snapshot(&latest);
    if(latest.state!=LF_RECOG_MATCH ||
       latest.attempts==auto_open_attempt ||
       latest.lyric_lines==0 || strcmp(latest.lyrics_state,"synced")!=0 ||
       page!=0 || asleep || nav_queued || nav_transition_active ||
       ui_pointer_is_down()){
        auto_open_pending=false;
        return;
    }
    /* Never mark a song handled before the target scene exists. The
       previous code did this and could strand the device on Home if the
       pending navigation was superseded or a touch release was missed. */
    auto_open_transition=true;
    auto_open_requested_ms=now_ms();
    build_preview();
}
/* A tiny amplitude envelope is published by the manual capture worker.
 * Draw only a bounded yellow line inside the existing Home pager. No PCM
 * samples or background microphone access cross into LVGL. */
static void home_recording_update(const lf_recog_snapshot_t *song,uint64_t current)
{
    if(!song || !home_record_layer)return;
    /* No LVGL display changes from the recording refresh while Stop is
       queued. In particular the 75ms waveform timer must not race the
       cancellation timer and invalidate the same full-screen object. */
    if(home_record_cancel_pending_attempt==song->attempts)return;
    /* The physical pager must not scroll during recording/upload/lookup.
       Re-enable once the scan has finished and the finger is released. */
    if(primary_pager && page==0){
        const bool locked=song->attempts!=home_stalled_attempt &&
            (song->state==LF_RECOG_CAPTURE ||
             song->state==LF_RECOG_UPLOAD || song->state==LF_RECOG_LYRICS);
        if(locked)lv_obj_remove_flag(primary_pager,LV_OBJ_FLAG_SCROLLABLE);
        else if(!ui_pointer_is_down())
            lv_obj_add_flag(primary_pager,LV_OBJ_FLAG_SCROLLABLE);
    }
    /* This UI-only deadline is independent of HTTP socket timeouts and the
       recognition worker. A wedged request must release the visible loading
       overlay so the screen remains usable, even before worker cleanup. */
    /* HTTP writes can stop halfway through a large stereo WAV. Do not hold
       the round recording overlay for the entire 60-second global budget:
       after 18 seconds without ANY successful upload bytes, cancel that
       attempt and expose an actionable Home warning. Worker cancellation is
       cooperative; a blocked socket uses its own bounded 15s timeout. */
    const bool upload_stalled=song->state==LF_RECOG_UPLOAD &&
        song->upload_bytes_total>0 &&
        song->upload_bytes_sent<song->upload_bytes_total &&
        song->upload_progress_ms && current>song->upload_progress_ms &&
        current-song->upload_progress_ms>18000U;
    /* A complete WAV upload does not imply AudD responded. Recover the
       display if the service never supplies the song ID. */
    const bool response_stalled=song->state==LF_RECOG_UPLOAD &&
        song->upload_bytes_total>0 &&
        song->upload_bytes_sent>=song->upload_bytes_total &&
        song->upload_progress_ms && current>song->upload_progress_ms &&
        current-song->upload_progress_ms>30000U;
    const bool overdue=song->stage_started_ms &&
        current>song->stage_started_ms &&
        ((song->state==LF_RECOG_CAPTURE &&
          current-song->stage_started_ms>30000U) ||
         (song->state==LF_RECOG_UPLOAD &&
          current-song->stage_started_ms>65000U) ||
         (song->state==LF_RECOG_LYRICS &&
          current-song->stage_started_ms>40000U));
    if((overdue || upload_stalled || response_stalled) && song->attempts!=home_stalled_attempt){
        home_stalled_attempt=song->attempts;
        home_warning=upload_stalled?"Upload stalled - retry":
            response_stalled?"Song ID response timed out - retry":
            "Identification timed out";
        home_warning_until_ms=current+10000U;
        lf_recog_stop(); /* cooperative cancel; no UI mutex or audio read */
        ESP_LOGW(TAG,"Recognition stage exceeded UI deadline, attempt=%lu",
                 (unsigned long)song->attempts);
    }
    if(home_stalled_attempt==song->attempts){
        /* A double-tap makes this attempt stale *during* a short-click
           callback. Never hide its touch target while the physical pointer
           is pressed, even from the independent 750ms Home refresh timer.
           On release the next UI tick safely retires the overlay. */
        if(!ui_pointer_is_down() &&
           !lv_obj_has_flag(home_record_layer,LV_OBJ_FLAG_HIDDEN))
            lv_obj_add_flag(home_record_layer,LV_OBJ_FLAG_HIDDEN);
        return;
    }
    const bool capturing=song->state==LF_RECOG_CAPTURE;
    const bool working=song->state==LF_RECOG_UPLOAD ||
                       song->state==LF_RECOG_LYRICS;
    const bool opening=song->state==LF_RECOG_MATCH &&
                       song->lyric_lines>0 &&
                       strcmp(song->lyrics_state,"synced")==0 &&
                       song->attempts!=auto_open_attempt;
    /* Hold LOADING on screen until deferred lyrics really open. */
    if(!capturing&&!working&&!opening){
        /* The recording worker can exit while the second finger is still
           down. Its completion must not invalidate the touched overlay. */
        if(!ui_pointer_is_down())
            lv_obj_add_flag(home_record_layer,LV_OBJ_FLAG_HIDDEN);
        return;
    }
    if(lv_obj_has_flag(home_record_layer,LV_OBJ_FLAG_HIDDEN)){
        lv_obj_remove_flag(home_record_layer,LV_OBJ_FLAG_HIDDEN);
        lv_obj_move_foreground(home_record_layer);
    }
    /* Only expose strips containing captured audio. A static waveform does
       not keep invalidating the full display while identifying. */
    for(unsigned i=0;i<LF_RECORD_WAVE_SEGMENTS;i++){
        lv_obj_t *strip=home_record_wave_segments[i];
        if(!strip)continue;
        if(capturing && (i==0 || home_record_wave_counts[i]>1))
            lv_obj_remove_flag(strip,LV_OBJ_FLAG_HIDDEN);
        else lv_obj_add_flag(strip,LV_OBJ_FLAG_HIDDEN);
    }
    if(home_record_centerline){
        if(capturing)lv_obj_remove_flag(home_record_centerline,LV_OBJ_FLAG_HIDDEN);
        else lv_obj_add_flag(home_record_centerline,LV_OBJ_FLAG_HIDDEN);
    }
    if(home_record_loading){
        if(capturing)lv_obj_add_flag(home_record_loading,LV_OBJ_FLAG_HIDDEN);
        else lv_obj_remove_flag(home_record_loading,LV_OBJ_FLAG_HIDDEN);
    }
    if(home_record_caption){
        char uploading[40]={0};
        const char *word=capturing?"LISTENING":
            song->state==LF_RECOG_UPLOAD?"IDENTIFYING":"FINDING LYRICS";
        if(song->state==LF_RECOG_UPLOAD && song->upload_bytes_total){
            if(song->upload_bytes_sent < song->upload_bytes_total){
                const unsigned pct=(unsigned)(((uint64_t)song->upload_bytes_sent*100U)/
                                               song->upload_bytes_total);
                snprintf(uploading,sizeof uploading,"UPLOADING %u%%",pct);
                word=uploading;
            }else{
                /* A live seconds indicator distinguishes slow AudD from
                   a frozen screen without changing microphone behavior. */
                const unsigned waited=(unsigned)(song->stage_started_ms &&
                    current>=song->stage_started_ms?
                    (current-song->stage_started_ms)/1000U:0U);
                snprintf(uploading,sizeof uploading,"AWAITING SONG ID %us",waited);
                word=uploading;
            }
        }
        if(strcmp(lv_label_get_text(home_record_caption),word)!=0)
            lv_label_set_text(home_record_caption,word);
    }
    if(!capturing||!home_record_wave)return;
    if(song->attempts!=home_record_attempt){
        home_record_attempt=song->attempts;
        home_record_cancel_tap_ms=0;
        home_record_wave_bias_q8=0;
        home_record_last_frames=UINT32_MAX;
        home_record_wave_last_y=130;
        memset(home_record_wave_counts,0,sizeof(home_record_wave_counts));
        for(unsigned i=0;i<LF_RECORD_WAVE_SEGMENTS;i++){
            home_record_wave_counts[i]=1;
            home_record_wave_points[i][0]=(lv_point_precise_t){0,130};
            lv_line_set_points(home_record_wave_segments[i],
                               home_record_wave_points[i],1);
            if(i)lv_obj_add_flag(home_record_wave_segments[i],LV_OBJ_FLAG_HIDDEN);
        }
        home_record_last_tick=0;
    }
    /* This visual is deliberately sampled at ~7 Hz, rather than redrawing
       a growing 360x260 line 13 times/second. Stereo PCM, AudD uploads
       and the 10-second capture are completely untouched. */
    if(current-home_record_last_tick<LF_RECORD_WAVE_REFRESH_MS ||
       song->captured_frames==home_record_last_frames)return;
    home_record_last_tick=current;
    home_record_last_frames=song->captured_frames;
    /* X follows the real 10-second capture; fewer visual samples do not
       change its timing or the recording used for recognition. */
    const uint32_t x=(uint32_t)(((uint64_t)song->captured_frames*360U)/
                                LF_SCAN_MAX_FRAMES);
    /* Real microphone envelope, never an invented waveform or raw PCM. */
    const int signed_peak=(int)song->capture_visual_signed;
    /* UI-only DC correction and the established, larger amplitude. */
    home_record_wave_bias_q8+=((signed_peak*256)-home_record_wave_bias_q8)/8;
    const int centered=signed_peak-home_record_wave_bias_q8/256;
    const int magnitude=centered<0?-centered:centered;
    int swing=magnitude*8;
    if(swing>108)swing=108;
    int y=130+(centered<0?swing:-swing);
    if(y<12)y=12;
    if(y>248)y=248;
    const unsigned strip=x>=360U?LF_RECORD_WAVE_SEGMENTS-1U:
                                      x/LF_RECORD_WAVE_STRIP_WIDTH;
    const int32_t local_x=(int32_t)x-(int32_t)(strip*LF_RECORD_WAVE_STRIP_WIDTH);
    uint8_t count=home_record_wave_counts[strip];
    if(count==1 && strip>0){
        /* Continue the previous strip's last envelope at the seam. */
        home_record_wave_points[strip][0].y=home_record_wave_last_y;
    }
    if(count<LF_RECORD_WAVE_STRIP_POINTS){
        home_record_wave_points[strip][count++]=(lv_point_precise_t){local_x,y};
    }else{
        /* Preserve the latest position if an exceptionally long capture
           delivers more screen samples than the bounded strip can hold. */
        home_record_wave_points[strip][count-1]=(lv_point_precise_t){local_x,y};
    }
    home_record_wave_counts[strip]=count;
    home_record_wave_last_y=(uint8_t)y;
    lv_obj_remove_flag(home_record_wave_segments[strip],LV_OBJ_FLAG_HIDDEN);
    /* Only this 90x260 strip becomes dirty, not the completed graph.
       The active draw cost stays bounded even near ten seconds. */
    lv_line_set_points(home_record_wave_segments[strip],
                       home_record_wave_points[strip],count);
}

/* The Home screen is intentionally sparse. The Play symbol conveys idle,
 * recording/stop and ready states; duplicate TAP PLAY/READY/MIC OFF captions
 * caused overlap on the round AMOLED. Show only actionable errors beneath
 * the large button, leaving recognized lyrics to open automatically. */
static void home_refresh(uint64_t current)
{
    if(!home_song_title||!home_scan_button||!home_play_symbol)return;
    lf_recog_snapshot_t song={0};
    lf_recog_snapshot(&song);
    /* The recording overlay is still the topmost touch surface until the
       dedicated cancel tick retires it. Defer ALL Home-style mutations. */
    if(home_record_cancel_pending_attempt==song.attempts)return;
    const bool busy=lf_recog_busy();
    home_recording_update(&song,current);
    const char *message="";
    if(home_scan_armed)message="GET READY";
    else if(home_warning && current<home_warning_until_ms)
        message=home_warning;
    else {
        home_warning=NULL;
        if(song.state==LF_RECOG_MATCH &&
           !(song.lyric_lines>0 && strcmp(song.lyrics_state,"synced")==0))
            message="No Song Lyrics Available";
        else if(song.state==LF_RECOG_NO_MATCH)
            message="Song Not Recognized";
        else if(song.state==LF_RECOG_ERROR)
            message="Identification Error";
    }
    home_set_text(home_song_title,message);
    /* Do not dirty Home when the empty caption was already hidden. */
    const bool hide_title=!message[0];
    if(lv_obj_has_flag(home_song_title,LV_OBJ_FLAG_HIDDEN)!=hide_title){
        if(hide_title)lv_obj_add_flag(home_song_title,LV_OBJ_FLAG_HIDDEN);
        else lv_obj_remove_flag(home_song_title,LV_OBJ_FLAG_HIDDEN);
    }
    home_set_text(home_play_symbol,(busy||home_scan_armed)?LV_SYMBOL_STOP:LV_SYMBOL_PLAY);
    if(home_visual_busy!=(int)busy){
        home_visual_busy=(int)busy;
        lv_obj_set_style_bg_color(home_scan_button,
            lv_color_hex(busy?0x0EBA8D:0x2884EA),0);
        lv_obj_set_style_border_color(home_scan_button,
            lv_color_hex(busy?0xFDD64D:0xAADFFF),0);
    }
    home_last_refresh_ms=current;
    if(!home_scan_armed && song.attempts!=home_stalled_attempt &&
       song.state==LF_RECOG_MATCH && song.lyric_lines>0 &&
       strcmp(song.lyrics_state,"synced")==0 &&
       song.attempts!=auto_open_attempt && !auto_open_pending){
        auto_open_pending=true;
        auto_open_requested_ms=current;
        atomic_fetch_add(&auto_open_queued_count,1);
        lv_async_call(auto_open_lyrics_deferred,NULL);
    }
}
static void on_home_scan(lv_event_t *e)
{
    (void)e;
    if(home_scan_armed) {
        home_scan_armed=false; /* second tap cancels before capture starts */
        home_scan_released_ms=0;
    }else if(lf_recog_busy()) {
        lf_recog_stop();
    }else if(!lf_network_is_connected() || !lf_recog_has_token()) {
        home_warning=!lf_network_is_connected()?
            "Connect to Wi-Fi in Settings":
            "Set AudD key in web settings";
        home_warning_until_ms=now_ms()+4500U;
    }else{
        /* LV_EVENT_CLICKED follows a finger release; on_tick independently
           confirms the physical pointer is up before starting the timer. */
        home_scan_armed=true;
        home_scan_released_ms=0;
        home_warning=NULL;
    }
    activity();
    home_refresh(now_ms());
}
/* Double short tap cancels recording/identification. An atomic stop request
 * is safe in the touch callback; LVGL mutations remain timer-deferred. */
static void on_record_cancel_double_tap(lv_event_t *e)
{
    (void)e;
    if(!home_record_layer ||
       lv_obj_has_flag(home_record_layer,LV_OBJ_FLAG_HIDDEN))return;
    lf_recog_snapshot_t scan={0};
    lf_recog_snapshot(&scan);
    if(!lf_recog_busy() ||
       (scan.state!=LF_RECOG_CAPTURE && scan.state!=LF_RECOG_UPLOAD &&
        scan.state!=LF_RECOG_LYRICS))return;
    const uint64_t current=now_ms();
    if(home_record_cancel_tap_ms && current>=home_record_cancel_tap_ms &&
       current-home_record_cancel_tap_ms<=400U){
        home_record_cancel_tap_ms=0;
        /* NEVER hide/refresh this LVGL touch target during its own callback.
           The periodic UI timer handles shutdown after the finger releases. */
        /* Block any late match from reopening lyrics after Stop. */
        home_stalled_attempt=scan.attempts;
        home_record_cancel_pending_attempt=scan.attempts;
        atomic_store(&last_record_cancel_ms,(uint32_t)current);
        atomic_store(&ui_record_cancel_phase,1U);
        /* This is only an atomic flag store; signal the worker immediately,
           without waiting for the UI timer or a later finger release.
           Never mutate or delete an LVGL touch target inside this callback. */
        lf_recog_stop(); /* atomic cooperative stop; no UI mutation */
    }else home_record_cancel_tap_ms=current;
}
static lv_obj_t *home_decor_ring(lv_obj_t *parent,int diameter,uint32_t color)
{
    lv_obj_t *ring=lv_obj_create(parent);
    lv_obj_set_size(ring,diameter,diameter);
    lv_obj_align(ring,LV_ALIGN_CENTER,0,LF_HOME_PLAYER_CENTER_Y);
    lv_obj_set_style_radius(ring,LV_RADIUS_CIRCLE,0);
    lv_obj_set_style_bg_opa(ring,LV_OPA_TRANSP,0);
    lv_obj_set_style_border_width(ring,2,0);
    lv_obj_set_style_border_color(ring,lv_color_hex(color),0);
    lv_obj_set_style_shadow_width(ring,0,0);
    lv_obj_set_style_pad_all(ring,0,0);
    lv_obj_remove_flag(ring,LV_OBJ_FLAG_CLICKABLE);
    lv_obj_remove_flag(ring,LV_OBJ_FLAG_SCROLLABLE);
    return ring;
}
static void primary_render_home_content(lv_obj_t *parent)
{
    /* A single flex header keeps the icon directly beside the wordmark,
       irrespective of glyph widths or round-screen clipping. The previous
       three independent absolute offsets separated logo and text. */
    lv_obj_t *brand_row=lv_obj_create(parent);
    lv_obj_set_size(brand_row,218,88);
    lv_obj_align(brand_row,LV_ALIGN_TOP_MID,0,47);
    lv_obj_set_style_bg_opa(brand_row,LV_OPA_TRANSP,0);
    lv_obj_set_style_border_width(brand_row,0,0);
    lv_obj_set_style_shadow_width(brand_row,0,0);
    lv_obj_set_style_pad_all(brand_row,0,0);
    lv_obj_remove_flag(brand_row,LV_OBJ_FLAG_CLICKABLE);
    lv_obj_remove_flag(brand_row,LV_OBJ_FLAG_SCROLLABLE);
    /* Explicit pixel anchors eliminate flex gaps on the round AMOLED. */
    /* The exact user-supplied LyricFlash icon, downsampled for the round
       AMOLED, not another hand-drawn cloud/bolt approximation. */
    lv_obj_t *mark=lv_obj_create(brand_row);
    lv_obj_set_size(mark,84,84);
    lv_obj_set_pos(mark,4,2);
    lv_obj_set_style_bg_opa(mark,LV_OPA_TRANSP,0);
    lv_obj_set_style_border_width(mark,0,0);
    lv_obj_set_style_pad_all(mark,0,0);
    lv_obj_remove_flag(mark,LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_remove_flag(mark,LV_OBJ_FLAG_CLICKABLE);
    (void)lf_brand_icon_create(mark);
    /* Two stacked lines sit to the right of the enlarged cloud-and-bolt.
       Compact vertical flex keeps both words within the round display. */
    lv_obj_t *wordmark_col=lv_obj_create(brand_row);
    lv_obj_set_size(wordmark_col,144,84);
    lv_obj_set_pos(wordmark_col,73,2); /* visibly tighter to the unchanged original mark */
    lv_obj_set_style_bg_opa(wordmark_col,LV_OPA_TRANSP,0);
    lv_obj_set_style_border_width(wordmark_col,0,0);
    lv_obj_set_style_pad_all(wordmark_col,0,0);
    lv_obj_set_style_pad_row(wordmark_col,0,0);
    lv_obj_remove_flag(wordmark_col,LV_OBJ_FLAG_CLICKABLE);
    lv_obj_remove_flag(wordmark_col,LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_flex_flow(wordmark_col,LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(wordmark_col,LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_START,LV_FLEX_ALIGN_CENTER);
    lv_obj_t *lyric_mark=lv_label_create(wordmark_col);
    lv_label_set_text(lyric_mark,"LYRIC");
    lv_obj_set_style_text_font(lyric_mark,&lv_font_montserrat_28,0);
    lv_obj_set_style_text_color(lyric_mark,lv_color_hex(0xFFFFFF),0);
    lv_obj_set_style_text_letter_space(lyric_mark,1,0);
    lv_obj_remove_flag(lyric_mark,LV_OBJ_FLAG_CLICKABLE);
    lv_obj_t *flash_mark=lv_label_create(wordmark_col);
    lv_label_set_text(flash_mark,"FLASH");
    lv_obj_set_style_text_font(flash_mark,&lv_font_montserrat_28,0);
    lv_obj_set_style_text_color(flash_mark,lv_color_hex(0x7BD3FF),0);
    lv_obj_set_style_text_letter_space(flash_mark,1,0);
    lv_obj_remove_flag(flash_mark,LV_OBJ_FLAG_CLICKABLE);
    battery_status=home_caption(parent,"",416,160,
                               &lv_font_montserrat_16,0x9ACFEA);
    /* Static concentric rings give the circular AMOLED a device-player
     * look without the continuous redraw cost of pulsing gradients. */
    (void)home_decor_ring(parent,200,0x163C65);
    (void)home_decor_ring(parent,184,0x23578D);
    home_scan_button=lv_button_create(parent);
    lv_obj_set_size(home_scan_button,154,154);
    lv_obj_align(home_scan_button,LV_ALIGN_CENTER,0,LF_HOME_PLAYER_CENTER_Y);
    lv_obj_set_style_radius(home_scan_button,LV_RADIUS_CIRCLE,0);
    lv_obj_set_style_bg_color(home_scan_button,lv_color_hex(0x2884EA),0);
    lv_obj_set_style_bg_opa(home_scan_button,LV_OPA_COVER,0);
    lv_obj_set_style_bg_color(home_scan_button,lv_color_hex(0xFFD24A),LV_STATE_PRESSED);
    /* Animated feedback without a continuously running full-screen effect:
       press grows the 154px button and warms its color, release eases back. */
    static const lv_style_prop_t press_props[]={
        LV_STYLE_BG_COLOR,LV_STYLE_TRANSFORM_WIDTH,
        LV_STYLE_TRANSFORM_HEIGHT,LV_STYLE_BORDER_COLOR,0};
    static lv_style_transition_dsc_t press_enter,press_exit;
    static lv_style_t press_style,release_style;
    static bool press_styles_ready;
    if(!press_styles_ready){
        lv_style_transition_dsc_init(&press_enter,press_props,
                                    lv_anim_path_ease_out,125,0,NULL);
        lv_style_transition_dsc_init(&press_exit,press_props,
                                    lv_anim_path_ease_out,160,50,NULL);
        lv_style_init(&press_style);
        lv_style_set_transition(&press_style,&press_enter);
        lv_style_set_transform_width(&press_style,12);
        lv_style_set_transform_height(&press_style,12);
        lv_style_set_bg_color(&press_style,lv_color_hex(0xFFD24A));
        lv_style_set_border_color(&press_style,lv_color_hex(0xFFFFFF));
        lv_style_init(&release_style);
        lv_style_set_transition(&release_style,&press_exit);
        press_styles_ready=true;
    }
    lv_obj_add_style(home_scan_button,&release_style,0);
    lv_obj_add_style(home_scan_button,&press_style,LV_STATE_PRESSED);
    lv_obj_set_style_border_width(home_scan_button,3,0);
    lv_obj_set_style_border_color(home_scan_button,lv_color_hex(0xAADFFF),0);
    lv_obj_set_style_shadow_width(home_scan_button,0,0);
    lv_obj_add_flag(home_scan_button,LV_OBJ_FLAG_EVENT_BUBBLE);
    lv_obj_add_flag(home_scan_button,LV_OBJ_FLAG_GESTURE_BUBBLE);
    lv_obj_add_event_cb(home_scan_button,on_home_scan,LV_EVENT_CLICKED,NULL);
    lv_obj_add_event_cb(home_scan_button,on_gesture_press,LV_EVENT_PRESSED,NULL);
    lv_obj_add_event_cb(home_scan_button,on_touch_pressing,LV_EVENT_PRESSING,NULL);
    lv_obj_add_event_cb(home_scan_button,on_touch_release,LV_EVENT_RELEASED,NULL);
    lv_obj_add_event_cb(home_scan_button,on_touch_cancel,LV_EVENT_PRESS_LOST,NULL);
    lv_obj_add_event_cb(home_scan_button,on_lvgl_gesture,LV_EVENT_GESTURE,NULL);
    home_play_symbol=lv_label_create(home_scan_button);
    lv_label_set_text(home_play_symbol,LV_SYMBOL_PLAY);
    lv_obj_set_style_text_font(home_play_symbol,&lv_font_montserrat_48,0);
    lv_obj_set_style_text_color(home_play_symbol,lv_color_hex(0xFFFFFF),0);
    lv_obj_align(home_play_symbol,LV_ALIGN_CENTER,3,0);
    lv_obj_remove_flag(home_play_symbol,LV_OBJ_FLAG_CLICKABLE);
    home_song_title=home_caption(parent,"",365,326,
                                 &lv_font_montserrat_18,0xB4DAFC);
    lv_obj_add_flag(home_song_title,LV_OBJ_FLAG_HIDDEN);
    /* Recording occupies the round AMOLED screen only after an explicit
       Play tap. It is also an invisible click target to STOP on demand. */
    home_record_layer=lv_obj_create(parent);
    lv_obj_set_size(home_record_layer,466,466);
    lv_obj_align(home_record_layer,LV_ALIGN_CENTER,0,0);
    lv_obj_set_style_bg_color(home_record_layer,lv_color_hex(0x020406),0);
    lv_obj_set_style_bg_opa(home_record_layer,LV_OPA_COVER,0);
    lv_obj_set_style_border_width(home_record_layer,0,0);
    lv_obj_set_style_radius(home_record_layer,0,0);
    lv_obj_set_style_pad_all(home_record_layer,0,0);
    lv_obj_remove_flag(home_record_layer,LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(home_record_layer,LV_OBJ_FLAG_CLICKABLE);
    /* Recording overlay is deliberately inert to touches: never stop
       the capture or bubble a pager gesture from this overlay. */
    lv_obj_remove_flag(home_record_layer,LV_OBJ_FLAG_EVENT_BUBBLE);
    lv_obj_remove_flag(home_record_layer,LV_OBJ_FLAG_GESTURE_BUBBLE);
    lv_obj_add_event_cb(home_record_layer,on_record_cancel_double_tap,
                        LV_EVENT_SHORT_CLICKED,NULL);
    lv_obj_t *record_ring=lv_obj_create(home_record_layer);
    lv_obj_set_size(record_ring,408,408);
    lv_obj_align(record_ring,LV_ALIGN_CENTER,0,-4);
    lv_obj_set_style_radius(record_ring,LV_RADIUS_CIRCLE,0);
    lv_obj_set_style_border_color(record_ring,lv_color_hex(0x158F50),0);
    lv_obj_set_style_border_width(record_ring,6,0);
    lv_obj_set_style_bg_opa(record_ring,LV_OPA_TRANSP,0);
    lv_obj_set_style_shadow_width(record_ring,0,0);
    lv_obj_remove_flag(record_ring,LV_OBJ_FLAG_CLICKABLE);
    lv_obj_remove_flag(record_ring,LV_OBJ_FLAG_SCROLLABLE);
    static const lv_point_precise_t baseline[]={{0,130},{360,130}};
    home_record_centerline=lv_line_create(home_record_layer);
    lv_line_set_points(home_record_centerline,baseline,2);
    lv_obj_set_size(home_record_centerline,360,260);
    lv_obj_align(home_record_centerline,LV_ALIGN_CENTER,0,-4);
    lv_obj_set_style_bg_opa(home_record_centerline,LV_OPA_TRANSP,0);
    lv_obj_set_style_line_color(home_record_centerline,lv_color_hex(0x258854),0);
    lv_obj_set_style_line_width(home_record_centerline,2,0);
    lv_obj_remove_flag(home_record_centerline,LV_OBJ_FLAG_CLICKABLE);
    /* Four native LVGL line children tile the same 360x260 waveform.
       Previously each new point invalidated the entire 360px graph, even
       though only its leading edge changed. No offscreen framebuffer. */
    for(unsigned i=0;i<LF_RECORD_WAVE_SEGMENTS;i++){
        lv_obj_t *strip=lv_line_create(home_record_layer);
        home_record_wave_segments[i]=strip;
        if(i==0)home_record_wave=strip;
        lv_obj_set_size(strip,LF_RECORD_WAVE_STRIP_WIDTH,260);
        lv_obj_align(strip,LV_ALIGN_CENTER,-135+(int32_t)i*90,-4);
        lv_obj_set_style_bg_opa(strip,LV_OPA_TRANSP,0);
        lv_obj_set_style_line_color(strip,lv_color_hex(0xFFD02C),0);
        lv_obj_set_style_line_width(strip,3,0);
        lv_obj_set_style_line_rounded(strip,true,0);
        lv_obj_remove_flag(strip,LV_OBJ_FLAG_CLICKABLE);
        home_record_wave_points[i][0]=(lv_point_precise_t){0,130};
        lv_line_set_points(strip,home_record_wave_points[i],1);
        home_record_wave_counts[i]=1;
        if(i)lv_obj_add_flag(strip,LV_OBJ_FLAG_HIDDEN);
    }
    home_record_wave_last_y=130;
    home_record_attempt=0;
    home_record_last_frames=UINT32_MAX;
    home_record_last_tick=0;
    home_record_caption=home_caption(home_record_layer,"LISTENING",356,206,
                                    &lv_font_montserrat_16,0x4A946F);
    home_record_loading=lv_label_create(home_record_layer);
    lv_label_set_text(home_record_loading,"IDENTIFYING...");
    lv_obj_set_width(home_record_loading,360);
    lv_obj_set_style_text_align(home_record_loading,LV_TEXT_ALIGN_CENTER,0);
    lv_obj_set_style_text_font(home_record_loading,&lv_font_montserrat_36,0);
    lv_obj_set_style_text_color(home_record_loading,lv_color_hex(0xF0F7FF),0);
    lv_obj_align(home_record_loading,LV_ALIGN_CENTER,0,-6);
    lv_obj_remove_flag(home_record_loading,LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(home_record_loading,LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(home_record_layer,LV_OBJ_FLAG_HIDDEN);

    /* Informational text is also the unobtrusive entry to the Lyrics page.
     * Swipes beginning on it still bubble through the primary pager. */
    /* The physical Play button is the only Home control. */
    /* Battery and connection details remain in LyricFlash Web UI and Settings.
     * Their previous Home labels and rectangular shortcut buttons are gone. */
    home_last_refresh_ms=0;
    home_visual_busy=-1;
    home_refresh(now_ms());
}
static void render_home(void)
{
    primary_pager_enter(1);
}
static void build_home_now(void)
{
    render_home();
}

static void on_toggle_pause(lv_event_t *e)
{
    (void)e; activity(); paused = !paused;
    if (!paused && scroll_panel) scroll_y = (float)lv_obj_get_scroll_y(scroll_panel);
    lv_obj_t *b = lv_event_get_target(e);
    lv_obj_t *l = lv_obj_get_child(b, 0);
    if (l) lv_label_set_text(l, paused ? "RESUME" : "PAUSE");
    last_scroll_ms = now_ms();
}

static void on_touch_scroll(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    /* Auto-scroll also generates scroll events. Only physical finger
       events may interrupt it; programmatic scrolling must never pause itself. */
    if (code == LV_EVENT_PRESSED) {
        user_dragging = true;
        finger_was_scrolling = false;
        touch_resume_at_ms = UINT64_MAX;
        activity();
    } else if (code == LV_EVENT_SCROLL_BEGIN) {
        if (user_dragging) finger_was_scrolling = true;
    } else if (code == LV_EVENT_RELEASED) {
        user_dragging = false;
        touch_resume_at_ms = now_ms() + LF_TOUCH_RESUME_DELAY_MS;
        if (!finger_was_scrolling && scroll_panel)
            scroll_y = (float)lv_obj_get_scroll_y(scroll_panel);
        last_scroll_ms = now_ms();
        activity();
    } else if (code == LV_EVENT_SCROLL_END && finger_was_scrolling) {
        finger_was_scrolling = false;
        if (scroll_panel) scroll_y = (float)lv_obj_get_scroll_y(scroll_panel);
        last_scroll_ms = now_ms();
        activity();
    }
}

/* The AudD/LRC clock remains the ONLY timing source. Rather than replacing
 * three lyric lines abruptly on timestamps, shift a retained four-line
 * viewport steadily upwards between two LRC timestamps. This moves each
 * line from "next" through "current" to "previous", allowing the listener
 * to anticipate words and still read the completed line above. No new
 * audio capture, network fetch, background thread, or display root. */
/* Uniform font and text width; no per-line shrink/enlarge animation. */
static void timed_set_row(int slot,const char *value)
{
    lv_obj_t *item=timed_rows[slot];
    if(!item)return;
    if(!value)value=" ";
    if(strcmp(lv_label_get_text(item),value)!=0)lv_label_set_text(item,value);
}
static uint16_t timed_line_height_at(int index)
{
    if(index<0 || (size_t)index>=timed_line_count)return 30U;
    return timed_line_heights[index];
}
static uint16_t timed_pitch(int from,int to)
{
    uint32_t gap=0;
    if(from>=0 && to>=0 && (size_t)to<timed_line_count &&
       (size_t)from<timed_line_count &&
       timed_stamps[to]>timed_stamps[from])
        gap=timed_stamps[to]-timed_stamps[from];
    return lf_sync_row_pitch_px(timed_line_height_at(from),
                                timed_line_height_at(to),gap);
}
static void timed_position_rows(int index)
{
    timed_row_centers[0]=-(int16_t)timed_pitch(index-1,index);
    timed_row_centers[1]=0;
    timed_row_centers[2]=(int16_t)timed_pitch(index,index+1);
    timed_row_centers[3]=timed_row_centers[2]+
                          (int16_t)timed_pitch(index+1,index+2);
    for(int slot=0;slot<4;slot++)
        if(timed_rows[slot])
            lv_obj_align(timed_rows[slot],LV_ALIGN_CENTER,0,timed_row_centers[slot]);
}
static uint8_t timed_future_mask(uint32_t ms,int index)
{
    uint8_t visible=0;
    if(!timed_clock.started)return 0;
    for(int slot=2;slot<4;slot++){
        const int line_index=index+slot-1;
        if(line_index>=0 && (size_t)line_index<timed_line_count &&
           (uint64_t)timed_stamps[line_index]<=(uint64_t)ms+3500ULL)
            visible|=(uint8_t)(1U<<slot);
    }
    return visible;
}
static uint32_t timed_song_position(uint64_t current)
{
    if(!timed_clock.started)return 0;
    const uint32_t base=lf_lyric_clock_position(&timed_clock,current);
    const uint32_t scaled=lf_sync_rate_position(base,timed_rate_clock_anchor_ms,
                                       timed_rate_song_anchor_ms,timed_rate_percent);
    return lf_sync_visible_position_ms(scaled);
}
static void timed_render(uint64_t current,bool force);
static void timed_layout_rows(uint32_t ms,int index)
{
    if(!timed_viewport||!timed_scroll_group)return;
    uint32_t progress=0;
    if(index>=0 && (size_t)(index+1)<timed_line_count)
        progress=lf_sync_slide_fraction(ms,timed_stamps[index],
                                        timed_stamps[index+1]);
    else if(index<0 && timed_line_count)
        progress=lf_sync_slide_fraction(ms,0,timed_stamps[0]);
    /* Hold over long gaps and move in the last second before the next LRC
       timestamp; the actual lyric switch stays pinned to its timestamp. */
    const int16_t pitch=timed_row_centers[2];
    const int16_t target=-(int16_t)((uint32_t)progress*(uint16_t)pitch/1024U);
    if(target!=timed_scroll_y){
        lv_obj_set_style_translate_y(timed_scroll_group,target,0);
        timed_scroll_y=target;
    }
    for(int slot=0;slot<4;slot++){
        lv_obj_t *row=timed_rows[slot];
        if(!row)continue;
        const int center=(int)timed_row_centers[slot]+target;
        const int distance=center<0?-center:center;
        const int room=180-distance;
        uint8_t opacity=room>=72?255:
            room<=0?0:(uint8_t)((room*255/72)&~15);
        if(timed_row_opa[slot]!=opacity){
            lv_obj_set_style_text_opa(row,opacity,0);
            timed_row_opa[slot]=opacity;
        }
    }
}
static void timed_scroll_frame(lv_timer_t *timer)
{
    (void)timer;
    if(page==1 && timed_lyrics_mode && !asleep && !nav_transition_active)
        timed_render(now_ms(),false);
}
static void timed_render(uint64_t current,bool force)
{
    /* The minimal player intentionally has no status label: its lyric compositor
       must continue advancing even when that optional decoration is absent. */
    if(!timed_lyrics_mode)return;
    if(timed_hint && timed_hint_until_ms && current>=timed_hint_until_ms){
        lv_obj_add_flag(timed_hint,LV_OBJ_FLAG_HIDDEN);
        timed_hint_until_ms=0;
    }
    if(timed_controls && timed_controls_until_ms &&
       current>=timed_controls_until_ms){
        lv_obj_add_flag(timed_controls,LV_OBJ_FLAG_HIDDEN);
        timed_controls_until_ms=0;
    }
    /* Cached AXP2101 status only: no I2C on the UI task. */
    if(timed_battery && (force || current-timed_power_update_ms>=1000)){
        timed_power_update_ms=current;
        lf_power_state_t battery={0};
        char battery_text[40];
        uint32_t battery_color=0xAFC6DC;
        if(lf_power_cached(&battery)){
            const char *symbol=battery.percentage<0?LV_SYMBOL_BATTERY_EMPTY:
                battery.percentage>=88?LV_SYMBOL_BATTERY_FULL:
                battery.percentage>=63?LV_SYMBOL_BATTERY_3:
                battery.percentage>=38?LV_SYMBOL_BATTERY_2:
                battery.percentage>=15?LV_SYMBOL_BATTERY_1:LV_SYMBOL_BATTERY_EMPTY;
            if(battery.percentage>=0)
                snprintf(battery_text,sizeof battery_text,"%s%s %d%%",
                    battery.charging?LV_SYMBOL_CHARGE:"",symbol,battery.percentage);
            else snprintf(battery_text,sizeof battery_text,"%s --%%",symbol);
            battery_color=battery.charging?0x42DC89:
                (battery.percentage>=0 && battery.percentage<=20)?0xFFD34D:0xAFC6DC;
        }else snprintf(battery_text,sizeof battery_text,"%s --%%",LV_SYMBOL_BATTERY_EMPTY);
        if(strcmp(lv_label_get_text(timed_battery),battery_text)!=0)
            lv_label_set_text(timed_battery,battery_text);
        lv_obj_set_style_text_color(timed_battery,lv_color_hex(battery_color),0);
    }
    lf_recog_snapshot_t state={0};
    lf_recog_snapshot(&state);
    if(state.attempts!=timed_scan_attempt || state.state!=LF_RECOG_MATCH ||
       strcmp(state.lyrics_state,"synced")!=0){
        timed_lyrics_mode=false;
        return;
    }
    /* Observed lyrics run ~1s ahead: adjust the rendered timeline only,
       keeping the user's saved manual clock offset independent. */
    const uint32_t ms=timed_song_position(current);
    const uint32_t second=ms/1000U;
    if(timed_status && (force||timed_last_second!=second)){
        char info[105];
        if(!timed_clock.started)
            snprintf(info,sizeof info,"HOLD TO OPEN ALIGNMENT CONTROLS");
        else{
            const int64_t offset=timed_clock.correction_ms<0?
                -(int64_t)timed_clock.correction_ms:(int64_t)timed_clock.correction_ms;
            snprintf(info,sizeof info,"SYNC %02lu:%02lu  |  offset %c%ld.%01lds",
                (unsigned long)(second/60),(unsigned long)(second%60),
                timed_clock.correction_ms<0?'-':'+',
                (long)(offset/1000),(long)((offset%1000)/100));
        }
        if(strcmp(lv_label_get_text(timed_status),info)!=0)
            lv_label_set_text(timed_status,info);
        timed_last_second=second;
    }
    const int index=timed_clock.started?
        lf_lyric_clock_line_at(timed_stamps,timed_line_count,ms):-1;
    const uint8_t future=timed_future_mask(ms,index);
    if(force||index!=timed_active_index||future!=timed_visible_future_mask){
        for(int slot=0;slot<4;slot++){
            const int line_index=index+slot-1;
            lf_lyric_line_t line={0};
            if(slot>=2 && !(future & (1U<<slot)))
                timed_set_row(slot," "); /* prevent early future verses */
            else if(line_index>=0 && (size_t)line_index<timed_line_count &&
                    lf_recog_lyric_line((uint32_t)line_index,&line))
                timed_set_row(slot,line.text[0]?line.text:" ");
            else if(slot==1 && index<0)
                timed_set_row(slot,timed_clock.started?
                    "Waiting for first lyric":"Hold for sync controls");
            else timed_set_row(slot," ");
        }
        if(force||index!=timed_active_index)timed_position_rows(index);
        timed_active_index=index;
        timed_visible_future_mask=future;
    }
    if(timed_clock.started)timed_layout_rows(ms,index);
}

static void timed_start(lv_event_t *e)
{
    (void)e;
    if(!timed_lyrics_mode)return;
    const uint64_t now=now_ms();
    const int32_t active_offset=timed_clock.correction_ms;
    lf_lyric_clock_start(&timed_clock,now);
    lf_lyric_clock_adjust(&timed_clock,active_offset);
    timed_rate_clock_anchor_ms=lf_lyric_clock_position(&timed_clock,now);
    timed_rate_song_anchor_ms=timed_rate_clock_anchor_ms;
    timed_estimated=false; /* Restart is deliberately manual alignment. */
    timed_controls_until_ms=0; /* user closes with DONE */
    timed_active_index=-999;timed_last_second=UINT32_MAX;
    paused=false;activity();
    if(timed_pause_button){
        lv_obj_t *caption=lv_obj_get_child(timed_pause_button,0);
        if(caption)lv_label_set_text(caption,"PAUSE");
    }
    timed_render(now,true);
}
static void timed_update_slider_labels(void)
{
    char text[42];
    if(timed_sync_value){
        const int32_t offset=timed_clock.correction_ms;
        snprintf(text,sizeof text,"Timing %c%ld.%02lds",
            offset<0?'-':'+',(long)(labs((long)offset)/1000),
            (long)((labs((long)offset)%1000)/10));
        lv_label_set_text(timed_sync_value,text);
    }
    if(timed_rate_value){
        snprintf(text,sizeof text,"Lyrics speed %u%%",(unsigned)timed_rate_percent);
        lv_label_set_text(timed_rate_value,text);
    }
}
/* Re-anchor the variable-speed song clock without an instantaneous jump. */
static void timed_change_rate(uint16_t new_rate)
{
    if(!timed_clock.started || new_rate<75 || new_rate>125)return;
    if(timed_rate_percent==new_rate)return;
    const uint64_t current=now_ms();
    const uint32_t base=lf_lyric_clock_position(&timed_clock,current);
    const uint32_t song=lf_sync_rate_position(base,timed_rate_clock_anchor_ms,
                                   timed_rate_song_anchor_ms,timed_rate_percent);
    timed_rate_clock_anchor_ms=base;
    timed_rate_song_anchor_ms=song;
    timed_rate_percent=new_rate;
    timed_profile_dirty=true;
    timed_active_index=-999;timed_last_second=UINT32_MAX;
    timed_update_slider_labels();
    activity();timed_render(current,true);
}
static void timed_change_offset(int32_t new_offset)
{
    if(!timed_clock.started)return;
    const int32_t delta=new_offset-timed_clock.correction_ms;
    if(!delta)return;
    const uint64_t current=now_ms();
    const uint32_t base=lf_lyric_clock_position(&timed_clock,current);
    const uint32_t song=lf_sync_rate_position(base,timed_rate_clock_anchor_ms,
                                   timed_rate_song_anchor_ms,timed_rate_percent);
    lf_lyric_clock_adjust(&timed_clock,delta);
    timed_profile_dirty=true;
    const int64_t updated=(int64_t)song+delta;
    timed_rate_clock_anchor_ms=lf_lyric_clock_position(&timed_clock,current);
    timed_rate_song_anchor_ms=(uint32_t)(updated<0?0:
                                            updated>UINT32_MAX?UINT32_MAX:updated);
    timed_active_index=-999;timed_last_second=UINT32_MAX;
    timed_update_slider_labels();
    activity();timed_render(current,true);
}
static void timed_on_sync_slide(lv_event_t *e)
{
    if(!timed_lyrics_mode || !timed_clock.started)return;
    int32_t v=lv_slider_get_value(lv_event_get_target(e));
    v=((v>=0?v+125:v-125)/250)*250; /* quarter-second steps */
    timed_change_offset(v);
}
static void timed_on_rate_slide(lv_event_t *e)
{
    if(!timed_lyrics_mode || !timed_clock.started)return;
    timed_change_rate((uint16_t)lv_slider_get_value(lv_event_get_target(e)));
}
static void timed_adjust(lv_event_t *e)
{
    if(!timed_lyrics_mode || !timed_clock.started)return;
    const intptr_t direction=(intptr_t)lv_event_get_user_data(e);
    timed_change_offset(timed_clock.correction_ms+(int32_t)(direction*500));
    if(timed_sync_slider)lv_slider_set_value(timed_sync_slider,timed_clock.correction_ms,LV_ANIM_OFF);
}
static void timed_profile_save_if_dirty(void)
{
    if(!timed_profile_dirty || !timed_lyrics_mode || !timed_clock.started)return;
    lf_recog_snapshot_t track={0};
    lf_recog_snapshot(&track);
    if(track.attempts!=timed_scan_attempt || !track.title[0] || !track.artist[0])
        return;
    if(lf_song_cache_store_profile(track.title,track.artist,
                                   timed_clock.correction_ms,timed_rate_percent)){
        timed_profile_dirty=false;
        timed_song_custom=true;
    }else{
        ESP_LOGW(TAG,"Per-track lyrics speed could not be saved; keeping it dirty");
    }
}
/* Persist the nudge glow briefly after finger release so a short tap visibly
   registers. One paused LVGL timer owns the flash; never hold touch callbacks. */
static void timed_nudge_feedback_end(lv_timer_t *timer)
{
    if(timed_nudge_feedback_button){
        lv_obj_remove_state(timed_nudge_feedback_button,LV_STATE_USER_1);
        timed_nudge_feedback_button=NULL;
    }
    lv_timer_pause(timer);
}
static void timed_nudge_feedback(lv_obj_t *control)
{
    if(timed_nudge_feedback_button && timed_nudge_feedback_button!=control)
        lv_obj_remove_state(timed_nudge_feedback_button,LV_STATE_USER_1);
    timed_nudge_feedback_button=control;
    lv_obj_add_state(control,LV_STATE_USER_1);
    if(!timed_nudge_feedback_timer)
        timed_nudge_feedback_timer=lv_timer_create(timed_nudge_feedback_end,280,NULL);
    else {
        lv_timer_reset(timed_nudge_feedback_timer);
        lv_timer_resume(timed_nudge_feedback_timer);
    }
}
static void timed_rate_nudge(lv_event_t *e)
{
    if(!timed_lyrics_mode || !timed_clock.started)return;
    const int step=(int)(intptr_t)lv_event_get_user_data(e);
    int value=(int)timed_rate_percent+step;
    if(value<75)value=75;
    if(value>125)value=125;
    timed_change_rate((uint16_t)value);
    if(timed_rate_slider)lv_slider_set_value(timed_rate_slider,value,LV_ANIM_OFF);
    timed_nudge_feedback(lv_event_get_current_target(e));
}
/* Large touch surfaces, compact glyphs, and visible pressed/confirmed glow. */
static void timed_add_nudge(lv_obj_t *parent,const char *glyph,int x,int step)
{
    lv_obj_t *control=lv_button_create(parent);
    lv_obj_set_size(control,72,64); /* was 40x40: 2.9x touch area */
    lv_obj_align(control,LV_ALIGN_CENTER,x,-112);
    lv_obj_set_style_radius(control,LV_RADIUS_CIRCLE,0);
    lv_obj_set_style_bg_color(control,lv_color_hex(0x274C65),0);
    lv_obj_set_style_bg_opa(control,LV_OPA_COVER,0);
    lv_obj_set_style_border_width(control,2,0);
    lv_obj_set_style_border_color(control,lv_color_hex(0x8DC9F3),0);
    lv_obj_set_style_shadow_width(control,0,0);
    /* Bright yellow during contact; blue glow lasts 280ms after a click. */
    lv_obj_set_style_bg_color(control,lv_color_hex(0xF4C746),LV_STATE_PRESSED);
    lv_obj_set_style_border_color(control,lv_color_white(),LV_STATE_PRESSED);
    lv_obj_set_style_shadow_width(control,14,LV_STATE_PRESSED);
    lv_obj_set_style_shadow_color(control,lv_color_hex(0xF4C746),LV_STATE_PRESSED);
    lv_obj_set_style_shadow_opa(control,LV_OPA_70,LV_STATE_PRESSED);
    lv_obj_set_style_transform_width(control,8,LV_STATE_PRESSED);
    lv_obj_set_style_transform_height(control,8,LV_STATE_PRESSED);
    lv_obj_set_style_bg_color(control,lv_color_hex(0x178DCD),LV_STATE_USER_1);
    lv_obj_set_style_border_color(control,lv_color_white(),LV_STATE_USER_1);
    lv_obj_set_style_shadow_width(control,12,LV_STATE_USER_1);
    lv_obj_set_style_shadow_color(control,lv_color_hex(0x43C6FF),LV_STATE_USER_1);
    lv_obj_set_style_shadow_opa(control,LV_OPA_70,LV_STATE_USER_1);
    lv_obj_remove_flag(control,LV_OBJ_FLAG_EVENT_BUBBLE);
    lv_obj_add_event_cb(control,timed_rate_nudge,LV_EVENT_CLICKED,
                        (void *)(intptr_t)step);
    lv_obj_t *caption=lv_label_create(control);
    lv_label_set_text(caption,glyph);
    lv_obj_set_style_text_font(caption,&lv_font_montserrat_28,0);
    lv_obj_set_style_text_color(caption,lv_color_white(),0);
    lv_obj_center(caption);
    lv_obj_remove_flag(caption,LV_OBJ_FLAG_CLICKABLE);
}
static void timed_load_default(lv_event_t *e)
{
    (void)e;
    timed_change_offset(prefs->timing_offset_ms);
    timed_change_rate(100U);
    if(timed_sync_slider)lv_slider_set_value(timed_sync_slider,timed_clock.correction_ms,LV_ANIM_OFF);
    if(timed_rate_slider)lv_slider_set_value(timed_rate_slider,100,LV_ANIM_OFF);
    timed_profile_dirty=true; /* RESET must replace any old custom NVS profile */
    timed_profile_save_if_dirty();
    timed_song_custom=false;
    timed_controls_until_ms=0;
    if(timed_controls)lv_obj_add_flag(timed_controls,LV_OBJ_FLAG_HIDDEN);
    activity();
}
static void timed_controls_done(lv_event_t *e)
{
    /* Tapping empty translucent space closes controls; slider/buttons stay. */
    if(e && lv_event_get_target(e)!=timed_controls)return;
    timed_profile_save_if_dirty();
    timed_controls_until_ms=0;
    timed_last_short_tap_ms=0;
    if(timed_controls)lv_obj_add_flag(timed_controls,LV_OBJ_FLAG_HIDDEN);
    activity();
}
static void timed_pause(lv_event_t *e)
{
    (void)e;
    if(!timed_lyrics_mode || !timed_clock.started)return;
    const uint64_t now=now_ms();
    if(timed_clock.running)lf_lyric_clock_pause(&timed_clock,now);
    else lf_lyric_clock_resume(&timed_clock,now);
    paused=!timed_clock.running;
    timed_controls_until_ms=0;
    lv_obj_t *caption=timed_pause_button?lv_obj_get_child(timed_pause_button,0):NULL;
    if(caption)lv_label_set_text(caption,timed_clock.running?"PAUSE":"RESUME");
    activity();timed_render(now,true);
}
/* LVGL SHORT_CLICKED excludes both swipes and long holds. Two short
   releases within 360ms return Home without stealing the long-press menu. */
static void timed_double_tap_home(lv_event_t *e)
{
    (void)e;
    if(page!=1 || !timed_lyrics_mode || !timed_overlay)return;
    if(timed_controls && !lv_obj_has_flag(timed_controls,LV_OBJ_FLAG_HIDDEN)){
        timed_last_short_tap_ms=0;
        return;
    }
    const uint64_t current=now_ms();
    if(timed_last_short_tap_ms && current>=timed_last_short_tap_ms &&
       current-timed_last_short_tap_ms<=360U){
        timed_last_short_tap_ms=0;
        activity();
        build_home(); /* deferred navigation: do not delete touch target here */
    }else timed_last_short_tap_ms=current;
}
static void timed_toggle_controls(lv_event_t *e)
{
    (void)e;
    if(timed_controls){
        if(lv_obj_has_flag(timed_controls,LV_OBJ_FLAG_HIDDEN)){
            lv_obj_remove_flag(timed_controls,LV_OBJ_FLAG_HIDDEN);
            lv_obj_move_foreground(timed_controls);
            timed_controls_until_ms=0; /* user must press DONE to close */
        }else{
            timed_profile_save_if_dirty();
            lv_obj_add_flag(timed_controls,LV_OBJ_FLAG_HIDDEN);
            timed_controls_until_ms=0;
        }
    }
    activity();
}
static lv_obj_t *timed_line_label(int slot,int width,uint32_t color)
{
    (void)slot; /* all four slots now use identical font and wrapping width */
    lv_obj_t *line=lv_label_create(timed_scroll_group);
    lv_label_set_text(line," ");
    lv_obj_set_width(line,width);
    lv_obj_set_height(line,LV_SIZE_CONTENT);
    lv_label_set_long_mode(line,LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_align(line,LV_TEXT_ALIGN_CENTER,0);
    lv_obj_set_style_text_color(line,lv_color_hex(color),0);
    lv_obj_set_style_text_line_space(line,3,0);
    lv_obj_set_style_text_font(line,&lv_font_montserrat_24,0);
    lv_obj_add_flag(line,LV_OBJ_FLAG_EVENT_BUBBLE);
    lv_obj_add_flag(line,LV_OBJ_FLAG_GESTURE_BUBBLE);
    lv_obj_align(line,LV_ALIGN_CENTER,0,0);
    return line;
}
static void build_timed_lyrics_now(const lf_recog_snapshot_t *song)
{
    if(timed_overlay)lyrics_overlay_close();
    timed_last_short_tap_ms=0;
    atomic_store(&ui_lyrics_scene_phase,1U);
    lv_obj_t *surface=NULL;
    if(primary_pager && screen){
        /* Preserve the existing Home pager and cached clock during auto-open.
           A separate opaque child provides lyrics without deleting old pages. */
        timed_overlay=lv_obj_create(screen);
        surface=timed_overlay;
        lv_obj_set_size(surface,466,466);
        lv_obj_center(surface);
        lv_obj_set_style_radius(surface,0,0);
        lv_obj_set_style_bg_color(surface,lv_color_hex(0x070D16),0);
        lv_obj_set_style_bg_opa(surface,LV_OPA_COVER,0);
        lv_obj_set_style_border_width(surface,0,0);
        lv_obj_set_style_shadow_width(surface,0,0);
        lv_obj_set_style_pad_all(surface,0,0);
        lv_obj_remove_flag(surface,LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_add_flag(surface,LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_flag(surface,LV_OBJ_FLAG_EVENT_BUBBLE);
        lv_obj_add_flag(surface,LV_OBJ_FLAG_GESTURE_BUBBLE);
        lv_obj_add_flag(primary_pager,LV_OBJ_FLAG_HIDDEN);
        page=1;
    }else{
        page=1;
        make_base();
        surface=screen;
    }
    atomic_store(&ui_lyrics_scene_phase,2U);
    timed_lyrics_mode=true;
    /* Preserve a manually started clock across menus for the same song.
     * A new AudD attempt invalidates the previous alignment. */
    if(timed_scan_attempt!=song->attempts){
        memset(&timed_clock,0,sizeof timed_clock);
        timed_estimated=false;
        if(song->audd_timecode_valid && song->capture_start_ms){
            lf_lyric_clock_start(&timed_clock,now_ms());
            timed_clock.elapsed_ms=lf_sync_estimate_position_ms(
                song->audd_timecode_ms,song->capture_start_ms,
                song->captured_frames,LF_SCAN_RATE,now_ms());
            int32_t offset=prefs->timing_offset_ms;
            uint16_t rate=100U;
            timed_song_custom=lf_song_cache_load_profile(
                song->title,song->artist,&offset,&rate);
            lf_lyric_clock_adjust(&timed_clock,offset);
            timed_rate_percent=rate;
            timed_rate_clock_anchor_ms=lf_lyric_clock_position(&timed_clock,now_ms());
            timed_rate_song_anchor_ms=timed_rate_clock_anchor_ms;
            timed_estimated=true;
        }
        if(!timed_clock.started){
            int32_t saved_offset=prefs->timing_offset_ms;
            uint16_t saved_rate=100U;
            timed_song_custom=lf_song_cache_load_profile(
                song->title,song->artist,&saved_offset,&saved_rate);
            timed_rate_percent=saved_rate;
        }
        timed_scan_attempt=song->attempts;
        timed_profile_dirty=false; /* freshly loaded profile is already saved */
    }
    timed_line_count=0;
    const size_t maximum=song->lyric_lines<LF_MAX_LINES?
        song->lyric_lines:LF_MAX_LINES;
    /* Preflight wrapping for all lyrics before painting the first line,
       using LVGL's actual glyph measurements (not byte/word guesses). */
    for(size_t i=0;i<maximum;i++){
        lf_lyric_line_t line={0};
        if(!lf_recog_lyric_line((uint32_t)i,&line))break;
        lv_point_t measured={0};
        lv_text_get_size(&measured,line.text[0]?line.text:" ",
                         &lv_font_montserrat_24,0,3,
                         LF_TIMED_TEXT_WIDTH,LV_TEXT_FLAG_NONE);
        if(measured.y<30)measured.y=30;
        if(measured.y>320)measured.y=320;
        timed_stamps[timed_line_count]=line.start_ms;
        timed_line_heights[timed_line_count]=(uint16_t)measured.y;
        timed_line_count++;
    }
    atomic_store(&ui_lyrics_scene_phase,3U);
    if(!timed_line_count){
        atomic_store(&ui_lyrics_scene_phase,0U);
        timed_lyrics_mode=false;
        label(surface,"No timed lyrics loaded",200,&lv_font_montserrat_20,prefs->text_color);
        button(surface,"HOME",150,49,0,172,0xA8D6FF,on_home,NULL);
        return;
    }
    /* Follow the supplied three-line circular reference: no song header,
       artist, time counter, focus panel or permanent transport controls. */
    lv_obj_set_style_bg_color(surface,lv_color_hex(0x070D16),0);
    lv_obj_t *ring=lv_obj_create(surface);
    lv_obj_set_size(ring,422,422);
    lv_obj_align(ring,LV_ALIGN_CENTER,0,0);
    lv_obj_set_style_radius(ring,LV_RADIUS_CIRCLE,0);
    lv_obj_set_style_bg_opa(ring,LV_OPA_TRANSP,0);
    lv_obj_set_style_border_width(ring,1,0);
    lv_obj_set_style_border_color(ring,lv_color_hex(0x203951),0);
    lv_obj_set_style_shadow_width(ring,0,0);
    lv_obj_set_style_pad_all(ring,0,0);
    lv_obj_remove_flag(ring,LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_remove_flag(ring,LV_OBJ_FLAG_CLICKABLE);
    /* The lyric surface is deliberately sacred: no branding or title.
       Battery stays compact in the corner; all controls stay hidden. */
    /* A compact icon + percent only; four-segment detail stays in Web UI. */
    timed_battery=lv_label_create(surface);
    lv_label_set_text(timed_battery,LV_SYMBOL_BATTERY_EMPTY " --%");
    lv_obj_set_style_text_font(timed_battery,&lv_font_montserrat_18,0);
    lv_obj_set_style_text_color(timed_battery,lv_color_hex(0xAFC6DC),0);
    lv_obj_set_width(timed_battery,120);
    lv_obj_set_style_text_align(timed_battery,LV_TEXT_ALIGN_CENTER,0);
    lv_obj_align(timed_battery,LV_ALIGN_TOP_MID,0,43);
    lv_obj_add_flag(timed_battery,LV_OBJ_FLAG_EVENT_BUBBLE);
    lv_obj_add_flag(timed_battery,LV_OBJ_FLAG_GESTURE_BUBBLE);
    /* An unmoving parent clips line ends above/below the lyric field.
       The animated child may translate, but its clip boundary never does. */
    timed_viewport=lv_obj_create(surface);
    lv_obj_set_size(timed_viewport,408,360);
    lv_obj_align(timed_viewport,LV_ALIGN_CENTER,0,0);
    lv_obj_set_style_bg_opa(timed_viewport,LV_OPA_TRANSP,0);
    lv_obj_set_style_border_width(timed_viewport,0,0);
    lv_obj_set_style_shadow_width(timed_viewport,0,0);
    lv_obj_set_style_radius(timed_viewport,0,0);
    lv_obj_set_style_pad_all(timed_viewport,0,0);
    lv_obj_remove_flag(timed_viewport,LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_remove_flag(timed_viewport,LV_OBJ_FLAG_CLICKABLE);
    lv_obj_remove_flag(timed_viewport,LV_OBJ_FLAG_OVERFLOW_VISIBLE);
    lv_obj_add_flag(timed_viewport,LV_OBJ_FLAG_EVENT_BUBBLE);
    lv_obj_add_flag(timed_viewport,LV_OBJ_FLAG_GESTURE_BUBBLE);
    timed_scroll_group=lv_obj_create(timed_viewport);
    lv_obj_set_size(timed_scroll_group,416,388);
    lv_obj_align(timed_scroll_group,LV_ALIGN_CENTER,0,0);
    lv_obj_set_style_bg_opa(timed_scroll_group,LV_OPA_TRANSP,0);
    lv_obj_set_style_border_width(timed_scroll_group,0,0);
    lv_obj_set_style_shadow_width(timed_scroll_group,0,0);
    lv_obj_set_style_pad_all(timed_scroll_group,0,0);
    lv_obj_remove_flag(timed_scroll_group,LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_remove_flag(timed_scroll_group,LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(timed_scroll_group,LV_OBJ_FLAG_EVENT_BUBBLE);
    lv_obj_add_flag(timed_scroll_group,LV_OBJ_FLAG_GESTURE_BUBBLE);
    timed_scroll_y=INT16_MIN;
    /* Uniform wrap width prevents reflow while a line changes slots. */
    timed_rows[0]=timed_line_label(0,LF_TIMED_TEXT_WIDTH,0x647A91);
    timed_rows[1]=timed_line_label(1,LF_TIMED_TEXT_WIDTH,0xFFFFFF);
    timed_rows[2]=timed_line_label(2,LF_TIMED_TEXT_WIDTH,0xA9C2D8);
    timed_rows[3]=timed_line_label(3,LF_TIMED_TEXT_WIDTH,0x637E96);
    timed_hint=label(surface,"Long-press for controls",374,&lv_font_montserrat_16,0x526B84);
    lv_obj_set_style_text_font(timed_hint,&lv_font_montserrat_16,0);
    lv_obj_set_width(timed_hint,260);
    timed_hint_until_ms=now_ms()+5000; /* Hint disappears after first five seconds. */
    /* A simple black translucent overlay. The live scrolling lyrics remain
       visible behind it. Two bottom actions replace the old control grid. */
    timed_controls=lv_obj_create(surface);
    lv_obj_set_size(timed_controls,466,466);
    lv_obj_align(timed_controls,LV_ALIGN_CENTER,0,0);
    lv_obj_set_style_radius(timed_controls,0,0);
    lv_obj_set_style_bg_color(timed_controls,lv_color_black(),0);
    lv_obj_set_style_bg_opa(timed_controls,(lv_opa_t)38,0); /* ~15% black */
    lv_obj_set_style_border_width(timed_controls,0,0);
    lv_obj_set_style_pad_all(timed_controls,0,0);
    lv_obj_remove_flag(timed_controls,LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(timed_controls,LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(timed_controls,LV_OBJ_FLAG_EVENT_BUBBLE);
    lv_obj_add_event_cb(timed_controls,timed_controls_done,LV_EVENT_SHORT_CLICKED,NULL);
    lv_obj_add_flag(timed_controls,LV_OBJ_FLAG_HIDDEN);
    lv_obj_t *slow_label=lv_label_create(timed_controls);
    lv_label_set_text(slow_label,"SLOW DOWN");
    lv_obj_set_style_text_font(slow_label,&lv_font_montserrat_18,0);
    lv_obj_set_style_text_color(slow_label,lv_color_white(),0);
    lv_obj_align(slow_label,LV_ALIGN_CENTER,-124,-55);
    lv_obj_t *fast_label=lv_label_create(timed_controls);
    lv_label_set_text(fast_label,"SPEED UP");
    lv_obj_set_style_text_font(fast_label,&lv_font_montserrat_18,0);
    lv_obj_set_style_text_color(fast_label,lv_color_white(),0);
    lv_obj_align(fast_label,LV_ALIGN_CENTER,128,-55);
    timed_add_nudge(timed_controls,"-",-128,-1);
    timed_add_nudge(timed_controls,"+",128,1);
    timed_rate_slider=lv_slider_create(timed_controls);
    lv_obj_set_size(timed_rate_slider,348,22);
    lv_slider_set_range(timed_rate_slider,75,125);
    lv_slider_set_value(timed_rate_slider,timed_rate_percent,LV_ANIM_OFF);
    lv_obj_align(timed_rate_slider,LV_ALIGN_CENTER,0,-10);
    lv_obj_remove_flag(timed_rate_slider,LV_OBJ_FLAG_EVENT_BUBBLE);
    lv_obj_remove_flag(timed_rate_slider,LV_OBJ_FLAG_GESTURE_BUBBLE);
    lv_obj_add_event_cb(timed_rate_slider,timed_on_rate_slide,LV_EVENT_VALUE_CHANGED,NULL);
    timed_rate_value=lv_label_create(timed_controls);
    lv_obj_set_style_text_font(timed_rate_value,&lv_font_montserrat_18,0);
    lv_obj_set_style_text_color(timed_rate_value,lv_color_white(),0);
    lv_obj_align(timed_rate_value,LV_ALIGN_CENTER,0,35);
    timed_update_slider_labels();
    button(timed_controls,"RESET",150,54,0,125,0x9CB7CB,timed_load_default,NULL);
    timed_save_note=NULL;
    lv_obj_remove_event_cb(surface,timed_toggle_controls);
    lv_obj_add_event_cb(surface,timed_toggle_controls,LV_EVENT_LONG_PRESSED,NULL);
    lv_obj_add_event_cb(surface,timed_double_tap_home,LV_EVENT_SHORT_CLICKED,NULL);
    paused=!timed_clock.running;
    timed_active_index=-999;timed_last_second=UINT32_MAX;
    timed_visible_future_mask=0xFF;
    timed_scroll_y=INT16_MIN;
    timed_last_update_ms=0;
    timed_power_update_ms=0;
    atomic_store(&ui_lyrics_scene_phase,4U);
    timed_render(now_ms(),true);
    atomic_store(&ui_lyrics_scene_phase,5U);
    /* Lyrics use a bounded, retained 65ms compositor; preserve 50ms pager. */
    timed_scroll_timer=lv_timer_create(timed_scroll_frame,65,NULL);
}

static void build_preview_now(void)
{
    lf_recog_snapshot_t song={0};
    lf_recog_snapshot(&song);
    if(song.state==LF_RECOG_MATCH && song.lyric_lines>0 &&
       strcmp(song.lyrics_state,"synced")==0) {
        build_timed_lyrics_now(&song);
        return;
    }
    /* The vertical panel is for real lyrics, never the old demo scroller.
       Leave Home and the Clock cache alive so swiping back is reversible. */
    page=1;
    lv_obj_t *surface=screen;
    if(primary_pager && screen){
        timed_overlay=lv_obj_create(screen);
        surface=timed_overlay;
        lv_obj_set_size(surface,466,466);
        lv_obj_center(surface);
        lv_obj_set_style_radius(surface,0,0);
        lv_obj_set_style_bg_color(surface,lv_color_hex(0x070D16),0);
        lv_obj_set_style_bg_opa(surface,LV_OPA_COVER,0);
        lv_obj_set_style_border_width(surface,0,0);
        lv_obj_set_style_pad_all(surface,0,0);
        lv_obj_remove_flag(surface,LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_add_flag(surface,LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_flag(surface,LV_OBJ_FLAG_EVENT_BUBBLE);
        lv_obj_add_flag(surface,LV_OBJ_FLAG_GESTURE_BUBBLE);
        lv_obj_add_flag(primary_pager,LV_OBJ_FLAG_HIDDEN);
    }else make_base();
    label(surface,"NO LYRICS YET",180,&lv_font_montserrat_24,0xE8F5FF);
    label(surface,"Swipe up to Home, then tap Play",228,
          &lv_font_montserrat_16,0xA8C8E1);
}

/* List rows are clickable; selecting a network opens the on-device keyboard. */
static void on_wifi_selected(lv_event_t *e)
{
    size_t idx = (size_t)(uintptr_t)lv_event_get_user_data(e);
    if (idx >= wifi_ap_count) return;
    strlcpy(selected_wifi, wifi_aps[idx].ssid, sizeof(selected_wifi));
    selected_wifi_secured = wifi_aps[idx].secured;
    activity();
    if (selected_wifi_secured) build_wifi_password();
    else {
        if (lf_network_save_wifi(selected_wifi, "")) {
            lf_network_restart();
            if (wifi_status) lv_label_set_text(wifi_status, "Saved. Restarting...");
        } else if (wifi_status) lv_label_set_text(wifi_status, "Could not save Wi-Fi.");
    }
}
static void draw_wifi_list(void)
{
    if (!wifi_list || !wifi_status) return;
    lv_obj_clean(wifi_list);
    if (!wifi_ap_count) {
        lv_label_set_text(wifi_status, "No networks found. Tap SCAN.");
        return;
    }
    lv_label_set_text(wifi_status, "Tap your Wi-Fi network");
    for (size_t i = 0; i < wifi_ap_count; i++) {
        lv_obj_t *b = lv_button_create(wifi_list);
        lv_obj_set_width(b, lv_pct(100));
        lv_obj_set_height(b, 49);
        lv_obj_set_style_bg_color(b, lv_color_hex(0x263944), 0);
        lv_obj_set_style_radius(b, 12, 0);
        lv_obj_add_event_cb(b, on_wifi_selected, LV_EVENT_CLICKED, (void *)(uintptr_t)i);
        lv_obj_t *name = lv_label_create(b);
        char row[60];
        snprintf(row, sizeof(row), "%s %s", wifi_aps[i].secured ? LV_SYMBOL_EYE_CLOSE : " ", wifi_aps[i].ssid);
        lv_label_set_text(name, row);
        lv_obj_set_style_text_font(name, &lv_font_montserrat_16, 0);
        lv_obj_set_style_text_color(name, lv_color_hex(0xFFFFFF), 0);
        lv_obj_set_width(name, lv_pct(100));
        lv_label_set_long_mode(name, LV_LABEL_LONG_DOT);
        lv_obj_center(name);
    }
}
static void on_wifi_scan(lv_event_t *e)
{
    (void)e;
    activity();
    if (lf_network_scan_start()) {
        if (wifi_status) lv_label_set_text(wifi_status, "Scanning nearby Wi-Fi...");
        wifi_state_previous = 1;
    } else if (wifi_status) lv_label_set_text(wifi_status, "Scan unavailable. Retry.");
}
static void build_wifi_now(void)
{
    page = 4;
    make_base();
    label(screen, "WI-FI NETWORKS", 42, &lv_font_montserrat_20, prefs->text_color);
    wifi_status = label(screen, "Select a nearby network", 77, &lv_font_montserrat_16, 0xC4D9E8);
    wifi_list = lv_obj_create(screen);
    lv_obj_set_size(wifi_list, 346, 239);
    lv_obj_align(wifi_list, LV_ALIGN_CENTER, 0, -4);
    lv_obj_set_flex_flow(wifi_list, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_bg_color(wifi_list, lv_color_hex(0x101C29), 0);
    lv_obj_set_style_border_width(wifi_list, 0, 0);
    lv_obj_set_style_radius(wifi_list, 16, 0);
    lv_obj_set_style_pad_all(wifi_list, 7, 0);
    lv_obj_set_scroll_dir(wifi_list, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(wifi_list, LV_SCROLLBAR_MODE_AUTO);
    draw_wifi_list();
    button(screen, "BACK", 140, 46, -81, 174, 0xAAC1D2, on_home, NULL);
    button(screen, "SCAN", 140, 46, 81, 174, 0xAAC1D2, on_wifi_scan, NULL);
    if (!wifi_ap_count) (void)lf_network_scan_start();
    wifi_state_previous = 1;
}
static void wheel_wifi_save(const char *password)
{
    if (lf_network_save_wifi(selected_wifi, password)) {
        lf_network_restart();
        build_wifi();
    } else if (screen) {
        label(screen, "Password must be 8-63 characters", 95,
              system_font(), 0xFFB7B7);
    }
}
static void wheel_wifi_cancel(void) { build_wifi(); }
static void wheel_weather_save(const char *city)
{
    lf_weather_config_t cfg;
    lf_weather_get_config(&cfg);
    snprintf(cfg.city, sizeof cfg.city, "%s", city);
    if (lf_weather_save_config(&cfg)) build_clock_settings();
}
static void wheel_weather_cancel(void) { build_clock_settings(); }
static void build_wifi_password_now(void)
{
    page = 5;
    make_base();
    lf_charwheel_show(screen, "Wi-Fi password", "", true, 63,
                      wheel_wifi_save, wheel_wifi_cancel);
}
static void on_brightness(lv_event_t *e)
{
    prefs->brightness = (uint8_t)lv_slider_get_value(lv_event_get_target(e));
    char s[40]; snprintf(s, sizeof s, "Brightness  %u%%", prefs->brightness);
    lv_label_set_text(brightness_text, s);
    apply_brightness();
    if (lv_event_get_code(e) == LV_EVENT_RELEASED) save_settings();
}
static void on_speed(lv_event_t *e)
{
    prefs->scroll_speed = (uint8_t)lv_slider_get_value(lv_event_get_target(e));
    char s[30]; snprintf(s, sizeof s, "Scroll speed  %u%%", prefs->scroll_speed);
    lv_label_set_text(speed_text, s);
    if (lv_event_get_code(e) == LV_EVENT_RELEASED) save_settings();
}
static void on_timeout(lv_event_t *e)
{
    (void)e;
    const uint8_t values[] = {15, 30, 60, 0};
    for (unsigned i = 0; i < 4; i++) {
        if (prefs->sleep_seconds == values[i]) {
            prefs->sleep_seconds = values[(i + 1) % 4];
            break;
        }
    }
    char s[32];
    if (prefs->sleep_seconds) snprintf(s, sizeof s, "Screen timeout  %us", prefs->sleep_seconds);
    else snprintf(s, sizeof s, "Screen timeout  Never");
    /* Update the tapped control in place; rebuilding the page here destroys
       the active LVGL event target and causes visible menu jumps. */
    lv_obj_t *target = lv_event_get_target(e);
    if (target && lv_obj_get_child_count(target) > 0) {
        lv_obj_t *caption = lv_obj_get_child(target, 0);
        lv_label_set_text(caption, s);
    }
    save_settings();
}
static void on_color(lv_event_t *e)
{
    uint32_t idx = (uint32_t)(uintptr_t)lv_event_get_user_data(e);
    if (idx >= sizeof(PALETTE) / sizeof(PALETTE[0])) return;
    prefs->text_color = PALETTE[idx];
    save_settings();
    build_lyrics_settings();
}
static void add_slider(lv_obj_t *parent, int y, int value,
                       int min, int max, lv_event_cb_t cb)
{
    lv_obj_t *s = lv_slider_create(parent);
    lv_obj_set_size(s, 262, 11);
    lv_obj_align(s, LV_ALIGN_TOP_MID, 0, y);
    lv_slider_set_range(s, min, max);
    lv_slider_set_value(s, value, LV_ANIM_OFF);
    lv_obj_add_event_cb(s, cb, LV_EVENT_VALUE_CHANGED, NULL);
    lv_obj_add_event_cb(s, cb, LV_EVENT_RELEASED, NULL);
}
static void on_lyrics_settings(lv_event_t *e)
{
    (void)e; activity(); build_lyrics_settings();
}
static void on_device_settings(lv_event_t *e)
{
    (void)e; activity(); build_device_settings();
}
static void primary_render_settings_content(lv_obj_t *parent)
{
    label(parent,"SETTINGS",41,&lv_font_montserrat_24,prefs->text_color);
    button(parent,"LYRICS",250,54,0,-99,0xB2C6D3,on_lyrics_settings,NULL);
    button(parent,"CLOCK & WEATHER",250,54,0,-33,0xB2C6D3,on_clock_options,NULL);
    button(parent,"DEVICE",250,54,0,33,0xB2C6D3,on_device_settings,NULL);
    button(parent,"WI-FI",250,54,0,99,0xB2C6D3,on_wifi,NULL);
    button(parent,"HOME",153,44,0,175,prefs->text_color,on_home,NULL);
}
static void render_settings(void)
{
    primary_pager_enter(2);
}
static void build_settings_now(void)
{
    render_settings();
}
static void on_lyric_font(lv_event_t *e)
{
    (void)e;
    prefs->font_size = (prefs->font_size + 1) % 3;
    save_settings();
    build_lyrics_settings();
}
static void build_lyrics_settings_now(void)
{
    page = 9;
    make_base();
    label(screen, "LYRIC SETTINGS", 40, &lv_font_montserrat_24, prefs->text_color);
    char text[60];
    snprintf(text, sizeof text, "Text size: %s",
             prefs->font_size == 0 ? "Small" : prefs->font_size == 1 ? "Medium" : "Large");
    button(screen, text, 270, 50, 0, -103, 0xB2C6D3, on_lyric_font, NULL);
    snprintf(text, sizeof text, "Scroll speed: %u%%", prefs->scroll_speed);
    speed_text = label(screen, text, 168, system_font(), 0xE4EDF3);
    add_slider(screen, 205, prefs->scroll_speed, 0, 100, on_speed);
    label(screen, "Lyric colour", 249, system_font(), 0xE4EDF3);
    for (unsigned i = 0; i < sizeof(PALETTE) / sizeof(PALETTE[0]); i++) {
        lv_obj_t *dot = lv_button_create(screen);
        lv_obj_set_size(dot, 43, 43);
        lv_obj_set_style_radius(dot, LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_bg_color(dot, lv_color_hex(PALETTE[i]), 0);
        lv_obj_set_style_border_width(dot, prefs->text_color == PALETTE[i] ? 3 : 0, 0);
        lv_obj_set_style_border_color(dot, lv_color_hex(0x7F8B9C), 0);
        lv_obj_align(dot, LV_ALIGN_TOP_MID, (int)i * 53 - 106, 294);
        lv_obj_add_event_cb(dot, on_color, LV_EVENT_CLICKED, (void *)(uintptr_t)i);
    }
    button(screen, "BACK", 155, 45, 0, 175, prefs->text_color, on_settings, NULL);
}
static void on_system_font(lv_event_t *e)
{
    prefs->system_font_size = (prefs->system_font_size + 1) % 3;
    /* Do not destroy the tapped LVGL control during its CLICKED callback.
       The selected font applies to newly rendered controls on navigation. */
    lv_obj_t *target = lv_event_get_target(e);
    if (target && lv_obj_get_child_count(target) > 0) {
        char caption[40];
        snprintf(caption, sizeof caption, "System font: %s pt",
                 prefs->system_font_size == 0 ? "18" :
                 prefs->system_font_size == 1 ? "20" : "24");
        lv_obj_t *label = lv_obj_get_child(target, 0);
        lv_label_set_text(label, caption);
        lv_obj_set_style_text_font(label, system_font(), 0);
    }
    save_settings();
}
/* A vertically scrollable device card gives IP, browser URL and firmware
   version room to breathe without clipping the circular 466px display. */
static lv_obj_t *device_line(lv_obj_t *parent,const char *text,int top,
                              const lv_font_t *font,uint32_t color)
{
    lv_obj_t *line=lv_label_create(parent);
    lv_label_set_text(line,text);
    lv_obj_set_size(line,260,45);
    lv_obj_set_style_text_font(line,font,0);
    lv_obj_set_style_text_color(line,lv_color_hex(color),0);
    lv_obj_set_style_text_align(line,LV_TEXT_ALIGN_CENTER,0);
    lv_label_set_long_mode(line,LV_LABEL_LONG_WRAP);
    lv_obj_align(line,LV_ALIGN_TOP_MID,0,top);
    return line;
}
static void build_device_settings_now(void)
{
    page=10;
    make_base();
    label(screen,"DEVICE SETTINGS",37,&lv_font_montserrat_24,0xEAF6FF);
    lv_obj_t *viewport=lv_obj_create(screen);
    lv_obj_set_size(viewport,308,306);
    lv_obj_align(viewport,LV_ALIGN_TOP_MID,0,98);
    lv_obj_set_style_radius(viewport,18,0);
    lv_obj_set_style_bg_color(viewport,lv_color_hex(0x0C1A2C),0);
    lv_obj_set_style_border_color(viewport,lv_color_hex(0x2D5A7B),0);
    lv_obj_set_style_border_width(viewport,1,0);
    lv_obj_set_style_pad_all(viewport,9,0);
    lv_obj_set_scroll_dir(viewport,LV_DIR_VER);
    lv_obj_set_scrollbar_mode(viewport,LV_SCROLLBAR_MODE_ACTIVE);
    lv_obj_t *content=lv_obj_create(viewport);
    lv_obj_set_size(content,272,610);
    lv_obj_align(content,LV_ALIGN_TOP_MID,0,0);
    lv_obj_set_style_bg_opa(content,LV_OPA_TRANSP,0);
    lv_obj_set_style_border_width(content,0,0);
    lv_obj_set_style_pad_all(content,0,0);
    lv_obj_remove_flag(content,LV_OBJ_FLAG_SCROLLABLE);

    char text[80];
    snprintf(text,sizeof text,"System font: %s pt",
        prefs->system_font_size==0?"18":prefs->system_font_size==1?"20":"24");
    button(content,text,248,55,0,-264,0xB2C6D3,on_system_font,NULL);
    snprintf(text,sizeof text,"Brightness: %u%%",prefs->brightness);
    brightness_text=device_line(content,text,94,&lv_font_montserrat_20,0xD6F0FF);
    lv_obj_t *slider=lv_slider_create(content);
    /* Large touch target for a finger on the 1.75-inch AMOLED. */
    lv_obj_set_size(slider,242,32);
    lv_obj_align(slider,LV_ALIGN_TOP_MID,0,142);
    lv_slider_set_range(slider,5,100);
    lv_slider_set_value(slider,prefs->brightness,LV_ANIM_OFF);
    lv_obj_set_style_bg_color(slider,lv_color_hex(0x294D69),LV_PART_MAIN);
    lv_obj_set_style_bg_color(slider,lv_color_hex(0x5DC4FF),LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(slider,lv_color_hex(0xF0FAFF),LV_PART_KNOB);
    lv_obj_set_style_pad_all(slider,10,LV_PART_KNOB);
    lv_obj_set_style_radius(slider,LV_RADIUS_CIRCLE,LV_PART_MAIN);
    lv_obj_set_style_radius(slider,LV_RADIUS_CIRCLE,LV_PART_INDICATOR);
    /* Keep vertical drags on brightness instead of chaining them to the
       scrollable Device page. The full-width track remains finger-sized. */
    lv_obj_remove_flag(slider,LV_OBJ_FLAG_SCROLL_CHAIN_VER);
    lv_obj_remove_flag(slider,LV_OBJ_FLAG_SCROLL_CHAIN_HOR);
    /* Consume drag gestures here rather than forwarding them to the pager. */
    lv_obj_remove_flag(slider,LV_OBJ_FLAG_EVENT_BUBBLE);
    lv_obj_remove_flag(slider,LV_OBJ_FLAG_GESTURE_BUBBLE);
    lv_obj_add_event_cb(slider,on_brightness,LV_EVENT_VALUE_CHANGED,NULL);
    lv_obj_add_event_cb(slider,on_brightness,LV_EVENT_RELEASED,NULL);

    snprintf(text,sizeof text,"Screen timeout: %s",
         prefs->sleep_seconds==0?"Never":
         prefs->sleep_seconds==15?"15s":
         prefs->sleep_seconds==30?"30s":"60s");
    button(content,text,248,55,0,-92,0xB2C6D3,on_timeout,NULL);
    /* The timeout action keeps its own caption in sync after a tap. */
    timeout_text=NULL;
    const char *address=lf_network_address();
    const bool online=address && address[0] &&
                       strcmp(address,"Offline")!=0;
    device_line(content,"IP ADDRESS",285,&lv_font_montserrat_16,0x87B8D9);
    device_line(content,online?address:"Not connected",308,
                &lv_font_montserrat_20,0xFFFFFF);
    device_line(content,"WEB ADDRESS",362,&lv_font_montserrat_16,0x87B8D9);
    snprintf(text,sizeof text,"%s",online?"":"Connect to Wi-Fi");
    if(online)snprintf(text,sizeof text,"http://%s/",address);
    device_line(content,text,388,&lv_font_montserrat_16,0x7FD4FF);
    device_line(content,"VERSION",460,&lv_font_montserrat_16,0x87B8D9);
    device_line(content,LF_VERSION,482,&lv_font_montserrat_20,0xF1F7FF);
    button(content,"BACK",204,50,0,251,0xB2C6D3,on_settings,NULL);
}

/* Clock dial elements must never consume swipe/hold touch events. */
static void clock_children_input_transparent(lv_obj_t *parent)
{
    uint32_t count = lv_obj_get_child_count(parent);
    for (uint32_t i = 0; i < count; i++) {
        lv_obj_t *child = lv_obj_get_child(parent, i);
        lv_obj_remove_flag(child, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_remove_flag(child, LV_OBJ_FLAG_SCROLLABLE);
        clock_children_input_transparent(child);
    }
}
static void render_clock(void)
{
    primary_pager_enter(0);
}
/* One active LVGL screen; an ordinary scrollable CHILD contains Clock,
   Home and Settings side by side. No secondary screen roots, no invalidation
   suppression, no synthetic divider. LVGL's own scroll inertia and snap
   handle the physical finger's movement, as the original user requested. */
/* Keep the live Clock in the native LVGL page tree throughout swipes.
   0.8.23's snapshot hid the clock and could leave the AMOLED black after
   an incomplete visibility invalidation; render the real scene instead. */
static void primary_pager_commit(int index)
{
    if(index<0)index=0;
    if(index>2)index=2;
    if(primary_pager_snapping) {
        uint32_t elapsed=(uint32_t)(now_ms()-primary_pager_snap_started_ms);
        atomic_store(&pager_snap_last_ms,elapsed);
        uint32_t maximum=atomic_load(&pager_snap_max_ms);
        if(elapsed>maximum)atomic_store(&pager_snap_max_ms,elapsed);
    }
    /* A verified LVGL REFR_READY for the final X already painted the
       destination. 0.8.94 still repainted the WHOLE page after that ACK,
       adding another 20-band pass and visible page flashing. 0.8.91
       skipped this repaint before any frame ACK existed and exposed black
       pages. Skip only with proof, and retain the protective redraw on
       every unconfirmed/forced/initial page settlement. */
    const bool frame_presented=primary_pager_snapping && pager_final_frame_ready &&
                               pager_render_scroll_x==-1;
    const bool settled_move=primary_pager_snapping || pager_scrolling ||
                            primary_pager_current!=index;
    primary_pager_current=index;
    primary_pager_origin=index;
    primary_pager_snap_to=index;
    primary_pager_snapping=false;
    pager_scrolling=false;
    primary_pager_frame_pending=false;
    pager_final_frame_ready=false;
    pager_render_scroll_x=-1;
    pager_arrived_at_ms=0;
    if(primary_pager_frame_timer)lv_timer_pause(primary_pager_frame_timer);
    /* Keep both physical swipe directions enabled for every pager page.
       Dynamic per-edge scroll flags in 0.8.74/0.8.75 trapped Settings:
       firmware must never change LVGL's input direction at page commit.
       The pager has no elastic overscroll or horizontal chain; guard
       out-of-range motion with normal LVGL scroll bounds instead. */
    /* No lv_obj_set_scroll_dir call here. */
    page=(index==0)?6:(index==1)?0:2;
    last_power_refresh_ms=0;
    clock_last_refresh=0;
    activity();
    if(index==1)home_refresh(now_ms());
    /* Preserve the protective repaint whenever the target was NOT
       already fully refreshed. When the final frame is acknowledged,
       repainting all 466x466 pixels again is wasted and may expose a
       second transient frame during the physical swipe. */
    if(settled_move && !frame_presented && primary_pager_pages[index])
        lv_obj_invalidate(primary_pager_pages[index]);
    if(settled_move && frame_presented)
        atomic_fetch_add(&pager_acknowledged_settles_no_full_repaint,1);
    if(index==0) {
        /* A touch used to reveal a black Clock after a return swipe. Do not
           depend on another touch to trigger the panel's dirty rectangle:
           unhide the live scene and explicitly repaint the settled page.
           Only one full Clock refresh, not repeated full-frame animation. */
        if(clock_scene) {
            lv_obj_remove_flag(clock_scene,LV_OBJ_FLAG_HIDDEN);
            /* Initial or unconfirmed entry remains fully repainted
               above. ACK-confirmed Clock entry needs only changed time
               elements; never force a second whole-scene redraw. */
            if(!settled_move)lv_obj_invalidate(clock_scene);
            atomic_fetch_add(&clock_repaint_recoveries,1);
        }
        lf_clock_update();
    }
}
/* Native LVGL easing, but commit only after the destination frame is
   presented. Press interrupts and the 1600ms stall guard are preserved. */
static void primary_pager_arrived(uint64_t current)
{
    if(!primary_pager || !primary_pager_snapping)return;
    if(pager_final_frame_ready){
        primary_pager_commit(primary_pager_snap_to);
        return;
    }
    if(pager_arrived_at_ms==0)pager_arrived_at_ms=current;
    /* Most final positions naturally refresh. If not, request exactly one
       active-page redraw; never hammer the renderer every 16ms. */
    if(!primary_pager_frame_pending && current-pager_arrived_at_ms>=240U){
        primary_pager_frame_pending=true;
        if(primary_pager_pages[primary_pager_snap_to]){
            lv_obj_invalidate(primary_pager_pages[primary_pager_snap_to]);
            atomic_fetch_add(&pager_final_frame_repaint_requests,1);
        }
    }
}
static void primary_pager_snap_frame(lv_timer_t *timer)
{
    (void)timer;
    if(!primary_pager||!primary_pager_snapping||asleep||timed_overlay)return;
    if(ui_pointer_is_down()){
        primary_pager_snapping=false;
        primary_pager_frame_pending=false;
        pager_final_frame_ready=false;
        pager_arrived_at_ms=0;
        if(primary_pager_frame_timer)lv_timer_pause(primary_pager_frame_timer);
        return;
    }
    const int32_t x=abs(lv_obj_get_scroll_x(primary_pager));
    if(abs(x-primary_pager_snap_to*466)<=2)
        primary_pager_arrived(now_ms());
}
static void primary_pager_start_snap(int target)
{
    if(!primary_pager)return;
    if(target<0)target=0;
    if(target>2)target=2;
    const int32_t current_x=abs(lv_obj_get_scroll_x(primary_pager));
    if(abs(current_x-target*466)<=2){
        primary_pager_commit(target);
        return;
    }
    primary_pager_snap_to=target;
    primary_pager_snap_started_ms=now_ms();
    primary_pager_frame_pending=false;
    pager_final_frame_ready=false;
    pager_render_scroll_x=-1;
    pager_arrived_at_ms=0;
    primary_pager_snapping=true;
    pager_scrolling=true;
    if(primary_pager_frame_timer){
        lv_timer_reset(primary_pager_frame_timer);
        lv_timer_resume(primary_pager_frame_timer);
    }
    /* LV_EVENT_SCROLL_BEGIN configures 185-290ms easing. LVGL may drop
       obsolete positions; finger press cancels the in-flight animation. */
    lv_obj_scroll_to_x(primary_pager,target*466,LV_ANIM_ON);
}
static int primary_pager_target(int32_t x)
{
    int target=(x+233)/466;
    if(target<0)target=0;
    if(target>2)target=2;
    int32_t displacement=x-primary_pager_origin*466;
    if(target==primary_pager_origin && abs(displacement)>=55)
        target=primary_pager_origin+(displacement>0?1:-1);
    if(target<0)target=0;
    if(target>2)target=2;
    return target;
}
static void primary_pager_watchdog(uint64_t current)
{
    if(!primary_pager||primary_pager_initializing||asleep||timed_overlay)return;
    /* Never seize page position while the finger remains on the glass. */
    lv_indev_t *indev=lv_indev_get_next(NULL);
    while(indev){
        if(lv_indev_get_type(indev)==LV_INDEV_TYPE_POINTER &&
           lv_indev_get_state(indev)==LV_INDEV_STATE_PRESSED)return;
        indev=lv_indev_get_next(indev);
    }
    int32_t x=abs(lv_obj_get_scroll_x(primary_pager));
    if(primary_pager_snapping){
        /* The final scroll coordinate can be reached before the pixel
           transfer finishes. Check the hard deadline FIRST, otherwise
           a missed REFR_READY leaves the pager stuck forever at target. */
        if(current-primary_pager_snap_started_ms>LF_PAGER_STALL_GUARD_MS){
            /* Force a complete, aligned page if panel transfers outrun the
               LVGL animation. This is preferable to getting stranded midpage. */
            int target=primary_pager_snap_to;
            lv_obj_scroll_to_x(primary_pager,target*466,LV_ANIM_OFF);
            primary_pager_commit(target);
            atomic_fetch_add(&pager_forced_snap_count,1);
            return;
        }
        if(abs(x-primary_pager_snap_to*466)<=2){
            primary_pager_arrived(current);
            return;
        }
    }else if((pager_scrolling || abs(x-primary_pager_current*466)>4) &&
             current-primary_pager_last_move_ms>75){
        int target=primary_pager_target(x);
        if(abs(x-target*466)<=2)primary_pager_commit(target);
        else primary_pager_start_snap(target);
    }
}
static void primary_pager_scroll_event(lv_event_t *e)
{
    /* A bubbled child scroll may not start a new whole-page animation. */
    if(!primary_pager || primary_pager_initializing ||
       lv_event_get_target(e)!=primary_pager) return;
    const lv_event_code_t code=lv_event_get_code(e);
    if(code==LV_EVENT_SCROLL_BEGIN){
        /* Native LVGL elapsed-time snap drops obsolete positions during
           costly redraws instead of accumulating one-second delays. */
        lv_anim_t *anim=lv_event_get_scroll_anim(e);
        if(primary_pager_snapping && anim) {
            int32_t remaining=abs(primary_pager_snap_to*466-abs(lv_obj_get_scroll_x(primary_pager)));
            if(remaining>466)remaining=466;
            /* PrintSphere-style ease-out timing with our own touch guard. */
            lv_anim_set_duration(anim,185U+(uint32_t)(105U*remaining/466U));
            lv_anim_set_path_cb(anim,lv_anim_path_ease_out);
        }
        pager_scrolling=true;
        primary_pager_last_move_ms=now_ms();
        activity(); /* A gesture in progress is never idle/screen-off. */
        lv_indev_t *indev=lv_indev_active();
        if(indev && lv_indev_get_state(indev)==LV_INDEV_STATE_PRESSED){
            primary_pager_snapping=false; /* Finger interrupts existing snap. */
            primary_pager_frame_pending=false;
            pager_final_frame_ready=false;
            pager_render_scroll_x=-1;
            pager_arrived_at_ms=0;
            if(primary_pager_frame_timer)lv_timer_pause(primary_pager_frame_timer);
            primary_pager_origin=primary_pager_current;
        }
        return;
    }
    if(code==LV_EVENT_SCROLL){
        primary_pager_last_move_ms=now_ms();
        activity();
        return;
    }
    if(code!=LV_EVENT_SCROLL_END)return;
    lv_indev_t *indev=lv_indev_active();
    if(indev && lv_indev_get_state(indev)==LV_INDEV_STATE_PRESSED)
        return; /* Watchdog reconciles the released position if LVGL misses it. */
    const int32_t x=abs(lv_obj_get_scroll_x(primary_pager));
    if(primary_pager_snapping){
        /* Some intermediate scroll-end callbacks fire during a new easing
           animation. Commit only if the target is actually reached. */
        if(abs(x-primary_pager_snap_to*466)<=2)
            primary_pager_arrived(now_ms());
        return;
    }
    /* Do not start a second LVGL scroll animation inside SCROLL_END.
       The existing 50 ms watchdog performs one snap after finger-up. */
    primary_pager_last_move_ms=now_ms();
    activity();
}
static lv_obj_t *primary_page_create(lv_obj_t *pager,int index)
{
    lv_obj_t *p=lv_obj_create(pager);
    lv_obj_set_size(p,466,466);
    lv_obj_set_pos(p,index*466,0);
    lv_obj_set_style_bg_color(p,lv_color_hex(prefs->background_color),0);
    lv_obj_set_style_bg_opa(p,LV_OPA_COVER,0);
    lv_obj_set_style_radius(p,0,0);
    lv_obj_set_style_border_width(p,0,0);
    lv_obj_set_style_pad_all(p,0,0);
    lv_obj_remove_flag(p,LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(p,LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(p,LV_OBJ_FLAG_EVENT_BUBBLE);
    lv_obj_add_flag(p,LV_OBJ_FLAG_GESTURE_BUBBLE);
    return p;
}
static void primary_pager_build_clock(void)
{
    if(!primary_pager_pages[0])return;
    if(clock_scene && (clock_scene_stale||clock_scene_style!=prefs->clock_face)){
        lf_clock_clear();
        lv_obj_delete(clock_scene);
        clock_scene=NULL;
    }
    if(clock_scene) {
        lv_obj_set_parent(clock_scene,primary_pager_pages[0]);
        lv_obj_remove_flag(clock_scene,LV_OBJ_FLAG_HIDDEN);
        lv_obj_center(clock_scene);
        atomic_fetch_add(&ui_clock_cache_hits,1);
        return;
    }
    uint64_t start=now_ms();
    clock_scene=lv_obj_create(primary_pager_pages[0]);
    lv_obj_set_size(clock_scene,466,466);
    lv_obj_center(clock_scene);
    lv_obj_set_style_pad_all(clock_scene,0,0);
    lv_obj_set_style_radius(clock_scene,0,0);
    lv_obj_set_style_border_width(clock_scene,0,0);
    lv_obj_set_style_bg_opa(clock_scene,LV_OPA_COVER,0);
    lv_obj_remove_flag(clock_scene,LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_remove_flag(clock_scene,LV_OBJ_FLAG_CLICKABLE);
    lf_clock_build(clock_scene,prefs->clock_face,prefs);
    clock_children_input_transparent(clock_scene);
    clock_scene_style=prefs->clock_face;
    clock_scene_stale=false;
    atomic_fetch_add(&ui_clock_scene_builds,1);
    uint32_t elapsed=(uint32_t)(now_ms()-start);
    atomic_store(&ui_clock_build_last_ms,elapsed);
    uint32_t maximum=atomic_load(&ui_clock_build_max_ms);
    if(elapsed>maximum)atomic_store(&ui_clock_build_max_ms,elapsed);
}
static void primary_pager_create(int target_index)
{
    if(!screen)return;
    primary_pager_initializing=true;
    primary_pager=lv_obj_create(screen);
    lv_obj_set_size(primary_pager,466,466);
    lv_obj_align(primary_pager,LV_ALIGN_CENTER,0,0);
    lv_obj_set_style_pad_all(primary_pager,0,0);
    lv_obj_set_style_pad_row(primary_pager,0,0);
    lv_obj_set_style_pad_column(primary_pager,0,0);
    lv_obj_set_style_border_width(primary_pager,0,0);
    lv_obj_set_style_radius(primary_pager,0,0);
    /* At the rightmost Settings edge, a momentary drag may expose the
       scroll container between clipped panels. LVGL's default is white:
       fill every such exposed pixel with the same dark page background.
       Keep bidirectional scrolling and non-elastic bounds unchanged. */
    lv_obj_set_style_bg_color(primary_pager,
                              lv_color_hex(prefs->background_color),0);
    /* 0.8.112: transparent pager. The screen behind it has the same
       colour, so edge overscroll still shows no white slice, and swipe
       midpoints (where no single page covers the screen) no longer paint
       an extra full-screen background layer. */
    lv_obj_set_style_bg_opa(primary_pager,LV_OPA_TRANSP,0);
    lv_obj_set_scroll_dir(primary_pager,LV_DIR_HOR);
    lv_obj_set_scroll_snap_x(primary_pager,LV_SCROLL_SNAP_NONE);
    lv_obj_set_scrollbar_mode(primary_pager,LV_SCROLLBAR_MODE_OFF);
    lv_obj_remove_flag(primary_pager,LV_OBJ_FLAG_SCROLL_ELASTIC);
    /* At either end there is no fourth page. Do not forward an outward
       horizontal swipe to the root (or another nested scrollable widget). */
    lv_obj_remove_flag(primary_pager,LV_OBJ_FLAG_SCROLL_CHAIN_HOR);
    /* One controlled snap on release; disable competing momentum. */
    lv_obj_remove_flag(primary_pager,LV_OBJ_FLAG_SCROLL_MOMENTUM);
    lv_obj_add_flag(primary_pager,LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(primary_pager,LV_OBJ_FLAG_EVENT_BUBBLE);
    lv_obj_add_flag(primary_pager,LV_OBJ_FLAG_GESTURE_BUBBLE);
    lv_obj_add_event_cb(primary_pager,primary_pager_scroll_event,LV_EVENT_ALL,NULL);
    for(int i=0;i<3;i++) primary_pager_pages[i]=primary_page_create(primary_pager,i);
    /* Construct static panels just once, rather than destroying/rebuilding
       each face whenever the user's finger crosses the 466px boundary. */
    primary_render_home_content(primary_pager_pages[1]);
    primary_render_settings_content(primary_pager_pages[2]);
    primary_pager_build_clock();
    primary_pager_frame_timer=lv_timer_create(primary_pager_snap_frame,16,NULL);
    if(primary_pager_frame_timer)lv_timer_pause(primary_pager_frame_timer);
    lv_obj_update_layout(primary_pager);
    lv_obj_scroll_to_x(primary_pager,target_index*466,LV_ANIM_OFF);
    primary_pager_initializing=false;
    primary_pager_last_move_ms=now_ms();
    primary_pager_commit(target_index);
    clock_present_start_us=esp_timer_get_time();
}
static void primary_pager_enter(int index)
{
    if(timed_overlay)lyrics_overlay_close();
    if(index<0||index>2)index=1;
    if(!primary_pager){
        make_base(); /* Original permanent root, gesture dispatch preserved. */
        primary_pager_create(index);
        return;
    }
    if(index==0)primary_pager_build_clock();
    if(index==primary_pager_current && !primary_pager_snapping){
        primary_pager_commit(index);
        return;
    }
    primary_pager_start_snap(index);
    activity();
}
static void build_clock_now(void)
{
    render_clock();
}

/* Weather's BACK button fires from within its own LVGL input callback.
   Defer destruction of the Weather scene until after that callback returns.
   Weather swipe exits already dispatch via commit_swipe asynchronously. */
static void weather_back_deferred(void *arg)
{
    (void)arg;
    if(page==11)build_clock();
}
static void on_clock_back(lv_event_t *e)
{
    (void)e; activity();
    if(page==11)lv_async_call(weather_back_deferred,NULL);
    else build_clock();
}
static void on_clock_options(lv_event_t *e)
{
    (void)e;
    activity();
    if(page==2)clock_settings_return_page=2;
    else if(page==6)clock_settings_return_page=6;
    build_clock_settings();
}
static void on_clock_settings_back(lv_event_t *e)
{
    (void)e;
    activity();
    if(clock_settings_return_page==6)build_clock();
    else build_settings();
}
static void on_cycle_face(lv_event_t *e)
{
    (void)e;
    prefs->clock_face = (uint8_t)((prefs->clock_face + 1) % 6);
    save_settings();
    build_clock_settings();
}
static void on_weather_interval(lv_event_t *e)
{
    (void)e;
    lf_weather_config_t cfg;
    lf_weather_get_config(&cfg);
    cfg.interval_minutes = cfg.interval_minutes == 15 ? 30 :
                           cfg.interval_minutes == 30 ? 60 :
                           cfg.interval_minutes == 60 ? 120 : 15;
    if (lf_weather_save_config(&cfg)) {
        activity();
        build_clock_settings();
    }
}
static void on_weather_location(lv_event_t *e)
{
    (void)e; activity(); build_weather_location();
}
static void on_basic_settings(lv_event_t *e)
{
    (void)e; activity(); build_basic_settings();
}
static void build_clock_settings_now(void)
{
    page = 7;
    make_base();
    label(screen, "CLOCK SETTINGS", 38, &lv_font_montserrat_24, prefs->text_color);
    char value[65];
    /* 0.8.112: single Basic face; no face picker. */
    (void)on_cycle_face;
    button(screen, "CUSTOMIZE CLOCK", 270, 55, 0, -70,
           0xB2C6D3, on_basic_settings, NULL);
    lf_weather_config_t cfg;
    lf_weather_get_config(&cfg);
    button(screen, "WEATHER LOCATION", 254, 51, 0, 40,
           0xB2C6D3, on_weather_location, NULL);
    snprintf(value, sizeof value, "Update: %u min", cfg.interval_minutes);
    button(screen, value, 239, 50, 0, 106,
           0xB2C6D3, on_weather_interval, NULL);
    button(screen, "BACK", 144, 50, 0, 176,
           prefs->text_color, on_clock_settings_back, NULL);
}
static void on_basic_font(lv_event_t *e)
{
    (void)e;
    prefs->basic_font_style = (prefs->basic_font_style + 1) % 20;
    save_settings();
    build_basic_settings();
}
static void on_basic_ring_mode(lv_event_t *e)
{
    (void)e;
    prefs->basic_ring_mode = (prefs->basic_ring_mode + 1) % 3;
    save_settings();
    build_basic_settings();
}
static void on_basic_date(lv_event_t *e)
{
    (void)e;
    prefs->basic_show_date = !prefs->basic_show_date;
    save_settings();
    build_basic_settings();
}
static void on_basic_weather(lv_event_t *e)
{
    (void)e;
    prefs->basic_weather = !prefs->basic_weather;
    if (prefs->basic_weather) {
        lf_weather_config_t cfg;
        lf_weather_get_config(&cfg);
        cfg.enabled = true;
        (void)lf_weather_save_config(&cfg);
    }
    save_settings();
    build_basic_settings();
}
static void on_basic_size(lv_event_t *e)
{
    prefs->basic_font_size = (uint8_t)lv_slider_get_value(lv_event_get_target(e));
    if (basic_font_size_label) {
        char text[40];
        snprintf(text, sizeof text, "Time size: %u%%", prefs->basic_font_size);
        lv_label_set_text(basic_font_size_label, text);
    }
    if (lv_event_get_code(e) == LV_EVENT_RELEASED) save_settings();
}
static void on_basic_colour(lv_event_t *e)
{
    selected_colour_target = (uint8_t)(uintptr_t)lv_event_get_user_data(e);
    activity();
    build_basic_colour();
}
static void on_basic_back(lv_event_t *e)
{
    (void)e; activity(); build_basic_settings();
}
static uint32_t *selected_basic_rgb(void)
{
    switch (selected_colour_target) {
        case 0: return &prefs->basic_background;
        case 1: return &prefs->basic_foreground;
        case 2: return &prefs->basic_ring_a;
        case 3: return &prefs->basic_ring_b;
        default: return &prefs->basic_ring_c;
    }
}
static uint32_t hue_rgb(int hue)
{
    int sector=(hue / 60) % 6;
    int d=(hue % 60)*255/60;
    int r=0, g=0, b=0;
    switch (sector) {
        case 0: r=255;g=d;break;
        case 1: r=255-d;g=255;break;
        case 2: g=255;b=d;break;
        case 3: g=255-d;b=255;break;
        case 4: r=d;b=255;break;
        default: r=255;b=255-d;break;
    }
    return (uint32_t)((r<<16)|(g<<8)|b);
}
static void on_basic_hue(lv_event_t *e)
{
    int hue=lv_slider_get_value(lv_event_get_target(e));
    uint32_t rgb=hue_rgb(hue);
    *selected_basic_rgb() = rgb;
    if (basic_colour_swatch)
        lv_obj_set_style_bg_color(basic_colour_swatch, lv_color_hex(rgb), 0);
    if (basic_hue_label) {
        char text[35];
        snprintf(text, sizeof text, "Spectrum: %d / 359", hue);
        lv_label_set_text(basic_hue_label,text);
    }
    if (lv_event_get_code(e)==LV_EVENT_RELEASED) save_settings();
}
static void build_basic_colour_now(void)
{
    page = 13;
    make_base();
    const char *titles[] = {"BACKGROUND","TIME TEXT","RING COLOR 1",
                            "RING COLOR 2","RING COLOR 3"};
    label(screen, titles[selected_colour_target], 48,
          &lv_font_montserrat_24, prefs->text_color);
    basic_colour_swatch = lv_obj_create(screen);
    lv_obj_set_size(basic_colour_swatch, 138, 94);
    lv_obj_align(basic_colour_swatch, LV_ALIGN_CENTER, 0, -64);
    lv_obj_set_style_bg_color(basic_colour_swatch, lv_color_hex(*selected_basic_rgb()), 0);
    lv_obj_set_style_radius(basic_colour_swatch, 28, 0);
    lv_obj_set_style_border_color(basic_colour_swatch, lv_color_hex(0xC6E2FF),0);
    lv_obj_set_style_border_width(basic_colour_swatch, 3, 0);
    basic_hue_label = label(screen, "Drag hue across the spectrum", 225,
                            &lv_font_montserrat_20, 0xCEDCEB);
    add_slider(screen, 283, 210, 0, 359, on_basic_hue);
    button(screen, "BACK", 161, 51, 0, 169, 0xB2C6D3, on_basic_back, NULL);
}
static void build_basic_settings_now(void)
{
    page = 12;
    make_base();
    label(screen, "BASIC CLOCK", 39, &lv_font_montserrat_24, prefs->text_color);
    lv_obj_t *list = lv_obj_create(screen);
    lv_obj_set_size(list, 322, 265);
    lv_obj_align(list, LV_ALIGN_CENTER, 0, -2);
    lv_obj_set_style_bg_color(list, lv_color_hex(0x0C1B2B), 0);
    lv_obj_set_style_border_width(list, 0, 0);
    lv_obj_set_style_radius(list, 18, 0);
    lv_obj_set_style_pad_all(list, 7, 0);
    lv_obj_set_style_pad_row(list, 8, 0);
    lv_obj_set_flex_flow(list, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(list, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_add_flag(list, LV_OBJ_FLAG_SCROLL_MOMENTUM);
    lv_obj_remove_flag(list, LV_OBJ_FLAG_GESTURE_BUBBLE);
    char text[65];
    snprintf(text, sizeof text, "Font preset %u / 20", prefs->basic_font_style + 1);
    button(list, text, 256, 50, 0, 0, 0xB2C6D3, on_basic_font, NULL);
    basic_font_size_label = lv_label_create(list);
    snprintf(text, sizeof text, "Time size: %u%%", prefs->basic_font_size);
    lv_label_set_text(basic_font_size_label, text);
    lv_obj_set_style_text_font(basic_font_size_label, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(basic_font_size_label, lv_color_hex(0xD2E7FD), 0);
    lv_obj_t *slider=lv_slider_create(list);
    lv_obj_set_size(slider, 249, 14);
    lv_slider_set_range(slider, 0, 100);
    lv_slider_set_value(slider, prefs->basic_font_size, LV_ANIM_OFF);
    lv_obj_add_event_cb(slider, on_basic_size, LV_EVENT_VALUE_CHANGED, NULL);
    lv_obj_add_event_cb(slider, on_basic_size, LV_EVENT_RELEASED, NULL);
    /* The ring renderer is disabled for performance. Retain saved values for
       backwards compatibility, but never expose controls that do nothing. */
    /* 0.8.112: background colour removed (fixed black). */
    button(list,"Time text color",256,50,0,0,0xB2C6D3,on_basic_colour,(void*)1);
    button(list, prefs->basic_show_date?"Date: ON":"Date: OFF",256,50,0,0,
           0xB2C6D3,on_basic_date,NULL);
    button(list, prefs->basic_weather?"Weather: ON":"Weather: OFF",256,50,0,0,
           0xB2C6D3,on_basic_weather,NULL);
    button(screen, "BACK", 150, 50, 0, 175, 0xB2C6D3, on_clock_options, NULL);
}
static void refresh_debug(void)
{
    if (!debug_text) return;
    lf_diag_snapshot_t d;
    lf_diag_snapshot(&d);
    char output[1400], battery[64], signal[24];
    bsp_display_profile_t lcd={0};
    bsp_display_profile(&lcd);
    lf_ui_bench_t bench={0};
    lf_ui_bench(&bench);
    if (d.battery_valid)
        snprintf(battery, sizeof battery, "%d%% %s%s",
                 d.battery.percentage,
                 d.battery.usb_connected ? "USB" : "Battery",
                 d.battery.charging ? " charging" : "");
    else snprintf(battery, sizeof battery, "unavailable");
    if (d.wifi_connected && d.wifi_rssi_dbm)
        snprintf(signal, sizeof signal, "%d dBm", d.wifi_rssi_dbm);
    else snprintf(signal, sizeof signal, "unavailable");
    snprintf(output, sizeof output,
             "Firmware: %s\nIP: %s\nWi-Fi: %.32s\nRSSI: %s\n"
             "Uptime: %lu seconds\nHeap: %lu KB free\n"
             "Min heap: %lu KB\nPSRAM: %lu KB free\n"
             "Max UI tick gap: %lu ms\nBattery: %s\n"
             "Microphone: OFF (not enabled)\n"
             "OTA: %s\nOTA bytes: %lu/%lu\n"
             "Internal RAM: %lu KB (largest %lu)\n"
             "LCD buffers: %s %lu rows\n"
             "LCD self-test: %lu KB/s, frame %lu ms\n"
             "CPU: %lu MHz (frames min %lu)\n"
             "Bench ms: blank %lu clock %lu home %lu set %lu\n"
             "Bench mid ms: clk/home %lu home/set %lu\n"
             "Recent errors:\n%s\n%s\n%s",
             LF_VERSION, d.ip_address,
             d.wifi_connected ? d.wifi_ssid : "offline",
             signal, (unsigned long)d.uptime_seconds,
             (unsigned long)(d.heap_free_bytes / 1024),
             (unsigned long)(d.heap_min_bytes / 1024),
             (unsigned long)(d.psram_free_bytes / 1024),
             (unsigned long)max_ui_tick_gap_ms, battery,
             lf_network_ota_result(),
             (unsigned long)lf_network_ota_received(),
             (unsigned long)lf_network_ota_total(),
             (unsigned long)(heap_caps_get_free_size(MALLOC_CAP_INTERNAL)/1024),
             (unsigned long)(heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL)/1024),
             lcd.lvgl_buffers_internal ? "internal" : "PSRAM",
             (unsigned long)lcd.lvgl_buffer_rows,
             (unsigned long)lcd.selftest_kbytes_per_s,
             (unsigned long)(lcd.selftest_frame_us/1000),
             (unsigned long)bench.cpu_mhz_measured,(unsigned long)bench.render_cpu_mhz_min,
             (unsigned long)bench.blank_frame_ms,(unsigned long)bench.page_frame_ms[0],
             (unsigned long)bench.page_frame_ms[1],(unsigned long)bench.page_frame_ms[2],
             (unsigned long)bench.mid_frame_ms[0],(unsigned long)bench.mid_frame_ms[1],
             d.errors[0][0] ? d.errors[0] : "None recorded", d.errors[1], d.errors[2]);
    lv_label_set_text(debug_text, output);
}

static void build_debug_now(void)
{
    page = 14;
    make_base();
    label(screen, "DEVICE DIAGNOSTICS", 33, &lv_font_montserrat_24, prefs->text_color);
    lv_obj_t *panel = lv_obj_create(screen);
    lv_obj_set_size(panel, 336, 287);
    lv_obj_align(panel, LV_ALIGN_CENTER, 0, -6);
    lv_obj_set_style_bg_color(panel, lv_color_hex(0x091625), 0);
    lv_obj_set_style_radius(panel, 15, 0);
    lv_obj_set_style_border_width(panel, 0, 0);
    lv_obj_set_style_pad_all(panel, 10, 0);
    lv_obj_set_scroll_dir(panel, LV_DIR_VER);
    debug_text = lv_label_create(panel);
    lv_obj_set_width(debug_text, 290);
    lv_label_set_long_mode(debug_text, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_color(debug_text, lv_color_hex(0xDFEAFE), 0);
    lv_obj_set_style_text_font(debug_text, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_line_space(debug_text, 7, 0);
    lv_obj_align(debug_text, LV_ALIGN_TOP_LEFT, 0, 0);
    last_debug_refresh = now_ms();
    refresh_debug();
    button(screen, "BACK HOME", 158, 49, 0, 172, 0xA8D6FF, on_home, NULL);
}

static void build_weather_details_now(void)
{
    page = 11;
    make_base();
    label(screen, "WEATHER", 25, &lv_font_montserrat_24, 0xE8F5FF);
    lf_weather_visual_build(screen);
    lf_weather_view_t weather;
    lf_weather_get_view(&weather);
    lf_weather_visual_update(&weather);
    weather_detail_refresh = now_ms();
    button(screen, "BACK TO CLOCK", 188, 50, 0, 173,
           0x9ACBF1, on_clock_back, NULL);
}
static void build_weather_location_now(void)
{
    page = 8;
    make_base();
    lf_weather_config_t cfg;
    lf_weather_get_config(&cfg);
    lf_charwheel_show(screen, "Weather city", cfg.city, false,
                      LF_WEATHER_CITY_MAX,
                      wheel_weather_save, wheel_weather_cancel);
}

/* Remove the wake overlay after the input event has finished. An explicit
   full-screen invalidation then redraws the pixels previously under it. */
static void wake_finish_async(void *arg)
{
    (void)arg;
    wake_cleanup_queued=false;
    /* LVGL async callbacks can run while the same wake finger is pressed.
       Never delete the active touch target; on_tick retries after release. */
    if(ui_pointer_is_down() || !sleep_overlay)return;
    lv_obj_t *cover=sleep_overlay;
    sleep_overlay=NULL;
    lv_obj_delete(cover);
    atomic_store(&wake_overlay_pending,false);
    const uint32_t started=atomic_load(&wake_started_ms);
    if(started)atomic_store(&wake_overlay_clear_ms,
                            (uint32_t)now_ms()-started);
    if(primary_pager && page==0 && home_record_layer){
        lf_recog_snapshot_t song={0};lf_recog_snapshot(&song);
        if(song.state!=LF_RECOG_CAPTURE && song.state!=LF_RECOG_UPLOAD &&
           song.state!=LF_RECOG_LYRICS)
            lv_obj_add_flag(home_record_layer,LV_OBJ_FLAG_HIDDEN);
    }
    invalidate_full_screen();
    /* Resume after deleting the wake cover, not during the active press. */
    if(clock_sweep_timer){
        lv_timer_reset(clock_sweep_timer);
        lv_timer_resume(clock_sweep_timer);
    }
}
static void on_wake(lv_event_t *e)
{
    (void)e;
    if(!asleep)return;
    const uint32_t current=(uint32_t)now_ms();
    const uint32_t started=atomic_load(&screen_off_started_ms);
    if(started)atomic_store(&last_screen_off_ms,current-started);
    atomic_fetch_add(&wake_count,1);
    atomic_store(&wake_started_ms,current);
    atomic_store(&wake_frame_pending_ms,current);
    asleep=false;
    atomic_store(&ui_screen_awake,true);
    atomic_store(&wake_overlay_pending,true);
    /* Prioritize the AMOLED and input task before creating the
       asynchronous Wi-Fi restart worker after prolonged radio-off. */
    lf_power_set_screen_awake(true);
    activity();
    apply_brightness();
    /* Reveal the unchanged page immediately, but the transparent clickable
       cover still captures this entire waking gesture. Removing it on the
       press (even via lv_async_call) would invalidate the active input target. */
    if(sleep_overlay){
        /* A pressed LVGL state can override the default style. Make both
           states transparent so releasing the waking finger cannot reveal
           an inherited opaque black cover before asynchronous cleanup. */
        lv_obj_set_style_bg_opa(sleep_overlay,LV_OPA_TRANSP,0);
        lv_obj_set_style_bg_opa(sleep_overlay,LV_OPA_TRANSP,LV_STATE_PRESSED);
        invalidate_full_screen();
    }
    /* Driver reconnection is asynchronous. It cannot gate the first
       visible wake frame, and on_tick retries after transient failures. */
    lf_network_set_idle(false);
    touch_held=false;
    gesture_origin_valid=false;
    gesture_indev=NULL;
    swipe_candidate_page=-1;
    touch_pressed_at_ms=0;
}
/* Optional hardware-level deep sleep. GPIO11 is CST9217 TP_INT on the
   Waveshare 1.75-in board (RTC GPIO). Requires physical wake testing: an
   asserted low INT at sleep entry would immediately wake, so guard it.
   Timer wake is a recovery fallback in case the touch controller will not
   generate an interrupt while the SoC sleeps. Default remains OFF. */
/* 0.8.112: on_tick samples `current` once, then helpers such as the pager
   watchdog call activity() with a fresh, slightly later now_ms(). The old
   unsigned `current - last_activity_ms` then wrapped to ~2^64 and blanked
   the screen instantly while it was being touched (logged idle_ms = 0). */
static uint64_t ui_idle_ms(uint64_t current)
{
    return current > last_activity_ms ? current - last_activity_ms : 0;
}
static void sleep_deep(void)
{
    if (gpio_get_level(GPIO_NUM_11) != 1) {
        ESP_LOGW(TAG,"Touch INT is low; rejecting deep sleep to avoid wake loop");
        return;
    }
    esp_err_t wake_err=esp_sleep_enable_ext0_wakeup(GPIO_NUM_11,0);
    if(wake_err!=ESP_OK){
        ESP_LOGW(TAG,"Cannot arm touch wake: %s",esp_err_to_name(wake_err));
        return;
    }
    (void)esp_sleep_enable_timer_wakeup(12ULL*60ULL*60ULL*1000000ULL);
    (void)bsp_display_brightness_set(0);
    ESP_LOGI(TAG,"Entering optional deep sleep; touch GPIO11 or timer will reboot device");
    esp_deep_sleep_start();
}
static void sleep_display(void)
{
    /* Screen timeout must never conceal a panel being actively touched,
       including nested Device Settings scroll/slider event targets. */
    if(asleep || ui_pointer_is_down() || pager_scrolling ||
       primary_pager_snapping || nav_transition_active)return;
    /* Preserve the current page, clock pointers and button tree. A single
       black touch-capture child handles wake without screen reconstruction. */
    {
        const uint64_t now_sleep=now_ms();
        sleep_info.count++;
        sleep_info.sleep_seconds=prefs?prefs->sleep_seconds:0;
        sleep_info.idle_ms=(uint32_t)ui_idle_ms(now_sleep);
        sleep_info.lvgl_idle_ms=lv_display_get_inactive_time(NULL);
        sleep_info.page=(uint32_t)page;
        sleep_info.uptime_s=(uint32_t)(now_sleep/1000ULL);
    }
    asleep=true;
    atomic_store(&ui_screen_awake,false);
    atomic_store(&wake_overlay_pending,false);
    atomic_store(&screen_off_started_ms,(uint32_t)now_ms());
    touch_held=false;
    gesture_origin_valid=false;
    gesture_indev=NULL;
    swipe_candidate_page=-1;
    wake_cleanup_queued=false;
    sleep_overlay=lv_obj_create(screen);
    lv_obj_set_size(sleep_overlay,466,466);
    lv_obj_center(sleep_overlay);
    lv_obj_set_style_bg_color(sleep_overlay,lv_color_black(),0);
    lv_obj_set_style_bg_opa(sleep_overlay,LV_OPA_COVER,0);
    lv_obj_set_style_border_width(sleep_overlay,0,0);
    lv_obj_set_style_radius(sleep_overlay,0,0);
    lv_obj_remove_flag(sleep_overlay,LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(sleep_overlay,LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(sleep_overlay,on_wake,LV_EVENT_PRESSED,NULL);
    /* Keep 50ms touch-to-wake live; suspend only idle 16ms clock callbacks. */
    if(clock_sweep_timer)lv_timer_pause(clock_sweep_timer);
    apply_brightness();
    /* Screen is already dark with a live touch target. Lower idle CPU
       frequency but retain Wi-Fi, OTA and all wake processing. */
    lf_power_set_screen_awake(false);
}
/* Avoid repeated LVGL label allocation and invalidation for unchanged text. */
static void ui_label_set_text_if_changed(lv_obj_t *obj,const char *text)
{
    if(obj && strcmp(lv_label_get_text(obj),text)!=0)
        lv_label_set_text(obj,text);
}
static void on_tick_body(lv_timer_t *timer)
{
    (void)timer;
    uint64_t current = now_ms();
    if (last_ui_tick_ms && current > last_ui_tick_ms) {
        uint64_t gap = current - last_ui_tick_ms;
        if (gap > max_ui_tick_gap_ms) max_ui_tick_gap_ms = (uint32_t)gap;
    }
    last_ui_tick_ms = current;
    /* 0.8.112: LVGL stamps every input-device press itself. Use it so a
       touch that no widget callback or 50 ms pointer poll caught (common
       during slow frames) still counts as activity; the screen timeout was
       blanking the display while it was in use. */
    if(!asleep){
        const uint32_t idle=lv_display_get_inactive_time(NULL);
        if(current>=idle && current-idle>last_activity_ms)last_activity_ms=current-idle;
    }
    atomic_store(&ui_last_tick_ms,(uint32_t)current);
    atomic_store(&ui_last_page,(uint32_t)page);
    /* Nested LVGL scrollable children do not always bubble PRESSED to the
       screen's on_gesture_press() callback. Polling the actual pointer
       keeps activity fresh on every page, including the device scroller,
       before nav-transition / overlay early-returns. */
    if(!asleep && ui_pointer_is_down())last_activity_ms=current;
    /* Recover transient radio restart failures without creating workers on
       every 50ms screen tick. No driver calls execute on the LVGL task. */
    static uint64_t last_wifi_resume_retry_ms;
    if(!asleep && current-last_wifi_resume_retry_ms>=1000U){
        last_wifi_resume_retry_ms=current;
        lf_network_set_idle(false);
    }
    if(primary_pager && !timed_overlay)primary_pager_watchdog(current);
    /* Process a confirmed double-tap only after input dispatch and the
       physical release. No overlay mutation or networking in the touch
       callback; never schedule more than one cancellation per scan. */
    if(home_record_cancel_pending_attempt && !ui_pointer_is_down() &&
       !nav_transition_active &&
       current-(uint64_t)atomic_load(&last_record_cancel_ms)>=150U){
        const uint32_t cancelled_attempt=home_record_cancel_pending_attempt;
        /* Avoid a second redraw while the input driver settles from the
           double-click. Snapshot may race a completed microphone worker,
           but this UI-only cancellation remains attempt scoped. */
        lf_recog_snapshot_t cancelled={0};
        lf_recog_snapshot(&cancelled);
        atomic_store(&ui_record_cancel_phase,2U);
        home_record_cancel_pending_attempt=0;
        /* The worker may finish after Stop but before the timer fires.
           Always retire this attempt's overlay once the finger is up. */
        if(page==0 && cancelled.attempts==cancelled_attempt){
            home_stalled_attempt=cancelled_attempt;
            /* Cancellation is already visible as the return to Home.
               Never leave a delayed "Cancelling scan..." label behind. */
            home_warning=NULL;
            home_warning_until_ms=0;
            lf_recog_stop(); /* cancellation flag; existing worker shuts down */
            /* Restore pager input immediately; the worker may still have a
               15s socket timeout to clear. Repeated timers must not relock. */
            if(primary_pager)lv_obj_add_flag(primary_pager,LV_OBJ_FLAG_SCROLLABLE);
            if(home_record_layer &&
               !lv_obj_has_flag(home_record_layer,LV_OBJ_FLAG_HIDDEN))
                lv_obj_add_flag(home_record_layer,LV_OBJ_FLAG_HIDDEN);
            atomic_store(&ui_record_cancel_phase,3U);
            /* Hiding the overlay ALREADY invalidates its old bounds.
               A second full-screen invalidation plus immediate Home label
               and button style updates can launch multiple display flushes
               from this single touch release. Let LVGL finish the overlay
               flush before the ordinary 750ms Home refresh runs. */
            home_last_refresh_ms=current;
            atomic_store(&ui_record_cancel_phase,4U);
        }
    }
    /* Never start a deferred microphone capture during touch, swipe or sleep. */
    if(home_scan_armed){
        if(page!=0 || asleep || nav_transition_active || nav_queued){
            home_scan_armed=false;
            home_scan_released_ms=0;
        }else if(ui_pointer_is_down()){
            home_scan_released_ms=0;
        }else if(!home_scan_released_ms){
            home_scan_released_ms=current;
        }else if(current-home_scan_released_ms>=LF_HOME_SCAN_SETTLE_MS){
            home_scan_armed=false;
            home_scan_released_ms=0;
            if(!lf_recog_start()){
                home_warning="Scanner unavailable - try again";
                home_warning_until_ms=current+4500U;
            }
            home_refresh(current);
        }
    }
    if (nav_transition_active) return; /* No UI activity until reveal completes. */
    /* Manual song identification continues after microphone capture,
       including AudD response and lyric lookup. Preserve the OLED until
       the worker completes; an already timed-out scan may sleep normally. */
    if (page == 0 && lf_recog_busy()) {
        lf_recog_snapshot_t active_scan={0};
        lf_recog_snapshot(&active_scan);
        if (active_scan.attempts &&
            active_scan.attempts != home_stalled_attempt)
            last_activity_ms = current;
    }
    /* Polling fallback for wake when the cover misses a press. A wake touch
       must never trigger navigation or click the previously shown buttons. */
    if(asleep) {
        /* The earlier late grace-period check was unreachable: this first
           asleep branch returned before reaching it. */
        const uint32_t off_at=atomic_load(&screen_off_started_ms);
        if(off_at && (uint32_t)current-off_at>=120000U)
            lf_network_set_idle(true);
        lv_indev_t *wake=lv_indev_get_next(NULL);
        while(wake) {
            if(lv_indev_get_type(wake)==LV_INDEV_TYPE_POINTER &&
               lv_indev_get_state(wake)==LV_INDEV_STATE_PRESSED) {
                on_wake(NULL);
                return;
            }
            wake=lv_indev_get_next(wake);
        }
        return;
    }
    /* After waking, consume the entire first press/release on the retained
       transparent cover. Only remove it from the next LVGL async turn once
       the physical pointer is up; do not let the wake tap navigate. */
    if(sleep_overlay){
        if(!ui_pointer_is_down() && !wake_cleanup_queued){
            wake_cleanup_queued=true;
            lv_async_call(wake_finish_async,NULL);
        }
        return;
    }
    /* Collect touch directly from the hardware input device, including
       gestures beginning over Weather's scrolling forecast children. */
    if (!touch_held && !recognition_touch_locked() &&
        is_swipe_page() && !clock_settings_popup) {
        lv_indev_t *ptr=lv_indev_get_next(NULL);
        while(ptr) {
            if(lv_indev_get_type(ptr)==LV_INDEV_TYPE_POINTER &&
               lv_indev_get_state(ptr)==LV_INDEV_STATE_PRESSED) {
                gesture_indev=ptr;
                lv_indev_get_point(ptr,&gesture_origin);
                gesture_origin_valid=true;
                touch_held=true;
                touch_pressed_at_ms=current;
                swipe_candidate_page=-1;
                swipe_candidate_animation=LV_SCR_LOAD_ANIM_NONE;
                break;
            }
            ptr=lv_indev_get_next(ptr);
        }
    }
    if (touch_held && gesture_indev) {
        if (lv_indev_get_state(gesture_indev) == LV_INDEV_STATE_RELEASED)
            finish_touch();
        else
            observe_touch_position();
    }
    if (page == 6 && touch_held && !pager_scrolling && touch_pressed_at_ms &&
        current - touch_pressed_at_ms >= LF_CLOCK_SETTINGS_HOLD_MS)
        show_clock_settings_popup();
    if(page==0 && !asleep && home_record_layer &&
       current-home_record_last_tick>=LF_RECORD_WAVE_REFRESH_MS){
        lf_recog_snapshot_t scan={0};
        lf_recog_snapshot(&scan);
        if(scan.attempts!=home_stalled_attempt &&
           (scan.state==LF_RECOG_CAPTURE || scan.state==LF_RECOG_UPLOAD ||
            scan.state==LF_RECOG_LYRICS ||
            !lv_obj_has_flag(home_record_layer,LV_OBJ_FLAG_HIDDEN)))
            home_recording_update(&scan,current);
    }
    /* Append title/artist after a completed match, whether or not LRCLIB
       returned synchronized lyrics. No new audio storage or recognition
       calls; this one-time NVS index is independent of the old song cache. */
    if(page==0 && !asleep){
        lf_recog_snapshot_t known={0};
        lf_recog_snapshot(&known);
        if(known.state==LF_RECOG_MATCH && known.attempts &&
           known.attempts!=home_history_logged_attempt &&
           known.title[0] && known.artist[0]){
            home_history_logged_attempt=known.attempts;
            (void)lf_song_history_remember(known.title,known.artist);
        }
    }
    /* UI-timer retry is independent of Home's 750ms text refresh and
       stale touch_held. Even if the first async request loses a race,
       the song reopens when the physical finger is actually released. */
    if(page==0 && !home_scan_armed && !auto_open_pending && !auto_open_transition &&
       current-auto_open_poll_ms>=200){
        auto_open_poll_ms=current;
        lf_recog_snapshot_t latest={0};
        lf_recog_snapshot(&latest);
        if(latest.state==LF_RECOG_MATCH &&
           latest.attempts!=home_stalled_attempt &&
           latest.attempts!=auto_open_attempt &&
           latest.lyric_lines>0 &&
           strcmp(latest.lyrics_state,"synced")==0 &&
           !ui_pointer_is_down()){
            auto_open_pending=true;
            auto_open_requested_ms=current;
            atomic_fetch_add(&auto_open_queued_count,1);
            lv_async_call(auto_open_lyrics_deferred,NULL);
        }
    }
    /* Hard fallback for the real observed freeze: if the song and synced
       lines are ready but an async navigation has not reached the lyrics
       scene in four seconds, recover directly on the LVGL TIMER thread.
       This is outside input dispatch, so deleting the retired Home child
       tree here does not destroy an active touch callback. One recovery
       attempt per scan prevents repeated rebuilding if no lines survive. */
    if(page==0 && auto_open_pending && auto_open_requested_ms &&
       current-auto_open_requested_ms>=4000 &&
       !nav_transition_active && !ui_pointer_is_down()){
        lf_recog_snapshot_t ready={0};
        lf_recog_snapshot(&ready);
        if(ready.state==LF_RECOG_MATCH && ready.lyric_lines>0 &&
           strcmp(ready.lyrics_state,"synced")==0 &&
           ready.attempts!=auto_open_attempt &&
           ready.attempts!=auto_open_fallback_attempt){
            auto_open_fallback_attempt=ready.attempts;
            /* Any deferred nav_drain still in the LVGL queue finds NULL
               and returns; do not run a second scene rebuild afterwards. */
            nav_pending=NULL;
            nav_queued=false;
            auto_open_transition=false;
            auto_open_pending=false;
            atomic_fetch_add(&auto_open_recovery_count,1);
            ESP_LOGW(TAG,"Recovering stalled lyric navigation for attempt %lu",
                     (unsigned long)ready.attempts);
            build_timed_lyrics_now(&ready);
            if(page==1 && timed_lyrics_mode && timed_line_count>0){
                auto_open_attempt=ready.attempts;
                atomic_fetch_add(&auto_open_completed_count,1);
                atomic_store(&ui_lyrics_scene_phase,6U);
                lv_obj_update_layout(screen);
                lv_obj_invalidate(screen);
            }
            return;
        }
        /* A finished scan without synced data must not leave the loading
           overlay visible forever, even if a prior callback was queued. */
        if(ready.state!=LF_RECOG_MATCH || ready.lyric_lines==0)
            auto_open_pending=false;
    }
    if (page == 0 && !touch_held && !pager_scrolling &&
        !ui_pointer_is_down() &&
        current-home_last_refresh_ms >= 750){
        home_refresh(current);
        if(atomic_load(&ui_record_cancel_phase)==4U)
            atomic_store(&ui_record_cancel_phase,5U);
    }
    if (page == 14 && current - last_debug_refresh >= 2500) {
        last_debug_refresh = current;
        refresh_debug();
    }
    if (page == 4 && !asleep && current - last_wifi_poll_ms >= 350) {
        last_wifi_poll_ms = current;
        size_t n = 0;
        int scan_state = lf_network_scan_results(wifi_aps, LF_WIFI_MAX_RESULTS, &n);
        if (scan_state != wifi_state_previous && scan_state != 1) {
            wifi_state_previous = scan_state;
            wifi_ap_count = n;
            draw_wifi_list();
            if (scan_state < 0 && wifi_status) lv_label_set_text(wifi_status, "Scan failed. Tap SCAN.");
        }
    }
    if ((page == 0 || page == 10 || page == 15) && !asleep && current - last_power_refresh_ms >= 1500) {
        last_power_refresh_ms = current;
        lf_power_state_t st;
        bool success=lf_power_cached(&st);
        if (battery_status) {
            char value[68];
            const char *icon = !success ? LV_SYMBOL_WARNING :
                st.charging ? LV_SYMBOL_CHARGE :
                st.percentage >= 0 && st.percentage < 20 ? LV_SYMBOL_BATTERY_1 :
                st.percentage >= 0 && st.percentage < 40 ? LV_SYMBOL_BATTERY_2 :
                st.percentage >= 0 && st.percentage < 70 ? LV_SYMBOL_BATTERY_3 :
                LV_SYMBOL_BATTERY_FULL;
            if (success && st.percentage >= 0)
                snprintf(value,sizeof value,"%s  %s  %d%%",icon,lf_power_symbol(&st),st.percentage);
            else if (success) snprintf(value,sizeof value,"%s  %s",icon,lf_power_symbol(&st));
            else snprintf(value,sizeof value,"%s  BATTERY",LV_SYMBOL_BATTERY_FULL);
            if (success) {
                ui_label_set_text_if_changed(battery_status,value);
                lv_obj_set_style_text_color(battery_status,lv_color_hex(st.usb_connected ? prefs->charging_color : lf_power_color(&st)),0);
            }
        }
        if (network_status) {
            char text[128];
            char ssid[33] = {0};
            if (page==0)
                snprintf(text,sizeof text,"Open: http://%s/",lf_network_address());
            else if (lf_network_connected_ssid(ssid, sizeof ssid))
                snprintf(text, sizeof text, "Wi-Fi: %.32s", ssid);
            else if (*lf_network_ap_ssid())
                snprintf(text, sizeof text, "Setup: %.32s", lf_network_ap_ssid());
            else snprintf(text, sizeof text, "Wi-Fi connecting...");
            if(page==0)home_set_text(network_status,text);
            else ui_label_set_text_if_changed(network_status,text);
        }
        if (network_password) {
            char text[72];
            if (*lf_network_ap_ssid())
                snprintf(text, sizeof text, "Password: %s", lf_network_ap_password());
            else snprintf(text, sizeof text, "Connected to home Wi-Fi");
            ui_label_set_text_if_changed(network_password, text);
        }
        if (network_url) {
            char url_text[85];
            snprintf(url_text, sizeof url_text, "Open browser to http://%s/", lf_network_address());
            ui_label_set_text_if_changed(network_url, url_text);
        }
    }
    /* Check deep-sleep deadline even after the screen has already turned off.
       Never deep sleep during diagnostics, onboarding, or active lyric play. */
    static uint64_t deep_sleep_retry_after_ms;
    if (prefs->deep_sleep_minutes && !(page == 1 && timed_lyrics_mode) &&
        page != 14 && page != 15 &&
        ui_idle_ms(current) >= (uint64_t)prefs->deep_sleep_minutes*60000ULL &&
        current >= deep_sleep_retry_after_ms) {
        /* If touch INT is held low or wake setup fails, remain safely in
           screen-off mode and retry once per minute, not every 50 ms. */
        deep_sleep_retry_after_ms=current+60000ULL;
        sleep_deep();
    }
    if (page == 11 && !touch_held && current >= screen_animation_until_ms && current - weather_detail_refresh >= 4000) {
        weather_detail_refresh = current;
        lf_weather_view_t weather;
        lf_weather_get_view(&weather);
        lf_weather_visual_update(&weather);
    }
    if (page == 6 && !pager_scrolling && !touch_held && !clock_settings_popup && current >= screen_animation_until_ms && current - clock_last_refresh >= 950) {
        clock_last_refresh = current;
        lf_clock_update();
    }
    /* Preserve the live song and alignment modal during calibration.
       The old paused-song sleep blacked out controls without restoration. */
    if(page==1 && timed_lyrics_mode)last_activity_ms=current;
    if (prefs->sleep_seconds && !(page == 1 && timed_lyrics_mode) &&
        page != 14 && page != 15 && !ui_pointer_is_down() &&
        !pager_scrolling && !primary_pager_snapping && !nav_transition_active &&
        ui_idle_ms(current) > (uint64_t)prefs->sleep_seconds * 1000ULL) {
        sleep_display();
        return;
    }
    if(page==1 && timed_lyrics_mode)
        return; /* Dedicated 25ms lyric timer owns timestamp-paced movement. */
    if (page != 1 || paused || touch_held || user_dragging || finger_was_scrolling ||
        !scroll_panel || (touch_resume_at_ms && current < touch_resume_at_ms)) {
        last_scroll_ms = current;
        return;
    }
    uint64_t dt = current - last_scroll_ms;
    if (dt > 200) dt = 200;
    last_scroll_ms = current;
    if (lv_obj_get_scroll_bottom(scroll_panel) <= 0) {
        /* The demo loops after a short reading pause; real songs will use
           playback/lyric timestamps instead of a perpetual preview loop. */
        if (!scroll_at_end_ms) scroll_at_end_ms = current;
        if (current - scroll_at_end_ms >= 3500) {
            lv_obj_scroll_to_y(scroll_panel, 0, LV_ANIM_OFF);
            scroll_y = 0;
            scroll_at_end_ms = 0;
        }
        return;
    }
    scroll_at_end_ms = 0;
    /* Accumulate subpixel motion. Reading back integer Y every tick would
       truncate the fractional step to zero and stop automatic scrolling. */
    /* User-calibrated: new 100% matches the previous 35% (~63.5 px/s).
       Subpixel float accumulation keeps motion smooth at low speeds. */
    /* Preserve the tuned 30..100% speeds. Open a much slower, controllable
       0..29% range without changing the existing high-end calibration. */
    const float px_sec=prefs->scroll_speed>=30 ?
        3.0f+0.605f*(float)prefs->scroll_speed :
        0.5f+(20.65f/900.0f)*(float)prefs->scroll_speed*(float)prefs->scroll_speed;
    scroll_y += (float)dt * (px_sec/1000.0f);
    lv_obj_scroll_to_y(scroll_panel, (int32_t)scroll_y, LV_ANIM_OFF);
}

/* Measures the entire 50ms UI callback, including all early returns. */
static void on_tick(lv_timer_t *timer)
{
    const int64_t start=esp_timer_get_time();
    on_tick_body(timer);
    const int64_t elapsed=esp_timer_get_time()-start;
    const uint32_t us=(uint32_t)(elapsed>0?elapsed:0);
    atomic_store(&ui_tick_work_last_us,us);
    if(us>atomic_load(&ui_tick_work_max_us))
        atomic_store(&ui_tick_work_max_us,us);
}

/* Only the active Rolex-style clock needs a high-rate redraw.
   The rest of the UI retains its lower-cost 50 ms timer. */
static void on_clock_sweep(lv_timer_t *timer)
{
    (void)timer;
    if (page == 6 && !nav_transition_active && !pager_scrolling && !asleep && !touch_held && !clock_settings_popup && now_ms() >= screen_animation_until_ms) lf_clock_animate();
}

/* Deliberately NOT PrintSphere's multi-page scroller. CO5300 keeps the
   departing pixels in panel GRAM while we build the new scene with LVGL
   invalidation disabled. Only the newly revealed strips are transferred.
   This prevents N entire-frame flushes during an N-frame transition.
   Exactly one LVGL screen stays active, preserving 0.8.19 touch safety. */
static void nav_transition_end(bool interrupted)
{
    if (!nav_transition_active) return;
    nav_transition_active=false;
    nav_transition_frame_pending=false;
    if(nav_transition_timer) {
        lv_timer_delete(nav_transition_timer);
        nav_transition_timer=NULL;
    }
    lv_display_t *disp=lv_display_get_default();
    bool enabled=disp && lv_display_is_invalidation_enabled(disp);
    if(enabled) lv_display_enable_invalidation(disp,false);
    if(nav_transition_catcher){lv_obj_delete(nav_transition_catcher);nav_transition_catcher=NULL;}
    if(enabled) lv_display_enable_invalidation(disp,true);
    /* The 466px reveal already invalidated all incoming pixels. A synthetic
       accent divider was visibly green and remained distracting during clock
       loading; no edge object or trailing edge repaint is necessary.
       Only an interrupted reveal needs a recovery full-screen refresh. */
    if(screen && interrupted) lv_obj_invalidate(screen);
    atomic_fetch_add(&nav_transition_completed_count,1);
    atomic_store(&nav_transition_last_ms,(uint32_t)(now_ms()-nav_transition_started_ms));
}
__attribute__((unused)) static void nav_transition_tick(lv_timer_t *timer)
{
    (void)timer;
    if(!nav_transition_active||!screen)return;
    const uint64_t current=now_ms();
    if(nav_transition_frame_pending){
        if(atomic_load(&ui_render_count)==nav_transition_frame_render_at_issue ||
           atomic_load(&ui_refresh_ready_count)==nav_transition_frame_ready_at_issue){
            if(current-nav_transition_last_frame_ms>1500U)nav_transition_end(true);
            return;
        }
        nav_transition_frame_pending=false;
    }
    /* The strip already invalidated is now actually on the display. Avoid
       advancing the 12-step wipe against stale panel memory. */
    if(nav_transition_step>=LF_NAV_REVEAL_STEPS){nav_transition_end(false);return;}
    if(nav_transition_step &&
       current-nav_transition_last_frame_ms<LF_NAV_REVEAL_PERIOD_MS)return;
    nav_transition_step++;
    /* Integer smoothstep eased 0..1024, 64-bit arithmetic avoids overflow. */
    int64_t t=(int64_t)nav_transition_step*1024/LF_NAV_REVEAL_STEPS;
    int64_t eased=t*t*(3072-2*t)/(1024*1024);
    int32_t next=(int32_t)(eased*LF_NAV_PANEL_SIZE/1024);
    if(nav_transition_step==LF_NAV_REVEAL_STEPS)next=LF_NAV_PANEL_SIZE;
    if(next<=nav_transition_cursor)next=nav_transition_cursor+1;
    if(next>LF_NAV_PANEL_SIZE)next=LF_NAV_PANEL_SIZE;
    const int32_t prev=nav_transition_cursor;
    const bool vertical=nav_transition_dir==LV_SCR_LOAD_ANIM_OVER_TOP||
                        nav_transition_dir==LV_SCR_LOAD_ANIM_OVER_BOTTOM;
    const bool reverse=nav_transition_dir==LV_SCR_LOAD_ANIM_OVER_LEFT||
                       nav_transition_dir==LV_SCR_LOAD_ANIM_OVER_TOP;
    lv_area_t region={0,0,LF_NAV_PANEL_SIZE-1,LF_NAV_PANEL_SIZE-1};
    if(vertical){
        region.y1=reverse?LF_NAV_PANEL_SIZE-next:prev;
        region.y2=reverse?LF_NAV_PANEL_SIZE-prev-1:next-1;
    }else{
        region.x1=reverse?LF_NAV_PANEL_SIZE-next:prev;
        region.x2=reverse?LF_NAV_PANEL_SIZE-prev-1:next-1;
    }
    nav_transition_cursor=next;
    nav_transition_frame_render_at_issue=atomic_load(&ui_render_count);
    nav_transition_frame_ready_at_issue=atomic_load(&ui_refresh_ready_count);
    nav_transition_frame_pending=true;
    nav_transition_last_frame_ms=current;
    lv_obj_invalidate_area(screen,&region);
    /* The final strip is completed before deleting the touch catcher. */
}
static void nav_transition_begin(lv_screen_load_anim_t direction)
{
    /* 0.8.112: instant screen change. The 12-strip reveal waited for a
       rendered frame ACK per strip and read as 1-2 fps stutter. One full
       redraw is faster and feels like a phone page switch. */
    (void)direction;
    if(screen)lv_obj_invalidate(screen);
    return;
#if 0
    if(!screen || !lv_display_get_default()){
        if(screen)lv_obj_invalidate(screen);
        return;
    }
    nav_transition_dir=direction;
    if(direction!=LV_SCR_LOAD_ANIM_OVER_LEFT && direction!=LV_SCR_LOAD_ANIM_OVER_RIGHT &&
       direction!=LV_SCR_LOAD_ANIM_OVER_TOP && direction!=LV_SCR_LOAD_ANIM_OVER_BOTTOM)
        nav_transition_dir=LV_SCR_LOAD_ANIM_OVER_LEFT;
    nav_transition_active=true;
    nav_transition_step=0;
    nav_transition_cursor=0;
    nav_transition_frame_pending=false;
    nav_transition_last_frame_ms=0;
    nav_transition_started_ms=now_ms();
    lv_display_t *disp=lv_display_get_default();
    lv_display_enable_invalidation(disp,false);
    nav_transition_catcher=lv_obj_create(screen);
    lv_obj_set_size(nav_transition_catcher,466,466);
    lv_obj_center(nav_transition_catcher);
    lv_obj_set_style_bg_opa(nav_transition_catcher,LV_OPA_TRANSP,0);
    lv_obj_set_style_border_width(nav_transition_catcher,0,0);
    lv_obj_set_style_radius(nav_transition_catcher,0,0);
    lv_obj_remove_flag(nav_transition_catcher,LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(nav_transition_catcher,LV_OBJ_FLAG_CLICKABLE);
    lv_display_enable_invalidation(disp,true);
    nav_transition_timer=lv_timer_create(nav_transition_tick,LF_NAV_REVEAL_PERIOD_MS,NULL);
    if(!nav_transition_timer)nav_transition_end(true);
#endif
}
/* Deferred scene construction, with all LVGL modifications on its owning task. */
static void nav_drain(void *arg)
{
    (void)arg;
    nav_queued=false;
    void (*target)(void)=nav_pending;
    nav_pending=NULL;
    if(!target)return;
    /* Recognition takes a direct, one-time transition: no 16-strip legacy
       reveal or duplicate screen rebuild while the audio worker finishes. */
    const bool recognition_open=auto_open_transition;
    auto_open_transition=false;
    if(nav_transition_active)nav_transition_end(true);
    bool was_pager=(primary_pager!=NULL);
    int previous_page=page;
    lv_screen_load_anim_t direction=nav_transition_hint;
    nav_transition_hint=LV_SCR_LOAD_ANIM_NONE;
    if(direction==LV_SCR_LOAD_ANIM_NONE)
        direction=(previous_page==6 || previous_page==11 || previous_page==7)
                    ? LV_SCR_LOAD_ANIM_OVER_RIGHT : LV_SCR_LOAD_ANIM_OVER_LEFT;
    lv_display_t *disp=lv_display_get_default();
    atomic_store(&ui_nav_phase,1U);
    /* Removing an opaque lyrics overlay requires real invalidations, not
       the stale panel GRAM from the previous recording screen. */
    const bool exiting_lyrics=timed_overlay!=NULL;
    const bool mute_invalidation=disp&&screen&&!recognition_open&&!exiting_lyrics;
    if(mute_invalidation)lv_display_enable_invalidation(disp,false);
    target();
    atomic_store(&ui_nav_phase,2U);
    if(disp&&screen){
        lv_obj_update_layout(screen);
        if(mute_invalidation)lv_display_enable_invalidation(disp,true);
    }
    atomic_fetch_add(&ui_nav_count,1);
    atomic_store(&ui_nav_phase,3U);
    if(recognition_open){
        auto_open_pending=false;
        if(page==1 && timed_lyrics_mode && timed_line_count>0){
            auto_open_attempt=timed_scan_attempt;
            atomic_fetch_add(&auto_open_completed_count,1);
            atomic_store(&ui_lyrics_scene_phase,6U);
            ESP_LOGI(TAG,"Automatic lyrics scene confirmed for attempt %lu",
                     (unsigned long)auto_open_attempt);
        }else{
            ESP_LOGW(TAG,"Automatic lyrics scene not ready; user may retry");
        }
        if(screen && !asleep)lv_obj_invalidate(screen);
        return; /* Never run the old panel-wipe after automatic lyrics. */
    }
    /* Cached pager needs an explicit redraw after the lyric child exits. */
    if(was_pager && primary_pager){
        if(screen && !asleep)lv_obj_invalidate(screen);
        return;
    }
    if(screen && primary_pager && previous_page!=page && !asleep){
        /* Main pager rebuilt after Clock Settings/Weather: render it in
           one pass, not 12 staggered strips showing a half-black face. */
        lv_obj_invalidate(screen);
    }else if(disp&&screen&&previous_page!=page&&!asleep)
        nav_transition_begin(direction);
    else if(screen)lv_obj_invalidate(screen);
}
static void nav_schedule(void (*target)(void))
{
    nav_pending=target;
    if(!nav_queued) {
        nav_queued=true;
        lv_async_call(nav_drain,NULL);
    }
}
static void build_connection_help(void) { nav_schedule(build_connection_help_now); }
static void build_home(void) { nav_schedule(build_home_now); }
static void build_preview(void) { nav_schedule(build_preview_now); }
static void build_wifi(void) { nav_schedule(build_wifi_now); }
static void build_wifi_password(void) { nav_schedule(build_wifi_password_now); }
static void build_settings(void) { nav_schedule(build_settings_now); }
static void build_lyrics_settings(void) { nav_schedule(build_lyrics_settings_now); }
static void build_device_settings(void) { nav_schedule(build_device_settings_now); }
static void build_clock(void) { nav_schedule(build_clock_now); }
static void build_clock_settings(void) { nav_schedule(build_clock_settings_now); }
static void build_basic_colour(void) { nav_schedule(build_basic_colour_now); }
static void build_basic_settings(void) { nav_schedule(build_basic_settings_now); }
static void build_debug(void) { nav_schedule(build_debug_now); }
static void build_weather_details(void) { nav_schedule(build_weather_details_now); }
static void build_weather_location(void) { nav_schedule(build_weather_location_now); }


/* ---- 0.8.112 boot benchmark ------------------------------------------- */
static lf_ui_bench_t ui_bench;
void lf_ui_bench(lf_ui_bench_t *out)
{
    if(!out)return;
    *out=ui_bench;
    const unsigned mn=atomic_load(&render_cpu_mhz_min);
    out->render_cpu_mhz_min=mn==UINT32_MAX?0:mn;
    out->render_cpu_mhz_last=atomic_load(&render_cpu_mhz_last);
}
static uint32_t bench_full_refresh_ms(void)
{
    lv_obj_t *active=lv_screen_active();
    if(!active)return 0;
    lv_obj_invalidate(active);
    const int64_t t0=esp_timer_get_time();
    lv_refr_now(NULL);
    return (uint32_t)((esp_timer_get_time()-t0+500)/1000);
}
static void ui_boot_benchmark(void)
{
    /* CPU clock from the cycle counter over 5 ms of wall time. */
    const uint32_t c0=esp_cpu_get_cycle_count();
    const int64_t t0=esp_timer_get_time();
    while(esp_timer_get_time()-t0<5000){}
    const uint32_t c1=esp_cpu_get_cycle_count();
    ui_bench.cpu_mhz_measured=(c1-c0)/5000U;
    volatile uint32_t acc=0;
    const int64_t l0=esp_timer_get_time();
    for(uint32_t i=0;i<2000000U;i++)acc+=i*2654435761U;
    ui_bench.cpu_loop_us=(uint32_t)(esp_timer_get_time()-l0);
    (void)acc;
    if(!primary_pager)return;
    const bool was_init=primary_pager_initializing;
    primary_pager_initializing=true; /* scroll handler stays passive */
    lv_obj_add_flag(primary_pager,LV_OBJ_FLAG_HIDDEN);
    ui_bench.blank_frame_ms=bench_full_refresh_ms();
    lv_obj_remove_flag(primary_pager,LV_OBJ_FLAG_HIDDEN);
    static const int32_t xs[5]={0,466,932,233,699};
    for(int i=0;i<5;i++){
        lv_obj_scroll_to_x(primary_pager,xs[i],LV_ANIM_OFF);
        lv_obj_update_layout(primary_pager);
        const uint32_t ms=bench_full_refresh_ms();
        if(i<3)ui_bench.page_frame_ms[i]=ms;
        else ui_bench.mid_frame_ms[i-3]=ms;
    }
    lv_obj_scroll_to_x(primary_pager,primary_pager_current*466,LV_ANIM_OFF);
    primary_pager_initializing=was_init;
    ESP_LOGI(TAG,"Benchmark: cpu=%luMHz loop=%luus blank=%lums clock=%lums home=%lums settings=%lums mid_ch=%lums mid_hs=%lums",
             (unsigned long)ui_bench.cpu_mhz_measured,(unsigned long)ui_bench.cpu_loop_us,
             (unsigned long)ui_bench.blank_frame_ms,(unsigned long)ui_bench.page_frame_ms[0],
             (unsigned long)ui_bench.page_frame_ms[1],(unsigned long)ui_bench.page_frame_ms[2],
             (unsigned long)ui_bench.mid_frame_ms[0],(unsigned long)ui_bench.mid_frame_ms[1]);
}

static void ui_boot_benchmark_timer(lv_timer_t *timer)
{
    (void)timer;
    if(asleep||!screen)return;
    ui_boot_benchmark();
    if(screen)lv_obj_invalidate(screen);
}
bool lf_ui_start(lf_settings_t *s)
{
    prefs = s;
    activity();
    asleep = false;
    atomic_store(&ui_screen_awake,true);
    atomic_store(&wake_overlay_pending,false);
    if (esp_lv_adapter_lock(-1) != ESP_OK) {
        ESP_LOGE(TAG, "Cannot lock display");
        return false;
    }
    apply_brightness();
    (void)lf_power_background_start();
    /* First root is built during startup, before any touch callback exists. */
    render_home();
    lv_display_t *disp=lv_display_get_default();
    if(disp) {
        lv_display_add_event_cb(disp,display_render_event,LV_EVENT_RENDER_START,NULL);
        lv_display_add_event_cb(disp,display_render_ready_event,LV_EVENT_RENDER_READY,NULL);
        lv_display_add_event_cb(disp,display_flush_wait_start_event,LV_EVENT_FLUSH_WAIT_START,NULL);
        lv_display_add_event_cb(disp,display_flush_wait_finish_event,LV_EVENT_FLUSH_WAIT_FINISH,NULL);
        lv_display_add_event_cb(disp,display_flush_start_event,LV_EVENT_FLUSH_START,NULL);
        lv_display_add_event_cb(disp,display_flush_finish_event,LV_EVENT_FLUSH_FINISH,NULL);
        lv_display_add_event_cb(disp,display_refresh_ready_event,LV_EVENT_REFR_READY,NULL);
    }
    invalidate_full_screen();
    /* Short scroll throw: LVGL decelerates and performs one ease-out snap,
       rather than competing with the clock's own 16ms animation timer. */
    lv_indev_t *touch=lv_indev_get_next(NULL);
    while(touch) {
        if(lv_indev_get_type(touch)==LV_INDEV_TYPE_POINTER){
            lv_indev_set_scroll_throw(touch,90);
            break;
        }
        touch=lv_indev_get_next(touch);
    }
    /* 0.8.112: run the benchmark once on the LVGL task (large stack), never
       from app_main: a full lv_refr_now() overflowed app_main's stack in
       0.8.112, crashed at boot and the OTA rollback restored 0.8.103. */
    lv_timer_t *bench_timer=lv_timer_create(ui_boot_benchmark_timer,1500,NULL);
    if(bench_timer)lv_timer_set_repeat_count(bench_timer,1);
    lv_timer_create(on_tick, 50, NULL);
    /* 30 Hz sweep: visually smooth, half the redraw load of 60 Hz. */
    clock_sweep_timer=lv_timer_create(on_clock_sweep, 33, NULL);
    esp_lv_adapter_unlock();
    return true;
}

esp_err_t lf_ui_apply_basic_settings(const lf_settings_t *basic)
{
    if (!prefs || !basic) return ESP_ERR_INVALID_ARG;
    esp_err_t err = esp_lv_adapter_lock(2000);
    if (err != ESP_OK) return err;
    lf_settings_t previous = *prefs;
    prefs->clock_face = 5;
    prefs->basic_font_style = basic->basic_font_style;
    prefs->basic_font_size = basic->basic_font_size;
    prefs->basic_time_x=basic->basic_time_x;
    prefs->basic_time_y=basic->basic_time_y;
    prefs->basic_battery_x=basic->basic_battery_x;
    prefs->basic_battery_y=basic->basic_battery_y;
    prefs->basic_date_x=basic->basic_date_x;
    prefs->basic_date_y=basic->basic_date_y;
    prefs->basic_weather_x=basic->basic_weather_x;
    prefs->basic_weather_y=basic->basic_weather_y;
    prefs->basic_battery_size=basic->basic_battery_size;
    prefs->basic_date_size=basic->basic_date_size;
    prefs->basic_weather_size=basic->basic_weather_size;
    prefs->basic_ring_mode = basic->basic_ring_mode;
    prefs->basic_show_date = basic->basic_show_date;
    prefs->basic_weather = basic->basic_weather;
    prefs->basic_background = basic->basic_background;
    prefs->basic_foreground = basic->basic_foreground;
    prefs->basic_ring_a = basic->basic_ring_a;
    prefs->basic_ring_b = basic->basic_ring_b;
    prefs->basic_ring_c = basic->basic_ring_c;
    memcpy(prefs->face_background,basic->face_background,sizeof(prefs->face_background));
    memcpy(prefs->face_foreground,basic->face_foreground,sizeof(prefs->face_foreground));
    memcpy(prefs->face_accent,basic->face_accent,sizeof(prefs->face_accent));
    memcpy(prefs->face_show_date,basic->face_show_date,sizeof(prefs->face_show_date));
    memcpy(prefs->face_show_seconds,basic->face_show_seconds,sizeof(prefs->face_show_seconds));
    memcpy(prefs->face_show_battery,basic->face_show_battery,sizeof(prefs->face_show_battery));
    memcpy(prefs->face_battery_x,basic->face_battery_x,sizeof(prefs->face_battery_x));
    memcpy(prefs->face_battery_y,basic->face_battery_y,sizeof(prefs->face_battery_y));
    memcpy(prefs->face_24h,basic->face_24h,sizeof(prefs->face_24h));
    err = lf_settings_save(prefs);
    if (err == ESP_OK) {
        clock_scene_stale=true;
        /* A Web UI clock save is a request to SEE that face on the device,
           not merely mark a hidden cache stale. Refresh the existing
           pager's clock scene and navigate to it without rebooting. */
        if(primary_pager){
            primary_pager_build_clock();
            primary_pager_enter(0);
        }else if(!timed_lyrics_mode && !lf_recog_busy()){
            build_clock();
        }
        /* Never interrupt manual microphone capture or active synced
           lyrics: the stale clock rebuilds on the next clock visit. */
    } else {
        *prefs = previous;
        lf_diag_error("settings", "Basic settings NVS save failed");
    }
    esp_lv_adapter_unlock();
    return err;
}

uint32_t lf_ui_max_tick_gap_ms(void)
{
    return max_ui_tick_gap_ms;
}

/* The ESP32 Web UI must apply preferences on the LVGL task lock, not on the
   HTTP server thread. Preserve every setting not explicitly editable here. */
esp_err_t lf_ui_apply_device_settings(const lf_settings_t *requested)
{
    if(!prefs||!requested)return ESP_ERR_INVALID_ARG;
    esp_err_t err=esp_lv_adapter_lock(2000);
    if(err!=ESP_OK)return err;
    lf_settings_t old=*prefs;
    prefs->brightness=requested->brightness;
    prefs->scroll_speed=requested->scroll_speed;
    prefs->font_size=requested->font_size;
    prefs->system_font_size=requested->system_font_size;
    prefs->sleep_seconds=requested->sleep_seconds;
    prefs->deep_sleep_minutes=requested->deep_sleep_minutes;
    prefs->charging_color=requested->charging_color;
    err=lf_settings_save(prefs);
    if(err==ESP_OK){
        apply_brightness();
        if(page==0)build_home();
        else if(page==1)build_preview();
        else if(page==2)build_settings();
        else if(page==6)build_clock();
    }else *prefs=old;
    esp_lv_adapter_unlock();
    return err;
}
