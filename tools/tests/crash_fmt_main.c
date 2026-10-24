/* Host check for platforms/switch/psyz_backend/nx_crash_fmt.h and
 * nx_log_ring.h (P5.4 crash report). gcc, no libnx. */
#include "nx_crash_fmt.h"
#include "nx_log_ring.h"

#include <stdio.h>
#include <string.h>

static int g_fail;

static void expect(const char *name, int cond) {
    printf("%s=%d\n", name, cond ? 1 : 0);
    if (!cond) {
        g_fail = 1;
    }
}

static int line_is(const Dw3CrashLine *l, const char *want) {
    return l->n == strlen(want) && memcmp(l->b, want, l->n) == 0;
}

/* --- ring walk collector --- */
static char g_walk[DW3_LOG_RING_LINES * (DW3_LOG_RING_LINE_MAX + 1)];
static unsigned g_walk_len;
static unsigned g_walk_lines;

static void collect(void *ctx, const char *line, unsigned len) {
    (void)ctx;
    memcpy(g_walk + g_walk_len, line, len);
    g_walk_len += len;
    g_walk_lines++;
}

static void walk_reset(void) {
    g_walk_len = 0;
    g_walk_lines = 0;
    memset(g_walk, 0, sizeof(g_walk));
}

static Dw3LogRing g_ring;

int main(void) {
    Dw3CrashLine l;
    /* A fake stack: frame records at increasing addresses. */
    uint64_t stack[64];
    uint64_t ret[DW3_CRASH_MAX_FRAMES];
    uint64_t lo;
    uint64_t hi;
    unsigned n;
    int stop;
    unsigned skipped;
    unsigned i;
    char buf[64];

    /* formatter */
    dw3_cl_reset(&l);
    dw3_cl_hex(&l, 0x207, 0);
    expect("hex_min0", line_is(&l, "0x207"));
    dw3_cl_reset(&l);
    dw3_cl_hex(&l, 0, 0);
    expect("hex_zero", line_is(&l, "0x0"));
    dw3_cl_reset(&l);
    dw3_cl_hex(&l, 0xb699233ecull, 16);
    expect("hex_pad16", line_is(&l, "0x0000000b699233ec"));
    dw3_cl_reset(&l);
    dw3_cl_hex(&l, 0xffffffffffffffffull, 16);
    expect("hex_max", line_is(&l, "0xffffffffffffffff"));
    dw3_cl_reset(&l);
    dw3_cl_dec(&l, 0);
    dw3_cl_char(&l, ' ');
    dw3_cl_dec(&l, 18446744073709551615ull);
    expect("dec", line_is(&l, "0 18446744073709551615"));
    dw3_cl_reset(&l);
    dw3_cl_addr(&l, 0xb699233ecull, 0xb697c2000ull, 0x59f000);
    expect("addr_in_text", line_is(&l, "0x0000000b699233ec base+0x1613ec"));
    dw3_cl_reset(&l);
    dw3_cl_addr(&l, 0x5d92ecf0ull, 0xb697c2000ull, 0x59f000);
    expect("addr_outside", line_is(&l, "0x000000005d92ecf0"));
    dw3_cl_reset(&l);
    dw3_cl_addr(&l, 0xb697c2000ull + 0x59f000, 0xb697c2000ull, 0x59f000);
    expect("addr_end_exclusive", line_is(&l, "0x0000000b69d61000"));
    dw3_cl_reset(&l);
    for (i = 0; i < 400; i++) {
        dw3_cl_char(&l, 'a');
    }
    dw3_cl_str(&l, "overflow");
    expect("line_bounded", l.n == DW3_CRASH_LINE_MAX);
    dw3_cl_reset(&l);
    dw3_cl_str(&l, NULL);
    expect("str_null", line_is(&l, "(null)"));

    /* ESR words: the rc6 OLED report (read of 0xd) and others. */
    dw3_cl_reset(&l);
    dw3_cl_esr(&l, 0x92000006u);
    expect("esr_rc6", line_is(&l, "data abort, read, translation fault L2"));
    dw3_cl_reset(&l);
    dw3_cl_esr(&l, 0x92000046u);
    expect("esr_write", line_is(&l, "data abort, write, translation fault L2"));
    dw3_cl_reset(&l);
    dw3_cl_esr(&l, 0x8200000fu);
    expect("esr_iabort", line_is(&l, "instruction abort, permission fault L3"));
    dw3_cl_reset(&l);
    dw3_cl_esr(&l, 0);
    expect("esr_none", line_is(&l, "none"));
    expect("lang_fr", strcmp(dw3_crash_eu_language_name(3), "fr") == 0);
    expect("lang_de", strcmp(dw3_crash_eu_language_name(5), "de") == 0);
    expect("lang_bad", strcmp(dw3_crash_eu_language_name(9), "unknown") == 0);

    /* software abort reasons (P5.4 PR 2) */
    dw3_cl_reset(&l);
    dw3_cl_reason_assert(&l, "/mnt/x/platforms/switch/psyz_backend/nx_crash.c",
                         812, "crash_force_assert",
                         "s_force_ptr != 0 && \"DW3_FORCE_CRASH=assert\"");
    expect("reason_assert",
           line_is(&l, "assert nx_crash.c:812 crash_force_assert: s_force_ptr "
                       "!= 0 && \"DW3_FORCE_CRASH=assert\""));
    dw3_cl_reset(&l);
    dw3_cl_reason_assert(&l, "C:\\src\\mprec.c", 99, NULL, "Balloc succeeded");
    expect("reason_assert_nofunc",
           line_is(&l, "assert mprec.c:99: Balloc succeeded"));
    dw3_cl_reset(&l);
    dw3_cl_reason_assert(&l, NULL, -1, "", NULL);
    expect("reason_assert_nulls", line_is(&l, "assert (null):-1: (no expression)"));
    {
        char longexpr[400];
        memset(longexpr, 'x', sizeof(longexpr) - 1);
        longexpr[sizeof(longexpr) - 1] = '\0';
        dw3_cl_reset(&l);
        dw3_cl_reason_assert(&l, "a.c", 1, "f", longexpr);
        expect("reason_assert_cap",
               l.n == DW3_CRASH_REASON_MAX && memcmp(l.b + l.n - 3, "...", 3) == 0);
    }
    dw3_cl_reset(&l);
    /* MAKERESULT(Module_Libnx = 345, LibnxError_ShouldNotHappen = 4) */
    dw3_cl_reason_fatal(&l, "diagAbortWithResult", 345u | (4u << 9));
    expect("reason_fatal",
           line_is(&l, "fatal 0x00000959 (2345-0004, diagAbortWithResult)"));
    dw3_cl_reset(&l);
    dw3_cl_reason_fatal(&l, "fatalThrow", 0xc815u);
    expect("reason_fatal_code", line_is(&l, "fatal 0x0000c815 (2021-0100, fatalThrow)"));
    dw3_cl_reset(&l);
    dw3_cl_reason_exit(&l, 3);
    expect("reason_exit", line_is(&l, "exit 3"));
    dw3_cl_reset(&l);
    dw3_cl_reason_exit(&l, -2147483647 - 1);
    expect("reason_exit_min", line_is(&l, "exit -2147483648"));
    expect("basename_plain", strcmp(dw3_crash_basename("main.c"), "main.c") == 0);
    expect("basename_trailing", strcmp(dw3_crash_basename("dir/"), "") == 0);

    /* frame walk: records at stack[8], [16], [24]; the last links to 0. */
    memset(stack, 0, sizeof(stack));
    lo = (uint64_t)(uintptr_t)&stack[4];
    hi = (uint64_t)(uintptr_t)&stack[64];
    stack[8] = (uint64_t)(uintptr_t)&stack[16];
    stack[9] = 0x1111;
    stack[16] = (uint64_t)(uintptr_t)&stack[24];
    stack[17] = 0x2222;
    stack[24] = 0;
    stack[25] = 0x3333;
    n = dw3_crash_walk_fp((uint64_t)(uintptr_t)&stack[8], lo, hi, ret,
                          DW3_CRASH_MAX_FRAMES, &stop);
    expect("walk_three", n == 3 && ret[0] == 0x1111 && ret[1] == 0x2222 &&
                             ret[2] == 0x3333);
    expect("walk_end", stop == DW3_FW_END);
    n = dw3_crash_walk_fp(0, lo, hi, ret, DW3_CRASH_MAX_FRAMES, &stop);
    expect("walk_fp0", n == 0 && stop == DW3_FW_END);
    /* below sp */
    n = dw3_crash_walk_fp((uint64_t)(uintptr_t)&stack[0], lo, hi, ret,
                          DW3_CRASH_MAX_FRAMES, &stop);
    expect("walk_below_sp", n == 0 && stop == DW3_FW_RANGE);
    /* record straddling the end of the stack */
    n = dw3_crash_walk_fp((uint64_t)(uintptr_t)&stack[63], lo, hi, ret,
                          DW3_CRASH_MAX_FRAMES, &stop);
    expect("walk_straddle", n == 0 && stop == DW3_FW_RANGE);
    /* wild pointer far away: never dereferenced */
    n = dw3_crash_walk_fp(0x10ull, lo, hi, ret, DW3_CRASH_MAX_FRAMES, &stop);
    expect("walk_wild", n == 0 && stop == DW3_FW_RANGE);
    n = dw3_crash_walk_fp(0xfffffffffffffff8ull, lo, hi, ret,
                          DW3_CRASH_MAX_FRAMES, &stop);
    expect("walk_wild_high", n == 0 && stop == DW3_FW_RANGE);
    /* misaligned */
    n = dw3_crash_walk_fp((uint64_t)(uintptr_t)&stack[8] + 4, lo, hi, ret,
                          DW3_CRASH_MAX_FRAMES, &stop);
    expect("walk_misaligned", n == 0 && stop == DW3_FW_ALIGN);
    /* loop: [16] links back to [8] */
    stack[16] = (uint64_t)(uintptr_t)&stack[8];
    n = dw3_crash_walk_fp((uint64_t)(uintptr_t)&stack[8], lo, hi, ret,
                          DW3_CRASH_MAX_FRAMES, &stop);
    expect("walk_loop", n == 2 && stop == DW3_FW_ORDER);
    /* self loop */
    stack[8] = (uint64_t)(uintptr_t)&stack[8];
    n = dw3_crash_walk_fp((uint64_t)(uintptr_t)&stack[8], lo, hi, ret,
                          DW3_CRASH_MAX_FRAMES, &stop);
    expect("walk_self", n == 1 && stop == DW3_FW_ORDER);
    /* null saved lr */
    stack[8] = (uint64_t)(uintptr_t)&stack[16];
    stack[9] = 0;
    n = dw3_crash_walk_fp((uint64_t)(uintptr_t)&stack[8], lo, hi, ret,
                          DW3_CRASH_MAX_FRAMES, &stop);
    expect("walk_null_lr", n == 0 && stop == DW3_FW_NULL_LR);
    /* long chain: capped at the limit */
    for (i = 4; i + 2 < 64; i += 2) {
        stack[i] = (uint64_t)(uintptr_t)&stack[i + 2];
        stack[i + 1] = 0x100 + i;
    }
    stack[62] = 0;
    stack[63] = 0x999;
    n = dw3_crash_walk_fp((uint64_t)(uintptr_t)&stack[4], lo, hi, ret, 100,
                          &stop);
    expect("walk_cap", n == DW3_CRASH_MAX_FRAMES && stop == DW3_FW_MAX);

    /* log ring */
    walk_reset();
    n = dw3_log_ring_walk(&g_ring, collect, NULL, &skipped);
    expect("ring_empty", n == 0 && skipped == 0);
    dw3_log_ring_push(&g_ring, "a\n", 2);
    dw3_log_ring_push(&g_ring, "b\n", 2);
    dw3_log_ring_push(&g_ring, "", 0);
    walk_reset();
    n = dw3_log_ring_walk(&g_ring, collect, NULL, &skipped);
    expect("ring_two", n == 2 && strcmp(g_walk, "a\nb\n") == 0);
    for (i = 0; i < 100; i++) {
        int len = snprintf(buf, sizeof(buf), "line %u\n", i);
        dw3_log_ring_push(&g_ring, buf, len);
    }
    walk_reset();
    n = dw3_log_ring_walk(&g_ring, collect, NULL, &skipped);
    expect("ring_wrap_count", n == DW3_LOG_RING_LINES && skipped == 0);
    expect("ring_oldest", strncmp(g_walk, "line 36\n", 8) == 0);
    expect("ring_newest", g_walk_len >= 8 &&
                              strcmp(g_walk + g_walk_len - 8, "line 99\n") == 0);
    /* a slot caught mid-write (tag cleared) is skipped, not printed */
    g_ring.tag[(g_ring.seq - 1) & (DW3_LOG_RING_LINES - 1)] = 0;
    walk_reset();
    n = dw3_log_ring_walk(&g_ring, collect, NULL, &skipped);
    expect("ring_torn_skipped", n == DW3_LOG_RING_LINES - 1 && skipped == 1);
    /* over-long line truncated to the slot */
    {
        static char big[1000];
        memset(big, 'x', sizeof(big));
        dw3_log_ring_push(&g_ring, big, (int)sizeof(big));
        walk_reset();
        dw3_log_ring_walk(&g_ring, collect, NULL, &skipped);
        expect("ring_truncate",
               g_walk_len >= DW3_LOG_RING_LINE_MAX &&
                   g_walk[g_walk_len - 1] == 'x' &&
                   g_walk[g_walk_len - DW3_LOG_RING_LINE_MAX - 1] == '\n');
    }

    return g_fail;
}
