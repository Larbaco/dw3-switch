/* P5.4 — user-exception crash dumper for the Switch game image.
 *
 * Provides __libnx_exception_handler + a larger __nx_exception_stack so a CPU
 * fault writes a readable report under sdmc:/switch/dw3/crash_report.txt
 * (mode/stage file ids + PC/LR/FAR/ESR). The sink is opened on the main
 * thread at install time via libnx fsFileWrite — no stdio lock in the
 * handler. Soft default: handler always linked; forced fault is opt-in.
 */
#include "nx_crash.h"

#include "nx.h"

#include <stdarg.h>
#include <stdalign.h>
#include <stdio.h>
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

/* Device-relative path for FsFileSystem ("sdmc:" is the mount name). */
#define DW3_NX_CRASH_FS_PATH "/switch/dw3/crash_report.txt"
#define DW3_NX_CRASH_LOG_HINT "sdmc:/switch/dw3/dw3.log"
#define DW3_NX_CRASH_REPORT_HINT "sdmc:/switch/dw3/crash_report.txt"

/* Larger than libnx's weak 0x400 default — handler formatting needs room. */
alignas(16) u8 __nx_exception_stack[0x4000];
u64 __nx_exception_stack_size = sizeof(__nx_exception_stack);
/* Allow the handler under nxlink / gdb so the SD report still lands. */
u32 __nx_exception_ignoredebug = 1;

static FsFile s_crash_file;
static int s_crash_file_ok;
static s64 s_crash_off;
static volatile int s_in_handler;

static void crash_emit(const char *buf, int len) {
    if (len <= 0) {
        return;
    }
    svcOutputDebugString(buf, (u64)len);
    if (!s_crash_file_ok) {
        return;
    }
    /* fsFileWrite does not extend past EOF — grow first, then write+flush. */
    if (R_FAILED(fsFileSetSize(&s_crash_file, s_crash_off + len))) {
        return;
    }
    if (R_SUCCEEDED(fsFileWrite(&s_crash_file, s_crash_off, buf, (u64)len,
                                FsWriteOption_Flush))) {
        s_crash_off += len;
    }
}

static void crash_printf(const char *fmt, ...) {
    char buf[512];
    va_list ap;
    int n;

    va_start(ap, fmt);
    n = vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    if (n < 0) {
        return;
    }
    if (n >= (int)sizeof(buf)) {
        n = (int)sizeof(buf) - 1;
    }
    crash_emit(buf, n);
}

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

static void crash_write_body(const char *reason, const ThreadExceptionDump *ctx) {
    int32_t mode_file = 0;
    int32_t stage_file = 0;

#ifdef DW3_NX_GAME_IMAGE
    mode_file = dw3_overlay_current_mode_file();
    stage_file = dw3_overlay_current_sub_file();
#endif

    crash_printf("dw3-switch crash report\n");
    crash_printf("version: %s\n", DW3_NRO_VERSION_STR);
    crash_printf("reason: %s\n", reason ? reason : "(none)");
    crash_printf("mode_file: 0x%x\n", (unsigned)mode_file);
    crash_printf("stage_file: 0x%x\n", (unsigned)stage_file);
    crash_printf("log: %s\n", DW3_NX_CRASH_LOG_HINT);
    crash_printf("report: %s\n", DW3_NX_CRASH_REPORT_HINT);

    if (ctx) {
        crash_printf("error_desc: 0x%x (%s)\n", (unsigned)ctx->error_desc,
                     error_desc_name(ctx->error_desc));
        crash_printf("pc:  0x%016llx\n", (unsigned long long)ctx->pc.x);
        crash_printf("lr:  0x%016llx\n", (unsigned long long)ctx->lr.x);
        crash_printf("sp:  0x%016llx\n", (unsigned long long)ctx->sp.x);
        crash_printf("fp:  0x%016llx\n", (unsigned long long)ctx->fp.x);
        crash_printf("far: 0x%016llx\n", (unsigned long long)ctx->far.x);
        crash_printf("esr: 0x%08x\n", (unsigned)ctx->esr);
        crash_printf("pstate: 0x%08x\n", (unsigned)ctx->pstate);
        crash_printf("x0:  0x%016llx\n",
                     (unsigned long long)ctx->cpu_gprs[0].x);
        crash_printf("x1:  0x%016llx\n",
                     (unsigned long long)ctx->cpu_gprs[1].x);
    }

    crash_printf("--- end ---\n");
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

void dw3_nx_crash_install(void) {
    /* Avoid PSY-Z INFOF here: this TU must stay free of <psyz.h> libc wraps. */
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

void __libnx_exception_handler(ThreadExceptionDump *ctx) {
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

    crash_write_body("user exception", ctx);
    if (s_crash_file_ok) {
        fsFileFlush(&s_crash_file);
    }

    /* Re-raise so Atmosphere creport also captures the fault. */
    svcBreak(BreakReason_Panic, 0, 0);
    for (;;) {
        svcSleepThread(1000000000ULL);
    }
}

#if DW3_NX_FORCE_CRASH
void dw3_nx_crash_force_fault(void) {
    volatile int *p = (volatile int *)0;
    psyz_log(1, __FILE__, __LINE__, __func__,
             "DW3_NX_FORCE_CRASH: deliberate null write");
    *p = 0x445733; /* "DW3" — should not return */
}
#endif
