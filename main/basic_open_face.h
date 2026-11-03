#pragma once
#include <stdint.h>
/* Genuine serif, ornamental serif and rounded digit shapes sampled from
 * SIL-OFL EB Garamond, Noto Serif Display and Comfortaa. Open-licensed
 * glyph-only 15x21 rasters; no TTF/OTF files distributed. */
#define LF_OPEN_FACE_W 15U
#define LF_OPEN_FACE_H 21U
#define LF_OPEN_FACE_FAMILIES 3U
#define LF_OPEN_FACE_RAW_BYTES 1260U
static const char lf_open_face_b64[]="AAAAAAAAAADgA3gGHAwMGAYYBhAGEAwYDBg4DvAH4AMAAAAAAAAAAAAAAAD4H/gf4AfAA8ADwAPAA8ADwAPAA8ADwAPAA8ADwAPAA+AH+B/8HwAAAAAAAAAA4APwB3gMOBg4ADAAcABgAMAAgAEGAw4G/A/8HwAAAAAAAAAAAADgA/AH8AxwCHAAcABgAMAA4APwB3gAOAA4ADgAMAAwAGAAwA8AHwAAAAAAAGAAYADgAOABYANgAmAEYAxgGPwf/B9gAGAAYABgAEAAAAAAAAAAAAAAAPgA8AEAAwAHAA+AD8AH4AHgAPAAcABwAGAAYADAAIAPAB8ADAAAAAAIAHgA4ADAAYADAAcADvAfeB4cHB4cDhwOHA4cDBwcDrgH4AMAAAAAAAD8D/wf+B8YEDAQMABgAEAAwACAAIABAAEAAwACAAYABAAMABgAGAAAAADgA/AHOA4YDBgcGA4wD+AHwAPgA/AHeA44HDwcHBwcHDgc8A/ABwAAAAAAAPADOAccDhwcDhwOHA4YHBwcHDwP+ANwAOAAwAGABwAeABAAAAAAAABgA3APOA48HjwePD48Pj4+Pj4+Pj4+PD48PjwePB44HnAOYAMAAAAAAADwAPAB8AfwPfAZ8AHwAfAB8AHwAfAB8AHwAfAB8AHwAfAB8AMAAAAAAABABngeeBw8PDw8PBx8AHgAcADgAMAAAAEABgAABhD+P/w//D8AAAAAAAAAAGACeA58HHwcfBx8AHgA4ADwADwAPAA+AD4YPDg8GHgcYAwAAAAAAAAAAPgA+AD4APgA+AD4APgA+AD4APgA+AD+P/4/+AD4APgA+AEAAAAAAAD8D/wP/A8EAAAAAAAAAPAAeAA8AD4APgA+AD4YPDg8OHgY4AQAAAAAAAAwATwHHA4cDhgeAB4AHng+PD48Pj4+Pj4+Hj4ePB48DjgGcAMAAAAAAAD+P/4//j/8PwAgAAAIABgAEAAwACAAYADAAMAAwAGAAYADAAMABwAAAABgAjgOPBw8HDwePB44H7AP4Af4A/wOfhw+PB48HjwcPDgcMAYAAAAAAABAAzAOOB48Hjw+Pj4+Pj4+Pj4+Hj4PPgM+ADwAPBg8GDgYcBjABAAAAADAA/APcA44HBgYHDgcOBw4HDgcOBw4HDgcOBw4OBw4HPAP8A/AAwAAAABgAPAD8AdwA3AAcABwAHAAcABwAHAAcABwAHAAcABwAHAAcABgAAAAAADAA/APcB44HBgYGDgYGDgAMABwAOAAwAGAA4ADAAcADgAc/D/4HwAAAADgA/APeA4YHBgYGBg4AHAA4AHwADgAHAAcMAw4HDgcGHge8A/gAwAAAAAAABAAOAB4APgA+AG4ATgDOAc4DjAM/B/+PzgAMAA4ABAAAAAAAAAAAAD8D/wfABwAHAAcAByAH/Af+A8cAA4ADhAOOA44DhgcHDwP+AfgAwAAAABgAOAAwADAAYADgAMAB8AH8A94DjgcGBgcOBw4GBg4HHge8A/AAwAAAAD4P/w/OAA4ADAAcABgAOAAwADAAcABgAOAAwAHAAcADgAOAAwADAAAAADAA/APcA44HDgcOBxwDvAP8A94HjgcHDgcOBw4HDg4HHge8A/AAwAAAADAB/APeB44OBw4HDAcMBg4OBzwH/AP4ADgAMABgAOAAwAHAAcABgAA";
static uint8_t lf_open_face_val(char c)
{
    if(c>='A'&&c<='Z')return (uint8_t)(c-'A');
    if(c>='a'&&c<='z')return (uint8_t)(c-'a'+26);
    if(c>='0'&&c<='9')return (uint8_t)(c-'0'+52);
    if(c=='+')return 62U;
    return 63U;
}
static uint8_t lf_open_face_byte(unsigned off)
{
    if(off>=LF_OPEN_FACE_RAW_BYTES)return 0;
    unsigned p=(off/3U)*4U;
    uint32_t x=((uint32_t)lf_open_face_val(lf_open_face_b64[p])<<18)|
         ((uint32_t)lf_open_face_val(lf_open_face_b64[p+1])<<12)|
         ((uint32_t)lf_open_face_val(lf_open_face_b64[p+2])<<6)|
         (uint32_t)lf_open_face_val(lf_open_face_b64[p+3]);
    return (uint8_t)((x>>(16U-8U*(off%3U)))&255U);
}
static uint16_t lf_open_face_row(unsigned family,unsigned digit,unsigned y)
{
    if(family>=LF_OPEN_FACE_FAMILIES||digit>=10||y>=LF_OPEN_FACE_H)return 0;
    unsigned p=2U*(family*10U*LF_OPEN_FACE_H+digit*LF_OPEN_FACE_H+y);
    return (uint16_t)(lf_open_face_byte(p)|
                      ((uint16_t)lf_open_face_byte(p+1U)<<8));
}
