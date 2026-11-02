#include "basic_clock.h"
#include <stdint.h>

static uint32_t ring_colour(const lf_settings_t *p,int segment);

/* A single ring draw widget replaces 30 LVGL arc objects.
   Cached pager scenes keep this widget until Basic Clock settings change. */
static void draw_basic_ring(lv_event_t *event)
{
    lv_obj_t *ring=lv_event_get_target(event);
    lv_layer_t *layer=lv_event_get_layer(event);
    const lf_settings_t *prefs=lv_event_get_user_data(event);
    if(!ring || !layer || !prefs) return;
    lv_area_t bounds;
    lv_obj_get_coords(ring,&bounds);
    int count=prefs->basic_ring_mode==0 ? 1 : 30;
    for(int i=0;i<count;i++) {
        lv_draw_arc_dsc_t arc;
        lv_draw_arc_dsc_init(&arc);
        arc.center.x=(bounds.x1+bounds.x2)/2;
        arc.center.y=(bounds.y1+bounds.y2)/2;
        arc.radius=220;
        arc.width=13;
        arc.start_angle=i*360/count;
        arc.end_angle=(i+1)*360/count;
        arc.color=lv_color_hex(ring_colour(prefs,i*60/count));
        arc.opa=LV_OPA_COVER;
        arc.rounded=0;
        lv_draw_arc(layer,&arc);
    }
}

static uint32_t lerp(uint32_t a, uint32_t b, unsigned x, unsigned maximum)
{
    if (!maximum) return a;
    unsigned inv = maximum - x;
    unsigned r = (((a >> 16) & 255U)*inv + ((b >> 16) & 255U)*x) / maximum;
    unsigned g = (((a >> 8) & 255U)*inv + ((b >> 8) & 255U)*x) / maximum;
    unsigned bl = ((a & 255U)*inv + (b & 255U)*x) / maximum;
    return (r << 16) | (g << 8) | bl;
}
static uint32_t ring_colour(const lf_settings_t *p, int segment)
{
    uint32_t a=p->basic_ring_a, b=p->basic_ring_b, c=p->basic_ring_c;
    if(p->basic_ring_mode == 0) return a;
    if(p->basic_ring_mode == 1)
        return segment < 30 ? lerp(a,b,segment,30) : lerp(b,a,segment-30,30);
    if(segment < 20) return lerp(a,b,segment,20);
    if(segment < 40) return lerp(b,c,segment-20,20);
    return lerp(c,a,segment-40,20);
}
static const lv_font_t *time_font(const lf_settings_t *p)
{
    /* Five onboard typography presets with separately adjustable sizes.
       Different font families can be added later as embedded LVGL assets. */
    uint8_t v=p->basic_font_size;
    switch(p->basic_font_style) {
        case 1: return v>70 ? &lv_font_montserrat_48 : &lv_font_montserrat_40;
        case 2: return v>70 ? &lv_font_montserrat_40 : &lv_font_montserrat_32;
        case 3: return v>70 ? &lv_font_montserrat_48 : &lv_font_montserrat_36;
        case 4: return v>70 ? &lv_font_montserrat_40 : &lv_font_montserrat_28;
        default: return v>80 ? &lv_font_montserrat_48 :
                        v>45 ? &lv_font_montserrat_40 : &lv_font_montserrat_32;
    }
}
lv_obj_t *lf_basic_clock_render(lv_obj_t *parent, const lf_settings_t *p)
{
    lv_obj_set_style_bg_color(parent, lv_color_hex(p->basic_background), 0);
    /* Persistent decorative ring: no per-segment LVGL objects. */
    lv_obj_t *ring=lv_obj_create(parent);
    lv_obj_set_size(ring,441,441);
    lv_obj_align(ring,LV_ALIGN_CENTER,0,0);
    lv_obj_set_style_bg_opa(ring,LV_OPA_TRANSP,0);
    lv_obj_set_style_border_width(ring,0,0);
    lv_obj_set_style_pad_all(ring,0,0);
    lv_obj_remove_flag(ring,LV_OBJ_FLAG_CLICKABLE);
    lv_obj_remove_flag(ring,LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(ring,draw_basic_ring,LV_EVENT_DRAW_MAIN,(void*)p);
    lv_obj_t *time=lv_label_create(parent);
    lv_label_set_text(time,"--:--");
    lv_obj_set_width(time,346);
    lv_label_set_long_mode(time,LV_LABEL_LONG_DOT);
    lv_obj_set_style_text_font(time,time_font(p),0);
    const int letter_spacing[5]={0,3,1,6,2};
    lv_obj_set_style_text_letter_space(time,letter_spacing[p->basic_font_style],0);
    lv_obj_set_style_text_color(time,lv_color_hex(p->basic_foreground),0);
    lv_obj_set_style_text_align(time,LV_TEXT_ALIGN_CENTER,0);
    lv_obj_align(time,LV_ALIGN_CENTER,0,-6);
    return time;
}
