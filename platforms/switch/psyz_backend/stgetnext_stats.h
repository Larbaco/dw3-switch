/* Diagnostic counters for the Switch StGetNext path (diag/stgetnext-movie-perf).
 *
 * Answers where movie-wait time goes: how often __wrap_StGetNext is entered,
 * how often the PR 106 empty-ring verdict is reused, how many full ring scans
 * actually run and how many slots they touch, and how the wrapper's own cost
 * compares with __real_StGetNext (which pumps the CD stream on every call).
 *
 * Self-contained on purpose: nx_present.c must not pull libnx headers in
 * (PSY-Z redirects libc names under __psyz there), so the tick read is inline
 * asm rather than armGetSystemTick().
 *
 * Overhead: two CNTPCT_EL0 reads per wrapped call. Report it with the numbers.
 */
#ifndef DW3_STGETNEXT_STATS_H
#define DW3_STGETNEXT_STATS_H

#include <stdint.h>

/* CNTPCT_EL0 is what libnx's armGetSystemTick() reads. Cheap, untrapped. */
static inline uint64_t dw3_stgetnext_ticks(void) {
#if defined(__aarch64__)
    uint64_t t;
    __asm__ volatile("mrs %0, cntpct_el0" : "=r"(t));
    return t;
#else
    return 0;
#endif
}

/* CNTFRQ_EL0: ticks per second, constant for the session. */
static inline uint64_t dw3_stgetnext_tick_freq(void) {
#if defined(__aarch64__)
    uint64_t f;
    __asm__ volatile("mrs %0, cntfrq_el0" : "=r"(f));
    return f;
#else
    return 1;
#endif
}

typedef struct Dw3StGetNextStats {
    uint64_t calls;          /* entries into __wrap_StGetNext */
    uint64_t real_calls;     /* entries into __real_StGetNext */
    uint64_t handovers;      /* __real_StGetNext returned a frame (rc == 0) */
    uint64_t gate_hits;      /* empty-ring verdict reused: scan skipped */
    uint64_t gate_misses;    /* no usable verdict at that point */
    uint64_t scans;          /* full ring scans actually performed */
    uint64_t slots_compared; /* ring slots inspected by those scans */
    uint64_t recoveries;     /* body-sector recovery branch entered */
    uint64_t ring_unusable;  /* NULL/short ring or guard bail-out */
    uint64_t wrap_ticks;     /* ticks inside __wrap_StGetNext (incl. real) */
    uint64_t real_ticks;     /* ticks inside __real_StGetNext */

    /* Psyz_CdPumpStream: the 150 sectors/s budget check, which every
     * StGetNext call reaches through __real_StGetNext, plus the one call per
     * frame from the vsync pump. Split by caller so the two can be told apart. */
    uint64_t pump_calls;
    uint64_t pump_ticks;
    uint64_t pump_calls_in_real; /* reached from inside __real_StGetNext */
    uint64_t pump_ticks_in_real;

    /* MDEC / VRAM upload path, to test whether the wait sits here instead. */
    uint64_t loadimage_calls;
    uint64_t loadimage_ticks;
    uint64_t decdctout_calls;
    uint64_t decdctout_ticks;

    int in_real; /* non-zero while inside a __real_StGetNext call */
} Dw3StGetNextStats;

extern Dw3StGetNextStats dw3_stgetnext_stats;

/* Copy the window's counters out and zero them, so each log line is a delta. */
void dw3_stgetnext_stats_take(Dw3StGetNextStats *out);

/* Write the counters as zero (used before the first window). */
void dw3_stgetnext_stats_reset(void);

#endif /* DW3_STGETNEXT_STATS_H */
