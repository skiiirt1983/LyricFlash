#include "settings.h"
#include "nvs.h"
#include <assert.h>
#include <stdio.h>
void mock_nvs_reset(void);
int mock_nvs_count(void);
int main(void)
{
    lf_settings_t initial={0},next={0};
    mock_nvs_reset();
    assert(lf_settings_load(&initial)==ESP_OK);
    assert(initial.brightness==55 && initial.scroll_speed==45);
    assert(initial.line_spacing_px==5 && initial.show_progress==1);
    initial.brightness=77;
    initial.scroll_speed=24;
    initial.font_size=2;
    initial.text_color=0xFF55AB;
    initial.background_color=0x123456;
    initial.accent_color=0xABCDEF;
    initial.timing_offset_ms=-420;
    initial.line_spacing_px=13;
    assert(lf_settings_save(&initial)==ESP_OK);
    assert(mock_nvs_count()>=10);
    // Simulated reboot or app-slot switch: new struct, same NVS storage.
    assert(lf_settings_load(&next)==ESP_OK);
    assert(next.brightness==77&&next.scroll_speed==24&&next.font_size==2);
    assert(next.text_color==0xFF55AB&&next.background_color==0x123456);
    assert(next.accent_color==0xABCDEF&&next.timing_offset_ms==-420);
    assert(next.line_spacing_px==13);
    // Corrupt/out-of-range values must be normalized, never require erasing NVS.
    nvs_handle_t h; assert(nvs_open(LF_CONFIG_NS,NVS_READWRITE,&h)==ESP_OK);
    assert(nvs_set_u8(h,"brightness",255)==ESP_OK);
    assert(nvs_set_i32(h,"offset_ms",-100000)==ESP_OK);
    nvs_close(h);
    assert(lf_settings_load(&next)==ESP_OK);
    assert(next.brightness==55 && next.timing_offset_ms==0);
    printf("Persistent settings and schema migration defaults passed\n");
}
