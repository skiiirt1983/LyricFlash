#include "ui.h"
#include "bsp/display.h"
#include "esp_lv_adapter.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "power.h"
#include "network.h"
#include "diagnostics.h"
#include "version.h"
#include "clock_face.h"
#include "weather.h"
#include "weather_visual.h"
#include "char_wheel.h"
#include "lvgl.h"
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

static const char *TAG = "LyricFlashUI";
static lf_settings_t *prefs;
static uint64_t last_activity_ms;
static uint64_t last_scroll_ms;
static float scroll_y;
static bool paused;
static bool asleep;
static lv_obj_t *scroll_panel;
static lv_obj_t *lyrics_label;
static lv_obj_t *speed_text;
static lv_obj_t *brightness_text;
static lv_obj_t *timeout_text;
static lv_obj_t *screen;
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
static void apply_brightness(void) { (void)bsp_display_brightness_set(asleep ? 0 : prefs->brightness); }

static void save_settings(void)
{
    /* On-device Basic changes must not leave Web UI face 6 out of sync. */
    prefs->face_background[5]=prefs->basic_background;
    prefs->face_foreground[5]=prefs->basic_foreground;
    prefs->face_show_date[5]=prefs->basic_show_date;
    esp_err_t r = lf_settings_save(prefs);
    if (r != ESP_OK) ESP_LOGE(TAG, "Could not save settings: %s", esp_err_to_name(r));
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
    lv_obj_align(o, LV_ALIGN_TOP_MID, 0, y);
    return o;
}

/* Shared touchscreen button treatment, including a 110 ms tactile press
   animation. Big hit areas and labels clamped to usable circle geometry. */
static void style_button(lv_obj_t *b, uint32_t color)
{
    /* Avoid costly per-widget gradients, shadows and pressed transforms. */
    lv_obj_set_style_radius(b, 19, 0);
    lv_obj_set_style_bg_color(b, lv_color_hex(color), 0);
    lv_obj_set_style_bg_grad_dir(b, LV_GRAD_DIR_NONE, 0);
    lv_obj_set_style_border_width(b, 1, 0);
    lv_obj_set_style_border_color(b, lv_color_hex(0xAACBE6), 0);
    lv_obj_set_style_shadow_width(b, 0, 0);
    lv_obj_set_style_bg_color(b, lv_color_hex(0x9BCFFF), LV_STATE_PRESSED);
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
    lv_obj_set_style_text_color(l, lv_color_hex(0x0B1C2A), 0);
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
#define LF_CLOCK_SETTINGS_HOLD_MS 2000ULL
#define LF_HOLD_MOVE_CANCEL_PX 20
static lv_point_t gesture_origin;
static lv_indev_t *gesture_indev;
static bool gesture_origin_valid;
static bool touch_held;
static uint64_t touch_pressed_at_ms;
static int pending_swipe_page = -1;
/* No dual-screen animation: 0.8.3 left old/new UI trees overlapping
   during navigation and made the touch target inconsistent. */
static uint64_t screen_animation_until_ms;
static uint64_t next_swipe_allowed_ms;
static lv_obj_t *clock_settings_popup;

/* One candidate per physical gesture; no screen swap until the finger lifts. */
static int swipe_candidate_page = -1;
static lv_screen_load_anim_t swipe_candidate_animation = LV_SCR_LOAD_ANIM_NONE;

static void build_home(void);
static void build_settings(void);
static void build_clock(void);
static void build_weather_details(void);
static void build_debug(void);
static void build_clock_settings(void);

static bool is_swipe_page(void)
{
    return page == 0 || page == 1 || page == 2 || page == 4 ||
           page == 5 || page == 6 || page == 7 || page == 8 ||
           page == 9 || page == 10 || page == 11 || page == 12 ||
           page == 13 || page == 14 || page == 15;
}

static void commit_swipe(void *arg)
{
    (void)arg;
    const int target = pending_swipe_page;
    pending_swipe_page = -1;
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
    (void)animation; /* Keep direction metadata without animating screens. */
    next_swipe_allowed_ms = current + 250;
    lv_async_call(commit_swipe, NULL);
}

static void swipe_intent(lv_dir_t direction)
{
    if (clock_settings_popup || swipe_candidate_page >= 0) return;
    if (page == 0) {
        if (direction == LV_DIR_LEFT) {
            swipe_candidate_page = 2;
            swipe_candidate_animation = LV_SCR_LOAD_ANIM_OVER_LEFT;
        } else if (direction == LV_DIR_RIGHT) {
            swipe_candidate_page = 6;
            swipe_candidate_animation = LV_SCR_LOAD_ANIM_OVER_RIGHT;
        } else if (direction == LV_DIR_BOTTOM) {
            swipe_candidate_page = 14;
            swipe_candidate_animation = LV_SCR_LOAD_ANIM_OVER_BOTTOM;
        }
    } else if (page == 6) {
        if (direction == LV_DIR_LEFT) {
            swipe_candidate_page = 0;
            swipe_candidate_animation = LV_SCR_LOAD_ANIM_OVER_LEFT;
        } else if (direction == LV_DIR_TOP) {
            swipe_candidate_page = 11;
            swipe_candidate_animation = LV_SCR_LOAD_ANIM_OVER_TOP;
        }
    } else if (page == 14 && (direction == LV_DIR_TOP || direction == LV_DIR_RIGHT)) {
        swipe_candidate_page = 0;
        swipe_candidate_animation = LV_SCR_LOAD_ANIM_OVER_RIGHT;
    } else if (page == 11 && (direction == LV_DIR_BOTTOM || direction == LV_DIR_RIGHT)) {
        swipe_candidate_page = 6;
        swipe_candidate_animation = LV_SCR_LOAD_ANIM_OVER_RIGHT;
    } else if (direction == LV_DIR_RIGHT) {
        /* Consistent Back from every settings submenu. */
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
    if (!touch_held || !gesture_origin_valid || !gesture_indev) return;
    lv_point_t current={0};
    lv_indev_get_point(gesture_indev, &current);
    int dx=current.x-gesture_origin.x;
    int dy=current.y-gesture_origin.y;
    int distance_sq=dx*dx+dy*dy;
    if (page == 6 && distance_sq > LF_HOLD_MOVE_CANCEL_PX*LF_HOLD_MOVE_CANCEL_PX)
        touch_pressed_at_ms=0;
    if (distance_sq < LF_SWIPE_MIN_PX*LF_SWIPE_MIN_PX) return;
    if (abs(dx) > abs(dy))
        swipe_intent(dx < 0 ? LV_DIR_LEFT : LV_DIR_RIGHT);
    else
        swipe_intent(dy < 0 ? LV_DIR_TOP : LV_DIR_BOTTOM);
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
    if (target >= 0) queue_swipe(target, animation);
    activity();
}

static void on_gesture_press(lv_event_t *e)
{
    (void)e;
    if (!is_swipe_page() || clock_settings_popup || touch_held) return;
    lv_indev_t *indev=lv_indev_active();
    if (!indev || lv_indev_get_type(indev)!=LV_INDEV_TYPE_POINTER) return;
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
    if (!touch_held || !gesture_origin_valid || clock_settings_popup) return;
    lv_indev_t *indev=lv_indev_active();
    if (!indev || indev!=gesture_indev) return;
    /* LVGL recognizes quick swipes even if the release arrives at a new
       widget. We also independently recognize slow drags by distance. */
    swipe_intent(lv_indev_get_gesture_dir(indev));
    observe_touch_position();
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
/* Heavy full-screen slide animations caused jitter with complex faces.
   Swap atomically to minimize extra redraw and memory pressure. */
static void make_base(void)
{
    lv_obj_t *old_screen = lv_screen_active();
    lf_weather_visual_clear();
    lf_clock_clear();
    screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(screen, lv_color_hex(prefs->background_color), 0);
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, 0);
    lv_obj_remove_flag(screen, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(screen, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(screen, on_gesture_press, LV_EVENT_PRESSED, NULL);
    lv_obj_add_event_cb(screen, on_touch_pressing, LV_EVENT_PRESSING, NULL);
    lv_obj_add_event_cb(screen, on_touch_release, LV_EVENT_RELEASED, NULL);
    lv_obj_add_event_cb(screen, on_touch_cancel, LV_EVENT_PRESS_LOST, NULL);
    lv_obj_add_event_cb(screen, on_lvgl_gesture, LV_EVENT_GESTURE, NULL);
    /* Replace active screen atomically. Defer old-screen destruction until
       LVGL exits its event/timer dispatch. Never display two LVGL screens. */
    lv_screen_load(screen);
    if (old_screen != screen) lv_obj_delete_async(old_screen);
    screen_animation_until_ms = 0;
    touch_held=false;
    gesture_origin_valid=false;
    gesture_indev=NULL;
    swipe_candidate_page=-1;
    swipe_candidate_animation=LV_SCR_LOAD_ANIM_NONE;
    touch_pressed_at_ms=0;
    clock_settings_popup=NULL;
    debug_text = NULL;
    scroll_panel = NULL;
    lyrics_label = NULL;
    battery_status = NULL;
    network_status = NULL;
    network_password = NULL;
    network_url = NULL;
    ota_status = NULL;
    wifi_list = NULL;
    wifi_status = NULL;
    wifi_password_status = NULL;
    pause_button = NULL;
}

static void on_wifi(lv_event_t *e)
{
    (void)e; activity(); build_wifi();
}
static void on_connection_help(lv_event_t *e)
{
    (void)e; activity(); build_connection_help();
}
static void build_connection_help(void)
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

static void build_home(void)
{
    page = 0;
    make_base();
    lf_clock_clear();
    label(screen, "LYRICFLASH", 56, &lv_font_montserrat_24, prefs->text_color);
    label(screen, LF_VERSION_LABEL, 103, &lv_font_montserrat_20, 0xB7C7D7);
    battery_status = label(screen, "BATTERY", 151, &lv_font_montserrat_20, 0xDCE8F0);
    button(screen, "LYRICS", 242, 82, 0, 0, 0xA8D6FF, on_preview, NULL);
    /* Connection and browser settings are always reachable without a PIN. */
    button(screen, "CONNECT / OTA", 206, 49, 0, 83, 0xA8D6FF, on_connection_help, NULL);
    network_status = label(screen, "Wi-Fi starting...", 354, &lv_font_montserrat_16, 0xB9CADB);
    label(screen, "SWIPE LEFT / RIGHT", 394, &lv_font_montserrat_16, 0x6E8CAC);
    last_power_refresh_ms = 0;
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

static void build_preview(void)
{
    page = 1;
    make_base();
    scroll_y = 0;
    paused = false;
    user_dragging = false;
    finger_was_scrolling = false;
    scroll_at_end_ms = 0;
    touch_resume_at_ms = 0;
    last_scroll_ms = now_ms();
    label(screen, "LYRIC PREVIEW", 54, &lv_font_montserrat_20, prefs->text_color);
    label(screen, "SAMPLE LYRICS - MIC OFF", 85, &lv_font_montserrat_16, 0xB7C7D7);
    scroll_panel = lv_obj_create(screen);
    lv_obj_set_size(scroll_panel, 338, 215);
    lv_obj_align(scroll_panel, LV_ALIGN_CENTER, 0, 10);
    lv_obj_set_style_bg_color(scroll_panel, lv_color_hex(0x090E15), 0);
    lv_obj_set_style_border_width(scroll_panel, 0, 0);
    lv_obj_set_style_radius(scroll_panel, 22, 0);
    lv_obj_set_style_pad_all(scroll_panel, 14, 0);
    lv_obj_set_scroll_dir(scroll_panel, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(scroll_panel, LV_SCROLLBAR_MODE_OFF);
    lv_obj_add_flag(scroll_panel, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(scroll_panel, LV_OBJ_FLAG_SCROLL_MOMENTUM);
    lv_obj_add_flag(scroll_panel, LV_OBJ_FLAG_SCROLL_ELASTIC);
    lv_obj_remove_flag(scroll_panel, LV_OBJ_FLAG_GESTURE_BUBBLE);
    lv_obj_remove_flag(scroll_panel, LV_OBJ_FLAG_SCROLL_CHAIN);
    lv_obj_add_event_cb(scroll_panel, on_touch_scroll, LV_EVENT_PRESSED, NULL);
    lv_obj_add_event_cb(scroll_panel, on_touch_scroll, LV_EVENT_SCROLL_BEGIN, NULL);
    lv_obj_add_event_cb(scroll_panel, on_touch_scroll, LV_EVENT_SCROLL_END, NULL);
    lv_obj_add_event_cb(scroll_panel, on_touch_scroll, LV_EVENT_RELEASED, NULL);


    lyrics_label = lv_label_create(scroll_panel);
    lv_obj_set_width(lyrics_label, 292);
    lv_obj_set_height(lyrics_label, LV_SIZE_CONTENT);
    lv_obj_remove_flag(lyrics_label, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_remove_flag(lyrics_label, LV_OBJ_FLAG_SCROLLABLE);
    lv_label_set_long_mode(lyrics_label, LV_LABEL_LONG_WRAP);
    lv_label_set_text(lyrics_label, DEMO_TEXT);
    lv_obj_set_style_text_align(lyrics_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_color(lyrics_label, lv_color_hex(prefs->text_color), 0);
    lv_obj_set_style_text_font(lyrics_label, font_for_size(prefs->font_size), 0);
    lv_obj_set_style_text_line_space(lyrics_label, prefs->line_spacing_px, 0);
    lv_obj_align(lyrics_label, LV_ALIGN_TOP_MID, 0, 0);
    /* Expand the computed content height and leave room to read the last
       verse. A 1px invisible child guarantees an actual LVGL scroll range. */
    lv_obj_update_layout(lyrics_label);
    lv_obj_t *tail = lv_obj_create(scroll_panel);
    lv_obj_set_size(tail, 1, 1);
    lv_obj_set_style_bg_opa(tail, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(tail, 0, 0);
    lv_obj_set_style_pad_all(tail, 0, 0);
    lv_obj_remove_flag(tail, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_align(tail, LV_ALIGN_TOP_MID, 0,
                 lv_obj_get_height(lyrics_label) + 80);
    lv_obj_update_layout(scroll_panel);
    ESP_LOGI(TAG, "Lyrics scroll: label=%ld px, viewport=%ld px, bottom=%ld px",
             (long)lv_obj_get_height(lyrics_label), (long)lv_obj_get_height(scroll_panel),
             (long)lv_obj_get_scroll_bottom(scroll_panel));
    lv_obj_remove_flag(lyrics_label, LV_OBJ_FLAG_GESTURE_BUBBLE);

    button(screen, "BACK", 130, 47, -76, 172, 0x9CB7CB, on_home, NULL);
    pause_button = button(screen, "PAUSE", 130, 47, 76, 172, prefs->text_color, on_toggle_pause, NULL);
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
static void build_wifi(void)
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
static void build_wifi_password(void)
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
    lv_label_set_text(timeout_text, s);
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
static void build_settings(void)
{
    page = 2;
    make_base();
    label(screen, "SETTINGS", 41, &lv_font_montserrat_24, prefs->text_color);
    button(screen, "LYRICS", 250, 54, 0, -99, 0xB2C6D3, on_lyrics_settings, NULL);
    button(screen, "CLOCK & WEATHER", 250, 54, 0, -33, 0xB2C6D3, on_clock_options, NULL);
    button(screen, "DEVICE", 250, 54, 0, 33, 0xB2C6D3, on_device_settings, NULL);
    button(screen, "WI-FI", 250, 54, 0, 99, 0xB2C6D3, on_wifi, NULL);
    button(screen, "HOME", 153, 44, 0, 175, prefs->text_color, on_home, NULL);
}
static void on_lyric_font(lv_event_t *e)
{
    (void)e;
    prefs->font_size = (prefs->font_size + 1) % 3;
    save_settings();
    build_lyrics_settings();
}
static void build_lyrics_settings(void)
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
    add_slider(screen, 205, prefs->scroll_speed, 10, 100, on_speed);
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
    (void)e;
    prefs->system_font_size = (prefs->system_font_size + 1) % 3;
    save_settings();
    build_device_settings();
}
static void build_device_settings(void)
{
    page = 10;
    make_base();
    label(screen, "DEVICE SETTINGS", 39, &lv_font_montserrat_24, prefs->text_color);
    char text[64];
    snprintf(text, sizeof text, "System font: %s",
             prefs->system_font_size == 0 ? "18" : prefs->system_font_size == 1 ? "20" : "24");
    button(screen, text, 260, 54, 0, -111, 0xB2C6D3, on_system_font, NULL);
    snprintf(text, sizeof text, "Brightness: %u%%", prefs->brightness);
    brightness_text = label(screen, text, 156, system_font(), 0xE4EDF3);
    add_slider(screen, 195, prefs->brightness, 5, 100, on_brightness);
    snprintf(text, sizeof text, "Screen timeout: %us", prefs->sleep_seconds);
    timeout_text = label(screen, text, 259, system_font(), 0xE4EDF3);
    lv_obj_add_flag(timeout_text, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(timeout_text, on_timeout, LV_EVENT_CLICKED, NULL);
    ota_status = label(screen, "Web settings and updates: no PIN required", 325,
                       &lv_font_montserrat_16, 0x8BCCFF);
    button(screen, "BACK", 152, 45, 0, 178, prefs->text_color, on_settings, NULL);
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
static void build_clock(void)
{
    page = 6;
    make_base();
    lf_clock_build(screen, prefs->clock_face, prefs);
    clock_children_input_transparent(screen);
    /* A deliberate 2-second hold produces a confirmation popup.
       The global LVGL short-long-press event no longer opens settings. */
    lv_obj_add_flag(screen, LV_OBJ_FLAG_CLICKABLE);
    clock_last_refresh = 0;
    activity();
}
static void on_clock_back(lv_event_t *e)
{
    (void)e; activity(); build_clock();
}
static void on_clock_options(lv_event_t *e)
{
    (void)e; activity(); build_clock_settings();
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
static void build_clock_settings(void)
{
    page = 7;
    make_base();
    label(screen, "CLOCK SETTINGS", 38, &lv_font_montserrat_24, prefs->text_color);
    char value[65];
    snprintf(value, sizeof value, "Face: %s", lf_clock_style_name(prefs->clock_face));
    button(screen, value, 270, 55, 0, -99,
           0xB2C6D3, on_cycle_face, NULL);
    button(screen, "CUSTOMIZE BASIC", 270, 52, 0, -29,
           0xB2C6D3, on_basic_settings, NULL);
    lf_weather_config_t cfg;
    lf_weather_get_config(&cfg);
    button(screen, "WEATHER LOCATION", 254, 51, 0, 40,
           0xB2C6D3, on_weather_location, NULL);
    snprintf(value, sizeof value, "Update: %u min", cfg.interval_minutes);
    button(screen, value, 239, 50, 0, 106,
           0xB2C6D3, on_weather_interval, NULL);
    button(screen, "BACK", 144, 50, 0, 176,
           prefs->text_color, on_settings, NULL);
}
static void on_basic_font(lv_event_t *e)
{
    (void)e;
    prefs->basic_font_style = (prefs->basic_font_style + 1) % 5;
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
static void build_basic_colour(void)
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
static void build_basic_settings(void)
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
    snprintf(text, sizeof text, "Font preset %u / 5", prefs->basic_font_style + 1);
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
    snprintf(text, sizeof text, "Ring: %s", prefs->basic_ring_mode==0?"Solid":
             prefs->basic_ring_mode==1?"2-color gradient":"3-color gradient");
    button(list, text, 256, 50, 0, 0, 0xB2C6D3, on_basic_ring_mode, NULL);
    const char *names[]={"Background color","Time text color","Ring color 1",
                         "Ring color 2","Ring color 3"};
    for(uintptr_t i=0;i<5;i++)
        button(list,names[i],256,50,0,0,0xB2C6D3,on_basic_colour,(void*)i);
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
    char output[1100], battery[64], signal[24];
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
             d.errors[0][0] ? d.errors[0] : "None recorded", d.errors[1], d.errors[2]);
    lv_label_set_text(debug_text, output);
}

static void build_debug(void)
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

static void build_weather_details(void)
{
    page = 11;
    make_base();
    lf_clock_clear();
    label(screen, "WEATHER", 25, &lv_font_montserrat_24, 0xE8F5FF);
    lf_weather_visual_build(screen);
    lf_weather_view_t weather;
    lf_weather_get_view(&weather);
    lf_weather_visual_update(&weather);
    weather_detail_refresh = now_ms();
    button(screen, "BACK TO CLOCK", 188, 50, 0, 173,
           0x9ACBF1, on_clock_back, NULL);
}
static void build_weather_location(void)
{
    page = 8;
    make_base();
    lf_weather_config_t cfg;
    lf_weather_get_config(&cfg);
    lf_charwheel_show(screen, "Weather city", cfg.city, false,
                      LF_WEATHER_CITY_MAX,
                      wheel_weather_save, wheel_weather_cancel);
}

static void on_wake(lv_event_t *e)
{
    (void)e;
    asleep = false;
    activity();
    apply_brightness();
    build_home();
}
static void sleep_display(void)
{
    asleep = true;
    page = 3;
    make_base();
    // CPU and touch controller remain running in alpha 0.1.
    // We avoid promising unverified deep-sleep wake behavior.
    lv_obj_add_flag(screen, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(screen, on_wake, LV_EVENT_PRESSED, NULL);
    apply_brightness();
}
static void on_tick(lv_timer_t *timer)
{
    (void)timer;
    uint64_t current = now_ms();
    if (last_ui_tick_ms && current > last_ui_tick_ms) {
        uint64_t gap = current - last_ui_tick_ms;
        if (gap > max_ui_tick_gap_ms) max_ui_tick_gap_ms = (uint32_t)gap;
    }
    last_ui_tick_ms = current;
    /* A drag can leave the original LVGL widget and never send RELEASED to
       it. The pointer driver's state is authoritative, not that callback. */
    if (touch_held && gesture_indev) {
        if (lv_indev_get_state(gesture_indev) == LV_INDEV_STATE_RELEASED)
            finish_touch();
        else
            observe_touch_position();
    }
    if (page == 6 && touch_held && touch_pressed_at_ms &&
        current - touch_pressed_at_ms >= LF_CLOCK_SETTINGS_HOLD_MS)
        show_clock_settings_popup();
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
                lv_label_set_text(battery_status,value);
                lv_obj_set_style_text_color(battery_status,lv_color_hex(lf_power_color(&st)),0);
            }
        }
        if (network_status) {
            char text[128];
            char ssid[33] = {0};
            if (lf_network_connected_ssid(ssid, sizeof ssid))
                snprintf(text, sizeof text, "Wi-Fi: %.32s", ssid);
            else if (*lf_network_ap_ssid())
                snprintf(text, sizeof text, "Setup: %.32s", lf_network_ap_ssid());
            else snprintf(text, sizeof text, "Wi-Fi connecting...");
            lv_label_set_text(network_status, text);
        }
        if (network_password) {
            char text[72];
            if (*lf_network_ap_ssid())
                snprintf(text, sizeof text, "Password: %s", lf_network_ap_password());
            else snprintf(text, sizeof text, "Connected to home Wi-Fi");
            lv_label_set_text(network_password, text);
        }
        if (network_url) {
            char url_text[85];
            snprintf(url_text, sizeof url_text, "Open browser to http://%s/", lf_network_address());
            lv_label_set_text(network_url, url_text);
        }
    }
    if (asleep) return;
    if (page == 11 && !touch_held && current >= screen_animation_until_ms && current - weather_detail_refresh >= 4000) {
        weather_detail_refresh = current;
        lf_weather_view_t weather;
        lf_weather_get_view(&weather);
        lf_weather_visual_update(&weather);
    }
    if (page == 6 && !touch_held && !clock_settings_popup && current >= screen_animation_until_ms && current - clock_last_refresh >= 950) {
        clock_last_refresh = current;
        lf_clock_update();
    }
    if (prefs->sleep_seconds && !(page == 1 && !paused) && page != 14 && page != 15 && current - last_activity_ms > (uint64_t)prefs->sleep_seconds * 1000ULL) {
        sleep_display();
        return;
    }
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
    scroll_y += (float)dt * (0.003f + prefs->scroll_speed * 0.000605f);
    lv_obj_scroll_to_y(scroll_panel, (int32_t)scroll_y, LV_ANIM_OFF);
}

/* Only the active Rolex-style clock needs a high-rate redraw.
   The rest of the UI retains its lower-cost 50 ms timer. */
static void on_clock_sweep(lv_timer_t *timer)
{
    (void)timer;
    if (page == 6 && !asleep && !touch_held && !clock_settings_popup && now_ms() >= screen_animation_until_ms) lf_clock_animate();
}

bool lf_ui_start(lf_settings_t *s)
{
    prefs = s;
    activity();
    asleep = false;
    if (esp_lv_adapter_lock(-1) != ESP_OK) {
        ESP_LOGE(TAG, "Cannot lock display");
        return false;
    }
    apply_brightness();
    (void)lf_power_background_start();
    build_home();
    lv_timer_create(on_tick, 50, NULL);
    lv_timer_create(on_clock_sweep, 16, NULL);
    esp_lv_adapter_unlock();
    return true;
}

esp_err_t lf_ui_apply_basic_settings(const lf_settings_t *basic)
{
    if (!prefs || !basic) return ESP_ERR_INVALID_ARG;
    esp_err_t err = esp_lv_adapter_lock(2000);
    if (err != ESP_OK) return err;
    lf_settings_t previous = *prefs;
    prefs->clock_face = basic->clock_face;
    prefs->basic_font_style = basic->basic_font_style;
    prefs->basic_font_size = basic->basic_font_size;
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
    memcpy(prefs->face_24h,basic->face_24h,sizeof(prefs->face_24h));
    err = lf_settings_save(prefs);
    if (err == ESP_OK) {
        if (page == 6) build_clock();
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
