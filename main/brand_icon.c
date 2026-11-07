#include "brand_icon.h"
#include <stdint.h>
#include <stddef.h>
/* Pixel source: LyricFlash_Brand_Assets.zip / LyricFlash_Icon_128.png,
   downsampled to 48x48 with a 16-color adaptive palette and enlarged to
   80x80. Preserves the original blue gradient, white cloud, gold bolt,
   music note and rounded-square icon instead of reconstructing geometry. */
static const char packed64[]=
    "/////rmKqYiIhmZmZmZmZmaIhm3v///////+yIipmIiIZmZmZmZmZoiIiYZsv/////64iaqpmYiGZmZVVVVWZmiJmXeYjv//"
    "//1od3p5iIZmZVVTMzMzVWaImXl6iM///7aHeqmYhmZlVVM6606zNVZmiJeqd47//oiaqqmIZmVTMzPiAAAuU1VmaImaqnjv"
    "6ImqepiGZlUzfU4AAAAA5TVWZoiaqqjP5oeqeYhmZVNuISIAAAAADjNVVmiHqqqOtneqeIZmVTWwAAAAAAAAAmM1VmaJeqqL"
    "iHqqmIZlUz4AAAAAAAAAAiuzVWaIeqybZ6qpmGZlPeIAAAAAAAAAAAAuNVZomqx9iaqqiGZT4gAAAAAAAAAAAAACY1Zomsqd"
    "iaqpiGY6IAAAAAAAAAAAAAAA41Zoiqp9iaqphmU+AAAAAAAAAAAAAAAA41ZoiqqtiayphmU+AAAAAAAAAAAAAAAA41Zoiax9"
    "iayphmY7IAAAAAAAAAAAAAACY1Zoisp9iarJhmZT4gACEAKyIAACIAArNVZoisytiarKiGZVO+vqvu4R7rv3rr6DVWZomsqt"
    "iazKmGZVUzMzXxERER+zMzM1VWZomqp9iazKmGZlVVMz4gERER8zMzNVVmZol6qth6rKmIZmVVU18REREfQzMzM1VmZomXqt"
    "iaqqmIZmVVM+IREREvve7v41VmZoh3qth6zMmIZmVVNfEREREiIhES81VmZoiXqtiqzMmYZmVVPiEREREREREfU1VWZoiXqt"
    "iszMmYZmVTXxERERERERH6NVVWZmiXqtiszMmYZmVT4hERERERERLjNVVVZmaJqtiszKmYZmU18RERIhERES8zVVVVVWaJqt"
    "iszKmIZmU+8v/+7xEREfUzNVVTNVZoqtiqzKmYZmVYuVMzshERH2MzMzMzVVZoqtl6zMmYhmVTMzMz8RERKzMzM1fu9zZomt"
    "mszMqYhmZVVTM18RES4zM07///9zVoetmszMqYiGZlVVM+IREfMzM+//7k5zVoetmszMypiGZmVVM/ERH2MzM+5DMzvTVoqt"
    "eszMypmIgmVVNuER+zMzM/UzMzvTZoqres3MzKmIZmVVPiES4zVTM/UzMzuzZoqrqs3MzKmYhmVTPxEfM1VVM/UzMz6jZpqr"
    "rN3czMqZhmZTYhH1NVVTM/UzT/9zaJqrnN3dzMqZiGVT4R+DVVUzR/U1/++DaKyr2t3dzMypiGVT8S41VVNf//U0//9Via2u"
    "x93d3MzKiGY4IeNVVVPv7/MzfrVWis2uus3d3czJmGU+L1NWZlOP/0NVMzVoesye7X3d3czKmGVf9jVmZmU1ZTVVZmaJrMqe"
    "/qzd3dzMmIV/o1ZmZmZlVWZmZoiazMrv/7jd3d3MqYWLNWaIiIhmZmiIh5qszZv///uc3d3cyoZVZoiXd3eYiJd6qqzNyb7/"
    "///srd3d3KmIiHqqzMyqqqzM3d3azv/////+vMzdzMqZl6qszMzMqszM3czL7///////7uu7u73d3bu7u7u7u7u7vb7v////";
static const uint16_t palette[16]={
    0xffff,0xf6ca,0x536d,0x0255,0x096c,0x0190,0x012d,0x00eb,
    0x00ec,0x00cb,0x00ca,0x00c9,0x00ca,0x00a9,0x0065,0x0000
};
#define LF_BRAND_SOURCE 48
#define LF_BRAND_SIZE 80
static uint16_t pixels[LF_BRAND_SIZE*LF_BRAND_SIZE];
static bool pixels_ready;
static unsigned char unpacked[LF_BRAND_SOURCE*LF_BRAND_SOURCE];
static int digit(unsigned char c){
    if(c>='A'&&c<='Z')return c-'A';
    if(c>='a'&&c<='z')return c-'a'+26;
    if(c>='0'&&c<='9')return c-'0'+52;
    return c=='+'?62:c=='/'?63:-1;
}
static void prepare(void){
    if(pixels_ready)return;
    unsigned char packed[LF_BRAND_SOURCE*LF_BRAND_SOURCE/2];
    size_t count=0;uint32_t bits=0;int nbits=0;
    for(const unsigned char *s=(const unsigned char *)packed64;
        *s&&count<sizeof packed;s++){
        const int v=digit(*s);
        if(v<0)continue;
        bits=(bits<<6)|(unsigned)v;nbits+=6;
        if(nbits>=8){
            nbits-=8;
            packed[count++]=(unsigned char)(bits>>nbits);
            bits&=(1U<<nbits)-1U;
        }
    }
    if(count!=sizeof packed)return;
    for(unsigned i=0;i<LF_BRAND_SOURCE*LF_BRAND_SOURCE;i++){
        const unsigned char pair=packed[i/2];
        unpacked[i]=(unsigned char)(i%2?pair&15U:pair>>4);
    }
    for(unsigned y=0;y<LF_BRAND_SIZE;y++){
        const unsigned sy=y*(LF_BRAND_SOURCE-1)/(LF_BRAND_SIZE-1);
        for(unsigned x=0;x<LF_BRAND_SIZE;x++){
            const unsigned sx=x*(LF_BRAND_SOURCE-1)/(LF_BRAND_SIZE-1);
            pixels[y*LF_BRAND_SIZE+x]=palette[unpacked[sy*LF_BRAND_SOURCE+sx]];
        }
    }
    pixels_ready=true;
}
lv_obj_t *lf_brand_icon_create(lv_obj_t *parent){
    prepare();
    if(!pixels_ready)return NULL;
    lv_obj_t *canvas=lv_canvas_create(parent);
    lv_canvas_set_buffer(canvas,pixels,LF_BRAND_SIZE,LF_BRAND_SIZE,LV_COLOR_FORMAT_RGB565);
    lv_obj_set_size(canvas,LF_BRAND_SIZE,LF_BRAND_SIZE);
    lv_obj_center(canvas);
    lv_obj_remove_flag(canvas,LV_OBJ_FLAG_CLICKABLE);
    lv_obj_remove_flag(canvas,LV_OBJ_FLAG_SCROLLABLE);
    return canvas;
}
