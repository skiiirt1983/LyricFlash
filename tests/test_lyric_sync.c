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
 return 0;
}
