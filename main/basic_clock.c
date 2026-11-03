#include "basic_clock.h"
#include <stdint.h>
#include <string.h>

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
/* Static ring as ONE LVGL object, not 30 separate arc widgets. Each segment
   is drawn directly into the current LVGL layer, preserving the same RGB
   gradient and 13px ring width. The retained clock scene means it is built
   once and only repainted when the display genuinely needs those pixels. */
static void basic_ring_draw(lv_event_t *e)
{
    const lf_settings_t *p=(const lf_settings_t *)lv_event_get_user_data(e);
    lv_obj_t *ring=lv_event_get_target(e);
    lv_layer_t *layer=lv_event_get_layer(e);
    if (!p || !ring || !layer) return;
    lv_area_t area;
    lv_obj_get_coords(ring,&area);
    const int segment_count=p->basic_ring_mode==0 ? 1 : 30;
    for (int i=0;i<segment_count;i++) {
        lv_draw_arc_dsc_t dsc;
        lv_draw_arc_dsc_init(&dsc);
        dsc.center.x=(area.x1+area.x2)/2;
        dsc.center.y=(area.y1+area.y2)/2;
        dsc.radius=220;
        dsc.width=13;
        dsc.start_angle=i*360/segment_count;
        dsc.end_angle=(i+1)*360/segment_count;
        dsc.color=lv_color_hex(ring_colour(p,i*60/segment_count));
        dsc.opa=LV_OPA_COVER;
        dsc.rounded=0;
        lv_draw_arc(layer,&dsc);
    }
}
/* Original vector numerals: no third-party or proprietary font assets.
   Digital 7 uses seven glowing segments; Retro 8 uses a blocky 5x7 bitmap;
   Vintage uses thin serif-edged 5x7 strokes. They repaint at most once/min. */
static lv_obj_t *custom_time_layer;
static char custom_time[8]="--:--";
void lf_basic_clock_clear(void){custom_time_layer=NULL;strcpy(custom_time,"--:--");}
void lf_basic_clock_update_time(const char *value)
{
    if(!value||!custom_time_layer)return;
    if(strcmp(custom_time,value)!=0){
        snprintf(custom_time,sizeof custom_time,"%.5s",value);
        lv_obj_invalidate(custom_time_layer);
    }
}
static void draw_bar(lv_layer_t *layer,int x,int y,int w,int h,
                     uint32_t rgb,int radius)
{
    if(w<1||h<1)return;
    lv_draw_rect_dsc_t d;
    lv_draw_rect_dsc_init(&d);
    d.bg_opa=LV_OPA_COVER;
    d.bg_color=lv_color_hex(rgb);
    d.border_opa=LV_OPA_TRANSP;
    d.radius=radius;
    lv_area_t a={.x1=x,.y1=y,.x2=x+w-1,.y2=y+h-1};
    lv_draw_rect(layer,&d,&a);
}
static uint8_t segments(char c)
{
    const uint8_t masks[10]={119,36,93,109,46,107,123,37,127,111};
    if(c>='0'&&c<='9')return masks[c-'0'];
    if(c=='-')return 8;
    return 0;
}
static void segment_digit(lv_layer_t *layer,int x,int y,int w,int h,
                          int thickness,char c,uint32_t color)
{
    if(c==':'){
        draw_bar(layer,x+w/2-thickness/2,y+h/3,thickness,thickness,color,2);
        draw_bar(layer,x+w/2-thickness/2,y+2*h/3,thickness,thickness,color,2);
        return;
    }
    const uint8_t m=segments(c),half=h/2,interior=w-thickness;
    if(m&1) draw_bar(layer,x+thickness/2,y,interior,thickness,color,thickness/3);
    if(m&2) draw_bar(layer,x,y+thickness/2,thickness,half-thickness,color,thickness/3);
    if(m&4) draw_bar(layer,x+w-thickness,y+thickness/2,thickness,half-thickness,color,thickness/3);
    if(m&8) draw_bar(layer,x+thickness/2,y+half-thickness/2,interior,thickness,color,thickness/3);
    if(m&16)draw_bar(layer,x,y+half+thickness/2,thickness,half-thickness,color,thickness/3);
    if(m&32)draw_bar(layer,x+w-thickness,y+half+thickness/2,thickness,half-thickness,color,thickness/3);
    if(m&64)draw_bar(layer,x+thickness/2,y+h-thickness,interior,thickness,color,thickness/3);
}
static const uint8_t grid_digits[10][7]={
    {14,17,19,21,25,17,14}, /* 0 */
    {4,12,4,4,4,4,14},     /* 1 */
    {14,17,1,2,4,8,31},    /* 2 */
    {30,1,1,14,1,1,30},    /* 3 */
    {2,6,10,18,31,2,2},   /* 4 */
    {31,16,16,30,1,1,30}, /* 5 */
    {14,16,16,30,17,17,14},/* 6 */
    {31,1,2,4,8,8,8},     /* 7 */
    {14,17,17,14,17,17,14},/* 8 */
    {14,17,17,15,1,1,14} /* 9 */
};
static void pixel_digit(lv_layer_t *layer,int x,int y,int cell,char c,
                        uint32_t color,bool serif)
{
    if(c==':'){
        draw_bar(layer,x+2*cell,y+2*cell,cell,cell,color,0);
        draw_bar(layer,x+2*cell,y+4*cell,cell,cell,color,0);
        return;
    }
    if(c=='-'){draw_bar(layer,x+cell,y+3*cell,3*cell,cell,color,0);return;}
    if(c<'0'||c>'9')return;
    const uint8_t *rows=grid_digits[c-'0'];
    for(int row=0;row<7;row++)for(int col=0;col<5;col++){
        if((rows[row]>>(4-col))&1){
            /* Vintage numerals have narrow strokes with small serif feet,
               distinct from Retro 8's square solid pixels. */
            const int inset=serif?cell/4:0;
            draw_bar(layer,x+col*cell+inset,y+row*cell,cell-2*inset,
                     serif?cell-1:cell,color,serif?1:0);
            if(serif&&(row==0||row==6))
                draw_bar(layer,x+col*cell-1,y+row*cell+cell-2,cell+2,2,color,0);
        }
    }
}
static void basic_digits_draw(lv_event_t *e)
{
    const lf_settings_t *p=(const lf_settings_t *)lv_event_get_user_data(e);
    lv_obj_t *obj=lv_event_get_target(e);
    lv_layer_t *layer=lv_event_get_layer(e);
    if(!p||!obj||!layer)return;
    lv_area_t box;lv_obj_get_coords(obj,&box);
    const int center_x=(box.x1+box.x2)/2,center_y=(box.y1+box.y2)/2;
    const int length=5;
    if(p->basic_font_style==1){
        const int h=44+(int)p->basic_font_size/2;
        const int w=h*54/100,gap=5+(int)p->basic_font_size/12;
        const int total=length*w+(length-1)*gap;
        const int start_x=center_x-total/2,start_y=center_y-h/2;
        const int thick=h/9;
        for(int i=0;i<length;i++)
            segment_digit(layer,start_x+i*(w+gap),start_y,w,h,thick,
                          custom_time[i],p->basic_foreground);
    }else{
        const int cell=4+(int)p->basic_font_size/18;
        const int w=5*cell,gap=2*cell,total=length*w+(length-1)*gap;
        const int start_x=center_x-total/2,start_y=center_y-7*cell/2;
        for(int i=0;i<length;i++)
            pixel_digit(layer,start_x+i*(w+gap),start_y,cell,
                        custom_time[i],p->basic_foreground,p->basic_font_style==3);
    }
}

lv_obj_t *lf_basic_clock_render(lv_obj_t *parent, const lf_settings_t *p)
{
    lv_obj_set_style_bg_color(parent, lv_color_hex(p->basic_background), 0);
    lv_obj_t *ring=lv_obj_create(parent);
    lv_obj_set_size(ring,441,441);
    lv_obj_align(ring,LV_ALIGN_CENTER,0,0);
    lv_obj_set_style_bg_opa(ring,LV_OPA_TRANSP,0);
    lv_obj_set_style_border_width(ring,0,0);
    lv_obj_set_style_pad_all(ring,0,0);
    lv_obj_remove_flag(ring,LV_OBJ_FLAG_CLICKABLE);
    lv_obj_remove_flag(ring,LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(ring,basic_ring_draw,LV_EVENT_DRAW_MAIN,(void *)p);
    lv_obj_t *time=lv_label_create(parent);
    lv_label_set_text(time,"--:--");
    lv_obj_set_width(time,346);
    lv_label_set_long_mode(time,LV_LABEL_LONG_DOT);
    lv_obj_set_style_text_font(time,time_font(p),0);
    const int letter_spacing[5]={0,3,1,6,2};
    lv_obj_set_style_text_letter_space(time,letter_spacing[p->basic_font_style],0);
    lv_obj_set_style_text_color(time,lv_color_hex(p->basic_foreground),0);
    lv_obj_set_style_text_align(time,LV_TEXT_ALIGN_CENTER,0);
    lv_obj_align(time,LV_ALIGN_CENTER,p->basic_time_x,p->basic_time_y);
    if(p->basic_font_style>=1 && p->basic_font_style<=3){
        lv_obj_add_flag(time,LV_OBJ_FLAG_HIDDEN);
        custom_time_layer=lv_obj_create(parent);
        lv_obj_set_size(custom_time_layer,412,132);
        lv_obj_align(custom_time_layer,LV_ALIGN_CENTER,
                     p->basic_time_x,p->basic_time_y);
        lv_obj_set_style_bg_opa(custom_time_layer,LV_OPA_TRANSP,0);
        lv_obj_set_style_border_width(custom_time_layer,0,0);
        lv_obj_set_style_pad_all(custom_time_layer,0,0);
        lv_obj_remove_flag(custom_time_layer,LV_OBJ_FLAG_CLICKABLE);
        lv_obj_remove_flag(custom_time_layer,LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_add_event_cb(custom_time_layer,basic_digits_draw,
                            LV_EVENT_DRAW_MAIN,(void *)p);
    }else{
        custom_time_layer=NULL;
    }
    return time;
}
