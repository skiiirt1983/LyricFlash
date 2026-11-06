#include "basic_clock.h"
#include "clock_fonts.h"
#include <stdint.h>
#include <string.h>
#include "basic_open_face.h"
#include <stdio.h>

/* Clock ring deliberately disabled: segmented arc rendering causes navigation lag. */
static const lv_font_t *time_font(const lf_settings_t *p)
{
    if(p->basic_font_style>=24){
        const lv_font_t *converted=lf_clock_custom_font(p->basic_font_style);
        if(converted)return converted;
    }
    return &lv_font_montserrat_48;
}
static uint32_t basic_digit_height(uint8_t v)
{
    /* 0=22px, 60=74px, 100=104px, 200=136px. More range
       without painting outside the 466px round screen. */
    return v<=60?22U+52U*v/60U:
           v<=100?74U+30U*(v-60U)/40U:
                  104U+32U*(v-100U)/100U;
}
static uint32_t basic_digit_cell(uint8_t v)
{
    return v<=60?2U+5U*v/60U:
           v<=100?7U+2U*(v-60U)/40U:
                  9U+2U*(v-100U)/100U;
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
    /* Every 24-row LVGL partial render previously queued the entire custom
       numeral stencil, including rectangles outside the drawable strip.
       Clip *before* creating draw descriptors/tasks; safe for all original
       seven-segment, bitmap and open-font styles. The input x/y coordinates
       are absolute (lv_obj_get_coords), like layer->_clip_area. */
    const lv_area_t *clip=&layer->_clip_area;
    if(x>clip->x2 || y>clip->y2 ||
       (int64_t)x+w-1<clip->x1 || (int64_t)y+h-1<clip->y1)return;
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
/* These fifteen additional designs are generated from original numeral
 * geometry, not loaded commercial font files. Their silhouettes, thickness,
 * shearing, borders and ornamentation differ on the actual AMOLED. */
static void styled_digit(lv_layer_t *layer,int x,int y,int cell,char c,
                         uint32_t color,uint8_t style)
{
    if(c<'0'||c>'9'){
        pixel_digit(layer,x,y,cell,c,color,false);
        return;
    }
    const uint8_t *rows=grid_digits[c-'0'];
    for(int row=0;row<7;row++)for(int col=0;col<5;col++){
        if(!((rows[row]>>(4-col))&1))continue;
        if(style==16 && col==2 && (row==1||row==4))continue;
        int skew=(style==11 || style==12)?(6-row)*cell/3:0;
        if(style==5)skew=(row>3?(row-3):-row)*cell/5; /* Deco */
        int jitter=style==10?(((row*7+col*5)%3)-1)*cell/3:0;
        int px=x+col*cell+skew+jitter;
        int py=y+row*cell;
        int w=cell,h=cell,r=0;
        if(style==6){r=cell/2;w=cell+1;}
        if(style==8){w=cell+2;h=cell+1;}
        if(style==9){h=cell-1;}
        if(style==11){h=cell-2;r=1;}
        if(style==14){w=cell-1;h=cell-1;}
        if(style==18){w=cell-1;h=cell-1;r=cell/2;}
        if(style==19){w=cell/3+1;h=cell-1;}
        if(style==13||style==15){
            draw_bar(layer,px+2,py+2,w,h,style==13?0x394855:0x254866,1);
        }
        if(style==17 && (row==0||row==6)){
            draw_bar(layer,px-1,py+cell-2,cell+2,2,color,0);
        }
        draw_bar(layer,px,py,w,h,color,r);
        if(style==7 && row<3)
            draw_bar(layer,px,py,w,1,0xF9DD8E,0);
        if(style==9 && row==0)
            draw_bar(layer,px+cell/2,py-cell/2,2,cell/2,color,0);
        if(style==15)
            draw_bar(layer,px+1,py+1,w>2?w-2:1,h>2?h-2:1,0xD2FAFF,1);
    }
}

/* Genuine non-LCD numeral letterforms, derived from open-licensed serif,
 * high-contrast display and rounded families. Styles with names like Deco,
 * Vintage and Rounded now use actual curved/serif digit skeletons instead
 * of perturbations of the SAME Retro 8 pixel grid. */
static int open_face_family(uint8_t style)
{
    switch(style){
    case 3:case 9:case 11:case 13:case 19:return 0; /* EB Garamond */
    case 5:case 7:case 8:case 16:case 17:return 1; /* display bold */
    case 6:case 10:case 14:case 15:return 2;       /* rounded */
    case 20:return 3; /* Cabin Bold */
    case 21:return 4; /* Comic Neue Bold */
    case 22:return 5; /* Noto Serif Display black condensed italic */
    case 23:return 6; /* Go Mono */
    default:return -1;
    }
}
static void open_face_digit(lv_layer_t *layer,int x,int y,int scale,
                            char c,uint32_t rgb,int family,uint8_t style)
{
    if(c==':'){
        draw_bar(layer,x+7*scale,y+6*scale,scale+1,scale+1,rgb,scale/2);
        draw_bar(layer,x+7*scale,y+15*scale,scale+1,scale+1,rgb,scale/2);
        return;
    }
    if(c<'0'||c>'9')return;
    for(unsigned row=0;row<LF_OPEN_FACE_H;row++){
        uint16_t bits=lf_open_face_row((unsigned)family,(unsigned)(c-'0'),row);
        /* Stencil and geometric variants intentionally interrupt strokes,
           but their underlying number contour remains conventional. */
        if(style==16 && (row==7 || row==14))bits=0;
        for(int col=0;col<15;){
            if(!(bits&(1U<<(14-col)))){col++;continue;}
            int end=col+1;
            while(end<15 && (bits&(1U<<(14-end))))end++;
            int skew=style==11?(int)(20U-row)*scale/7:
                     style==10?(int)(20U-row)*scale/12:0;
            int xx=x+col*scale+skew,yy=y+(int)row*scale;
            int w=(end-col)*scale,h=scale;
            if(style==7 || style==13 || style==15)
                draw_bar(layer,xx+2,yy+2,w,h,
                         style==15?0x246DA7:0x2D4659,1);
            if(style==8)w+=1;
            if(style==17)w+=scale; /* bold Gothic strokes, even with empty outer glyph rows */ /* bold block display */
            if(style==9 || style==14)w=w>1?w-1:1;
            if(style==19){w=w>2?w-2:1;h=h>1?h-1:1;xx++;}
            if(style==17&&(row==0 || row==20))
                draw_bar(layer,xx-1,yy+h-1,w+2,1,rgb,0);
            draw_bar(layer,xx,yy,w,h,rgb,
                     (style==6 || style==10)?scale/2:0);
            col=end;
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
    if(p->basic_font_style==1 || p->basic_font_style==12){
        const int h=(int)basic_digit_height(p->basic_font_size);
        const int w=h*54/100,gap=5+(int)p->basic_font_size/12;
        const int total=length*w+(length-1)*gap;
        const int start_x=center_x-total/2,start_y=center_y-h/2;
        const int thick=p->basic_font_style==12?h/6:h/9;
        for(int i=0;i<length;i++)
            segment_digit(layer,start_x+i*(w+gap),start_y,w,h,thick,
                          custom_time[i],p->face_time_color[5]);
    }else if(open_face_family(p->basic_font_style)>=0){
        const int cell=p->basic_font_size<30?1:
            p->basic_font_size<75?2:
            p->basic_font_size<125?3:
            p->basic_font_size<175?4:5;
        const int w=15*cell,gap=cell;
        const int total=length*w+(length-1)*gap;
        const int start_x=center_x-total/2,start_y=center_y-(21*cell)/2;
        const int family=open_face_family(p->basic_font_style);
        for(int i=0;i<length;i++)
            open_face_digit(layer,start_x+i*(w+gap),start_y,cell,
                            custom_time[i],p->face_time_color[5],family,
                            p->basic_font_style);
    }else{
        /* Only Digital 7, Retro 8, Roadster and Dot Matrix use synthetic
           segment/pixel styles. Ordinary typography is no longer LCD. */
        const int cell=(int)basic_digit_cell(p->basic_font_size);
        const int w=5*cell,gap=2*cell,total=length*w+(length-1)*gap;
        const int start_x=center_x-total/2,start_y=center_y-7*cell/2;
        for(int i=0;i<length;i++){
            if(p->basic_font_style>=5)
                styled_digit(layer,start_x+i*(w+gap),start_y,cell,
                             custom_time[i],p->face_time_color[5],p->basic_font_style);
            else
                pixel_digit(layer,start_x+i*(w+gap),start_y,cell,
                            custom_time[i],p->face_time_color[5],false);
        }
    }
}

lv_obj_t *lf_basic_clock_render(lv_obj_t *parent, const lf_settings_t *p)
{
    lv_obj_set_style_bg_color(parent, lv_color_hex(p->basic_background), 0);
    lv_obj_t *time=lv_label_create(parent);
    lv_label_set_text(time,"--:--");
    lv_obj_set_width(time,346);
    lv_label_set_long_mode(time,LV_LABEL_LONG_DOT);
    lv_obj_set_style_text_font(time,time_font(p),0);
    /* 0% is genuinely small, 200% large. 0.8.104: pick a natively generated
       font size instead of a scale transform. A scaled label is re-rendered
       into an offscreen ARGB layer for every strip it touches, every frame,
       which made the Clock page and pager swipes crawl. */
    int32_t zoom=256;
    if(p->basic_font_style==0 || p->basic_font_style==4 || p->basic_font_style>=24){
        zoom=96+(int32_t)p->basic_font_size*416/200;
        const int base_px=p->basic_font_style>=24?72:48;
        const uint8_t family=p->basic_font_style>=24?p->basic_font_style:31;
        const lv_font_t *native=lf_clock_custom_font_px(family,(int)(base_px*zoom/256));
        if(native)lv_obj_set_style_text_font(time,native,0);
        int32_t width=346*zoom/256;
        if(width<346)width=346;
        if(width>466)width=466;
        lv_obj_set_width(time,width);
    }
    const int letter_spacing[5]={0,3,1,6,2};
    lv_obj_set_style_text_letter_space(time,
         letter_spacing[p->basic_font_style<5?p->basic_font_style:0]*zoom/256,0);
    lv_obj_set_style_text_color(time,lv_color_hex(p->face_time_color[5]),0);
    lv_obj_set_style_text_align(time,LV_TEXT_ALIGN_CENTER,0);
    lv_obj_align(time,LV_ALIGN_CENTER,p->basic_time_x,p->basic_time_y);
    if(p->basic_font_style!=0 && p->basic_font_style!=4 && p->basic_font_style<24){
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
