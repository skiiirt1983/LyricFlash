#include "ota_health_logic.h"
#include <stddef.h>

bool lf_ota_health_sample_ok(const lf_ota_health_sample_t *sample)
{
    /* The UI timer runs every 50ms. Give 1.5s tolerance for a busy QSPI
       frame, but reject stalled/no heartbeat and absent panel refreshes.
       16 KiB heap reserve is a minimal safety floor, not a performance goal. */
    return sample && sample->ui_tick_age_ms<=1500U &&
           sample->render_count>0 && sample->flush_finishes>0 &&
           sample->refresh_ready>0 && sample->free_heap_bytes>=16384U;
}
