#include "weather_visual.h"
#include <stdio.h>
#include <string.h>

static lv_obj_t *scrollbox;
static lf_weather_view_t last;
static bool last_known;

static lv_obj_t *panel(lv_obj_t *parent, int w, int h, uint32_t bg, int radius)
{
    lv_obj_t *obj = lv_obj_create(parent);
    lv_obj_set_size(obj, w, h);
    lv_obj_set_style_radius(obj, radius, 0);
    lv_obj_set_style_bg_color(obj, lv_color_hex(bg), 0);
    lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(obj, 0, 0);
    lv_obj_set_style_pad_all(obj, 0, 0);
    lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
    return obj;
}
static lv_obj_t *txt(lv_obj_t *parent, const char *value, int x, int y,
                     const lv_font_t *font, uint32_t colour)
{
    lv_obj_t *o = lv_label_create(parent);
    lv_label_set_text(o, value);
    lv_obj_set_style_text_font(o, font, 0);
    lv_obj_set_style_text_color(o, lv_color_hex(colour), 0);
    lv_obj_set_style_text_align(o, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(o, LV_ALIGN_TOP_LEFT, x, y);
    return o;
}
static void shape(lv_obj_t *parent,int x,int y,int w,int h,
                  uint32_t rgb,int radius)
{
    lv_obj_t *o = panel(parent,w,h,rgb,radius);
    lv_obj_set_pos(o,x,y);
    lv_obj_remove_flag(o,LV_OBJ_FLAG_CLICKABLE);
}
static bool is_rain(int code)
{
    return (code>=51&&code<=67) || (code>=80&&code<=82);
}
static bool is_snow(int code)
{
    return (code>=71&&code<=77) || code==85 || code==86;
}
static void symbol(lv_obj_t *parent,int code,int x,int y,int scale)
{
    /* Self-contained LVGL vector icon, deliberately oversized and coloured.
       scale=1 for forecast tiles, 2 for a prominent 96px condition graphic. */
    if(code==0 || code==1) {
        shape(parent,x+14*scale,y+8*scale,30*scale,30*scale,0xFFCB55,LV_RADIUS_CIRCLE);
        shape(parent,x+27*scale,y+0,5*scale,7*scale,0xFFE69A,4);
        shape(parent,x+27*scale,y+40*scale,5*scale,7*scale,0xFFE69A,4);
        shape(parent,x+3*scale,y+19*scale,7*scale,5*scale,0xFFE69A,4);
        shape(parent,x+47*scale,y+19*scale,7*scale,5*scale,0xFFE69A,4);
        if(code==0) return;
    }
    if(code==1 || code>=2) {
        shape(parent,x+8*scale,y+17*scale,25*scale,22*scale,0xA8C9E5,LV_RADIUS_CIRCLE);
        shape(parent,x+23*scale,y+11*scale,29*scale,30*scale,0xEDF5FC,LV_RADIUS_CIRCLE);
        shape(parent,x+3*scale,y+27*scale,52*scale,15*scale,0xBEDAF0,10*scale);
    }
    if(is_rain(code)||is_snow(code)) {
        for(int i=0;i<3;i++)
            shape(parent,x+(13+i*14)*scale,y+44*scale,5*scale,9*scale,
                  is_snow(code)?0xEDF8FF:0x4CB4FF,4);
    }
    if(code>=95) shape(parent,x+30*scale,y+42*scale,11*scale,15*scale,0xF9D45E,3);
}
static const char *condition_name(int code)
{
    if(code==0) return "Clear sky";
    if(code<=2) return "Mostly clear";
    if(code==3) return "Overcast";
    if(code==45||code==48) return "Foggy";
    if(is_snow(code)) return "Snow";
    if(is_rain(code)) return "Rain";
    if(code>=95) return "Thunderstorms";
    return "Cloudy";
}
static void forecast_hour(lv_obj_t *row,const lf_weather_hour_t *w,int index)
{
    lv_obj_t *card=panel(row,76,110,0x172F4B,16);
    lv_obj_set_pos(card,4+82*index,0);
    txt(card,w->hour,13,8,&lv_font_montserrat_16,0xC8DFEF);
    symbol(card,w->condition,12,33,1);
    char temp[16];snprintf(temp,sizeof temp,"%d°",w->temperature);
    txt(card,temp,17,87,&lv_font_montserrat_20,0xF6FAFF);
}
static void forecast_day(lv_obj_t *row,const lf_weather_day_t *w,int index)
{
    lv_obj_t *card=panel(row,103,93,0x182F4C,16);
    lv_obj_set_pos(card,4+109*index,0);
    const char *days[]={"Today","Tomorrow","Next"};
    txt(card,days[index],10,7,&lv_font_montserrat_16,0xD2E8F8);
    symbol(card,w->condition,14,22,1);
    char temps[24];snprintf(temps,sizeof temps,"%d° / %d°",w->high,w->low);
    txt(card,temps,10,70,&lv_font_montserrat_16,0xFFFFFF);
}
void lf_weather_visual_clear(void)
{
    scrollbox=NULL;
    last_known=false;
    memset(&last,0,sizeof last);
}
void lf_weather_visual_build(lv_obj_t *parent)
{
    lf_weather_visual_clear();
    lv_obj_set_style_bg_color(parent,lv_color_hex(0x041225),0);
    lv_obj_set_style_bg_grad_color(parent,lv_color_hex(0x173F66),0);
    lv_obj_set_style_bg_grad_dir(parent,LV_GRAD_DIR_VER,0);
    scrollbox=lv_obj_create(parent);
    lv_obj_set_size(scrollbox,350,309);
    lv_obj_align(scrollbox,LV_ALIGN_CENTER,0,0);
    lv_obj_set_style_bg_color(scrollbox,lv_color_hex(0x0B1E35),0);
    lv_obj_set_style_border_width(scrollbox,2,0);
    lv_obj_set_style_border_color(scrollbox,lv_color_hex(0x325D88),0);
    lv_obj_set_style_radius(scrollbox,27,0);
    lv_obj_set_style_pad_all(scrollbox,5,0);
    lv_obj_set_scroll_dir(scrollbox,LV_DIR_VER);
    lv_obj_set_scrollbar_mode(scrollbox,LV_SCROLLBAR_MODE_AUTO);
    lv_obj_add_flag(scrollbox,LV_OBJ_FLAG_SCROLL_MOMENTUM);
    /* Let horizontal Weather exit gestures reach the owning screen.
       Vertical gestures remain scrollable inside the forecast. */
    lv_obj_add_flag(scrollbox,LV_OBJ_FLAG_EVENT_BUBBLE);
    lv_obj_add_flag(scrollbox,LV_OBJ_FLAG_GESTURE_BUBBLE);
    lv_obj_remove_flag(scrollbox,LV_OBJ_FLAG_SCROLL_CHAIN);
}
void lf_weather_visual_update(const lf_weather_view_t *w)
{
    if(!scrollbox || !w) return;
    if(last_known && memcmp(w,&last,sizeof last)==0) return;
    int previous_scroll=lv_obj_get_scroll_y(scrollbox);
    last=*w; last_known=true;
    lv_obj_clean(scrollbox);
    if(!w->valid) {
        lv_obj_t *p=panel(scrollbox,324,225,0x102943,22);
        lv_obj_set_pos(p,5,5);
        symbol(p,3,99,18,2);
        txt(p,"Waiting for weather",20,154,&lv_font_montserrat_24,0xECF5FF);
        txt(p,"Set location and enable weather",13,193,&lv_font_montserrat_16,0xB0CBE5);
        return;
    }
    lv_obj_t *hero=panel(scrollbox,324,213,0x173D67,23);
    lv_obj_set_pos(hero,5,5);
    lv_obj_set_style_bg_grad_color(hero,lv_color_hex(0x29679D),0);
    lv_obj_set_style_bg_grad_dir(hero,LV_GRAD_DIR_VER,0);
    txt(hero,w->location,20,10,&lv_font_montserrat_20,0xE0EEFA);
    symbol(hero,w->condition,12,42,2);
    char line[28];
    snprintf(line,sizeof line,"%d°",w->temperature);
    txt(hero,line,160,69,&lv_font_montserrat_48,0xFFFFFF);
    txt(hero,condition_name(w->condition),160,132,&lv_font_montserrat_18,0xEDF8FF);
    snprintf(line,sizeof line,"Feels like %d°",w->feels_like);
    txt(hero,line,160,159,&lv_font_montserrat_16,0xCBE1F7);

    lv_obj_t *stats=panel(scrollbox,324,69,0x112940,19);
    lv_obj_set_pos(stats,5,228);
    snprintf(line,sizeof line,"%d%%",w->humidity_pct);
    txt(stats,line,14,7,&lv_font_montserrat_24,0xF4FAFF);
    txt(stats,"HUMID",14,40,&lv_font_montserrat_12,0xA9CADC);
    snprintf(line,sizeof line,"%d km/h",w->wind_kmh);
    txt(stats,line,119,9,&lv_font_montserrat_20,0xF4FAFF);
    txt(stats,"WIND",135,40,&lv_font_montserrat_12,0xA9CADC);
    snprintf(line,sizeof line,"%d.%d mm",w->precipitation_tenths_mm/10,
             w->precipitation_tenths_mm%10);
    txt(stats,line,234,9,&lv_font_montserrat_20,0xF4FAFF);
    txt(stats,"PRECIP",241,40,&lv_font_montserrat_12,0xA9CADC);

    txt(scrollbox,"NEXT HOURS",15,309,&lv_font_montserrat_20,0xF0F8FF);
    lv_obj_t *hour_row=panel(scrollbox,329,111,0x0B1E35,0);
    lv_obj_set_pos(hour_row,5,341);
    for(unsigned i=0;i<w->hour_count && i<LF_WEATHER_HOURS;i++)
        forecast_hour(hour_row,&w->hourly[i],(int)i);
    txt(scrollbox,"3-DAY FORECAST",15,468,&lv_font_montserrat_20,0xF0F8FF);
    lv_obj_t *day_row=panel(scrollbox,329,100,0x0B1E35,0);
    lv_obj_set_pos(day_row,5,500);
    for(unsigned i=0;i<w->day_count && i<LF_WEATHER_DAYS;i++)
        forecast_day(day_row,&w->daily[i],(int)i);
    lv_obj_update_layout(scrollbox);
    if(previous_scroll>0) lv_obj_scroll_to_y(scrollbox,previous_scroll,LV_ANIM_OFF);
}
