/* Host check for platforms/switch/psyz_backend/stgetnext_gate.h.
 *
 * gcc, no libnx and no PSY-Z: the ring, the consumer (StGetNext), the wrapper
 * scan and the producer transitions StCdInterrupt/StFreeRing make are modelled
 * here, and the gate under test is the real shared header. Every scenario is
 * run twice over the same starting state -- ungated and gated -- and the two
 * traces (return value, read cursor, ring contents) must match exactly. A
 * counter proves the gate actually skips scan work.
 *
 * Ring size and producer pacing are the production ones: StSetRing uses 32
 * slots (menus/title/movie.c) and one pump publishes at most CD_PACE_CATCHUP
 * (8) sectors (vendor/psyz .../libcd.c). The stress obeys that bound, because
 * regression scenarios also include reset and early-wrap ABA.
 *
 * The model mirrors:
 *   vendor/psyz/decomp/src/libcd/c_009.c   StGetNext (id 1 reset, id 2 hand-off)
 *   vendor/psyz/decomp/src/libcd/c_004.c   data_ready_callback (id 2)
 *   vendor/psyz/decomp/src/libcd/c_007.c   StFreeRing (zero + advance idx3)
 *   vendor/psyz/decomp/src/libcd/c_002.c   StClearRing
 *   vendor/psyz/psyz/src/psyz/libcd.c      StCdInterrupt publication order
 */
#include "stgetnext_gate.h"

#include <stdio.h>
#include <string.h>

#define RING_SIZE 32 /* StSetRing(..., 32) */
#define PACE_CATCHUP 8 /* CD_PACE_CATCHUP: most sectors one pump can publish */
#define TRACE_MAX 8192

typedef struct Slot {
    int id; /* 0 free, 1 sentinel, 2 ready, 3 body, 4 handed */
    int frameCount;
    int secCount;
    int nSectors;
} Slot;

typedef struct RingState {
    Slot a[RING_SIZE];
    Slot b[RING_SIZE];
    Slot *ring;
    int idx1, idx2, idx3;
    int endframe;
} RingState;

extern int CdReady(int mode, unsigned char *result);
extern void StSetRing(unsigned long *ring, unsigned long size);
extern void StClearRing(void);
extern void data_ready_callback(void);

static RingState g;
static long g_slot_visits; /* scan-loop slot comparisons, the cost metric */
static int g_trace_rc[TRACE_MAX];
static int g_trace_idx3[TRACE_MAX];
static int g_trace_len;
static int g_last_handed;
static int g_fail;
static int g_publish_moves_idx1; /* invariant: every id 1/2 publish moves idx1 */

static void expect(const char *name, int cond) {
    printf("%s=%d\n", name, cond ? 1 : 0);
    if (!cond) {
        g_fail = 1;
    }
}

static void restore(const RingState *in) { g = *in; }

/* --- consumer: c_009.c StGetNext --- */
static int model_real_getnext(void) {
    Slot *ptr = &g.ring[g.idx3];

    if (ptr->id == 1) {
        g.idx3 = 0;
        if (g.endframe != 0) {
            ptr->id = 0; /* writes the slot that held the sentinel */
        }
        ptr = &g.ring[g.idx3];
    }
    if (ptr->id == 2) {
        ptr->id = 4;
        g_last_handed = g.idx3;
        return 0;
    }
    return 1;
}

/* --- wrapper: nx_present.c __wrap_StGetNext, with the gate optional --- */
static int model_wrap(int gated, Dw3StGetNextGate *gate) {
    int guard = 0;

    for (;;) {
        int rc = model_real_getnext();
        Slot *slot;
        Slot *head;
        int start;
        int n;
        int j;

        if (rc == 0 || g.idx3 < 0 || g.idx3 >= RING_SIZE || guard++ > 4) {
            if (gated) {
                dw3_stgetnext_gate_forget(gate);
            }
            return rc;
        }
        if (gated && dw3_stgetnext_gate_hit(gate, g.ring, RING_SIZE, g.idx1,
                                            g.idx3)) {
            return rc;
        }
        slot = &g.ring[g.idx3];
        if (slot->id == 3 && slot->secCount > 0 && slot->secCount <= 31) {
            if (gated) {
                dw3_stgetnext_gate_forget(gate);
            }
            start = g.idx3 - slot->secCount;
            if (start >= 0 && start < RING_SIZE) {
                head = &g.ring[start];
                if (head->frameCount == slot->frameCount && head->id == 2) {
                    g.idx3 = start;
                    continue;
                }
                if (head->frameCount == slot->frameCount && head->id == 3) {
                    return rc;
                }
            }
            slot->id = 0;
        }
        for (n = 1; n < RING_SIZE; n++) {
            g_slot_visits++;
            j = g.idx3 + n;
            if (j >= RING_SIZE) {
                j -= RING_SIZE;
            }
            if (g.ring[j].id == 2 || g.ring[j].id == 1) {
                g.idx3 = j;
                break;
            }
        }
        if (n >= RING_SIZE) {
            if (gated) {
                dw3_stgetnext_gate_remember(gate, g.ring, RING_SIZE, g.idx1,
                                            g.idx3);
            }
            return rc;
        }
    }
}

/* --- producer transitions --- */
/* One video body sector at the write cursor, then the cursor advances. */
static void model_stcd_body(int fc, int sec, int ns) {
    CdReady(1, NULL); /* actual production invalidation hook via --wrap */
    Slot *r = &g.ring[g.idx1];

    r->frameCount = fc;
    r->secCount = sec;
    r->nSectors = ns;
    r->id = 3;
    g.idx1++;
}

/* The StCdInterrupt wrap: sentinel at the write cursor, then the cursor to 0. */
static void model_stcd_wrap_sentinel(void) {
    int before = g.idx1;

    CdReady(1, NULL);
    g.ring[g.idx1].id = 1;
    g.idx1 = 0;
    if (g.idx1 == before) {
        g_publish_moves_idx1 = 0;
    }
}

/* One whole frame: reserve room (sentinel if not), write its sectors, then mark
 * its first slot ready -- exactly the StCdInterrupt order. Returns sectors
 * published. */
static int model_publish_frame(int fc, int ns) {
    int start;
    int k;
    int before;

    if (RING_SIZE - g.idx1 - 1 < ns) {
        if (g.idx1 == 0) {
            return 0; /* would not fit even from 0: not a legal frame size */
        }
        model_stcd_wrap_sentinel();
    }
    start = g.idx1;
    g.idx2 = start;
    before = g.idx1;
    for (k = 0; k < ns; k++) {
        model_stcd_body(fc, k, ns);
    }
    data_ready_callback();
    g.ring[start].id = 2; /* data_ready_callback: StRingAddr[StRingIdx2] */
    g.idx2 = g.idx1;
    if (g.idx1 == before) {
        g_publish_moves_idx1 = 0;
    }
    return ns;
}

static void model_stfree_at(int start) {
    int ns = g.ring[start].nSectors;
    int i;

    for (i = 0; i < ns; i++) {
        g.ring[start + i].id = 0;
    }
    g.idx3 = start + ns;
}

static void model_clear_ring(Slot *ring) {
    StSetRing((unsigned long *)ring, RING_SIZE);
    StClearRing();
    memset(ring, 0, sizeof(Slot) * RING_SIZE);
    g.ring = ring;
    g.idx1 = 0;
    g.idx2 = 0;
    g.idx3 = 0;
}

/* --- trace --- */
static void rec(int rc) {
    if (g_trace_len < TRACE_MAX) {
        g_trace_rc[g_trace_len] = rc;
        g_trace_idx3[g_trace_len] = g.idx3;
        g_trace_len++;
    }
}

static void run_steps(int gated, Dw3StGetNextGate *gate, int count) {
    int i;

    for (i = 0; i < count; i++) {
        rec(model_wrap(gated, gate));
    }
}

/* Runs one scenario body twice (ungated then gated) from the same state and
 * asserts identical traces and final ring bytes. */
typedef void (*Scenario)(int gated, Dw3StGetNextGate *gate);

static void compare_scenario(const char *name, Scenario fn,
                             const RingState *start, long *ungated_visits,
                             long *gated_visits) {
    RingState keep = *start;
    Dw3StGetNextGate *gate = &dw3_stgetnext_empty_gate;
    int rc_u[TRACE_MAX];
    int rc_g[TRACE_MAX];
    int idx_u[TRACE_MAX];
    int idx_g[TRACE_MAX];
    int len_u, len_g;
    long visits_u, visits_g;
    RingState end_u, end_g;
    int same = 1;
    int i;

    memset(gate, 0, sizeof(*gate));
    restore(&keep);
    g_trace_len = 0;
    g_slot_visits = 0;
    fn(0, gate);
    len_u = g_trace_len;
    visits_u = g_slot_visits;
    memcpy(rc_u, g_trace_rc, sizeof(rc_u));
    memcpy(idx_u, g_trace_idx3, sizeof(idx_u));
    end_u = g;

    memset(gate, 0, sizeof(*gate));
    restore(&keep);
    g_trace_len = 0;
    g_slot_visits = 0;
    fn(1, gate);
    len_g = g_trace_len;
    visits_g = g_slot_visits;
    memcpy(rc_g, g_trace_rc, sizeof(rc_g));
    memcpy(idx_g, g_trace_idx3, sizeof(idx_g));
    end_g = g;

    if (len_u != len_g) {
        same = 0;
    }
    for (i = 0; i < len_u && i < len_g; i++) {
        if (rc_u[i] != rc_g[i] || idx_u[i] != idx_g[i]) {
            same = 0;
        }
    }
    if (end_u.idx1 != end_g.idx1 || end_u.idx2 != end_g.idx2 ||
        end_u.idx3 != end_g.idx3 ||
        memcmp(end_u.a, end_g.a, sizeof(end_u.a)) != 0 ||
        memcmp(end_u.b, end_g.b, sizeof(end_u.b)) != 0) {
        same = 0;
    }
    if (name) {
        char label[96];
        snprintf(label, sizeof(label), "same_%s", name);
        expect(label, same);
    }
    *ungated_visits = visits_u;
    *gated_visits = visits_g;
}

static void scn_empty_polls(int gated, Dw3StGetNextGate *gate) {
    run_steps(gated, gate, 64);
}

static void scn_ready_at_cursor(int gated, Dw3StGetNextGate *gate) {
    model_publish_frame(1, 2);
    g.idx3 = 0;
    rec(model_wrap(gated, gate));
}

static void scn_body_same_head(int gated, Dw3StGetNextGate *gate) {
    /* idx3 on a body whose head is also id 3: the wrapper must bail, not drop. */
    g.ring[0].id = 3;
    g.ring[0].frameCount = 7;
    g.ring[0].secCount = 0;
    g.ring[0].nSectors = 4;
    g.ring[1].id = 3;
    g.ring[1].frameCount = 7;
    g.ring[1].secCount = 1;
    g.ring[1].nSectors = 4;
    g.idx3 = 1;
    rec(model_wrap(gated, gate));
}

static void scn_resync_to_head(int gated, Dw3StGetNextGate *gate) {
    /* idx3 on a body whose head is a ready frame: reposition to the head. */
    g.ring[0].id = 2;
    g.ring[0].frameCount = 9;
    g.ring[0].secCount = 0;
    g.ring[0].nSectors = 3;
    g.ring[2].id = 3;
    g.ring[2].frameCount = 9;
    g.ring[2].secCount = 2;
    g.ring[2].nSectors = 3;
    g.idx3 = 2;
    rec(model_wrap(gated, gate));
}

static void scn_orphan_drop(int gated, Dw3StGetNextGate *gate) {
    /* idx3 on a tail body with no matching head: drop to id 0, then scan. */
    g.ring[3].id = 3;
    g.ring[3].frameCount = 11;
    g.ring[3].secCount = 2;
    g.ring[3].nSectors = 5;
    g.idx3 = 3;
    run_steps(gated, gate, 3);
}

static void scn_sentinel_at_cursor(int gated, Dw3StGetNextGate *gate) {
    g.endframe = -1;
    g.ring[2].id = 1;
    g.idx1 = 3;
    g.idx3 = 2;
    run_steps(gated, gate, 3);
}

static void scn_sentinel_no_endframe(int gated, Dw3StGetNextGate *gate) {
    g.endframe = 0;
    g.ring[2].id = 1;
    g.idx1 = 3;
    g.idx3 = 2;
    run_steps(gated, gate, 3);
}

static void scn_scan_finds_ready(int gated, Dw3StGetNextGate *gate) {
    /* A ready frame further along the ring, nothing at the cursor. */
    g.ring[5].id = 2;
    g.ring[5].frameCount = 4;
    g.ring[5].nSectors = 2;
    g.idx1 = 6;
    g.idx3 = 0;
    run_steps(gated, gate, 4);
}

static void scn_wrap_sentinel(int gated, Dw3StGetNextGate *gate) {
    g.endframe = -1;
    g.idx1 = RING_SIZE - 1;
    g.idx3 = 6;
    rec(model_wrap(gated, gate)); /* empty poll: scan clean, remember */
    model_stcd_wrap_sentinel();
    run_steps(gated, gate, 3);
}

static void scn_publish_invalidates(int gated, Dw3StGetNextGate *gate) {
    /* The critical case: an empty poll caches "clean", then a legal producer
     * transition makes a frame ready away from the cursor. The next poll must
     * still find it, because the transition moved StRingIdx1. */
    g.idx3 = 0;
    rec(model_wrap(gated, gate)); /* scan clean, remember */
    model_publish_frame(20, 3);
    run_steps(gated, gate, 3);
}

static void scn_stfree_reposition(int gated, Dw3StGetNextGate *gate) {
    /* Consume a frame, free it, and confirm the gate does not hide the next. */
    model_publish_frame(30, 3);
    g.idx3 = 0;
    rec(model_wrap(gated, gate)); /* hand-off -> gate forgotten */
    model_stfree_at(g_last_handed);
    model_publish_frame(31, 3);
    run_steps(gated, gate, 3);
}

static void scn_ring_reset(int gated, Dw3StGetNextGate *gate) {
    run_steps(gated, gate, 4); /* cache a verdict at idx1=0, idx3=0 */
    model_clear_ring(g.b);
    run_steps(gated, gate, 3);
}

/* Deterministic stress. At most PACE_CATCHUP sectors are published between two
 * consecutive polls -- the pump's own bound -- so the write cursor can never
 * wrap a full ring unseen; the operations mix publishes, wrap, free, cursor
 * moves and a ring swap. Traces must match at every step. */
static void scn_random(int gated, Dw3StGetNextGate *gate) {
    unsigned s = 0x9e3779b9u;
    int pub = 0;
    int i;

    g.ring = g.a;
    for (i = 0; i < 6000; i++) {
        int op;

        if (pub >= PACE_CATCHUP) {
            rec(model_wrap(gated, gate));
            pub = 0;
        }
        s = s * 1103515245u + 12345u;
        op = (int)((s >> 16) & 0x7u);
        switch (op) {
        case 0:
        case 1:
            break; /* idle: poll below */
        case 2:
        case 3:
        case 4: { /* a frame, as the pump delivers one */
            int fc = (int)((s >> 8) & 0x3F);
            int ns = 1 + (int)((s >> 4) & 0x3);
            pub += model_publish_frame(fc, ns);
            break;
        }
        case 5: /* the wrap sentinel on its own */
            if (g.idx1 > 0) {
                model_stcd_wrap_sentinel();
            }
            break;
        case 6: /* consumer frees the frame it holds */
            if (g.ring[g.idx3].id == 4) {
                model_stfree_at(g.idx3);
            }
            break;
        case 7:
            break; /* poll */
        }
        rec(model_wrap(gated, gate));
    }
}

static void scn_reset_publish_aba(int gated, Dw3StGetNextGate *gate) {
    for (int k=0;k<5;++k) model_stcd_body(60,k,9);
    g.idx2=0; g.idx3=20;
    rec(model_wrap(gated,gate));
    CdReady(1, NULL);
    /* StCdInterrupt secCount mismatch: init_ring_status then idx1=idx2. */
    for (int k=g.idx2;k<g.idx1;++k) g.ring[k].id=0;
    g.idx1=g.idx2;
    model_publish_frame(61,5);
    rec(model_wrap(gated,gate));
}
static void scn_early_wrap_aba(int gated, Dw3StGetNextGate *gate) {
    g.idx1 = 6;
    g.idx3 = 10;
    rec(model_wrap(gated, gate));
    /* Actual StCdInterrupt: 32 - 6 - 1 < 26; sentinel at 6,
       reset to 0, six sectors of a still-incomplete 26-sector frame. */
    model_stcd_wrap_sentinel();
    g.idx2 = 0;
    for (int k = 0; k < 6; ++k) model_stcd_body(50, k, 26);
    rec(model_wrap(gated, gate));
}
static void scn_same_address_reuse(int gated, Dw3StGetNextGate *gate) {
    rec(model_wrap(gated, gate));
    model_clear_ring(g.ring);
    /* Direct data-ready callback need not change either cursor. */
    data_ready_callback();
    g.ring[5].id = 2;
    g.ring[5].nSectors = 1;
    rec(model_wrap(gated, gate));
}

#define MAX_SCENARIOS 15

int main(void) {
    RingState start;
    Scenario scns[MAX_SCENARIOS];
    const char *names[MAX_SCENARIOS];
    long ung[MAX_SCENARIOS], gat[MAX_SCENARIOS];
    int count = 0;
    int i;
    Dw3StGetNextGate probe;

    g_publish_moves_idx1 = 1;
    g.endframe = -1;

    scns[count] = scn_empty_polls;
    names[count++] = "empty_polls";
    scns[count] = scn_ready_at_cursor;
    names[count++] = "ready_at_cursor";
    scns[count] = scn_body_same_head;
    names[count++] = "body_same_head";
    scns[count] = scn_resync_to_head;
    names[count++] = "resync_to_head";
    scns[count] = scn_orphan_drop;
    names[count++] = "orphan_drop";
    scns[count] = scn_sentinel_at_cursor;
    names[count++] = "sentinel_at_cursor";
    scns[count] = scn_sentinel_no_endframe;
    names[count++] = "sentinel_no_endframe";
    scns[count] = scn_scan_finds_ready;
    names[count++] = "scan_finds_ready";
    scns[count] = scn_wrap_sentinel;
    names[count++] = "wrap_sentinel";
    scns[count] = scn_publish_invalidates;
    names[count++] = "publish_invalidates";
    scns[count] = scn_stfree_reposition;
    names[count++] = "stfree_reposition";
    scns[count] = scn_ring_reset;
    names[count++] = "ring_reset";

    scns[count] = scn_reset_publish_aba;
    names[count++] = "reset_publish_aba";
    scns[count] = scn_early_wrap_aba;
    names[count++] = "early_wrap_aba";
    scns[count] = scn_same_address_reuse;
    names[count++] = "same_address_reuse";

    memset(&start, 0, sizeof(start));
    start.ring = g.a;
    start.endframe = -1;

    for (i = 0; i < count; i++) {
        compare_scenario(names[i], scns[i], &start, &ung[i], &gat[i]);
    }

    /* The empty-poll scenario must actually skip work: 64 polls of a 32-slot
     * ring scan 64 * 31 slots ungated, and only the first 31 gated. */
    expect("gate_cuts_scan_empty_polls", gat[0] < ung[0]);
    expect("empty_ungated_visits", ung[0] == 64L * 31L);
    expect("empty_gated_visits", gat[0] == 31L);
    printf("visits_empty_ungated=%ld\n", ung[0]);
    printf("visits_empty_gated=%ld\n", gat[0]);

    /* Stress: gated and ungated traces must match. */
    compare_scenario("random", scn_random, &start, &ung[0], &gat[0]);

    /* A ring at or below DW3_STGETNEXT_RING_MIN must never hit. */
    memset(&probe, 0, sizeof(probe));
    dw3_stgetnext_gate_remember(&probe, g.a, DW3_STGETNEXT_RING_MIN, 0, 0);
    expect("guard_rejects_small_ring",
           dw3_stgetnext_gate_hit(&probe, g.a, DW3_STGETNEXT_RING_MIN, 0, 0) ==
               0);
    dw3_stgetnext_gate_remember(&probe, g.a, DW3_STGETNEXT_RING_MIN + 1, 0, 0);
    expect("guard_allows_bigger_ring",
           dw3_stgetnext_gate_hit(&probe, g.a, DW3_STGETNEXT_RING_MIN + 1, 0,
                                  0) == 1);
    /* A cursor move or a different ring must miss. */
    expect("guard_rejects_idx1_move",
           dw3_stgetnext_gate_hit(&probe, g.a, DW3_STGETNEXT_RING_MIN + 1, 1,
                                  0) == 0);
    expect("guard_rejects_idx3_move",
           dw3_stgetnext_gate_hit(&probe, g.a, DW3_STGETNEXT_RING_MIN + 1, 0,
                                  1) == 0);
    expect("guard_rejects_other_ring",
           dw3_stgetnext_gate_hit(&probe, g.b, DW3_STGETNEXT_RING_MIN + 1, 0,
                                  0) == 0);

    /* A producer attempt invalidates even if every key remains identical. */
    dw3_stgetnext_gate_remember(&dw3_stgetnext_empty_gate, g.a, 32, 5, 20);
    CdReady(1, NULL);
    expect("producer_attempt_invalidates", !dw3_stgetnext_empty_gate.valid);
    return g_fail;
}
