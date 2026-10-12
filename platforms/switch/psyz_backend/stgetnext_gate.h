/* Empty-ring fast path for __wrap_StGetNext (Switch wrapper).
 *
 * The wrapper scans every ring slot for a ready frame (id 2) or the end-of-ring
 * sentinel (id 1) whenever a poll finds the read cursor on neither. On the movie
 * busy-wait that scan runs on ~every poll and almost always finds nothing, so it
 * is the empty-poll cost this gate removes.
 *
 * The scan is only needed while the ring can have changed. The producer sets
 * id 2 only in data_ready_callback, reached from StCdInterrupt after it already
 * advanced StRingIdx1, and sets id 1 (the wrap sentinel) immediately before
 * StRingIdx1 = 0 -- so a ready frame or sentinel cannot appear without moving
 * the write cursor. One pump delivers at most CD_PACE_CATCHUP (8) sectors, far
 * short of a StRingSize (32) cursor wrap, so the write cursor cannot leave and
 * re-enter the same value between two consecutive polls. StFreeRing's
 * reposition and any resync move the read cursor StRingIdx3 instead. Therefore a
 * slot set that a poll already scanned clean stays clean while both cursors are
 * unmoved, and the gate misses as soon as either one moves.
 *
 * The transport/consumer paths that can only *remove* a ready or sentinel slot
 * (StClearRing, the StCdInterrupt reset, the sentinel reset) keep the cached
 * "empty" verdict valid: they cannot create a frame the gate would skip.
 *
 * The proof needs one bound, checked here as DW3_STGETNEXT_RING_MIN: a single
 * pump publishes at most CD_PACE_CATCHUP (8) sectors, so the write cursor must
 * move by fewer than StRingSize slots between two consecutive polls. Two polls
 * are one pump apart (the game only spins in StGetNext between them), so a ring
 * of more than 8 slots cannot let the write cursor leave and re-enter the same
 * value unseen. The 32-slot STR ring meets this with a 4x margin; if a future
 * change raised the catch-up cap or shrank the ring below it, the gate stops
 * engaging rather than risk a stale verdict.
 *
 * No libnx or PSY-Z types: nx_present.c feeds it cursors and the ring pointer,
 * and the host test (tools/tests) compiles this header with gcc.
 */
#ifndef DW3_STGETNEXT_GATE_H
#define DW3_STGETNEXT_GATE_H

/* Must be >= the pump's CD_PACE_CATCHUP. Rings at or below it fall back to the
 * full scan. */
#define DW3_STGETNEXT_RING_MIN 8

typedef struct Dw3StGetNextGate {
    const void *ring; /* StRingAddr: ring identity, set by StSetRing */
    long size;        /* StRingSize */
    long idx1;        /* StRingIdx1: producer write cursor */
    long idx3;        /* StRingIdx3: consumer read cursor */
    int valid;        /* zero-init: nothing remembered yet */
} Dw3StGetNextGate;

/* Drop any remembered verdict. Used on a hand-off, on an unusable ring, and
 * whenever the wrapper enters the body-sector recovery path. */
static inline void dw3_stgetnext_gate_forget(Dw3StGetNextGate *g) {
    g->valid = 0;
}

/* Non-zero when this exact (ring, size, write cursor, read cursor) was already
 * scanned clean, so the scan may be skipped. Rings at or below
 * DW3_STGETNEXT_RING_MIN never hit: the cursor-wrap bound above does not hold
 * for them, so they keep the full scan. */
static inline int dw3_stgetnext_gate_hit(const Dw3StGetNextGate *g,
                                         const void *ring, long size, long idx1,
                                         long idx3) {
    return g->valid && size > DW3_STGETNEXT_RING_MIN && g->ring == ring &&
           g->size == size && g->idx1 == idx1 && g->idx3 == idx3;
}

/* Record that the whole ring at these cursors was scanned and no slot carried
 * id 2 or id 1. */
static inline void dw3_stgetnext_gate_remember(Dw3StGetNextGate *g,
                                               const void *ring, long size,
                                               long idx1, long idx3) {
    g->ring = ring;
    g->size = size;
    g->idx1 = idx1;
    g->idx3 = idx3;
    g->valid = 1;
}

#endif /* DW3_STGETNEXT_GATE_H */
