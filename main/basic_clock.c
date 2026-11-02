#include "basic_clock.h"
#include <stdint.h>

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
    /* A solid ring requires one arc. For blended rings, 30 anti-aliased
       sections preserve a smooth appearance with half the prior objects. */
    const int segment_count = p->basic_ring_mode == 0 ? 1 : 30;
    for(int i=0;i<segment_count;i++) {
        lv_obj_t *arc=lv_arc_create(parent);
        lv_obj_set_size(arc, 441, 441);
        lv_obj_align(arc,LV_ALIGN_CENTER,0,0);
        lv_arc_set_bg_angles(arc,i*360/segment_count,(i+1)*360/segment_count);
        lv_obj_set_style_arc_color(arc,lv_color_hex(ring_colour(p,i*60/segment_count)),LV_PART_MAIN);
        lv_obj_set_style_arc_width(arc,13,LV_PART_MAIN);
        lv_obj_set_style_arc_width(arc,0,LV_PART_INDICATOR);
        lv_obj_set_style_bg_opa(arc,LV_OPA_TRANSP,0);
        lv_obj_set_style_border_width(arc,0,0);
        lv_obj_remove_flag(arc,LV_OBJ_FLAG_CLICKABLE);
        lv_obj_remove_flag(arc,LV_OBJ_FLAG_SCROLLABLE);
    }
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
