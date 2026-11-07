#include "clock_face.h"
#include "power.h"
#include "weather.h"
#include "basic_clock.h"
#include "esp_timer.h"
#include <math.h>
#include <stdint.h>
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
static lv_obj_t *date_window; /* Brolex magnifier moves with its date */
static lv_obj_t *time_label;
static lv_obj_t *battery_label;
static lv_obj_t *weather_label;
static lv_obj_t *meridiem_label;
static lv_obj_t *sundial_time_label;
static lv_obj_t *sundial_shadow;
static lv_point_precise_t sundial_shadow_points[2];
static uint8_t current_style;
static time_t last_second = (time_t)-1;
static uint32_t displayed_battery_color = UINT32_MAX;
/* lv_label_set_text() invalidates pixels even if the text did not change.
   The Basic face has a blended ring underneath these labels: avoid triggering
   an expensive refresh every second for a clock that displays minutes. */
static void update_label_if_changed(lv_obj_t *label, const char *value)
{
    if (!label || !value) return;
    const char *shown = lv_label_get_text(label);
    if (!shown || strcmp(shown, value) != 0) lv_label_set_text(label, value);
}
static bool clock_active;
static const lf_settings_t *clock_settings;
static uint32_t colour_bg(void){return clock_settings->face_background[current_style];}
static uint32_t colour_fg(void){return clock_settings->face_foreground[current_style];}
static uint32_t colour_ac(void){return clock_settings->face_accent[current_style];}
static lv_obj_t *weather_icon;
static int last_weather_icon = -999;
static int64_t anchor_mono_us;
static int64_t anchor_wall_us;

static lv_obj_t *weather_icon;
static int weather_icon_code = -999;
static void symbol_shape(lv_obj_t *parent, int x, int y, int w, int h,
                         uint32_t color, int round)
{
    lv_obj_t *part = lv_obj_create(parent);
    lv_obj_set_size(part, w, h);
    lv_obj_align(part, LV_ALIGN_CENTER, x, y);
    lv_obj_set_style_bg_color(part, lv_color_hex(color), 0);
    lv_obj_set_style_bg_opa(part, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(part, 0, 0);
    lv_obj_set_style_radius(part, round, 0);
    lv_obj_set_style_pad_all(part, 0, 0);
    lv_obj_remove_flag(part, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_remove_flag(part, LV_OBJ_FLAG_CLICKABLE);
}
static void update_weather_icon(int code)
{
    if (!weather_icon || code == weather_icon_code) return;
    weather_icon_code = code;
    lv_obj_clean(weather_icon);
    bool rain = (code >= 51 && code <= 67) || (code >= 80 && code <= 82);
    bool snow = (code >= 71 && code <= 77) || code == 85 || code == 86;
    if (code == 0) {
        symbol_shape(weather_icon, 0, 0, 28, 28, 0xFFD05E, LV_RADIUS_CIRCLE);
        symbol_shape(weather_icon, 0,-21, 4,8,0xFFD05E,2);
        symbol_shape(weather_icon, 0,21, 4,8,0xFFD05E,2);
        symbol_shape(weather_icon, -21,0,8,4,0xFFD05E,2);
        symbol_shape(weather_icon, 21,0,8,4,0xFFD05E,2);
    } else {
        symbol_shape(weather_icon,-10,-3,24,24,0xC0DCF2,LV_RADIUS_CIRCLE);
        symbol_shape(weather_icon,7,-7,30,29,0xE2F0FA,LV_RADIUS_CIRCLE);
        symbol_shape(weather_icon,1,6,44,18,0xCBDFF0,9);
        if (rain || snow) for (int i=-1;i<=1;i++)
            symbol_shape(weather_icon,i*13,23,5,10,
                         snow ? 0xF4FAFF : 0x56A8FB,3);
        if (code >= 95)
            symbol_shape(weather_icon,9,22,8,15,0xFFD044,3);
    }
}


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
/* Use only enabled LVGL fonts; these remain within the 466px display. */
static const lv_font_t *basic_element_font(uint8_t size)
{
    /* Wider native range: 12px at 0%, 40px at 100%, 48px before
       higher-scale styling from 130% through 200%. */
    return size>=130?&lv_font_montserrat_48:
           size>=94?&lv_font_montserrat_40:
           size>=74?&lv_font_montserrat_32:
           size>=54?&lv_font_montserrat_28:
           size>=32?&lv_font_montserrat_24:
           size>=15?&lv_font_montserrat_18:&lv_font_montserrat_12;
}
static void basic_scale_element(lv_obj_t *item,uint8_t size)
{
    /* 0.8.110: no scale transform. Sizes above 100% use the largest native
       font (basic_element_font) instead of an offscreen scaled layer that
       was re-rendered on every frame. */
    (void)item;(void)size;
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
/* 0.8.110: an lv_line's box spans from its parent's origin to its furthest
   point, so moving a clock hand used to invalidate a ~400x400 region (most of
   the dial) 30 times per second. Keep each moving line's object tightly
   around its two endpoints so only the pixels the hand covers are redrawn. */
static void tight_line_set(lv_obj_t *line, const lv_point_precise_t *abs_pts,
                           lv_point_precise_t *rel_store, int width)
{
    if(!line)return;
    const int pad=width+2;
    int32_t x0=(int32_t)abs_pts[0].x, y0=(int32_t)abs_pts[0].y;
    int32_t x1=(int32_t)abs_pts[1].x, y1=(int32_t)abs_pts[1].y;
    const int32_t minx=(x0<x1?x0:x1)-pad, miny=(y0<y1?y0:y1)-pad;
    const int32_t maxx=(x0>x1?x0:x1)+pad, maxy=(y0>y1?y0:y1)+pad;
    rel_store[0]=(lv_point_precise_t){x0-minx,y0-miny};
    rel_store[1]=(lv_point_precise_t){x1-minx,y1-miny};
    lv_obj_set_pos(line,minx,miny);
    lv_obj_set_size(line,maxx-minx+1,maxy-miny+1);
    lv_line_set_points(line,rel_store,2);
}
static lv_point_precise_t hand_rel_points[3][2];
static int hand_widths[3];
static lv_point_precise_t sundial_shadow_rel[2];
static void polar(int r, float angle, lv_point_precise_t *point)
{
    point->x = (int)(CC + cosf(angle) * r);
    point->y = (int)(CC + sinf(angle) * r);
}
/* One custom LVGL drawing widget replaces 60 separate tick widgets.
   It uses the exact 60 tick positions and anti-aliased lines of the old dial. */
/* Dial geometry never changes between refreshes. Previous code recalculated
   60 sin/cos pairs inside each LVGL draw callback, repeated for every dirty
   24-row strip (up to 20 times per full-frame render). Build it once per
   static dial type, then issue exactly the same LVGL line descriptors. */
typedef struct {
    int16_t x1,y1,x2,y2;
} lf_dial_tick_t;
static lf_dial_tick_t dial_tick_cache[2][60];
static bool dial_ticks_ready[2];
static void dial_ticks_prepare(bool diver)
{
    const int style=diver?1:0;
    if(dial_ticks_ready[style])return;
    for(int i=0;i<60;i++){
        const bool major=(i%5)==0;
        const float angle=(float)(i*6-90)*(PI_F/180.f);
        const float c=cosf(angle),t=sinf(angle);
        const int outer=diver?185:181;
        const int inner=major?(diver?171:160):(diver?181:175);
        dial_tick_cache[style][i]=(lf_dial_tick_t){
            .x1=(int16_t)(CC+c*inner), .y1=(int16_t)(CC+t*inner),
            .x2=(int16_t)(CC+c*outer), .y2=(int16_t)(CC+t*outer)
        };
    }
    dial_ticks_ready[style]=true;
}
static void tick_draw_event(lv_event_t *e)
{
    lv_obj_t *o=lv_event_get_target(e);
    lv_layer_t *layer=lv_event_get_layer(e);
    lv_area_t area;
    lv_obj_get_coords(o,&area);
    const bool diver=(uintptr_t)lv_event_get_user_data(e)!=0;
    const lf_dial_tick_t *ticks=dial_tick_cache[diver?1:0];
    for(int i=0;i<60;i++){
        /* Dress watch has only twelve markers, diver has sixty. */
        if(!diver && i%5)continue;
        const bool major=(i%5)==0;
        /* Discard invisible ticks BEFORE lv_draw_line adds a draw task.
           Screen clips are typically 24px high, so submitting 60 strokes
           for every band (including remote clock panels) is needless work.
           Expand the bound for stroke width and antialiasing. */
        const int32_t ax=area.x1+ticks[i].x1;
        const int32_t ay=area.y1+ticks[i].y1;
        const int32_t bx=area.x1+ticks[i].x2;
        const int32_t by=area.y1+ticks[i].y2;
        const int32_t pad=(diver?(major?5:2):2)+3;
        const lv_area_t *clip=&layer->_clip_area;
        if((ax>bx?ax:bx)+pad<clip->x1 ||
           (ax<bx?ax:bx)-pad>clip->x2 ||
           (ay>by?ay:by)+pad<clip->y1 ||
           (ay<by?ay:by)-pad>clip->y2)continue;
        lv_draw_line_dsc_t line;
        lv_draw_line_dsc_init(&line);
        line.p1.x=ax;
        line.p1.y=ay;
        line.p2.x=bx;
        line.p2.y=by;
        line.color=lv_color_hex(major?colour_fg():colour_ac());
        line.width=diver?(major?5:2):2;
        line.round_start=1;
        line.round_end=1;
        lv_draw_line(layer,&line);
    }
}
static void draw_ticks(lv_obj_t *parent, bool diver)
{
    dial_ticks_prepare(diver);
    lv_obj_t *dial=lv_obj_create(parent);
    lv_obj_set_size(dial,CW,CW);
    lv_obj_center(dial);
    lv_obj_set_style_pad_all(dial,0,0);
    lv_obj_set_style_bg_opa(dial,LV_OPA_TRANSP,0);
    lv_obj_set_style_border_width(dial,0,0);
    lv_obj_remove_flag(dial,LV_OBJ_FLAG_CLICKABLE);
    lv_obj_remove_flag(dial,LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(dial,tick_draw_event,LV_EVENT_DRAW_MAIN,(void *)(uintptr_t)(diver?1:0));
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
static void update_hand_if_moved(int index,float angle,int tip_radius)
{
    if(index<0||index>=3||!hands[index])return;
    lv_point_precise_t ends[2];
    polar(-13,angle,&ends[0]);
    polar(tip_radius,angle,&ends[1]);
    if(hand_points[index][0].x==ends[0].x &&
       hand_points[index][0].y==ends[0].y &&
       hand_points[index][1].x==ends[1].x &&
       hand_points[index][1].y==ends[1].y)return;
    hand_points[index][0]=ends[0];
    hand_points[index][1]=ends[1];
    tight_line_set(hands[index],hand_points[index],hand_rel_points[index],hand_widths[index]);
}
static void create_hands(lv_obj_t *parent, bool diver)
{
    int widths[3] = {diver ? 11 : 5, diver ? 7 : 3, diver ? 2 : 1};
    uint32_t colours[3] = {clock_settings->face_time_color[current_style],
        clock_settings->face_time_color[current_style],
        clock_settings->face_time_color[current_style]};
    for (int i = 0; i < 3; i++) {
        hand_points[i][0] = (lv_point_precise_t){CC, CC};
        hand_points[i][1] = (lv_point_precise_t){CC, CC};
        hands[i] = dial_line(parent, hand_points[i], widths[i], colours[i]);
        hand_widths[i] = widths[i];
        tight_line_set(hands[i], hand_points[i], hand_rel_points[i], widths[i]);
    }
    lv_obj_t *pin = round_panel(parent, 13, diver ? 0xD6E9E0 : 0xC7E4FF,
                               0x0E1A25, 1);
    (void)pin;
}
static void build_diver(lv_obj_t *parent)
{
    round_panel(parent, 452, 0x070B0E, 0x416574, 11);
    round_panel(parent, 423, 0x09161B, 0x8797A3, 3);
    round_panel(parent, 389, colour_bg(), colour_ac(), 2);
    draw_ticks(parent, true);
    add_hour_markers(parent);
    lv_obj_t *dw = lv_obj_create(parent);
    date_window=dw;
    lv_obj_set_size(dw, 45, 34);
    lv_obj_align(dw, LV_ALIGN_CENTER, 99, -2);
    lv_obj_set_style_bg_color(dw, lv_color_hex(0xEFF5EC), 0);
    lv_obj_set_style_border_width(dw, 3, 0);
    lv_obj_set_style_border_color(dw, lv_color_hex(0xB5C2D1), 0);
    lv_obj_set_style_radius(dw, 6, 0);
    lv_obj_set_style_pad_all(dw, 0, 0);
    date_label = face_text(dw, "--", 0, 0, &lv_font_montserrat_18, 0x111B1B);
    /* Original "Brolex" lettering and five-ridge mountain insignia:
       a playful homage, NOT Rolex's protected crown artwork. */
    face_text(parent,"BROLEX",0,-98,&lv_font_montserrat_20,0xE9D99C);
    face_text(parent,"MOUNTAIN DIVER",0,-75,&lv_font_montserrat_12,0xA3C7CB);
    static lv_point_precise_t ridge[5][2]={
        {{211,135},{211,122}},{{222,135},{222,113}},
        {{233,135},{233,106}},{{244,135},{244,113}},
        {{255,135},{255,122}}
    };
    for(int peak=0;peak<5;peak++)
        dial_line(parent,ridge[peak],2,0xD7BE75);
    create_hands(parent, true);
    battery_label = face_text(parent, "BAT --", 0, -137, &lv_font_montserrat_16, colour_ac());
    info_label = face_text(parent, "", 0, 111, &lv_font_montserrat_12, 0xB5D9E8);
}
static void build_digital(lv_obj_t *parent)
{
    round_panel(parent, 447, 0x030815, 0x245285, 5);
    round_panel(parent, 409, colour_bg(), colour_ac(), 3);
    time_label = face_text(parent, "--:--", 0, -11, &lv_font_montserrat_48, colour_fg());
    date_label = face_text(parent, "NO TIME", 0, 72, &lv_font_montserrat_20, 0xA9D5FF);
    battery_label = face_text(parent, "BAT --", 0, -146, &lv_font_montserrat_18, 0x7DCEFF);
    info_label = face_text(parent, "", 0, 110, &lv_font_montserrat_12, 0x7D92AD);
}
static void build_classic(lv_obj_t *parent)
{
    /* Restrained dress-watch face deliberately unlike the diver: no
       diving bezel, luminous bars or large date magnifier. Roman markers,
       slim hands and a simple brushed inner rim. */
    round_panel(parent,449,0x101822,0x70808F,3);
    round_panel(parent,417,colour_bg(),colour_ac(),1);
    draw_ticks(parent,false);
    face_text(parent,"XII",0,-158,&lv_font_montserrat_24,colour_fg());
    face_text(parent,"III",160,0,&lv_font_montserrat_20,colour_fg());
    face_text(parent,"VI",0,159,&lv_font_montserrat_24,colour_fg());
    face_text(parent,"IX",-160,0,&lv_font_montserrat_20,colour_fg());
    face_text(parent,"CLASSIC",0,-78,&lv_font_montserrat_16,0xA5B4C3);
    create_hands(parent,false);
    date_label=face_text(parent,"--",0,106,&lv_font_montserrat_16,0xC3D5E3);
    battery_label=face_text(parent,"BAT --",0,-109,
                            &lv_font_montserrat_16,0xA5D6E8);
    info_label=face_text(parent,"",0,129,&lv_font_montserrat_12,0x809DB8);
}

/* Retro green 1980s LCD watch. Intentionally an original face, not a
   reproduction of any company's trademark or printed watch artwork. */
static void build_retro_lcd(lv_obj_t *parent)
{
    round_panel(parent, 450, 0x090B10, 0x6A737C, 9);
    round_panel(parent, 423, colour_bg(), colour_ac(), 3);

    lv_obj_t *lcd = lv_obj_create(parent);
    lv_obj_set_size(lcd, 347, 219);
    lv_obj_align(lcd, LV_ALIGN_CENTER, 0, -1);
    lv_obj_set_style_radius(lcd, 20, 0);
    lv_obj_set_style_pad_all(lcd, 0, 0);
    lv_obj_set_style_border_width(lcd, 8, 0);
    lv_obj_set_style_border_color(lcd, lv_color_hex(0x364641), 0);
    lv_obj_set_style_bg_color(lcd, lv_color_hex(colour_bg()), 0);
    lv_obj_set_style_bg_opa(lcd, LV_OPA_COVER, 0);
    lv_obj_remove_flag(lcd, LV_OBJ_FLAG_SCROLLABLE);

    face_text(lcd, "ALARM     CHRONO     LIGHT", 0, -82,
              &lv_font_montserrat_12, 0x384F38);
    date_label = face_text(parent, "MON 01-01", 0, -53,
                           &lv_font_montserrat_20, 0x253A2C);
    time_label = face_text(lcd, "--:--", 0, 7,
                           &lv_font_montserrat_48, colour_fg());
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
    round_panel(parent, 383, colour_bg(), colour_ac(), 2);
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
    tight_line_set(sundial_shadow, sundial_shadow_points, sundial_shadow_rel, 10);
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
    date_label = face_text(parent, "--", 0, 135,
                           &lv_font_montserrat_18, 0x6A5136);
}

const char *lf_clock_style_name(uint8_t style)
{
    switch (style) {
        case 0: return "Brolex Diver";
        case 1: return "Midnight Digital";
        case 2: return "Classic Analog";
        case 3: return "Retro Green LCD";
        case 4: return "Solar-ish Sundial";
        case 5: return "Basic Custom";
        default: return "Brolex Diver";
    }
}
void lf_clock_clear(void)
{
    lf_basic_clock_clear();
    clock_active = false;
    displayed_battery_color = UINT32_MAX;
    for (int i = 0; i < 3; i++) hands[i] = NULL;
    info_label = NULL;
    date_label = NULL;
    date_window = NULL;
    time_label = NULL;
    battery_label = NULL;
    weather_label = NULL;
    meridiem_label = NULL;
    weather_icon = NULL;
    weather_icon_code = -999;
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
    lv_obj_set_style_bg_color(parent, lv_color_hex(colour_bg()), 0);
    lv_obj_set_style_bg_opa(parent, LV_OPA_COVER, 0);
    if (current_style == 0) build_diver(parent);
    else if (current_style == 1) build_digital(parent);
    else if (current_style == 2) build_classic(parent);
    else if (current_style == 3) build_retro_lcd(parent);
    else if (current_style == 4) build_sundial(parent);
    else {
        time_label = lf_basic_clock_render(parent, settings);
        if(settings->basic_font_style!=0 && settings->basic_font_style!=4 &&
           !settings->face_24h[5])
            meridiem_label=face_text(parent,"",settings->basic_time_x+139,
                                    settings->basic_time_y+22,
                                    &lv_font_montserrat_16,
                                    settings->face_time_color[5]);
        battery_label = face_text(parent, LV_SYMBOL_BATTERY_FULL " --%",
                                  settings->face_battery_x[5], settings->face_battery_y[5],
                                  basic_element_font(settings->face_battery_size[5]),
                                  settings->face_battery_color[5]);
        if (settings->face_show_date[5])
            date_label = face_text(parent, "--",
                                   settings->face_date_x[5], settings->face_date_y[5],
                                   basic_element_font(settings->face_date_size[5]),
                                   settings->face_date_color[5]);
        info_label = face_text(parent, "", 0, 109, &lv_font_montserrat_16,
                               settings->basic_foreground);
    }
    /* Non-default date formats use a normal movable label instead of
       forcing a full date into the diver's 34px magnifier. */
    if(current_style==0 && settings->face_date_format[0]!=0){
        if(date_window)lv_obj_add_flag(date_window,LV_OBJ_FLAG_HIDDEN);
        date_label=face_text(parent,"",settings->face_date_x[0],
                             settings->face_date_y[0],
                             basic_element_font(settings->face_date_size[0]),colour_fg());
    }
    lf_weather_config_t weather_cfg;
    lf_weather_get_config(&weather_cfg);
    /* Weather is an opt-in, movable part of each independent face. */
    if (weather_cfg.enabled && settings->face_show_weather[current_style]) {
        uint32_t colour = settings->face_weather_color[current_style];
        const uint8_t wz=settings->face_weather_size[current_style];
        weather_icon = lv_obj_create(parent);
        int icon_size=46+wz/4;
        lv_obj_set_size(weather_icon,icon_size,icon_size);
        lv_obj_align(weather_icon,LV_ALIGN_CENTER,
             settings->face_weather_x[current_style]-52,
             settings->face_weather_y[current_style]);
        lv_obj_set_style_bg_opa(weather_icon, LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_width(weather_icon, 0, 0);
        lv_obj_set_style_pad_all(weather_icon, 0, 0);
        lv_obj_remove_flag(weather_icon, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_remove_flag(weather_icon, LV_OBJ_FLAG_SCROLLABLE);
        weather_label = face_text(parent, "-- C",
                   settings->face_weather_x[current_style]+50,
                   settings->face_weather_y[current_style],
                   basic_element_font(wz),colour);
        basic_scale_element(weather_label,wz);
    }
    /* All six faces independently persist battery/date placement and scale.
       The diver date window remains a single movable unit, preserving the
       existing white magnifier instead of separating its label from its frame. */
    if(battery_label){
        lv_obj_set_style_text_color(battery_label,
            lv_color_hex(settings->face_battery_color[current_style]),0);
        const uint8_t z=settings->face_battery_size[current_style];
        lv_obj_align(battery_label,LV_ALIGN_CENTER,
                     settings->face_battery_x[current_style],
                     settings->face_battery_y[current_style]);
        lv_obj_set_style_text_font(battery_label,basic_element_font(z),0);
        basic_scale_element(battery_label,z);
    }
    if(date_window && current_style==0 && settings->face_date_format[0]==0){
        const uint8_t z=settings->face_date_size[0];
        const int delta=(int)z-60;
        lv_obj_set_size(date_window,45+delta/4>34?45+delta/4:34,
                        34+delta/5>26?34+delta/5:26);
        lv_obj_align(date_window,LV_ALIGN_CENTER,
                     settings->face_date_x[0],settings->face_date_y[0]);
        lv_obj_set_style_text_font(date_label,basic_element_font(z),0);
        basic_scale_element(date_label,z);
    }else if(date_label){
        const uint8_t z=settings->face_date_size[current_style];
        lv_obj_align(date_label,LV_ALIGN_CENTER,
                     settings->face_date_x[current_style],
                     settings->face_date_y[current_style]);
        lv_obj_set_style_text_font(date_label,basic_element_font(z),0);
        basic_scale_element(date_label,z);
    }
    /* Time and AM/PM share a colour; Battery, Date and Weather do not. */
    if(time_label)lv_obj_set_style_text_color(time_label,
        lv_color_hex(settings->face_time_color[current_style]),0);
    if(sundial_time_label)lv_obj_set_style_text_color(sundial_time_label,
        lv_color_hex(settings->face_time_color[current_style]),0);
    if(meridiem_label)lv_obj_set_style_text_color(meridiem_label,
        lv_color_hex(settings->face_time_color[current_style]),0);
    if(date_label)lv_obj_set_style_text_color(date_label,
        lv_color_hex(settings->face_date_color[current_style]),0);
    if(!settings->face_show_date[current_style] && date_label)
        lv_obj_add_flag(date_label,LV_OBJ_FLAG_HIDDEN);
    if(!settings->face_show_date[current_style] && date_window)
        lv_obj_add_flag(date_window,LV_OBJ_FLAG_HIDDEN);
    if(!settings->face_show_seconds[current_style] && hands[2])
        lv_obj_add_flag(hands[2],LV_OBJ_FLAG_HIDDEN);
    if((!settings->face_show_battery[current_style] ||
        (!settings->face_show_icon[current_style] &&
         !settings->face_show_percent[current_style])) && battery_label)
        lv_obj_add_flag(battery_label,LV_OBJ_FLAG_HIDDEN);
    clock_active = true;
    lf_clock_update();
}
void lf_clock_update(void)
{
    if (!clock_active) return;
    lf_power_state_t state;
    if (battery_label && lf_power_cached(&state)) {
        char text[44]={0};
        const bool show_icon=clock_settings->face_show_icon[current_style]!=0;
        const bool show_percent=clock_settings->face_show_percent[current_style]!=0;
        if(state.percentage>=0){
            const char *symbol=state.percentage>=88?LV_SYMBOL_BATTERY_FULL:
                state.percentage>=63?LV_SYMBOL_BATTERY_3:
                state.percentage>=38?LV_SYMBOL_BATTERY_2:
                state.percentage>=15?LV_SYMBOL_BATTERY_1:LV_SYMBOL_BATTERY_EMPTY;
            if(show_icon)snprintf(text,sizeof text,"%s%s",
                state.charging?LV_SYMBOL_CHARGE:"",symbol);
            if(show_percent){
                size_t used=strlen(text);
                snprintf(text+used,sizeof(text)-used,"%s%d%%",
                         show_icon?" ":"",state.percentage);
            }
        }else if(show_icon){
            snprintf(text,sizeof text,"%s",state.charging?
                     LV_SYMBOL_CHARGE:LV_SYMBOL_BATTERY_EMPTY);
        }else if(show_percent){
            snprintf(text,sizeof text,"--%%");
        }
        update_label_if_changed(battery_label, text);
        uint32_t color = state.usb_connected ? clock_settings->charging_color :
                          clock_settings->face_battery_color[current_style];
        if (color != displayed_battery_color) {
            lv_obj_set_style_text_color(battery_label, lv_color_hex(color), 0);
            displayed_battery_color = color;
        }
    }
    if (weather_label) {
        int temperature=0, condition=0;
        if (lf_weather_get_current(&temperature, &condition)) {
            char value[22];
            snprintf(value, sizeof value, "%d C", temperature);
            update_label_if_changed(weather_label, value);
            update_weather_icon(condition);
        } else {
            update_label_if_changed(weather_label, "-- C");
        }
    }
    time_t now = time(NULL);
    if (now == last_second) return;
    last_second = now;
    bool synced = now > 1700000000;
    if (!synced) {
        update_label_if_changed(info_label, "CONNECT WI-FI TO SYNC");
        update_label_if_changed(time_label, "--:--");
        lf_basic_clock_update_time("--:--");
        update_label_if_changed(meridiem_label, "");
        update_label_if_changed(sundial_time_label, "??:??");
        return;
    }
    struct tm local;
    localtime_r(&now, &local);
    update_label_if_changed(info_label, "");
    if(meridiem_label){
        char ampm[4]={0};
        strftime(ampm,sizeof ampm,"%p",&local);
        update_label_if_changed(meridiem_label,ampm);
    }
    if (date_label) {
        char date[32];
        const uint8_t format=clock_settings->face_date_format[current_style];
        const char *fmt=format==1?"%a %b %d":
                        format==2?"%b %d":
                        format==3?"%d/%m/%Y":
                        format==4?"%Y-%m-%d":
                        format==5?"%A, %B %d":
                        current_style==0?"%d":
                        current_style==3?"%a %m-%d":"%a %b %d";
        strftime(date,sizeof(date),fmt,&local);
        update_label_if_changed(date_label, date);
    }
    if (time_label) {
        char t[20];
        const bool custom_time_font=(current_style==5 && clock_settings->basic_font_style>=24);
        strftime(t, sizeof(t),
                 clock_settings->face_24h[current_style] ? "%H:%M" :
                 custom_time_font ? "%I:%M" : "%I:%M %p", &local);
        update_label_if_changed(time_label, t);
        if(current_style==5)lf_basic_clock_update_time(t);
    }
    if (sundial_time_label) {
        char t[20];
        strftime(t, sizeof(t), clock_settings->face_24h[current_style] ? "%H:%M" : "%I:%M", &local);
        update_label_if_changed(sundial_time_label, t);
        update_label_if_changed(info_label, "SHADE ACCURACY: QUESTIONABLE");
    }
    if (sundial_shadow) {
        /* Noon points upward, night is still a joke. */
        float angle = (float)(((local.tm_hour + local.tm_min / 60.f) - 12.f) * 15.f - 90.f) * PI_F / 180.f;
        lv_point_precise_t new_shadow[2];
        polar(0,angle,&new_shadow[0]);
        polar(104,angle,&new_shadow[1]);
        if(sundial_shadow_points[0].x!=new_shadow[0].x ||
           sundial_shadow_points[0].y!=new_shadow[0].y ||
           sundial_shadow_points[1].x!=new_shadow[1].x ||
           sundial_shadow_points[1].y!=new_shadow[1].y){
            sundial_shadow_points[0]=new_shadow[0];
            sundial_shadow_points[1]=new_shadow[1];
            tight_line_set(sundial_shadow,sundial_shadow_points,sundial_shadow_rel,10);
        }
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
        update_hand_if_moved(i,angles[i],lengths[i]);
    }
}


/* Time-of-day includes microseconds, unlike time(NULL). Updating just the
   moving second hand avoids repainting clock labels, weather, and battery
   60 times per second. LVGL's display pipeline may cap the actual FPS. */
/* Monotonic second hand: refresh civil-time anchor once per two minutes,
   never snap a competing time(NULL) hand onto the same LVGL line. */
void lf_clock_animate(void)
{
    if (!clock_active || current_style != 0 || !hands[2] ||
        !clock_settings->face_show_seconds[current_style]) return;
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
    /* The 16ms sweep timer often lands on the same physical pixels.
       Suppress no-op invalidations instead of redrawing the entire hand. */
    update_hand_if_moved(2,angle,164);
}
