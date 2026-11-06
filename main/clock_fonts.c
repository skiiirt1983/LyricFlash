#include "clock_fonts.h"
LV_FONT_DECLARE(lf_clock_font_kablammo);
LV_FONT_DECLARE(lf_clock_font_mystery_quest);
LV_FONT_DECLARE(lf_clock_font_moirai_one);
LV_FONT_DECLARE(lf_clock_font_rampart_one);
LV_FONT_DECLARE(lf_clock_font_hanalei);
LV_FONT_DECLARE(lf_clock_font_bitcount_grid_double);
LV_FONT_DECLARE(lf_clock_font_iceberg);
LV_FONT_DECLARE(lf_clock_font_montserrat);
LV_FONT_DECLARE(lf_clock_font_sancreek);
LV_FONT_DECLARE(lf_clock_font_modak);
LV_FONT_DECLARE(lf_clock_font_alumni_sans_collegiate_one);
LV_FONT_DECLARE(lf_clock_font_honk);
LV_FONT_DECLARE(lf_clock_font_nabla);
LV_FONT_DECLARE(lf_clock_font_carlito);
LV_FONT_DECLARE(lf_clock_font_tinos);
LV_FONT_DECLARE(lf_clock_font_asap);

const lv_font_t *lf_clock_custom_font(uint8_t style)
{
    switch(style) {
    case 24: return &lf_clock_font_kablammo;
    case 25: return &lf_clock_font_mystery_quest;
    case 26: return &lf_clock_font_moirai_one;
    case 27: return &lf_clock_font_rampart_one;
    case 28: return &lf_clock_font_hanalei;
    case 29: return &lf_clock_font_bitcount_grid_double;
    case 30: return &lf_clock_font_iceberg;
    case 31: return &lf_clock_font_montserrat;
    case 32: return &lf_clock_font_sancreek;
    case 33: return &lf_clock_font_modak;
    case 34: return &lf_clock_font_alumni_sans_collegiate_one;
    case 35: return &lf_clock_font_honk;
    case 36: return &lf_clock_font_nabla;
    case 37: return &lf_clock_font_carlito;
    case 38: return &lf_clock_font_tinos;
    case 39: return &lf_clock_font_asap;
    default: return NULL;
    }
}

LV_FONT_DECLARE(lf_clock_font_kablammo_40);
LV_FONT_DECLARE(lf_clock_font_kablammo_56);
LV_FONT_DECLARE(lf_clock_font_kablammo_96);
LV_FONT_DECLARE(lf_clock_font_kablammo_120);
LV_FONT_DECLARE(lf_clock_font_mystery_quest_40);
LV_FONT_DECLARE(lf_clock_font_mystery_quest_56);
LV_FONT_DECLARE(lf_clock_font_mystery_quest_96);
LV_FONT_DECLARE(lf_clock_font_mystery_quest_120);
LV_FONT_DECLARE(lf_clock_font_moirai_one_40);
LV_FONT_DECLARE(lf_clock_font_moirai_one_56);
LV_FONT_DECLARE(lf_clock_font_moirai_one_96);
LV_FONT_DECLARE(lf_clock_font_moirai_one_120);
LV_FONT_DECLARE(lf_clock_font_rampart_one_40);
LV_FONT_DECLARE(lf_clock_font_rampart_one_56);
LV_FONT_DECLARE(lf_clock_font_rampart_one_96);
LV_FONT_DECLARE(lf_clock_font_rampart_one_120);
LV_FONT_DECLARE(lf_clock_font_hanalei_40);
LV_FONT_DECLARE(lf_clock_font_hanalei_56);
LV_FONT_DECLARE(lf_clock_font_hanalei_96);
LV_FONT_DECLARE(lf_clock_font_hanalei_120);
LV_FONT_DECLARE(lf_clock_font_bitcount_grid_double_40);
LV_FONT_DECLARE(lf_clock_font_bitcount_grid_double_56);
LV_FONT_DECLARE(lf_clock_font_bitcount_grid_double_96);
LV_FONT_DECLARE(lf_clock_font_bitcount_grid_double_120);
LV_FONT_DECLARE(lf_clock_font_iceberg_40);
LV_FONT_DECLARE(lf_clock_font_iceberg_56);
LV_FONT_DECLARE(lf_clock_font_iceberg_96);
LV_FONT_DECLARE(lf_clock_font_iceberg_120);
LV_FONT_DECLARE(lf_clock_font_montserrat_40);
LV_FONT_DECLARE(lf_clock_font_montserrat_56);
LV_FONT_DECLARE(lf_clock_font_montserrat_96);
LV_FONT_DECLARE(lf_clock_font_montserrat_120);
LV_FONT_DECLARE(lf_clock_font_sancreek_40);
LV_FONT_DECLARE(lf_clock_font_sancreek_56);
LV_FONT_DECLARE(lf_clock_font_sancreek_96);
LV_FONT_DECLARE(lf_clock_font_sancreek_120);
LV_FONT_DECLARE(lf_clock_font_modak_40);
LV_FONT_DECLARE(lf_clock_font_modak_56);
LV_FONT_DECLARE(lf_clock_font_modak_96);
LV_FONT_DECLARE(lf_clock_font_modak_120);
LV_FONT_DECLARE(lf_clock_font_alumni_sans_collegiate_one_40);
LV_FONT_DECLARE(lf_clock_font_alumni_sans_collegiate_one_56);
LV_FONT_DECLARE(lf_clock_font_alumni_sans_collegiate_one_96);
LV_FONT_DECLARE(lf_clock_font_alumni_sans_collegiate_one_120);
LV_FONT_DECLARE(lf_clock_font_honk_40);
LV_FONT_DECLARE(lf_clock_font_honk_56);
LV_FONT_DECLARE(lf_clock_font_honk_96);
LV_FONT_DECLARE(lf_clock_font_honk_120);
LV_FONT_DECLARE(lf_clock_font_nabla_40);
LV_FONT_DECLARE(lf_clock_font_nabla_56);
LV_FONT_DECLARE(lf_clock_font_nabla_96);
LV_FONT_DECLARE(lf_clock_font_nabla_120);
LV_FONT_DECLARE(lf_clock_font_carlito_40);
LV_FONT_DECLARE(lf_clock_font_carlito_56);
LV_FONT_DECLARE(lf_clock_font_carlito_96);
LV_FONT_DECLARE(lf_clock_font_carlito_120);
LV_FONT_DECLARE(lf_clock_font_tinos_40);
LV_FONT_DECLARE(lf_clock_font_tinos_56);
LV_FONT_DECLARE(lf_clock_font_tinos_96);
LV_FONT_DECLARE(lf_clock_font_tinos_120);
LV_FONT_DECLARE(lf_clock_font_asap_40);
LV_FONT_DECLARE(lf_clock_font_asap_56);
LV_FONT_DECLARE(lf_clock_font_asap_96);
LV_FONT_DECLARE(lf_clock_font_asap_120);

static const int lf_font_sizes[5]={40,56,72,96,120};
static const lv_font_t *const lf_font_table[16][5]={
    {&lf_clock_font_kablammo_40,&lf_clock_font_kablammo_56,&lf_clock_font_kablammo,&lf_clock_font_kablammo_96,&lf_clock_font_kablammo_120},
    {&lf_clock_font_mystery_quest_40,&lf_clock_font_mystery_quest_56,&lf_clock_font_mystery_quest,&lf_clock_font_mystery_quest_96,&lf_clock_font_mystery_quest_120},
    {&lf_clock_font_moirai_one_40,&lf_clock_font_moirai_one_56,&lf_clock_font_moirai_one,&lf_clock_font_moirai_one_96,&lf_clock_font_moirai_one_120},
    {&lf_clock_font_rampart_one_40,&lf_clock_font_rampart_one_56,&lf_clock_font_rampart_one,&lf_clock_font_rampart_one_96,&lf_clock_font_rampart_one_120},
    {&lf_clock_font_hanalei_40,&lf_clock_font_hanalei_56,&lf_clock_font_hanalei,&lf_clock_font_hanalei_96,&lf_clock_font_hanalei_120},
    {&lf_clock_font_bitcount_grid_double_40,&lf_clock_font_bitcount_grid_double_56,&lf_clock_font_bitcount_grid_double,&lf_clock_font_bitcount_grid_double_96,&lf_clock_font_bitcount_grid_double_120},
    {&lf_clock_font_iceberg_40,&lf_clock_font_iceberg_56,&lf_clock_font_iceberg,&lf_clock_font_iceberg_96,&lf_clock_font_iceberg_120},
    {&lf_clock_font_montserrat_40,&lf_clock_font_montserrat_56,&lf_clock_font_montserrat,&lf_clock_font_montserrat_96,&lf_clock_font_montserrat_120},
    {&lf_clock_font_sancreek_40,&lf_clock_font_sancreek_56,&lf_clock_font_sancreek,&lf_clock_font_sancreek_96,&lf_clock_font_sancreek_120},
    {&lf_clock_font_modak_40,&lf_clock_font_modak_56,&lf_clock_font_modak,&lf_clock_font_modak_96,&lf_clock_font_modak_120},
    {&lf_clock_font_alumni_sans_collegiate_one_40,&lf_clock_font_alumni_sans_collegiate_one_56,&lf_clock_font_alumni_sans_collegiate_one,&lf_clock_font_alumni_sans_collegiate_one_96,&lf_clock_font_alumni_sans_collegiate_one_120},
    {&lf_clock_font_honk_40,&lf_clock_font_honk_56,&lf_clock_font_honk,&lf_clock_font_honk_96,&lf_clock_font_honk_120},
    {&lf_clock_font_nabla_40,&lf_clock_font_nabla_56,&lf_clock_font_nabla,&lf_clock_font_nabla_96,&lf_clock_font_nabla_120},
    {&lf_clock_font_carlito_40,&lf_clock_font_carlito_56,&lf_clock_font_carlito,&lf_clock_font_carlito_96,&lf_clock_font_carlito_120},
    {&lf_clock_font_tinos_40,&lf_clock_font_tinos_56,&lf_clock_font_tinos,&lf_clock_font_tinos_96,&lf_clock_font_tinos_120},
    {&lf_clock_font_asap_40,&lf_clock_font_asap_56,&lf_clock_font_asap,&lf_clock_font_asap_96,&lf_clock_font_asap_120}
};
const lv_font_t *lf_clock_custom_font_px(uint8_t style, int px)
{
    if(style<24 || style>=24+16)return NULL;
    const lv_font_t *const *row=lf_font_table[style-24];
    int best=0;
    for(int i=1;i<5;i++){
        int d=px-lf_font_sizes[i]; if(d<0)d=-d;
        int b=px-lf_font_sizes[best]; if(b<0)b=-b;
        if(d<b)best=i;
    }
    return row[best];
}
