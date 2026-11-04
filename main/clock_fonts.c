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
    default: return NULL;
    }
}
