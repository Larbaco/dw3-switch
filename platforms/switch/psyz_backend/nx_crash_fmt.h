/* P5.4 — formatting and frame-walk helpers for the Switch crash report.
 *
 * The exception handler (nx_crash.c) must not use stdio, malloc or locks,
 * so the report is built with these: a fixed line buffer with string / hex /
 * decimal appenders, and an AArch64 frame-pointer walker that validates
 * every frame record before reading it. No libnx types: the host test
 * (tools/tests/crash_fmt_main.c) compiles this header with gcc.
 */
#ifndef DW3_NX_CRASH_FMT_H
#define DW3_NX_CRASH_FMT_H

#include <stdint.h>

#define DW3_CRASH_LINE_MAX 320u
#define DW3_CRASH_MAX_FRAMES 16u

/* --- line buffer ------------------------------------------------------- */

typedef struct Dw3CrashLine {
    char b[DW3_CRASH_LINE_MAX];
    unsigned n;
} Dw3CrashLine;

static inline void dw3_cl_reset(Dw3CrashLine *l) { l->n = 0; }

static inline void dw3_cl_char(Dw3CrashLine *l, char c) {
    if (l->n < DW3_CRASH_LINE_MAX) {
        l->b[l->n++] = c;
    }
}

static inline void dw3_cl_mem(Dw3CrashLine *l, const char *s, unsigned n) {
    unsigned i;

    for (i = 0; i < n && l->n < DW3_CRASH_LINE_MAX; i++) {
        l->b[l->n++] = s[i];
    }
}

static inline void dw3_cl_str(Dw3CrashLine *l, const char *s) {
    if (!s) {
        s = "(null)";
    }
    while (*s && l->n < DW3_CRASH_LINE_MAX) {
        l->b[l->n++] = *s++;
    }
}

/* "0x" + at least min_digits lowercase hex digits (min_digits 0..16). */
static inline void dw3_cl_hex(Dw3CrashLine *l, uint64_t v, unsigned min_digits) {
    static const char digits[] = "0123456789abcdef";
    char tmp[16];
    unsigned n = 0;

    do {
        tmp[n++] = digits[v & 0xfu];
        v >>= 4;
    } while (v != 0 && n < 16);
    while (n < min_digits && n < 16) {
        tmp[n++] = '0';
    }
    dw3_cl_str(l, "0x");
    while (n > 0) {
        dw3_cl_char(l, tmp[--n]);
    }
}

static inline void dw3_cl_dec(Dw3CrashLine *l, uint64_t v) {
    char tmp[20];
    unsigned n = 0;

    do {
        tmp[n++] = (char)('0' + (unsigned)(v % 10u));
        v /= 10u;
    } while (v != 0 && n < 20);
    while (n > 0) {
        dw3_cl_char(l, tmp[--n]);
    }
}

/* Absolute address, then " base+0x..." when it is inside [base, base+size)
 * (the main module's text) — the offset addr2line wants for the .elf. */
static inline void dw3_cl_addr(Dw3CrashLine *l, uint64_t addr, uint64_t base,
                               uint64_t size) {
    dw3_cl_hex(l, addr, 16);
    if (base != 0 && addr >= base && addr - base < size) {
        dw3_cl_str(l, " base+");
        dw3_cl_hex(l, addr - base, 0);
    }
}

/* --- AArch64 frame-pointer walk ---------------------------------------- */

/* Why the walk stopped. END is the normal end of the chain (fp == 0). */
enum {
    DW3_FW_END = 0,       /* fp == 0: outermost frame reached */
    DW3_FW_MAX = 1,       /* DW3_CRASH_MAX_FRAMES recorded */
    DW3_FW_ALIGN = 2,     /* fp not 8-byte aligned */
    DW3_FW_RANGE = 3,     /* frame record not inside [lo, hi) */
    DW3_FW_ORDER = 4,     /* next fp not above this one (loop / garbage) */
    DW3_FW_NULL_LR = 5,   /* saved lr == 0 */
};

static inline const char *dw3_fw_stop_name(int stop) {
    switch (stop) {
    case DW3_FW_END:
        return "end of chain (fp=0)";
    case DW3_FW_MAX:
        return "frame limit";
    case DW3_FW_ALIGN:
        return "fp misaligned";
    case DW3_FW_RANGE:
        return "fp outside the stack";
    case DW3_FW_ORDER:
        return "fp not increasing";
    case DW3_FW_NULL_LR:
        return "saved lr is 0";
    default:
        return "unknown";
    }
}

/* Walk the AAPCS64 frame records from fp: each record is {next fp, saved lr}
 * at [fp, fp + 16). A record is read only when it is 8-byte aligned and
 * entirely inside [lo, hi) — the caller passes the faulting thread's stack
 * (lo = sp, hi = end of the readable mapping that holds sp), so the walk
 * cannot fault. The chain must strictly increase, so a loop ends it.
 * Saved lrs go to ret[0..]; returns how many, *stop says why it ended. */
static inline unsigned dw3_crash_walk_fp(uint64_t fp, uint64_t lo, uint64_t hi,
                                         uint64_t *ret, unsigned max,
                                         int *stop) {
    unsigned n = 0;
    int why = DW3_FW_END;

    if (max > DW3_CRASH_MAX_FRAMES) {
        max = DW3_CRASH_MAX_FRAMES;
    }
    for (;;) {
        const volatile uint64_t *rec;
        uint64_t next;
        uint64_t lr;

        if (fp == 0) {
            why = DW3_FW_END;
            break;
        }
        if (n >= max) {
            why = DW3_FW_MAX;
            break;
        }
        if ((fp & 7u) != 0) {
            why = DW3_FW_ALIGN;
            break;
        }
        if (fp < lo || hi < 16 || fp > hi - 16) {
            why = DW3_FW_RANGE;
            break;
        }
        rec = (const volatile uint64_t *)(uintptr_t)fp;
        next = rec[0];
        lr = rec[1];
        if (lr == 0) {
            why = DW3_FW_NULL_LR;
            break;
        }
        ret[n++] = lr;
        if (next != 0 && next <= fp) {
            why = DW3_FW_ORDER;
            break;
        }
        fp = next;
    }
    if (stop) {
        *stop = why;
    }
    return n;
}

/* --- readable context ---------------------------------------------------- */

/* Short words for an AArch64 ESR_EL1 taken from EL0: the exception class
 * and, for aborts, read/write and the fault status (ARM ARM D17.2.40). */
static inline void dw3_cl_esr(Dw3CrashLine *l, uint32_t esr) {
    unsigned ec = (esr >> 26) & 0x3fu;
    unsigned fsc = esr & 0x3fu;
    int is_abort = 0;

    if (esr == 0) {
        dw3_cl_str(l, "none");
        return;
    }
    switch (ec) {
    case 0x24:
        dw3_cl_str(l, "data abort");
        is_abort = 1;
        if (esr & (1u << 6)) {
            dw3_cl_str(l, ", write");
        } else {
            dw3_cl_str(l, ", read");
        }
        break;
    case 0x20:
        dw3_cl_str(l, "instruction abort");
        is_abort = 1;
        break;
    case 0x22:
        dw3_cl_str(l, "pc alignment");
        break;
    case 0x26:
        dw3_cl_str(l, "sp alignment");
        break;
    case 0x3c:
        dw3_cl_str(l, "brk");
        break;
    case 0x0e:
        dw3_cl_str(l, "illegal state");
        break;
    case 0x00:
        dw3_cl_str(l, "unknown / undefined instruction");
        break;
    default:
        dw3_cl_str(l, "ec ");
        dw3_cl_hex(l, ec, 2);
        break;
    }
    if (!is_abort) {
        return;
    }
    dw3_cl_str(l, ", ");
    if (fsc <= 0x03u) {
        dw3_cl_str(l, "address size fault L");
        dw3_cl_dec(l, fsc & 3u);
    } else if (fsc <= 0x07u) {
        dw3_cl_str(l, "translation fault L");
        dw3_cl_dec(l, fsc & 3u);
    } else if (fsc >= 0x09u && fsc <= 0x0bu) {
        dw3_cl_str(l, "access flag fault L");
        dw3_cl_dec(l, fsc & 3u);
    } else if (fsc >= 0x0du && fsc <= 0x0fu) {
        dw3_cl_str(l, "permission fault L");
        dw3_cl_dec(l, fsc & 3u);
    } else if (fsc == 0x21u) {
        dw3_cl_str(l, "alignment fault");
    } else {
        dw3_cl_str(l, "fsc ");
        dw3_cl_hex(l, fsc, 2);
    }
}

/* EU LANGUAGE (engine/file.h, set by CNTY_SEL): 0 is the Japanese-logo
 * bank, 1 and 2 English, then French, Italian, German, Spanish
 * (port/options/title_logo_capture.c reads the banks the same way). */
static inline const char *dw3_crash_eu_language_name(int32_t language) {
    switch (language) {
    case 0:
        return "jp-logo";
    case 1:
    case 2:
        return "en";
    case 3:
        return "fr";
    case 4:
        return "it";
    case 5:
        return "de";
    case 6:
        return "es";
    default:
        return "unknown";
    }
}

/* --- software abort reasons (P5.4 PR 2) ------------------------------------ */

/* The reason line stays short enough that version / reason / mode_file /
 * stage_file fit in the first 512 bytes the next boot's latch reads. */
#define DW3_CRASH_REASON_MAX 192u

/* Last path component (both separators), for __FILE__ strings. */
static inline const char *dw3_crash_basename(const char *path) {
    const char *base = path;

    if (!path) {
        return "(null)";
    }
    for (; *path; path++) {
        if (*path == '/' || *path == '\\') {
            base = path + 1;
        }
    }
    return base;
}

/* Signed decimal (exit codes). */
static inline void dw3_cl_sdec(Dw3CrashLine *l, int64_t v) {
    if (v < 0) {
        dw3_cl_char(l, '-');
        dw3_cl_dec(l, (uint64_t)0 - (uint64_t)v);
    } else {
        dw3_cl_dec(l, (uint64_t)v);
    }
}

/* Keep at most max bytes; a cut line ends in "...". */
static inline void dw3_cl_cap(Dw3CrashLine *l, unsigned max) {
    if (max < 4 || l->n <= max) {
        return;
    }
    l->n = max - 3;
    dw3_cl_str(l, "...");
}

/* "assert <file>:<line> <func>: <expr>" (newlib __assert_func arguments;
 * func may be NULL). */
static inline void dw3_cl_reason_assert(Dw3CrashLine *l, const char *file,
                                        int line, const char *func,
                                        const char *expr) {
    dw3_cl_str(l, "assert ");
    dw3_cl_str(l, dw3_crash_basename(file));
    dw3_cl_char(l, ':');
    dw3_cl_sdec(l, line);
    if (func && func[0]) {
        dw3_cl_char(l, ' ');
        dw3_cl_str(l, func);
    }
    dw3_cl_str(l, ": ");
    dw3_cl_str(l, expr ? expr : "(no expression)");
    dw3_cl_cap(l, DW3_CRASH_REASON_MAX);
}

/* Horizon result as the console shows it: 2000 + module, then the
 * description (0xc815 -> "2021-0100"). */
static inline void dw3_cl_result_code(Dw3CrashLine *l, uint32_t rc) {
    uint32_t module = rc & 0x1ffu;
    uint32_t desc = (rc >> 9) & 0x1fffu;
    unsigned w;
    uint32_t div;

    dw3_cl_dec(l, 2000u + module);
    dw3_cl_char(l, '-');
    for (w = 0, div = 1000; w < 4; w++, div /= 10) {
        dw3_cl_char(l, (char)('0' + (desc / div) % 10u));
    }
}

/* "fatal 0x0000c815 (2021-0100, diagAbortWithResult)". */
static inline void dw3_cl_reason_fatal(Dw3CrashLine *l, const char *what,
                                       uint32_t rc) {
    dw3_cl_str(l, "fatal ");
    dw3_cl_hex(l, rc, 8);
    dw3_cl_str(l, " (");
    dw3_cl_result_code(l, rc);
    dw3_cl_str(l, ", ");
    dw3_cl_str(l, what);
    dw3_cl_char(l, ')');
    dw3_cl_cap(l, DW3_CRASH_REASON_MAX);
}

/* "exit <code>" for a non-zero exit nobody announced. */
static inline void dw3_cl_reason_exit(Dw3CrashLine *l, int code) {
    dw3_cl_str(l, "exit ");
    dw3_cl_sdec(l, code);
}

#endif /* DW3_NX_CRASH_FMT_H */
