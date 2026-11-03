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
    assert(initial.brightness==55 && initial.scroll_speed==70);
    assert(initial.line_spacing_px==5 && initial.show_progress==1);
    assert(initial.timing_offset_ms==-2000);
    assert(initial.basic_battery_size==85 && initial.basic_battery_y==-160);
    assert(initial.basic_date_size==70 && initial.basic_weather_y==135);
    /* Virgin NVS must use the NEW default (70), not migrate a nonexistent
       legacy preference as if it came from alpha 0.8.4. */
    assert(initial.face_background[0]==0x070F14);
    assert(initial.face_battery_y[0]==-137);
    assert(initial.face_battery_y[2]==-109);

    assert(initial.charging_color==0x42DC89);
    initial.brightness=77;
    initial.scroll_speed=0;
    initial.deep_sleep_minutes=15;
    initial.font_size=2;
    initial.text_color=0xFF55AB;
    initial.background_color=0x123456;
    initial.accent_color=0xABCDEF;
    initial.timing_offset_ms=-420;
    initial.basic_battery_size=97;
    initial.basic_battery_y=-140;
    initial.basic_date_size=82;
    initial.basic_weather_x=22;
    initial.basic_time_y=-13;
    initial.line_spacing_px=13;
    initial.face_background[0]=0x445566;
    initial.face_accent[2]=0xF0AC22;
    initial.face_show_seconds[0]=0;
    initial.face_24h[1]=1;
    initial.face_battery_x[2]=39;
    initial.face_battery_y[2]=-72;

    assert(lf_settings_save(&initial)==ESP_OK);
    assert(mock_nvs_count()>=10);
    // Simulated reboot or app-slot switch: new struct, same NVS storage.
    assert(lf_settings_load(&next)==ESP_OK);
    assert(next.brightness==77&&next.scroll_speed==0&&next.deep_sleep_minutes==15&&next.font_size==2);
    assert(next.text_color==0xFF55AB&&next.background_color==0x123456);
    assert(next.accent_color==0xABCDEF&&next.timing_offset_ms==-420);
    assert(next.basic_battery_size==97 && next.basic_battery_y==-140);
    assert(next.basic_date_size==82 && next.basic_weather_x==22 && next.basic_time_y==-13);
    assert(next.line_spacing_px==13);
    assert(next.face_background[0]==0x445566);
    assert(next.face_accent[2]==0xF0AC22);
    assert(next.face_show_seconds[0]==0 && next.face_24h[1]==1);
    assert(next.face_battery_x[2]==39 && next.face_battery_y[2]==-72);

    // Corrupt/out-of-range values must be normalized, never require erasing NVS.
    nvs_handle_t h; assert(nvs_open(LF_CONFIG_NS,NVS_READWRITE,&h)==ESP_OK);
    assert(nvs_set_u8(h,"brightness",255)==ESP_OK);
    assert(nvs_set_i32(h,"offset_ms",-100000)==ESP_OK);
    nvs_close(h);
    assert(lf_settings_load(&next)==ESP_OK);
    assert(next.brightness==55 && next.timing_offset_ms==-2000);
    /* Legacy 0.8.42 default zero maps to the new calibrated baseline. */
    assert(nvs_open(LF_CONFIG_NS,NVS_READWRITE,&h)==ESP_OK);
    assert(nvs_set_u32(h,"schema",9)==ESP_OK);
    assert(nvs_set_i32(h,"offset_ms",0)==ESP_OK);
    nvs_close(h);
    assert(lf_settings_load(&next)==ESP_OK && next.timing_offset_ms==-2000);
    /* Schema 10 permits deliberate zero as a user preference. */
    assert(nvs_open(LF_CONFIG_NS,NVS_READWRITE,&h)==ESP_OK);
    assert(nvs_set_u32(h,"schema",10)==ESP_OK);
    assert(nvs_set_i32(h,"offset_ms",0)==ESP_OK);
    nvs_close(h);
    assert(lf_settings_load(&next)==ESP_OK && next.timing_offset_ms==0);
    next.timing_offset_ms=-2000;
    assert(lf_settings_save(&next)==ESP_OK);
    assert(lf_settings_load(&next)==ESP_OK && next.timing_offset_ms==-2000);
    printf("Persistent settings and schema migration defaults passed\n");
}
