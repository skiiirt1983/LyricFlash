#include "ui.h"
#include "bsp/display.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "power.h"
#include "network.h"
#include "lvgl.h"
#include <stdbool.h>
#include <stdio.h>

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
static lv_obj_t *ota_status;
static uint64_t last_power_refresh_ms;
static int page; // 0=home, 1=preview, 2=settings

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
    ota_status = NULL;
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
static void build_home(void)
{
    page = 0;
    make_base();
    label(screen, "LYRICFLASH", 34, &lv_font_montserrat_20, prefs->text_color);
    label(screen, "ALPHA 0.3  |  HARDWARE TEST", 65, &lv_font_montserrat_12, 0x919CA6);
    battery_status = label(screen, "Battery reading...", 96, &lv_font_montserrat_16, 0xDCE8F0);
    network_status = label(screen, "Wi-Fi starting...", 126, &lv_font_montserrat_12, 0x8F9BA6);
    network_password = label(screen, "", 147, &lv_font_montserrat_12, 0x8F9BA6);
    button(screen, "LYRIC PREVIEW", 224, 91, 0, -5, prefs->text_color, on_preview, NULL);
    label(screen, "Mic OFF  |  No audio recorded", 285, &lv_font_montserrat_12, 0x8F9BA6);
    ota_status = label(screen, "", 310, &lv_font_montserrat_12, prefs->accent_color);
    button(screen, "ARM OTA", 144, 42, 0, 117, 0xB2C6D3, on_arm_ota, NULL);
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

static void build_preview(void)
{
    page = 1;
    make_base();
    scroll_y = 0;
    paused = false;
    last_scroll_ms = now_ms();
    label(screen, "LYRIC PREVIEW", 54, &lv_font_montserrat_20, prefs->text_color);
    label(screen, "Original sample text  |  No recording", 85, &lv_font_montserrat_12, 0x8797A4);
    scroll_panel = lv_obj_create(screen);
    lv_obj_set_size(scroll_panel, 338, 215);
    lv_obj_align(scroll_panel, LV_ALIGN_CENTER, 0, 10);
    lv_obj_set_style_bg_color(scroll_panel, lv_color_hex(0x090E15), 0);
    lv_obj_set_style_border_width(scroll_panel, 0, 0);
    lv_obj_set_style_radius(scroll_panel, 22, 0);
    lv_obj_set_style_pad_all(scroll_panel, 14, 0);
    lv_obj_set_scroll_dir(scroll_panel, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(scroll_panel, LV_SCROLLBAR_MODE_OFF);

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
    button(screen, "PAUSE", 130, 47, 76, 172, prefs->text_color, on_toggle_pause, NULL);
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
    button(screen, "BACK", 155, 44, 0, 171, prefs->text_color, on_home, NULL);
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
    if (page == 0 && !asleep && current - last_power_refresh_ms >= 4000) {
        last_power_refresh_ms = current;
        lf_power_state_t st;
        bool success=lf_power_read(&st);
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
            else snprintf(value,sizeof value,"%s  Battery unavailable",icon);
            lv_label_set_text(battery_status,value);
            lv_obj_set_style_text_color(battery_status,lv_color_hex(lf_power_color(&st)),0);
        }
        if (network_status) {
            char text[128];
            if (*lf_network_ap_ssid()) snprintf(text,sizeof text,"Wi-Fi setup: %s",lf_network_ap_ssid());
            else snprintf(text,sizeof text,"Open http://%s/",lf_network_address());
            lv_label_set_text(network_status,text);
        }
        if (network_password) {
            char text[72];
            if (*lf_network_ap_ssid()) snprintf(text,sizeof text,"Wi-Fi password: %s",lf_network_ap_password());
            else text[0]=0;
            lv_label_set_text(network_password,text);
        }
        if(ota_status && !lf_network_ota_armed()) lv_label_set_text(ota_status,"");
    }
    if (asleep) return;
    if (prefs->sleep_seconds && current - last_activity_ms > (uint64_t)prefs->sleep_seconds * 1000ULL) {
        sleep_display();
        return;
    }
    if (page != 1 || paused || !scroll_panel) {
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
    if (bsp_display_lock((uint32_t)-1) != ESP_OK) {
        ESP_LOGE(TAG, "Cannot lock display");
        return false;
    }
    apply_brightness();
    build_home();
    lv_timer_create(on_tick, 50, NULL);
    bsp_display_unlock();
    return true;
}
