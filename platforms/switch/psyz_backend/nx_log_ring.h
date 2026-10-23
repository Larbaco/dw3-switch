/* P5.4 — ring of the last formatted log lines, for the crash report.
 *
 * psyz_log (nx_log.c) pushes every formatted line; the exception handler
 * (nx_crash.c) walks the ring and appends the lines to crash_report.txt.
 *
 * Fixed static storage, no allocation, no locks. A writer claims a sequence
 * number with one atomic add, copies the line into slot (seq % LINES) and
 * then publishes seq + 1 in the slot's tag. A reader takes the slots whose
 * tag matches the sequence it expects and skips the rest, so a line being
 * written (or overwritten) while the handler reads it is dropped or torn,
 * never a fault: every read stays inside the static arrays.
 *
 * No libnx types: the host test (tools/tests/log_ring_main.c) compiles this
 * header with gcc.
 */
#ifndef DW3_NX_LOG_RING_H
#define DW3_NX_LOG_RING_H

#include <stdint.h>
#include <string.h>

#define DW3_LOG_RING_LINES 64u /* power of two */
#define DW3_LOG_RING_LINE_MAX 256u

typedef struct Dw3LogRing {
    uint32_t seq; /* next sequence number; slot = seq % LINES */
    uint32_t tag[DW3_LOG_RING_LINES]; /* seq + 1 once the slot is complete */
    uint16_t len[DW3_LOG_RING_LINES];
    char line[DW3_LOG_RING_LINES][DW3_LOG_RING_LINE_MAX];
} Dw3LogRing;

/* Copy one line in (truncated to LINE_MAX bytes). Safe from any thread. */
static inline void dw3_log_ring_push(Dw3LogRing *r, const char *s, int n) {
    uint32_t seq;
    uint32_t slot;

    if (n <= 0) {
        return;
    }
    if ((unsigned)n > DW3_LOG_RING_LINE_MAX) {
        n = (int)DW3_LOG_RING_LINE_MAX;
    }
    seq = __atomic_fetch_add(&r->seq, 1u, __ATOMIC_RELAXED);
    slot = seq & (DW3_LOG_RING_LINES - 1u);
    /* Unpublish first, so a reader never pairs the old tag with new bytes. */
    __atomic_store_n(&r->tag[slot], 0u, __ATOMIC_RELEASE);
    memcpy(r->line[slot], s, (size_t)n);
    r->len[slot] = (uint16_t)n;
    __atomic_store_n(&r->tag[slot], seq + 1u, __ATOMIC_RELEASE);
}

/* Call emit(ctx, line, len) for the complete lines still in the ring, oldest
 * first. Lines are passed as stored (normally ending in '\n'; a truncated
 * line may not). Returns the number of lines emitted; *skipped (optional)
 * counts slots that were mid-write or overwritten during the walk. No
 * locks, no allocation; the handler calls it on the exception stack. */
static inline unsigned dw3_log_ring_walk(const Dw3LogRing *r,
                                         void (*emit)(void *ctx,
                                                      const char *line,
                                                      unsigned len),
                                         void *ctx, unsigned *skipped) {
    uint32_t end = __atomic_load_n(&r->seq, __ATOMIC_ACQUIRE);
    uint32_t start = end > DW3_LOG_RING_LINES ? end - DW3_LOG_RING_LINES : 0u;
    uint32_t s;
    unsigned emitted = 0;
    unsigned skip = 0;

    for (s = start; s != end; s++) {
        uint32_t slot = s & (DW3_LOG_RING_LINES - 1u);
        unsigned len;

        if (__atomic_load_n(&r->tag[slot], __ATOMIC_ACQUIRE) != s + 1u) {
            skip++;
            continue;
        }
        len = r->len[slot];
        if (len > DW3_LOG_RING_LINE_MAX) {
            len = DW3_LOG_RING_LINE_MAX;
        }
        emit(ctx, r->line[slot], len);
        emitted++;
    }
    if (skipped) {
        *skipped = skip;
    }
    return emitted;
}

#endif /* DW3_NX_LOG_RING_H */
