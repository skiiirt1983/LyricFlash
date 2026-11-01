#include "clock_face.h"
#include "power.h"
#include "weather.h"
#include "basic_clock.h"
#include "esp_timer.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>

/* A luxury diver-inspired face for personal use, plus two original alternatives.
   No watchmaker graphics, images, fonts, or proprietary assets are required. */
#define CW 466
#define CC 233
#define PI_F 3.14159265359f

static lv_obj_t *hands[3];
static lv_point_precise_t hand_points[3][2];
static lv_point_precise_t tick_points[60][2];
static lv_obj_t *info_label;
static lv_obj_t *date_label;
static lv_obj_t *time_label;
static lv_obj_t *battery_label;
static lv_obj_t *weather_label;
static lv_obj_t *sundial_time_label;
static lv_obj_t *sundial_shadow;
static lv_point_precise_t sundial_shadow_points[2];
static uint8_t current_style;
static time_t last_second = (time_t)-1;
static bool clock_active;
static const lf_settings_t *clock_settings;
static lv_obj_t *weather_icon;
static int last_weather_icon = -999;
static int64_t anchor_mono_us;
static int64_t anchor_wall_us;

static lv_obj_t *round_panel(lv_obj_t *parent, int diameter, uint32_t fill,
                            uint32_t border, int border_width)
{
    lv_obj_t *o = lv_obj_create(parent);
    lv_obj_remove_flag(o, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_remove_flag(o, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_size(o, diameter, diameter);
    lv_obj_align(o, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_radius(o, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_opa(o, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(o, lv_color_hex(fill), 0);
    lv_obj_set_style_border_color(o, lv_color_hex(border), 0);
    lv_obj_set_style_border_width(o, border_width, 0);
    lv_obj_set_style_pad_all(o, 0, 0);
    return o;
}
static lv_obj_t *face_text(lv_obj_t *parent, const char *text, int x, int y,
                           const lv_font_t *font, uint32_t colour)
{
    lv_obj_t *o = lv_label_create(parent);
    lv_label_set_text(o, text);
    lv_obj_set_style_text_font(o, font, 0);
    lv_obj_set_style_text_color(o, lv_color_hex(colour), 0);
    lv_obj_set_style_text_align(o, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(o, LV_ALIGN_CENTER, x, y);
    return o;
}
static lv_obj_t *dial_line(lv_obj_t *parent, lv_point_precise_t *points, int width,
                           uint32_t colour)
{
    lv_obj_t *o = lv_line_create(parent);
    lv_obj_remove_flag(o, LV_OBJ_FLAG_CLICKABLE);
    lv_line_set_points(o, points, 2);
    lv_obj_set_style_line_width(o, width, 0);
    lv_obj_set_style_line_color(o, lv_color_hex(colour), 0);
    lv_obj_set_style_line_rounded(o, true, 0);
    return o;
}
static void polar(int r, float angle, lv_point_precise_t *point)
{
    point->x = (int)(CC + cosf(angle) * r);
    point->y = (int)(CC + sinf(angle) * r);
}
static void draw_ticks(lv_obj_t *parent, bool diver)
{
    for (int i = 0; i < 60; i++) {
        bool hour_mark = i % 5 == 0;
        float angle = (float)(i * 6 - 90) * (PI_F / 180.f);
        int outer = diver ? 185 : 181;
        int inner = hour_mark ? (diver ? 171 : 160) : (diver ? 181 : 175);
        polar(inner, angle, &tick_points[i][0]);
        polar(outer, angle, &tick_points[i][1]);
        dial_line(parent, tick_points[i], hour_mark ? 5 : 2,
                  diver ? (hour_mark ? 0xEAF3F0 : 0x7299A6) :
                          (hour_mark ? 0xCDE9FF : 0x486C8B));
    }
}
static void add_hour_markers(lv_obj_t *parent)
{
    /* Luminous diving markers: circles, bars, and a twin marker at 12. */
    for (int i = 1; i <= 12; i++) {
        if (i == 3) continue; /* date window */
        float a = (float)(i * 30 - 90) * (PI_F / 180.f);
        int x = (int)(cosf(a) * 158.f);
        int y = (int)(sinf(a) * 158.f);
        if (i == 12) {
            for (int j = -1; j <= 1; j += 2) {
                lv_obj_t *mark = lv_obj_create(parent);
                lv_obj_remove_flag(mark, LV_OBJ_FLAG_SCROLLABLE);
                lv_obj_remove_flag(mark, LV_OBJ_FLAG_CLICKABLE);
                lv_obj_set_size(mark, 10, 19);
                lv_obj_align(mark, LV_ALIGN_CENTER, x + j * 8, y);
                lv_obj_set_style_bg_color(mark, lv_color_hex(0xCDECE5), 0);
                lv_obj_set_style_border_width(mark, 1, 0);
                lv_obj_set_style_border_color(mark, lv_color_hex(0xD9C584), 0);
                lv_obj_set_style_radius(mark, 4, 0);
            }
        } else {
            lv_obj_t *mark = lv_obj_create(parent);
            lv_obj_remove_flag(mark, LV_OBJ_FLAG_SCROLLABLE);
            lv_obj_remove_flag(mark, LV_OBJ_FLAG_CLICKABLE);
            lv_obj_set_size(mark, (i == 6 || i == 9) ? 25 : 18,
                                 (i == 6 || i == 9) ? 17 : 18);
            lv_obj_align(mark, LV_ALIGN_CENTER, x, y);
            lv_obj_set_style_bg_color(mark, lv_color_hex(0xCDECE5), 0);
            lv_obj_set_style_border_color(mark, lv_color_hex(0xD9C584), 0);
            lv_obj_set_style_border_width(mark, 2, 0);
            lv_obj_set_style_radius(mark, (i == 6 || i == 9) ? 4 : LV_RADIUS_CIRCLE, 0);
        }
    }
}
static void create_hands(lv_obj_t *parent, bool diver)
{
    int widths[3] = {diver ? 11 : 9, diver ? 7 : 5, 2};
    uint32_t colours[3] = {diver ? 0xD8ECE9 : 0xE5EDF8,
                           diver ? 0xD8ECE9 : 0xA8D7FF,
                           diver ? 0x40C0E6 : 0x6DBFFF};
    for (int i = 0; i < 3; i++) {
        hand_points[i][0] = (lv_point_precise_t){CC, CC};
        hand_points[i][1] = (lv_point_precise_t){CC, CC};
        hands[i] = dial_line(parent, hand_points[i], widths[i], colours[i]);
    }
    lv_obj_t *pin = round_panel(parent, 13, diver ? 0xD6E9E0 : 0xC7E4FF,
                               0x0E1A25, 1);
    (void)pin;
}
static void build_diver(lv_obj_t *parent)
{
    round_panel(parent, 452, 0x070B0E, 0x416574, 11);
    round_panel(parent, 423, 0x09161B, 0x8797A3, 3);
    round_panel(parent, 389, 0x070F14, 0x24404C, 2);
    draw_ticks(parent, true);
    add_hour_markers(parent);
    lv_obj_t *dw = lv_obj_create(parent);
    lv_obj_set_size(dw, 45, 34);
    lv_obj_align(dw, LV_ALIGN_CENTER, 99, -2);
    lv_obj_set_style_bg_color(dw, lv_color_hex(0xEFF5EC), 0);
    lv_obj_set_style_border_width(dw, 3, 0);
    lv_obj_set_style_border_color(dw, lv_color_hex(0xB5C2D1), 0);
    lv_obj_set_style_radius(dw, 6, 0);
    lv_obj_set_style_pad_all(dw, 0, 0);
    date_label = face_text(dw, "--", 0, 0, &lv_font_montserrat_18, 0x111B1B);
    create_hands(parent, true);
    battery_label = face_text(parent, "BAT --", 0, -137, &lv_font_montserrat_16, 0xA5D6E8);
    info_label = face_text(parent, "", 0, 111, &lv_font_montserrat_12, 0xB5D9E8);
}
static void build_digital(lv_obj_t *parent)
{
    round_panel(parent, 447, 0x030815, 0x245285, 5);
    round_panel(parent, 409, 0x081426, 0x153454, 3);
    time_label = face_text(parent, "--:--", 0, -11, &lv_font_montserrat_48, 0xEDF7FF);
    date_label = face_text(parent, "NO TIME", 0, 72, &lv_font_montserrat_20, 0xA9D5FF);
    battery_label = face_text(parent, "BAT --", 0, -146, &lv_font_montserrat_18, 0x7DCEFF);
    info_label = face_text(parent, "", 0, 110, &lv_font_montserrat_12, 0x7D92AD);
}
static void build_classic(lv_obj_t *parent)
{
    round_panel(parent, 442, 0x05090D, 0x527AA6, 5);
    round_panel(parent, 397, 0x080C13, 0x283E55, 2);
    draw_ticks(parent, false);
    create_hands(parent, false);
    date_label = face_text(parent, "--", 0, 109, &lv_font_montserrat_12, 0x718FA8);
    battery_label = face_text(parent, "BAT --", 0, -143, &lv_font_montserrat_16, 0xA5D6E8);
    info_label = face_text(parent, "", 0, 87, &lv_font_montserrat_12, 0x809DB8);
}

/* Retro green 1980s LCD watch. Intentionally an original face, not a
   reproduction of any company's trademark or printed watch artwork. */
static void build_retro_lcd(lv_obj_t *parent)
{
    round_panel(parent, 450, 0x090B10, 0x6A737C, 9);
    round_panel(parent, 423, 0x15191C, 0x31383C, 3);

    lv_obj_t *lcd = lv_obj_create(parent);
    lv_obj_set_size(lcd, 347, 219);
    lv_obj_align(lcd, LV_ALIGN_CENTER, 0, -1);
    lv_obj_set_style_radius(lcd, 20, 0);
    lv_obj_set_style_pad_all(lcd, 0, 0);
    lv_obj_set_style_border_width(lcd, 8, 0);
    lv_obj_set_style_border_color(lcd, lv_color_hex(0x364641), 0);
    lv_obj_set_style_bg_color(lcd, lv_color_hex(0xA7C7A0), 0);
    lv_obj_set_style_bg_opa(lcd, LV_OPA_COVER, 0);
    lv_obj_remove_flag(lcd, LV_OBJ_FLAG_SCROLLABLE);

    face_text(lcd, "ALARM     CHRONO     LIGHT", 0, -82,
              &lv_font_montserrat_12, 0x384F38);
    date_label = face_text(lcd, "MON 01-01", 0, -53,
                           &lv_font_montserrat_20, 0x253A2C);
    time_label = face_text(lcd, "--:--", 0, 7,
                           &lv_font_montserrat_48, 0x162C1F);
    info_label = face_text(lcd, "NO TIME SYNC", 0, 64,
                           &lv_font_montserrat_12, 0x385341);
    battery_label = face_text(parent, "BAT --", 0, -167,
                              &lv_font_montserrat_16, 0xACDDB0);
}

/* Joke sundial: decorative 'sun' and its animated shadow are driven by
   synced civil time, NOT by a light sensor or physical solar position. */
static void build_sundial(lv_obj_t *parent)
{
    round_panel(parent, 451, 0x241E17, 0xA88E55, 9);
    round_panel(parent, 420, 0xB6A47D, 0x65553B, 4);
    round_panel(parent, 383, 0xC8BB94, 0xE4D0A0, 2);
    /* The ornamental sun isn't an astronomical calculation. */
    lv_obj_t *sun = lv_obj_create(parent);
    lv_obj_set_size(sun, 38, 38);
    lv_obj_align(sun, LV_ALIGN_CENTER, -115, -65);
    lv_obj_set_style_radius(sun, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(sun, lv_color_hex(0xF2CC58), 0);
    lv_obj_set_style_border_color(sun, lv_color_hex(0xA77B28), 0);
    lv_obj_set_style_border_width(sun, 3, 0);
    lv_obj_remove_flag(sun, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_remove_flag(sun, LV_OBJ_FLAG_SCROLLABLE);

    for (int i = 0; i < 12; i++) {
        float angle = (float)(i * 30 - 90) * (PI_F / 180.f);
        polar(111, angle, &tick_points[i][0]);
        polar(128, angle, &tick_points[i][1]);
        dial_line(parent, tick_points[i], 3, 0x786342);
    }
    face_text(parent, "XII", 0, -134,
              &lv_font_montserrat_16, 0x55452C);
    face_text(parent, "VI", 0, 134,
              &lv_font_montserrat_16, 0x55452C);
    /* The stylized gnomon and its hour shadow. */
    sundial_shadow_points[0] = (lv_point_precise_t){CC, CC};
    sundial_shadow_points[1] = (lv_point_precise_t){CC + 30, CC + 30};
    sundial_shadow = dial_line(parent, sundial_shadow_points, 10, 0x504333);
    lv_obj_t *pivot = lv_obj_create(parent);
    lv_obj_set_size(pivot, 24, 24);
    lv_obj_align(pivot, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_radius(pivot, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(pivot, lv_color_hex(0x8B6C40), 0);
    lv_obj_set_style_border_color(pivot, lv_color_hex(0x3F3021), 0);
    lv_obj_set_style_border_width(pivot, 2, 0);
    lv_obj_remove_flag(pivot, LV_OBJ_FLAG_CLICKABLE);

    sundial_time_label = face_text(parent, "--:--", 0, 71,
                                   &lv_font_montserrat_36, 0x332719);
    info_label = face_text(parent, "", 0, 107,
                           &lv_font_montserrat_12, 0x5A4830);
    battery_label = face_text(parent, "BAT --", 0, -172,
                              &lv_font_montserrat_16, 0x443425);
}

const char *lf_clock_style_name(uint8_t style)
{
    switch (style) {
        case 0: return "Rolex Diver";
        case 1: return "Midnight Digital";
        case 2: return "Classic Analog";
        case 3: return "Retro Green LCD";
        case 4: return "Solar-ish Sundial";
        case 5: return "Basic Custom";
        default: return "Rolex Diver";
    }
}
void lf_clock_clear(void)
{
    clock_active = false;
    for (int i = 0; i < 3; i++) hands[i] = NULL;
    info_label = NULL;
    date_label = NULL;
    time_label = NULL;
    battery_label = NULL;
    weather_label = NULL;
    weather_icon = NULL;
    last_weather_icon = -999;
    sundial_time_label = NULL;
    sundial_shadow = NULL;
}
void lf_clock_build(lv_obj_t *parent, uint8_t style, const lf_settings_t *settings)
{
    lf_clock_clear();
    current_style = style > 5 ? 0 : style;
    clock_settings = settings;
    anchor_mono_us = 0;
    last_second = (time_t)-1;
    lv_obj_set_style_bg_color(parent, lv_color_hex(0x03070A), 0);
    lv_obj_set_style_bg_opa(parent, LV_OPA_COVER, 0);
    if (current_style == 0) build_diver(parent);
    else if (current_style == 1) build_digital(parent);
    else if (current_style == 2) build_classic(parent);
    else if (current_style == 3) build_retro_lcd(parent);
    else if (current_style == 4) build_sundial(parent);
    else {
        time_label = lf_basic_clock_render(parent, settings);
        battery_label = face_text(parent, LV_SYMBOL_BATTERY_FULL " --%",
                                  0, -166, &lv_font_montserrat_20, settings->basic_foreground);
        if (settings->basic_show_date)
            date_label = face_text(parent, "--", 0, 70, &lv_font_montserrat_24,
                                   settings->basic_foreground);
        info_label = face_text(parent, "", 0, 109, &lv_font_montserrat_16,
                               settings->basic_foreground);
    }
    lf_weather_config_t weather_cfg;
    lf_weather_get_config(&weather_cfg);
    /* Fancy faces stay free of forecast overlays. */
    if (weather_cfg.enabled &&
        (current_style == 1 || (current_style == 5 && settings->basic_weather))) {
        uint32_t colour = current_style == 5 ? settings->basic_foreground : 0x9DC9F3;
        weather_label = face_text(parent, "-- C", 45, 140, &lv_font_montserrat_24, colour);
    }
    clock_active = true;
    lf_clock_update();
}
void lf_clock_update(void)
{
    if (!clock_active) return;
    lf_power_state_t state;
    if (battery_label && lf_power_cached(&state)) {
        char text[44];
        if (state.percentage >= 0)
            snprintf(text, sizeof(text), "%s %d%%", state.charging ? LV_SYMBOL_CHARGE : LV_SYMBOL_BATTERY_FULL,
                     state.percentage);
        else
            snprintf(text, sizeof(text), "%s", state.charging ? "CHARGING" : "BATTERY");
        lv_label_set_text(battery_label, text);
        lv_obj_set_style_text_color(battery_label, lv_color_hex(lf_power_color(&state)), 0);
    }
    if (weather_label) {
        char strip[68];
        lf_weather_get_strip(strip, sizeof strip);
        lv_label_set_text(weather_label, strip[0] ? strip : "WEATHER SYNCING");
    }
    time_t now = time(NULL);
    if (now == last_second) return;
    last_second = now;
    bool synced = now > 1700000000;
    if (!synced) {
        if (info_label) lv_label_set_text(info_label, "CONNECT WI-FI TO SYNC");
        if (time_label) lv_label_set_text(time_label, "--:--");
        if (sundial_time_label) lv_label_set_text(sundial_time_label, "??:??");
        return;
    }
    struct tm local;
    localtime_r(&now, &local);
    if (info_label) lv_label_set_text(info_label, "");
    if (date_label) {
        char date[32];
        if (current_style == 0) strftime(date, sizeof(date), "%d", &local);
        else if (current_style == 3) strftime(date, sizeof(date), "%a %m-%d", &local);
        else strftime(date, sizeof(date), "%a %b %d", &local);
        lv_label_set_text(date_label, date);
    }
    if (time_label) {
        char t[20];
        strftime(t, sizeof(t), "%I:%M %p", &local);
        lv_label_set_text(time_label, t);
    }
    if (sundial_time_label) {
        char t[20];
        strftime(t, sizeof(t), "%I:%M", &local);
        lv_label_set_text(sundial_time_label, t);
        if (info_label) lv_label_set_text(info_label, "SHADE ACCURACY: QUESTIONABLE");
    }
    if (sundial_shadow) {
        /* Noon points upward, night is still a joke. */
        float angle = (float)(((local.tm_hour + local.tm_min / 60.f) - 12.f) * 15.f - 90.f) * PI_F / 180.f;
        polar(0, angle, &sundial_shadow_points[0]);
        polar(104, angle, &sundial_shadow_points[1]);
        lv_line_set_points(sundial_shadow, sundial_shadow_points, 2);
    }
    if (current_style == 1 || current_style == 3 || current_style == 4 || current_style == 5) return;
    float sec = (float)local.tm_sec;
    float min = (float)local.tm_min + sec / 60.f;
    float hour = (float)(local.tm_hour % 12) + min / 60.f;
    float angles[3] = {
        (hour * 30.f - 90.f) * PI_F / 180.f,
        (min * 6.f - 90.f) * PI_F / 180.f,
        (sec * 6.f - 90.f) * PI_F / 180.f
    };
    int lengths[3] = {94, 133, 164};
    for (int i = 0; i < 3; i++) {
        if (current_style == 0 && i == 2) continue;
        if (!hands[i]) continue;
        polar(-13, angles[i], &hand_points[i][0]);
        polar(lengths[i], angles[i], &hand_points[i][1]);
        lv_line_set_points(hands[i], hand_points[i], 2);
    }
}


/* Time-of-day includes microseconds, unlike time(NULL). Updating just the
   moving second hand avoids repainting clock labels, weather, and battery
   60 times per second. LVGL's display pipeline may cap the actual FPS. */
/* Monotonic second hand: refresh civil-time anchor once per two minutes,
   never snap a competing time(NULL) hand onto the same LVGL line. */
void lf_clock_animate(void)
{
    if (!clock_active || current_style != 0 || !hands[2]) return;
    int64_t mono = esp_timer_get_time();
    if (!anchor_mono_us || mono - anchor_mono_us >= 120000000LL) {
        struct timeval t;
        if (gettimeofday(&t, NULL) != 0 || t.tv_sec <= 1700000000) return;
        int64_t fresh = (int64_t)t.tv_sec * 1000000LL + t.tv_usec;
        if (anchor_mono_us) {
            int64_t projected = anchor_wall_us + mono - anchor_mono_us;
            int64_t correction = fresh - projected;
            if (correction > 200000) correction = 200000;
            if (correction < -200000) correction = -200000;
            fresh = projected + correction;
        }
        anchor_mono_us = mono;
        anchor_wall_us = fresh;
    }
    int64_t wall = anchor_wall_us + mono - anchor_mono_us;
    float seconds = (float)(wall % 60000000LL) / 1000000.f;
    float angle = (seconds * 6.f - 90.f) * PI_F / 180.f;
    polar(-13, angle, &hand_points[2][0]);
    polar(164, angle, &hand_points[2][1]);
    lv_line_set_points(hands[2], hand_points[2], 2);
}
