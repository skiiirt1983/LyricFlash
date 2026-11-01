#include "image_policy.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(void)
{
    unsigned char app[600]={0};
    app[0]=0xe9; app[12]=9;
    app[0x20]=0x32; app[0x21]=0x54; app[0x22]=0xcd; app[0x23]=0xab;
    memcpy(app+0x20+48,"lyricflash",11);
    assert(lf_check_ota_image(app,sizeof app,5*1024*1024)==LF_IMAGE_OK);
    assert(lf_check_ota_image(app,40,5*1024*1024)==LF_IMAGE_TOO_SHORT);
    assert(lf_check_ota_image(app,sizeof app,300)==LF_IMAGE_TOO_LARGE);
    app[0]=0; assert(lf_check_ota_image(app,sizeof app,5*1024*1024)==LF_IMAGE_NOT_ESP);
    app[0]=0xe9; app[12]=5; assert(lf_check_ota_image(app,sizeof app,5*1024*1024)==LF_IMAGE_WRONG_CHIP);
    app[12]=9; app[0x20]=0; assert(lf_check_ota_image(app,sizeof app,5*1024*1024)==LF_IMAGE_NOT_APP);
    app[0x20]=0x32; memcpy(app+0x20+48,"printsphere",12);
    assert(lf_check_ota_image(app,sizeof app,5*1024*1024)==LF_IMAGE_WRONG_PROJECT);
    puts("OTA header policy tests passed");
}
