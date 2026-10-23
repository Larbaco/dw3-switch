/* P5.4 — user-exception crash dumper for the Switch game image.
 *
 * Provides __libnx_exception_handler + a larger __nx_exception_stack so a CPU
 * fault writes a readable report under sdmc:/switch/dw3/crash_report.txt:
 * version / git / build-id, mode and stage overlay (id + name), disc and EU
 * language, vsync frame counters, registers with main-module offsets
 * (base+0x...), a validated frame-pointer backtrace, and the last ~64 log
 * lines. tools/symbolize_crash.py turns the offsets into file:line with the
 * matching .elf. The handler also appends a `CRASH` line to dw3.log.
 *
 * Handler rules: no malloc, no stdio, no locks, bounded stack (it runs on
 * the 16 KiB __nx_exception_stack). Text is built with nx_crash_fmt.h into a
 * static buffer and written with libnx fsFileWrite on a sink opened at boot.
 *
 * Install also rotates a report left over from the previous run to
 * crash_report_prev.txt and latches its summary into the log, so one crash
 * does not overwrite the next. Soft default: handler always linked; forced
 * fault is opt-in (DW3_NX_FORCE_CRASH builds only).
 */
#include "nx_crash.h"

#include "nx.h"
#include "nx_crash_fmt.h"

#include <stdalign.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <switch.h>

#ifdef DW3_NX_GAME_IMAGE
#include "overlay_host.h"
#endif

#ifndef DW3_NX_FORCE_CRASH
#define DW3_NX_FORCE_CRASH 0
#endif

#ifndef DW3_NRO_VERSION_STR
#define DW3_NRO_VERSION_STR "unknown"
#endif
#ifndef DW3_GIT_HASH_STR
#define DW3_GIT_HASH_STR "unknown"
#endif

#if defined(DW3_NX_GAME_IMAGE) && defined(VERSION_EU)
#define DW3_CRASH_DISC "EU"
extern int32_t LANGUAGE; /* engine/file.h; set by CNTY_SEL */
#elif defined(DW3_NX_GAME_IMAGE) && defined(VERSION_US)
#define DW3_CRASH_DISC "US"
#else
#define DW3_CRASH_DISC "n/a"
#endif

/* Device-relative path for FsFileSystem ("sdmc:" is the mount name). */
#define DW3_NX_CRASH_FS_PATH "/switch/dw3/crash_report.txt"
#define DW3_NX_CRASH_FS_PATH_PREV "/switch/dw3/crash_report_prev.txt"
#define DW3_NX_CRASH_LOG_HINT "sdmc:/switch/dw3/dw3.log"
#define DW3_NX_CRASH_REPORT_HINT "sdmc:/switch/dw3/crash_report.txt"
#define DW3_NX_CRASH_PREV_HINT "sdmc:/switch/dw3/crash_report_prev.txt"

/* Larger than libnx's weak 0x400 default — handler formatting needs room. */
alignas(16) u8 __nx_exception_stack[0x4000];
u64 __nx_exception_stack_size = sizeof(__nx_exception_stack);
/* Allow the handler under nxlink / gdb so the SD report still lands. */
u32 __nx_exception_ignoredebug = 1;

static FsFile s_crash_file;
static int s_crash_file_ok;
static s64 s_crash_off;
static volatile int s_in_handler;
/* Frame-loop progress noted from the vsync pump (report-only; no locking:
 * the handler runs on whichever thread faulted, and a torn value is an
 * acceptable cost for two counters). */
static volatile uint32_t s_note_pumps;
static volatile uint64_t s_note_dt_us;

/* Main module text mapping and GNU build-id, found once at install. */
static u64 s_text_base;
static u64 s_text_size;
static u8 s_build_id[32];
static unsigned s_build_id_len;

/* The report is built here and written in a few large fsFileWrite calls
 * (one per section) rather than one flushed write per line. */
#define DW3_CRASH_BUF_SIZE 0x8000
static char s_report_buf[DW3_CRASH_BUF_SIZE];
static unsigned s_report_len;

/* --- sink ---------------------------------------------------------------- */

static void crash_flush(void) {
    if (s_report_len == 0) {
        return;
    }
    if (s_crash_file_ok) {
        /* fsFileWrite does not extend past EOF — grow first, then write. */
        if (R_SUCCEEDED(fsFileSetSize(&s_crash_file,
                                      s_crash_off + s_report_len)) &&
            R_SUCCEEDED(fsFileWrite(&s_crash_file, s_crash_off, s_report_buf,
                                    s_report_len, FsWriteOption_Flush))) {
            s_crash_off += s_report_len;
        }
    }
    s_report_len = 0;
}

static void crash_emit(const char *buf, unsigned len) {
    if (len == 0) {
        return;
    }
    /* Live channel for emulators and gdb (no lag, unlike Ryubing's sdmc:). */
    svcOutputDebugString(buf, (u64)len);
    if (s_report_len + len > sizeof(s_report_buf)) {
        crash_flush();
    }
    if (len > sizeof(s_report_buf)) {
        len = sizeof(s_report_buf);
    }
    memcpy(s_report_buf + s_report_len, buf, len);
    s_report_len += len;
}

static void crash_line_end(Dw3CrashLine *l) {
    if (l->n >= DW3_CRASH_LINE_MAX) {
        l->n = DW3_CRASH_LINE_MAX - 1;
    }
    l->b[l->n++] = '\n';
    crash_emit(l->b, l->n);
    dw3_cl_reset(l);
}

static void crash_kv(Dw3CrashLine *l, const char *key, const char *val) {
    dw3_cl_str(l, key);
    dw3_cl_str(l, val);
    crash_line_end(l);
}

/* --- module identity ----------------------------------------------------- */

/* The text mapping that holds our own code is the main module's: elf2nro
 * puts .text at offset 0, so its base is what the .elf calls address 0. */
static void crash_find_text(u64 *base, u64 *size) {
    MemoryInfo mi;
    u32 pi;

    *base = 0;
    *size = 0;
    memset(&mi, 0, sizeof(mi));
    if (R_SUCCEEDED(svcQueryMemory(&mi, &pi,
                                   (u64)(uintptr_t)&crash_find_text)) &&
        (mi.perm & Perm_X) != 0) {
        *base = mi.addr;
        *size = mi.size;
    }
}

/* The linker's .note.gnu.build-id is the last section of the read-only
 * segment that follows .text. Scan that mapping backwards for the note
 * header (namesz 4, type NT_GNU_BUILD_ID, "GNU\0"). Boot-time only. */
static void crash_find_build_id(void) {
    MemoryInfo mi;
    u32 pi;
    const volatile u32 *w;
    u64 n;
    u64 i;

    s_build_id_len = 0;
    if (s_text_base == 0) {
        return;
    }
    memset(&mi, 0, sizeof(mi));
    if (R_FAILED(svcQueryMemory(&mi, &pi, s_text_base + s_text_size)) ||
        mi.addr != s_text_base + s_text_size || (mi.perm & Perm_R) == 0 ||
        mi.size < 32) {
        return;
    }
    w = (const volatile u32 *)(uintptr_t)mi.addr;
    n = mi.size / 4;
    for (i = n - 4; i > 0; i--) {
        u32 descsz;

        if (w[i] != 4u || w[i + 2] != 3u || w[i + 3] != 0x00554e47u) {
            continue;
        }
        descsz = w[i + 1];
        if (descsz < 8 || descsz > sizeof(s_build_id) ||
            i + 4 + (descsz + 3) / 4 > n) {
            continue;
        }
        memcpy(s_build_id, (const void *)(uintptr_t)(mi.addr + (i + 4) * 4),
               descsz);
        s_build_id_len = descsz;
        return;
    }
}

static void crash_cl_build_id(Dw3CrashLine *l) {
    static const char digits[] = "0123456789abcdef";
    unsigned i;

    if (s_build_id_len == 0) {
        dw3_cl_str(l, "unknown");
        return;
    }
    for (i = 0; i < s_build_id_len; i++) {
        dw3_cl_char(l, digits[s_build_id[i] >> 4]);
        dw3_cl_char(l, digits[s_build_id[i] & 0xf]);
    }
}

/* --- report body ----------------------------------------------------------- */

static const char *error_desc_name(u32 desc) {
    switch (desc) {
    case ThreadExceptionDesc_InstructionAbort:
        return "InstructionAbort";
    case ThreadExceptionDesc_MisalignedPC:
        return "MisalignedPC";
    case ThreadExceptionDesc_MisalignedSP:
        return "MisalignedSP";
    case ThreadExceptionDesc_SError:
        return "SError";
    case ThreadExceptionDesc_BadSVC:
        return "BadSVC";
    case ThreadExceptionDesc_Trap:
        return "Trap";
    case ThreadExceptionDesc_Other:
        return "Other";
    default:
        return "Unknown";
    }
}

static void crash_cl_file(Dw3CrashLine *l, const char *key, int32_t file) {
    dw3_cl_str(l, key);
    dw3_cl_hex(l, (u32)file, 0);
#ifdef DW3_NX_GAME_IMAGE
    {
        const char *name = file != 0 ? dw3_overlay_file_name(file) : NULL;
        if (name) {
            dw3_cl_str(l, " (");
            dw3_cl_str(l, name);
            dw3_cl_char(l, ')');
        } else if (file == 0) {
            dw3_cl_str(l, " (none)");
        }
    }
#endif
    crash_line_end(l);
}

static void crash_write_head(Dw3CrashLine *l, const char *reason) {
    int32_t mode_file = 0;
    int32_t stage_file = 0;

#ifdef DW3_NX_GAME_IMAGE
    mode_file = dw3_overlay_current_mode_file();
    stage_file = dw3_overlay_current_sub_file();
#endif

    /* Keep version / reason / mode_file / stage_file in the first 512 bytes:
     * the next boot's latch (crash_latch_previous) reads only that much. */
    crash_kv(l, "dw3-switch crash report", "");
    crash_kv(l, "version: ", DW3_NRO_VERSION_STR);
    crash_kv(l, "reason: ", reason ? reason : "(none)");
    crash_cl_file(l, "mode_file: ", mode_file);
    crash_cl_file(l, "stage_file: ", stage_file);
    dw3_cl_str(l, "disc: ");
    dw3_cl_str(l, DW3_CRASH_DISC);
#if defined(DW3_NX_GAME_IMAGE) && defined(VERSION_EU)
    dw3_cl_str(l, " language: ");
    dw3_cl_dec(l, (u32)LANGUAGE);
    dw3_cl_str(l, " (");
    dw3_cl_str(l, dw3_crash_eu_language_name(LANGUAGE));
    dw3_cl_char(l, ')');
#endif
    crash_line_end(l);
    crash_kv(l, "git: ", DW3_GIT_HASH_STR);
    dw3_cl_str(l, "build_id: ");
    crash_cl_build_id(l);
    crash_line_end(l);
    /* Frame-loop liveness at the fault: pumps completed and the last sampled
     * 60-pump frame dt (0 = the fault hit before the first sample). */
    dw3_cl_str(l, "vsync_pumps: ");
    dw3_cl_dec(l, s_note_pumps);
    crash_line_end(l);
    dw3_cl_str(l, "last_dt_us: ");
    dw3_cl_dec(l, s_note_dt_us);
    crash_line_end(l);
    crash_kv(l, "log: ", DW3_NX_CRASH_LOG_HINT);
    crash_kv(l, "report: ", DW3_NX_CRASH_REPORT_HINT);
    dw3_cl_str(l, "module_base: ");
    dw3_cl_hex(l, s_text_base, 16);
    dw3_cl_str(l, " text_size: ");
    dw3_cl_hex(l, s_text_size, 0);
    crash_line_end(l);
}

static void crash_reg(Dw3CrashLine *l, const char *key, u64 v, int code) {
    dw3_cl_str(l, key);
    if (code) {
        dw3_cl_addr(l, v, s_text_base, s_text_size);
    } else {
        dw3_cl_hex(l, v, 16);
    }
    crash_line_end(l);
}

static void crash_write_regs(Dw3CrashLine *l, const ThreadExceptionDump *ctx) {
    unsigned i;

    dw3_cl_str(l, "error_desc: ");
    dw3_cl_hex(l, ctx->error_desc, 0);
    dw3_cl_str(l, " (");
    dw3_cl_str(l, error_desc_name(ctx->error_desc));
    dw3_cl_char(l, ')');
    crash_line_end(l);
    crash_reg(l, "pc:  ", ctx->pc.x, 1);
    crash_reg(l, "lr:  ", ctx->lr.x, 1);
    crash_reg(l, "sp:  ", ctx->sp.x, 0);
    crash_reg(l, "fp:  ", ctx->fp.x, 0);
    crash_reg(l, "far: ", ctx->far.x, 0);
    dw3_cl_str(l, "esr: ");
    dw3_cl_hex(l, ctx->esr, 8);
    dw3_cl_str(l, " (");
    dw3_cl_esr(l, ctx->esr);
    dw3_cl_char(l, ')');
    crash_line_end(l);
    dw3_cl_str(l, "pstate: ");
    dw3_cl_hex(l, ctx->pstate, 8);
    crash_line_end(l);
    for (i = 0; i < 29; i++) {
        dw3_cl_char(l, 'x');
        dw3_cl_dec(l, i);
        dw3_cl_str(l, i < 10 ? ":  " : ": ");
        dw3_cl_addr(l, ctx->cpu_gprs[i].x, s_text_base, s_text_size);
        crash_line_end(l);
    }
}

/* Frame records live on the faulting thread's stack: [sp, end of the
 * readable mapping that holds sp). Anything else is not read. */
static void crash_write_backtrace(Dw3CrashLine *l,
                                  const ThreadExceptionDump *ctx) {
    MemoryInfo mi;
    u32 pi;
    u64 frames[DW3_CRASH_MAX_FRAMES];
    unsigned n;
    unsigned i;
    int stop = DW3_FW_END;

    crash_kv(l, "--- backtrace (fp chain; saved lr per frame) ---", "");
    memset(&mi, 0, sizeof(mi));
    if (R_FAILED(svcQueryMemory(&mi, &pi, ctx->sp.x)) ||
        (mi.perm & Perm_R) == 0 || mi.type == MemType_Unmapped) {
        dw3_cl_str(l, "stack: sp not in readable memory (type ");
        dw3_cl_hex(l, mi.type, 0);
        dw3_cl_str(l, " perm ");
        dw3_cl_hex(l, mi.perm, 0);
        dw3_cl_str(l, "); no backtrace");
        crash_line_end(l);
        return;
    }
    dw3_cl_str(l, "stack: ");
    dw3_cl_hex(l, mi.addr, 16);
    dw3_cl_str(l, "..");
    dw3_cl_hex(l, mi.addr + mi.size, 16);
    dw3_cl_str(l, " type ");
    dw3_cl_hex(l, mi.type, 0);
    crash_line_end(l);
    n = dw3_crash_walk_fp(ctx->fp.x, ctx->sp.x, mi.addr + mi.size, frames,
                          DW3_CRASH_MAX_FRAMES, &stop);
    for (i = 0; i < n; i++) {
        dw3_cl_char(l, '#');
        if (i < 10) {
            dw3_cl_char(l, '0');
        }
        dw3_cl_dec(l, i);
        dw3_cl_char(l, ' ');
        dw3_cl_addr(l, frames[i], s_text_base, s_text_size);
        crash_line_end(l);
    }
    dw3_cl_str(l, "backtrace_end: ");
    dw3_cl_dec(l, n);
    dw3_cl_str(l, " frames, ");
    dw3_cl_str(l, dw3_fw_stop_name(stop));
    crash_line_end(l);
}

static void crash_ring_emit(void *ctx, const char *line, unsigned len) {
    (void)ctx;
    crash_emit(line, len);
    if (len == 0 || line[len - 1] != '\n') {
        crash_emit("\n", 1); /* truncated line */
    }
}

static void crash_write_log_tail(Dw3CrashLine *l) {
    unsigned skipped = 0;
    unsigned n;

    crash_kv(l, "--- last log lines ---", "");
    n = dw3_nx_log_ring_walk(crash_ring_emit, NULL, &skipped);
    dw3_cl_str(l, "--- ");
    dw3_cl_dec(l, n);
    dw3_cl_str(l, " log lines");
    if (skipped) {
        dw3_cl_str(l, ", ");
        dw3_cl_dec(l, skipped);
        dw3_cl_str(l, " skipped mid-write");
    }
    dw3_cl_str(l, " ---");
    crash_line_end(l);
}

static void crash_write_body(const char *reason, const ThreadExceptionDump *ctx) {
    Dw3CrashLine l;

    dw3_cl_reset(&l);
    s_report_len = 0;
    if (s_text_base == 0) {
        crash_find_text(&s_text_base, &s_text_size);
    }
    crash_write_head(&l, reason);
    if (ctx) {
        crash_write_regs(&l, ctx);
        crash_flush(); /* the fields above are on SD before the walks */
        crash_write_backtrace(&l, ctx);
        crash_flush();
    }
    crash_write_log_tail(&l);
    crash_kv(&l, "--- end ---", "");
    crash_flush();
}

/* One `[E]` line at the end of dw3.log pointing at the report. */
static void crash_log_marker(const char *reason, const ThreadExceptionDump *ctx) {
    Dw3CrashLine l;

    dw3_cl_reset(&l);
    dw3_cl_str(&l, "[E][nx_crash.c][crash] CRASH ");
    dw3_cl_str(&l, reason);
    if (ctx) {
        dw3_cl_str(&l, " pc=");
        dw3_cl_addr(&l, ctx->pc.x, s_text_base, s_text_size);
        dw3_cl_str(&l, " far=");
        dw3_cl_hex(&l, ctx->far.x, 0);
    }
    dw3_cl_str(&l, " version=");
    dw3_cl_str(&l, DW3_NRO_VERSION_STR);
    dw3_cl_str(&l, "; see ");
    dw3_cl_str(&l, DW3_NX_CRASH_REPORT_HINT);
    if (l.n >= DW3_CRASH_LINE_MAX) {
        l.n = DW3_CRASH_LINE_MAX - 1;
    }
    l.b[l.n++] = '\n';
    svcOutputDebugString(l.b, l.n);
    dw3_nx_log_crash_append(l.b, (int)l.n);
}

static int crash_open_sink(void) {
    FsFileSystem *fs = fsdevGetDeviceFileSystem("sdmc");
    Result rc;

    if (!fs) {
        return 0;
    }
    if (s_crash_file_ok) {
        fsFileClose(&s_crash_file);
        s_crash_file_ok = 0;
        s_crash_off = 0;
    }
    /* Create if missing; ignore AlreadyExists. */
    rc = fsFsCreateFile(fs, DW3_NX_CRASH_FS_PATH, 0, 0);
    (void)rc;
    if (R_FAILED(fsFsOpenFile(fs, DW3_NX_CRASH_FS_PATH, FsOpenMode_Write,
                              &s_crash_file))) {
        return 0;
    }
    if (R_FAILED(fsFileSetSize(&s_crash_file, 0))) {
        fsFileClose(&s_crash_file);
        return 0;
    }
    s_crash_off = 0;
    s_crash_file_ok = 1;
    return 1;
}

/* Copy the value of "key" (a whole line in the report) into out (NUL-ended).
 * The head buffer is writable, so value scanning + newline stripping is safe. */
static void crash_scan_field(char *head, const char *key, char *out,
                             size_t out_sz) {
    const char *line;
    const char *val;
    size_t klen = strlen(key);
    size_t i;

    line = head;
    while ((val = strstr(line, key)) != NULL) {
        /* Require the key at the start of a line: a value occurrence of a
         * shorter key (mode_file inside stage_file) must not match. */
        if (val == head || val[-1] == '\n') {
            break;
        }
        line = val + 1;
    }
    if (val == NULL) {
        snprintf(out, out_sz, "(none)");
        return;
    }
    val += klen;
    for (i = 0; i + 1 < out_sz && val[i] != '\0' && val[i] != '\n'; i++) {
        out[i] = val[i];
    }
    out[i] = '\0';
    if (i == 0) {
        snprintf(out, out_sz, "(none)");
    }
}

/* Startup "previous run crashed?" latch: before the sink is truncated, move
 * the previous report to crash_report_prev.txt and log its first fields.
 * Keeps one previous report (overwrite on a crash pair run); archival copies
 * stay the owner's evidence flow (AGENTS machine rule 9). */
static void crash_latch_previous(void) {
    FsFileSystem *fs = fsdevGetDeviceFileSystem("sdmc");
    FsFile prev;
    char head[0x200];
    u64 n = 0;
    Result rc;

    if (!fs) {
        return; /* No SD mount: nothing could have been written there. */
    }
    rc = fsFsOpenFile(fs, DW3_NX_CRASH_FS_PATH, FsOpenMode_Read, &prev);
    if (R_FAILED(rc)) {
        return; /* Nothing left from a previous run (normal boot path). */
    }
    if (R_FAILED(fsFileRead(&prev, 0, head, sizeof(head) - 1,
                            FsReadOption_None, &n))) {
        fsFileClose(&prev);
        return;
    }
    fsFileClose(&prev);
    if (n == 0 || n > sizeof(head) - 1) {
        return;
    }
    head[n] = '\0';
    if (memcmp(head, "dw3-switch crash report", 23) != 0) {
        return; /* Not our file; leave the bytes alone. */
    }

    /* Overwrite-protect the copy, then rotate. Delete-before-rename because
     * FAT rename semantics on an existing target are middleware-dependent. */
    fsFsDeleteFile(fs, DW3_NX_CRASH_FS_PATH_PREV);
    if (R_FAILED(fsFsRenameFile(fs, DW3_NX_CRASH_FS_PATH,
                                DW3_NX_CRASH_FS_PATH_PREV))) {
        psyz_log(2, __FILE__, __LINE__, __func__,
                 "crash report rotation failed; keep both %s files before "
                 "the next run", DW3_NX_CRASH_REPORT_HINT);
        return;
    }
    {
        /* Report fields are one per line (see crash_write_head). Extract the
         * first four and say them in one log line. */
        char version[40];
        char reason[64];
        char mode[32];
        char stage[32];

        crash_scan_field(head, "version: ", version, sizeof(version));
        crash_scan_field(head, "reason: ", reason, sizeof(reason));
        crash_scan_field(head, "mode_file: ", mode, sizeof(mode));
        crash_scan_field(head, "stage_file: ", stage, sizeof(stage));
        psyz_log(1, __FILE__, __LINE__, __func__,
                 "previous run crashed: version=%s reason=%s mode_file=%s "
                 "stage_file=%s (kept: %s)",
                 version, reason, mode, stage, DW3_NX_CRASH_PREV_HINT);
    }
}

#if DW3_NX_FORCE_CRASH
static uint32_t s_force_at_pump;
static void crash_force_now(void);
#endif

void dw3_nx_crash_note_vsync(uint32_t pumps, uint64_t last_dt_us) {
    s_note_pumps = pumps;
    s_note_dt_us = last_dt_us;
#if DW3_NX_FORCE_CRASH
    if (s_force_at_pump != 0 && pumps >= s_force_at_pump) {
        s_force_at_pump = 0;
        crash_force_now();
    }
#endif
}

void dw3_nx_crash_install(void) {
    char id[2 * sizeof(s_build_id) + 1];
    unsigned i;

    /* Rotate + latch any report left over from a previous run before
     * crash_open_sink() truncates the live path. */
    crash_latch_previous();
    crash_find_text(&s_text_base, &s_text_size);
    crash_find_build_id();
    for (i = 0; i < s_build_id_len; i++) {
        snprintf(id + 2 * i, 3, "%02x", s_build_id[i]);
    }
    id[2 * s_build_id_len] = '\0';
    /* Avoid PSY-Z INFOF here: this TU must stay free of <psyz.h> libc wraps. */
    psyz_log(1, __FILE__, __LINE__, __func__,
             "crash report ids: module_base=0x%llx text_size=0x%llx "
             "git=%s build_id=%s",
             (unsigned long long)s_text_base,
             (unsigned long long)s_text_size, DW3_GIT_HASH_STR,
             s_build_id_len ? id : "unknown");
    if (crash_open_sink()) {
        psyz_log(1, __FILE__, __LINE__, __func__,
                 "crash report sink ready (%s)", DW3_NX_CRASH_REPORT_HINT);
    } else {
        psyz_log(3, __FILE__, __LINE__, __func__,
                 "crash report sink open failed (%s)",
                 DW3_NX_CRASH_REPORT_HINT);
    }
}

void dw3_nx_crash_write_report(const char *reason) {
    if (!s_crash_file_ok) {
        crash_open_sink();
    } else {
        /* Truncate for a fresh software report. */
        if (R_SUCCEEDED(fsFileSetSize(&s_crash_file, 0))) {
            s_crash_off = 0;
        }
    }
    crash_write_body(reason ? reason : "software report", NULL);
    if (s_crash_file_ok) {
        fsFileFlush(&s_crash_file);
    }
}

static void crash_handle(const char *reason, const ThreadExceptionDump *ctx) {
    if (s_in_handler) {
        svcBreak(BreakReason_Panic, 0, 0);
        for (;;) {
            svcSleepThread(1000000000ULL);
        }
    }
    s_in_handler = 1;

    if (!s_crash_file_ok) {
        crash_open_sink();
    } else if (R_SUCCEEDED(fsFileSetSize(&s_crash_file, 0))) {
        s_crash_off = 0;
    }

    crash_write_body(reason, ctx);
    if (s_crash_file_ok) {
        fsFileFlush(&s_crash_file);
    }
    /* After the report, so a fault in here cannot cost the report. */
    crash_log_marker(reason, ctx);

    /* Re-raise so Atmosphere creport also captures the fault. */
    svcBreak(BreakReason_Panic, 0, 0);
    for (;;) {
        svcSleepThread(1000000000ULL);
    }
}

void __libnx_exception_handler(ThreadExceptionDump *ctx) {
    crash_handle("user exception", ctx);
}

#if DW3_NX_FORCE_CRASH
/* The fault sites are separate noinline functions so the report shows a
 * real chain: leaf store (pc) <- crash_force_now (lr) <- caller (frame #00)
 * <- ... The pointer comes from a volatile so GCC cannot see a constant
 * null store (and turn it into a trap); no sibling calls, so no frame of
 * the chain turns into a tail jump and drops out of the backtrace. */
#define DW3_FORCE_FN __attribute__((noinline, optimize("no-optimize-sibling-calls")))
static volatile uintptr_t s_force_ptr;
static int s_force_sim;

DW3_FORCE_FN static void crash_force_null_write(void) {
    volatile int *p = (volatile int *)s_force_ptr;
    *p = 0x445733; /* "DW3" — should not return */
}

/* Emulators (Ryubing) do not deliver a guest fault to the libnx handler, so
 * DW3_FORCE_CRASH=sim calls it directly with this thread's live registers.
 * Everything after the fault (report, backtrace, log tail, dw3.log marker,
 * svcBreak) is the same code path. */
DW3_FORCE_FN static void crash_force_sim(void) {
    static ThreadExceptionDump d;
    u64 fp;
    u64 sp;
    u64 pc;

    memset(&d, 0, sizeof(d));
    __asm__ volatile("mov %0, x29" : "=r"(fp));
    __asm__ volatile("mov %0, sp" : "=r"(sp));
    __asm__ volatile("adr %0, ." : "=r"(pc));
    d.error_desc = ThreadExceptionDesc_Other;
    d.fp.x = fp;
    d.sp.x = sp;
    d.pc.x = pc;
    d.lr.x = (u64)(uintptr_t)__builtin_return_address(0);
    d.far.x = 0;
    crash_handle("simulated fault (DW3_FORCE_CRASH=sim)", &d);
}

DW3_FORCE_FN static void crash_force_now(void) {
    psyz_log(1, __FILE__, __LINE__, __func__,
             "DW3_NX_FORCE_CRASH: %s at vsync pump %u",
             s_force_sim ? "simulated fault" : "deliberate null write",
             (unsigned)s_note_pumps);
    if (s_force_sim) {
        crash_force_sim();
    } else {
        crash_force_null_write();
    }
}

/* Boot hook. DW3_FORCE_CRASH=sim selects the emulator path above;
 * DW3_FORCE_CRASH_PUMPS=N defers the fault to vsync pump N (from the
 * vsync pump, inside the running game) instead of now. Both can be baked
 * with DW3_NX_TEST_ENV. */
DW3_FORCE_FN void dw3_nx_crash_force_fault(void) {
    const char *mode = getenv("DW3_FORCE_CRASH");
    const char *pumps = getenv("DW3_FORCE_CRASH_PUMPS");

    s_force_sim = mode != NULL && strcmp(mode, "sim") == 0;
    if (pumps != NULL && pumps[0] != '\0') {
        unsigned long n = strtoul(pumps, NULL, 0);
        if (n > 0) {
            s_force_at_pump = (uint32_t)n;
            psyz_log(1, __FILE__, __LINE__, __func__,
                     "DW3_NX_FORCE_CRASH: armed for vsync pump %lu (%s)", n,
                     s_force_sim ? "sim" : "null write");
            return;
        }
    }
    crash_force_now();
}
#endif
