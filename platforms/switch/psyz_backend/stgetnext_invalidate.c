/* Switch-only mutation boundaries for the empty StGetNext scan cache.
 * At the pinned PSY-Z revision, StCdInterrupt calls CdReady in a separate TU
 * before every ring write (including mismatch reset and early-wrap sentinel).
 * Invalidate even when CdReady reports no data: conservative and independent
 * of sector count, cursor wrap, frame length, and pacing catch-up limits.
 * Stream production and StGetNext run on the host/game thread; the audio mixer
 * does not call these stream APIs. This is not a cross-thread cache.
 */
#include <libcd.h>
#include "stgetnext_gate.h"

Dw3StGetNextGate dw3_stgetnext_empty_gate;

int __real_CdReady(int mode, u_char *result);
void __real_StSetRing(u_long *ring, u_long size);
void __real_StClearRing(void);
void __real_data_ready_callback(void);

int __wrap_CdReady(int mode, u_char *result) {
    dw3_stgetnext_gate_forget(&dw3_stgetnext_empty_gate);
    return __real_CdReady(mode, result);
}

void __wrap_StSetRing(u_long *ring, u_long size) {
    dw3_stgetnext_gate_forget(&dw3_stgetnext_empty_gate);
    __real_StSetRing(ring, size);
}

void __wrap_StClearRing(void) {
    dw3_stgetnext_gate_forget(&dw3_stgetnext_empty_gate);
    __real_StClearRing();
}

void __wrap_data_ready_callback(void) {
    dw3_stgetnext_gate_forget(&dw3_stgetnext_empty_gate);
    __real_data_ready_callback();
}
