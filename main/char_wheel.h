#pragma once
#include <stdbool.h>
#include "lvgl.h"

/* A compact character wheel for the round touch display.
   Swipe through one large character, add it, change character set, or delete. */
void lf_charwheel_show(lv_obj_t *parent, const char *title, const char *initial,
                       bool password, unsigned max_length,
                       void (*on_save)(const char *value), void (*on_cancel)(void));
