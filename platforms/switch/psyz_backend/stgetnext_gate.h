/* Switch empty-ring verdict; valid only until an explicit mutation hook.
 * Cursor equality is a supplementary guard, never a publication proof. */
#ifndef DW3_STGETNEXT_GATE_H
#define DW3_STGETNEXT_GATE_H

/* Zero-sized rings are unusable. Safety uses explicit invalidation, not a
 * pacing bound: reset/early-wrap can restore identical cursors in one pump. */
#define DW3_STGETNEXT_RING_MIN 0

typedef struct Dw3StGetNextGate {
    const void *ring; /* StRingAddr: ring identity, set by StSetRing */
    long size;        /* StRingSize */
    long idx1;        /* StRingIdx1: producer write cursor */
    long idx3;        /* StRingIdx3: consumer read cursor */
    int valid;        /* zero-init: nothing remembered yet */
} Dw3StGetNextGate;

extern Dw3StGetNextGate dw3_stgetnext_empty_gate;

/* Drop any remembered verdict. Used on a producer attempt, ring reset,
 * a hand-off, an unusable ring, and
 * whenever the wrapper enters the body-sector recovery path. */
static inline void dw3_stgetnext_gate_forget(Dw3StGetNextGate *g) {
    g->valid = 0;
}

/* Non-zero when this exact (ring, size, write cursor, read cursor) was already
 * scanned clean, so the scan may be skipped. Rings at or below
 * DW3_STGETNEXT_RING_MIN never hit. A producer attempt or ring reset must
 * invalidate the verdict BEFORE changing the ring, even if cursors return to
 * their previous values. stgetnext_invalidate.c installs those hooks. */
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
