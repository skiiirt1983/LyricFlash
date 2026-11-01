#include "ui.h"
#include "bsp/display.h"
#include "esp_lv_adapter.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "power.h"
#include "network.h"
#include "clock_face.h"
#include "weather.h"
#include "lvgl.h"
#include <stdbool.h>
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
static lv_obj_t *font_text;
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
static lv_obj_t *pause_button;
static lv_obj_t *wifi_list;
static lv_obj_t *wifi_status;
static lv_obj_t *wifi_password_status;
static lv_obj_t *wifi_password_input;
static lf_wifi_ap_t wifi_aps[LF_WIFI_MAX_RESULTS];
static size_t wifi_ap_count;
static char selected_wifi[33];
static bool selected_wifi_secured;
static int wifi_state_previous;
static lv_obj_t *clockface_text;
static lv_obj_t *weather_status_text;
static lv_obj_t *weather_city_text;
static lv_obj_t *weather_location_input;
static uint64_t clock_last_refresh;
static int page; // 0 home, 1 preview, 2 settings, 3 sleep, 4 scan, 5 Wi-Fi pass, 6 clock, 7 clock settings, 8 weather city

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
static const lv_font_t *font_for_size(uint8_t size)
{
    return size == 0 ? &lv_font_montserrat_18 :
           size == 2 ? &lv_font_montserrat_26 : &lv_font_montserrat_24;
}
static void apply_brightness(void) { (void)bsp_display_brightness_set(asleep ? 0 : prefs->brightness); }

static void save_settings(void)
{
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
    lv_obj_set_style_text_font(o, font, 0);
    lv_obj_set_width(o, 360);
    lv_obj_set_style_text_align(o, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(o, LV_ALIGN_TOP_MID, 0, y);
    return o;
}

static lv_obj_t *button(lv_obj_t *parent, const char *text, int width, int height,
                        int x, int y, uint32_t bg, lv_event_cb_t cb, void *arg)
{
    lv_obj_t *b = lv_button_create(parent);
    lv_obj_set_size(b, width, height);
    lv_obj_align(b, LV_ALIGN_CENTER, x, y);
    lv_obj_set_style_bg_color(b, lv_color_hex(bg), 0);
    lv_obj_set_style_radius(b, height / 2, 0);
    lv_obj_set_style_border_width(b, 0, 0);
    lv_obj_add_event_cb(b, cb, LV_EVENT_CLICKED, arg);
    lv_obj_t *l = lv_label_create(b);
    lv_label_set_text(l, text);
    lv_obj_set_style_text_font(l, &lv_font_montserrat_18, 0);
    lv_obj_set_style_text_color(l, lv_color_hex(0x07100B), 0);
    lv_obj_center(l);
    return b;
}

static void on_sleep(lv_event_t *e);
static void build_home(void);
static void build_preview(void);
static void build_settings(void);
static void build_wifi(void);
static void build_wifi_password(void);
static void build_clock(void);
static void build_clock_settings(void);
static void build_weather_location(void);
static void on_clock_options(lv_event_t *e);
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
static void on_swipe(lv_event_t *e)
{
    (void)e;
    lv_indev_t *indev = lv_indev_active();
    if (!indev) return;
    lv_dir_t dir = lv_indev_get_gesture_dir(indev);
    activity();
    if (page == 0) {
        if (dir == LV_DIR_LEFT) build_settings();
        else if (dir == LV_DIR_RIGHT) build_clock();
    } else if (page == 2 && dir == LV_DIR_RIGHT) {
        build_home();
    } else if (page == 6 && dir == LV_DIR_LEFT) {
        build_home();
    }
}

static void make_base(void)
{
    screen = lv_screen_active();
    lv_obj_clean(screen);
    lv_obj_set_style_bg_color(screen, lv_color_hex(prefs->background_color), 0);
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, 0);
    lv_obj_remove_flag(screen, LV_OBJ_FLAG_SCROLLABLE);
    scroll_panel = NULL;
    lyrics_label = NULL;
    battery_status = NULL;
    network_status = NULL;
    network_password = NULL;
    network_url = NULL;
    clockface_text = NULL;
    weather_status_text = NULL;
    weather_city_text = NULL;
    weather_location_input = NULL;
    ota_status = NULL;
    wifi_list = NULL;
    wifi_status = NULL;
    wifi_password_status = NULL;
    wifi_password_input = NULL;
    pause_button = NULL;
}

static void on_arm_ota(lv_event_t *e)
{
    (void)e;
    lf_network_arm_ota();
    activity();
    if(ota_status) {
        char text[60];
        snprintf(text,sizeof text,"OTA PIN: %s  (2 min)",lf_network_ota_pin());
        lv_label_set_text(ota_status,text);
    }
}
static void on_wifi(lv_event_t *e)
{
    (void)e; activity(); build_wifi();
}

static void build_home(void)
{
    page = 0;
    make_base();
    lf_clock_clear();
    label(screen, "LYRICFLASH", 34, &lv_font_montserrat_20, prefs->text_color);
    label(screen, "ALPHA 0.4  |  HARDWARE TEST", 65, &lv_font_montserrat_16, 0xB7C7D7);
    battery_status = label(screen, "BATTERY", 96, &lv_font_montserrat_16, 0xDCE8F0);
    network_status = label(screen, "Wi-Fi", 124, &lv_font_montserrat_16, 0xB9CADB);
    network_password = label(screen, "", 142, &lv_font_montserrat_16, 0xB9CADB);
    network_url = label(screen, "Open browser to http://192.168.4.1/", 165, &lv_font_montserrat_16, 0xB9CADB);
    button(screen, "LYRIC PREVIEW", 224, 91, 0, 5, prefs->text_color, on_preview, NULL);
    label(screen, "MIC OFF", 285, &lv_font_montserrat_16, 0xB9CADB);
    ota_status = label(screen, "", 310, &lv_font_montserrat_16, prefs->accent_color);
    button(screen, "WI-FI", 134, 42, -77, 117, 0xB2C6D3, on_wifi, NULL);
    button(screen, "ARM OTA", 134, 42, 77, 117, 0xB2C6D3, on_arm_ota, NULL);
    button(screen, "SETTINGS", 143, 42, -74, 171, 0xAAC1D2, on_settings, NULL);
    button(screen, "SLEEP", 108, 42, 92, 171, 0xAAC1D2, on_sleep, NULL);
}

static void on_toggle_pause(lv_event_t *e)
{
    (void)e; activity(); paused = !paused;
    lv_obj_t *b = lv_event_get_target(e);
    lv_obj_t *l = lv_obj_get_child(b, 0);
    if (l) lv_label_set_text(l, paused ? "RESUME" : "PAUSE");
    last_scroll_ms = now_ms();
}

static void on_touch_scroll(lv_event_t *e)
{
    /* A finger gesture takes control from the automatic scroll timer. */
    if (lv_event_get_code(e) == LV_EVENT_PRESSED ||
        lv_event_get_code(e) == LV_EVENT_SCROLL_BEGIN) {
        user_dragging = true;
        paused = true;
        activity();
        if (pause_button) {
            lv_obj_t *l = lv_obj_get_child(pause_button, 0);
            if (l) lv_label_set_text(l, "RESUME");
        }
    }
    if (lv_event_get_code(e) == LV_EVENT_SCROLL_END ||
        lv_event_get_code(e) == LV_EVENT_RELEASED) {
        user_dragging = false;
        scroll_y = (float)lv_obj_get_scroll_y(scroll_panel);
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
    lv_obj_add_event_cb(scroll_panel, on_touch_scroll, LV_EVENT_PRESSED, NULL);
    lv_obj_add_event_cb(scroll_panel, on_touch_scroll, LV_EVENT_SCROLL_BEGIN, NULL);
    lv_obj_add_event_cb(scroll_panel, on_touch_scroll, LV_EVENT_SCROLL_END, NULL);
    lv_obj_add_event_cb(scroll_panel, on_touch_scroll, LV_EVENT_RELEASED, NULL);

    lyrics_label = lv_label_create(scroll_panel);
    lv_obj_set_width(lyrics_label, 292);
    lv_label_set_long_mode(lyrics_label, LV_LABEL_LONG_WRAP);
    lv_label_set_text(lyrics_label, DEMO_TEXT);
    lv_obj_set_style_text_align(lyrics_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_color(lyrics_label, lv_color_hex(prefs->text_color), 0);
    lv_obj_set_style_text_font(lyrics_label, font_for_size(prefs->font_size), 0);
    lv_obj_set_style_text_line_space(lyrics_label, prefs->line_spacing_px, 0);
    lv_obj_align(lyrics_label, LV_ALIGN_TOP_MID, 0, 0);

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
static void on_wifi_password_done(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_CANCEL) { activity(); build_wifi(); return; }
    if (code != LV_EVENT_READY || !wifi_password_input) return;
    const char *p = lv_textarea_get_text(wifi_password_input);
    if (lf_network_save_wifi(selected_wifi, p)) {
        if (wifi_password_status) lv_label_set_text(wifi_password_status, "Saved. Restarting...");
    } else if (wifi_password_status) {
        lv_label_set_text(wifi_password_status, "Password must be 8-63 characters.");
    }
    activity();
}
static void build_wifi_password(void)
{
    page = 5;
    make_base();
    label(screen, "WI-FI PASSWORD", 37, &lv_font_montserrat_20, prefs->text_color);
    char display[48];
    snprintf(display, sizeof(display), "%.32s", selected_wifi);
    label(screen, display, 73, &lv_font_montserrat_16, 0xDCE8F0);
    wifi_password_status = label(screen, "Enter password, then tap OK", 104, &lv_font_montserrat_16, 0xB9CADB);
    wifi_password_input = lv_textarea_create(screen);
    lv_obj_set_size(wifi_password_input, 318, 48);
    lv_obj_align(wifi_password_input, LV_ALIGN_TOP_MID, 0, 137);
    lv_textarea_set_one_line(wifi_password_input, true);
    lv_textarea_set_password_mode(wifi_password_input, true);
    lv_textarea_set_max_length(wifi_password_input, 63);
    lv_obj_set_style_text_font(wifi_password_input, &lv_font_montserrat_18, 0);
    lv_obj_t *keyboard = lv_keyboard_create(screen);
    lv_obj_set_size(keyboard, 390, 213);
    lv_obj_align(keyboard, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_keyboard_set_textarea(keyboard, wifi_password_input);
    lv_obj_add_event_cb(keyboard, on_wifi_password_done, LV_EVENT_READY, NULL);
    lv_obj_add_event_cb(keyboard, on_wifi_password_done, LV_EVENT_CANCEL, NULL);
}
static void on_brightness(lv_event_t *e)
{
    prefs->brightness = (uint8_t)lv_slider_get_value(lv_event_get_target(e));
    char s[30]; snprintf(s, sizeof s, "Brightness  %u%%", prefs->brightness);
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
static void on_font(lv_event_t *e)
{
    (void)e; prefs->font_size = (prefs->font_size + 1) % 3;
    lv_label_set_text(font_text, prefs->font_size == 0 ? "Font  Small  >" :
                      prefs->font_size == 2 ? "Font  Large  >" : "Font  Medium  >");
    save_settings();
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
    if (prefs->sleep_seconds) snprintf(s, sizeof s, "Screen timeout  %us  >", prefs->sleep_seconds);
    else snprintf(s, sizeof s, "Screen timeout  Never  >");
    lv_label_set_text(timeout_text, s);
    save_settings();
}
static void on_color(lv_event_t *e)
{
    uint32_t idx = (uint32_t)(uintptr_t)lv_event_get_user_data(e);
    if (idx >= sizeof(PALETTE) / sizeof(PALETTE[0])) return;
    prefs->text_color = PALETTE[idx];
    save_settings();
    build_settings();
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
static void build_settings(void)
{
    page = 2;
    make_base();
    label(screen, "SETTINGS", 46, &lv_font_montserrat_20, prefs->text_color);
    char s[36];
    snprintf(s, sizeof s, "Brightness  %u%%", prefs->brightness);
    brightness_text = label(screen, s, 103, &lv_font_montserrat_16, 0xE4EDF3);
    add_slider(screen, 131, prefs->brightness, 5, 100, on_brightness);
    snprintf(s, sizeof s, "Scroll speed  %u%%", prefs->scroll_speed);
    speed_text = label(screen, s, 170, &lv_font_montserrat_16, 0xE4EDF3);
    add_slider(screen, 198, prefs->scroll_speed, 10, 100, on_speed);
    label(screen, "Lyric colour", 228, &lv_font_montserrat_16, 0xE4EDF3);
    for (unsigned i = 0; i < sizeof(PALETTE) / sizeof(PALETTE[0]); i++) {
        lv_obj_t *dot = lv_button_create(screen);
        lv_obj_set_size(dot, 36, 36);
        lv_obj_set_style_radius(dot, LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_bg_color(dot, lv_color_hex(PALETTE[i]), 0);
        lv_obj_set_style_border_width(dot, prefs->text_color == PALETTE[i] ? 3 : 0, 0);
        lv_obj_set_style_border_color(dot, lv_color_hex(0x7F8B9C), 0);
        lv_obj_align(dot, LV_ALIGN_TOP_MID, (int)i * 50 - 100, 253);
        lv_obj_add_event_cb(dot, on_color, LV_EVENT_CLICKED, (void *)(uintptr_t)i);
    }
    font_text = label(screen, prefs->font_size == 0 ? "Font  Small  >" :
                      prefs->font_size == 2 ? "Font  Large  >" : "Font  Medium  >",
                      306, &lv_font_montserrat_16, 0xD0E4EF);
    lv_obj_add_flag(font_text, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(font_text, on_font, LV_EVENT_CLICKED, NULL);
    if (prefs->sleep_seconds) snprintf(s, sizeof s, "Screen timeout  %us  >", prefs->sleep_seconds);
    else snprintf(s, sizeof s, "Screen timeout  Never  >");
    timeout_text = label(screen, s, 339, &lv_font_montserrat_16, 0xD0E4EF);
    lv_obj_add_flag(timeout_text, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(timeout_text, on_timeout, LV_EVENT_CLICKED, NULL);
    button(screen, "BACK", 122, 44, -86, 171, prefs->text_color, on_home, NULL);
    button(screen, "CLOCK / WEATHER", 174, 44, 77, 171, 0xB2C6D3, on_clock_options, NULL);
}


static void build_clock(void)
{
    page = 6;
    make_base();
    lf_clock_build(screen, prefs->clock_face);
    clock_last_refresh = 0;
    activity();
}
static void on_clock_options(lv_event_t *e)
{
    (void)e; activity(); build_clock_settings();
}
static void on_cycle_face(lv_event_t *e)
{
    (void)e;
    prefs->clock_face = (uint8_t)((prefs->clock_face + 1) % 5);
    save_settings();
    if (clockface_text) {
        char label_text[55];
        snprintf(label_text, sizeof label_text, "Face: %s", lf_clock_style_name(prefs->clock_face));
        lv_label_set_text(clockface_text, label_text);
    }
}
static void on_weather_toggle(lv_event_t *e)
{
    lf_weather_config_t cfg;
    lf_weather_get_config(&cfg);
    cfg.enabled = !cfg.enabled;
    if (lf_weather_save_config(&cfg)) {
        lv_obj_t *name = lv_obj_get_child(lv_event_get_target(e), 0);
        if (name) lv_label_set_text(name, cfg.enabled ? "WEATHER: ON" : "WEATHER: OFF");
        activity();
    }
}
static void on_weather_interval(lv_event_t *e)
{
    (void)e;
    lf_weather_config_t cfg;
    lf_weather_get_config(&cfg);
    cfg.interval_minutes = cfg.interval_minutes == 15 ? 30 :
                           cfg.interval_minutes == 30 ? 60 :
                           cfg.interval_minutes == 60 ? 120 : 15;
    if (lf_weather_save_config(&cfg) && weather_status_text) {
        char value[48];
        snprintf(value, sizeof value, "Refresh: %u min", cfg.interval_minutes);
        lv_label_set_text(weather_status_text, value);
        activity();
    }
}
static void on_weather_location(lv_event_t *e)
{
    (void)e; activity(); build_weather_location();
}
static void build_clock_settings(void)
{
    page = 7;
    make_base();
    label(screen, "CLOCK & WEATHER", 37, &lv_font_montserrat_20, prefs->text_color);
    button(screen, "CHANGE CLOCK FACE", 264, 49, 0, -114,
           prefs->text_color, on_cycle_face, NULL);
    char value[65];
    snprintf(value, sizeof value, "Face: %s", lf_clock_style_name(prefs->clock_face));
    clockface_text = label(screen, value, 151, &lv_font_montserrat_16, 0xDEE9F2);

    lf_weather_config_t cfg;
    lf_weather_get_config(&cfg);
    button(screen, cfg.enabled ? "WEATHER: ON" : "WEATHER: OFF", 224, 45, 0, -30,
           0xB2C6D3, on_weather_toggle, NULL);
    button(screen, "REFRESH INTERVAL", 223, 45, 0, 29,
           0xB2C6D3, on_weather_interval, NULL);
    snprintf(value, sizeof value, "Refresh: %u min", cfg.interval_minutes);
    weather_status_text = label(screen, value, 293, &lv_font_montserrat_16, 0xB9CADB);
    button(screen, "WEATHER LOCATION", 226, 43, 0, 100,
           0xB2C6D3, on_weather_location, NULL);
    snprintf(value, sizeof value, "City: %.40s", cfg.city[0] ? cfg.city : "(not set)");
    weather_city_text = label(screen, value, 361, &lv_font_montserrat_16, 0xB9CADB);
    button(screen, "BACK", 122, 42, 0, 181, prefs->text_color, on_settings, NULL);
}
static void on_weather_city_keyboard(lv_event_t *e)
{
    if (lv_event_get_code(e) == LV_EVENT_CANCEL) {
        activity(); build_clock_settings(); return;
    }
    if (lv_event_get_code(e) != LV_EVENT_READY || !weather_location_input) return;
    lf_weather_config_t cfg;
    lf_weather_get_config(&cfg);
    strlcpy(cfg.city, lv_textarea_get_text(weather_location_input), sizeof cfg.city);
    if (lf_weather_save_config(&cfg)) {
        activity();
        build_clock_settings();
    }
}
static void build_weather_location(void)
{
    page = 8;
    make_base();
    label(screen, "WEATHER LOCATION", 34, &lv_font_montserrat_20, prefs->text_color);
    label(screen, "Enter a town or city, then OK", 74, &lv_font_montserrat_16, 0xB9CADB);
    weather_location_input = lv_textarea_create(screen);
    lv_obj_set_size(weather_location_input, 330, 52);
    lv_obj_align(weather_location_input, LV_ALIGN_TOP_MID, 0, 116);
    lf_weather_config_t cfg;
    lf_weather_get_config(&cfg);
    lv_textarea_set_text(weather_location_input, cfg.city);
    lv_textarea_set_one_line(weather_location_input, true);
    lv_textarea_set_max_length(weather_location_input, LF_WEATHER_CITY_MAX);
    lv_obj_set_style_text_font(weather_location_input, &lv_font_montserrat_18, 0);
    lv_obj_t *keyboard = lv_keyboard_create(screen);
    lv_obj_set_size(keyboard, 390, 218);
    lv_obj_align(keyboard, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_keyboard_set_textarea(keyboard, weather_location_input);
    lv_obj_add_event_cb(keyboard, on_weather_city_keyboard, LV_EVENT_READY, NULL);
    lv_obj_add_event_cb(keyboard, on_weather_city_keyboard, LV_EVENT_CANCEL, NULL);
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
static void on_sleep(lv_event_t *e)
{
    (void)e; activity(); sleep_display();
}

static void on_tick(lv_timer_t *timer)
{
    (void)timer;
    uint64_t current = now_ms();
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
    if (page == 0 && !asleep && current - last_power_refresh_ms >= 4000) {
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
        if(ota_status && !lf_network_ota_armed()) lv_label_set_text(ota_status,"");
    }
    if (asleep) return;
    if (page == 6 && current - clock_last_refresh >= 950) {
        clock_last_refresh = current;
        lf_clock_update();
    }
    if (prefs->sleep_seconds && current - last_activity_ms > (uint64_t)prefs->sleep_seconds * 1000ULL) {
        sleep_display();
        return;
    }
    if (page != 1 || paused || user_dragging || !scroll_panel) {
        last_scroll_ms = current;
        return;
    }
    uint64_t dt = current - last_scroll_ms;
    if (dt > 200) dt = 200;
    last_scroll_ms = current;
    scroll_y += (float)dt * (0.004f + prefs->scroll_speed * 0.0004f);
    lv_obj_scroll_to_y(scroll_panel, (int32_t)scroll_y, LV_ANIM_OFF);
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
    lv_obj_add_event_cb(lv_screen_active(), on_swipe, LV_EVENT_GESTURE, NULL);
    lv_timer_create(on_tick, 50, NULL);
    esp_lv_adapter_unlock();
    return true;
}
