/* Storage and window hand-off for the StGetNext diagnostic counters.
 *
 * Kept in its own TU so nx_present.c only sees the small asm header.
 * Single-threaded: stream production and StGetNext run on the game thread
 * (same assumption as stgetnext_invalidate.c).
 */
#include <string.h>

#include "stgetnext_stats.h"

Dw3StGetNextStats dw3_stgetnext_stats;

/* PSY-Z's pump lives in a different TU from StGetNext (psyz/src/psyz/libcd.c
 * vs decomp/src/libcd/c_009.c), so the link-time --wrap reaches the call made
 * from inside __real_StGetNext as well as the per-frame one. */
void __real_Psyz_CdPumpStream(void);

void __wrap_Psyz_CdPumpStream(void) {
    Dw3StGetNextStats *st = &dw3_stgetnext_stats;
    uint64_t t0 = dw3_stgetnext_ticks();
    uint64_t d;

    __real_Psyz_CdPumpStream();

    d = dw3_stgetnext_ticks() - t0;
    st->pump_calls++;
    st->pump_ticks += d;
    if (st->in_real) {
        st->pump_calls_in_real++;
        st->pump_ticks_in_real += d;
    }
}

void dw3_stgetnext_stats_take(Dw3StGetNextStats *out) {
    *out = dw3_stgetnext_stats;
    memset(&dw3_stgetnext_stats, 0, sizeof(dw3_stgetnext_stats));
}

void dw3_stgetnext_stats_reset(void) {
    memset(&dw3_stgetnext_stats, 0, sizeof(dw3_stgetnext_stats));
}
