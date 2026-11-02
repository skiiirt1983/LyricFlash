#include "char_wheel.h"
#include <stdio.h>
#include <string.h>

static lv_obj_t *roller;
static lv_obj_t *entry;
static lv_obj_t *mode_button;
static char input[65];
static bool masked;
static unsigned max_chars, alphabet;
static void (*save_handler)(const char *);
static void (*back_handler)(void);

static const char *charsets[] = {
    "a\nb\nc\nd\ne\nf\ng\nh\ni\nj\nk\nl\nm\nn\no\np\nq\nr\ns\nt\nu\nv\nw\nx\ny\nz",
    "A\nB\nC\nD\nE\nF\nG\nH\nI\nJ\nK\nL\nM\nN\nO\nP\nQ\nR\nS\nT\nU\nV\nW\nX\nY\nZ",
    "0\n1\n2\n3\n4\n5\n6\n7\n8\n9",
    "!\n@\n#\n$\n%\n&\n*\n-\n_\n.\n?\n \n:"
};
static const char *mode_labels[] = {"abc", "ABC", "123", "#+= "};

static lv_obj_t *btn(lv_obj_t *parent,const char *caption,int x,int y,
                     int width,lv_event_cb_t cb)
{
    lv_obj_t *b=lv_button_create(parent);
    lv_obj_set_size(b,width,46);
    lv_obj_align(b,LV_ALIGN_CENTER,x,y);
    lv_obj_set_style_radius(b,18,0);
    lv_obj_set_style_bg_color(b,lv_color_hex(0x9BCFFA),0);
    lv_obj_set_style_bg_grad_color(b,lv_color_hex(0x578CC7),0);
    lv_obj_set_style_bg_grad_dir(b,LV_GRAD_DIR_VER,0);
    lv_obj_set_style_border_width(b,2,0);
    lv_obj_set_style_border_color(b,lv_color_hex(0xA3D7F7),0);
    lv_obj_set_style_transform_width(b,-4,LV_STATE_PRESSED);
    lv_obj_set_style_transform_height(b,-4,LV_STATE_PRESSED);
    lv_obj_set_style_bg_color(b,lv_color_hex(0xC9EDFF),LV_STATE_PRESSED);
    lv_obj_add_event_cb(b,cb,LV_EVENT_CLICKED,NULL);
    lv_obj_t *text=lv_label_create(b);
    lv_label_set_text(text,caption);
    lv_obj_set_style_text_font(text,&lv_font_montserrat_18,0);
    lv_obj_set_style_text_color(text,lv_color_hex(0x0D2234),0);
    lv_obj_center(text);
    return b;
}
static void refresh(void)
{
    if(!entry) return;
    if(masked) {
        char message[28];
        snprintf(message,sizeof message,"%u characters entered",(unsigned)strlen(input));
        lv_label_set_text(entry,message);
    } else lv_label_set_text(entry,input[0]?input:"(empty)");
}
static void add(lv_event_t *e)
{
    (void)e;
    char chosen[8]={0};
    lv_roller_get_selected_str(roller,chosen,sizeof chosen);
    size_t n=strlen(input);
    if(strlen(chosen)==1 && n<max_chars) {
        input[n]=chosen[0];
        input[n+1]=0;
        refresh();
    }
}
static void add_space(lv_event_t *e)
{
    (void)e;
    size_t n=strlen(input);
    if(n<max_chars){input[n]=' ';input[n+1]=0;refresh();}
}
static void erase(lv_event_t *e)
{
    (void)e;
    size_t n=strlen(input);
    if(n) input[n-1]=0;
    refresh();
}
static void mode_change(lv_event_t *e)
{
    (void)e;
    alphabet=(alphabet+1)%4;
    lv_roller_set_options(roller,charsets[alphabet],LV_ROLLER_MODE_INFINITE);
    lv_obj_t *caption=lv_obj_get_child(mode_button,0);
    if(caption) lv_label_set_text(caption,mode_labels[alphabet]);
}
static void save(lv_event_t *e)
{
    (void)e;
    if(save_handler) save_handler(input);
}
static void cancel(lv_event_t *e)
{
    (void)e;
    if(back_handler) back_handler();
}
void lf_charwheel_show(lv_obj_t *parent,const char *title,const char *initial,
                       bool password,unsigned max_length,
                       void (*on_save)(const char *),void (*on_cancel)(void))
{
    roller=NULL;
    entry=NULL;
    mode_button=NULL;
    alphabet=0;
    masked=password;
    max_chars=max_length<64?max_length:64;
    save_handler=on_save;
    back_handler=on_cancel;
    snprintf(input,sizeof input,"%s",initial?initial:"");
    input[max_chars]=0;
    lv_obj_t *heading=lv_label_create(parent);
    lv_label_set_text(heading,title);
    lv_obj_set_style_text_font(heading,&lv_font_montserrat_24,0);
    lv_obj_set_style_text_color(heading,lv_color_hex(0xB8DFFF),0);
    lv_obj_align(heading,LV_ALIGN_CENTER,0,-158);
    entry=lv_label_create(parent);
    lv_obj_set_width(entry,270);
    lv_label_set_long_mode(entry,LV_LABEL_LONG_DOT);
    lv_obj_set_style_text_font(entry,&lv_font_montserrat_18,0);
    lv_obj_set_style_text_align(entry,LV_TEXT_ALIGN_CENTER,0);
    lv_obj_set_style_text_color(entry,lv_color_hex(0xF0F7FF),0);
    lv_obj_align(entry,LV_ALIGN_CENTER,0,-109);
    refresh();
    roller=lv_roller_create(parent);
    lv_roller_set_options(roller,charsets[0],LV_ROLLER_MODE_INFINITE);
    lv_roller_set_visible_row_count(roller,3);
    lv_obj_set_size(roller,133,123);
    lv_obj_align(roller,LV_ALIGN_CENTER,0,-15);
    lv_obj_set_style_text_font(roller,&lv_font_montserrat_24,0);
    lv_obj_set_style_bg_color(roller,lv_color_hex(0x12293B),0);
    lv_obj_set_style_text_color(roller,lv_color_hex(0xEEF7FF),0);
    btn(parent,"ADD",-119,87,70,add);
    btn(parent,"SPACE",-40,87,75,add_space);
    btn(parent,"DEL",40,87,70,erase);
    mode_button=btn(parent,"abc",119,87,70,mode_change);
    btn(parent,"BACK",-71,164,117,cancel);
    btn(parent,"SAVE",71,164,117,save);
}
