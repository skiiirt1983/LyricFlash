#include "lyric_sync.h"
#include <assert.h>
#include <stdint.h>
int main(void){
 uint32_t ms=0;
 assert(lf_sync_parse_timecode_ms("01:23",&ms)&&ms==83000);
 assert(lf_sync_parse_timecode_ms("00:01:23.456",&ms)&&ms==83456);
 assert(lf_sync_parse_timecode_ms("2:03.5",&ms)&&ms==123500);
 assert(!lf_sync_parse_timecode_ms("1:60",&ms));
 assert(!lf_sync_parse_timecode_ms("invalid",&ms));
 assert(!lf_sync_parse_timecode_ms("1:23.1234",&ms));
 assert(lf_sync_estimate_position_ms(60000,10000,160000,16000,30000)==75000);
 assert(lf_sync_estimate_position_ms(60000,10000,160000,16000,12000)==60000);
 assert(lf_sync_estimate_position_ms(UINT32_MAX,10000,160000,16000,30000)==UINT32_MAX);
 assert(lf_sync_visible_position_ms(0)==0);
 assert(lf_sync_visible_position_ms(900)==0);
 assert(lf_sync_visible_position_ms(1000)==0);
 assert(lf_sync_visible_position_ms(2000)==1000);
 assert(lf_sync_slide_fraction(6000,6000,10000)==0);
 assert(lf_sync_slide_fraction(8950,6000,10000)==0);
 assert(lf_sync_slide_fraction(9500,6000,10000)==512);
 assert(lf_sync_slide_fraction(10000,6000,10000)==1024);
 assert(lf_sync_slide_fraction(10200,10000,10400)==512);
 assert(lf_sync_slide_fraction(10000,10000,10000)==1024);
 assert(lf_sync_row_pitch_px(30,30,1000)==92);
 assert(lf_sync_row_pitch_px(30,30,12000)==127);
 assert(lf_sync_row_pitch_px(110,80,1000)==113);
 return 0;
}
