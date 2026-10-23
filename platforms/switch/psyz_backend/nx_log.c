/* PSY-Z platform logging for the Switch host (P4.5).
 *
 * Three destinations, because no single one is enough:
 *
 *   - stderr, which nxlink forwards to the host when the .nro is launched that
 *     way (the workflow platforms/switch/README.md documents for hardware);
 *   - sdmc:/switch/dw3/dw3.log, the record a hardware run leaves behind;
 *   - svcOutputDebugString, which is the only *live* channel under an emulator.
 *
 * The third one earns its place from a specific trap, learned the hard way:
 * Ryubing mirrors sdmc: onto a host directory, but it does not write a guest's
 * file data through as the guest flushes — the mirror lags, and its tail on
 * shutdown is whatever the emulator got around to. Reading dw3.log while the
 * emulator is still up therefore shows a log that has silently stopped short,
 * which reads exactly like a hang. svcOutputDebugString has no such lag: the
 * emulator logs it (at warning level) into its own file as it arrives, with
 * emulator timestamps attached. On hardware it costs one no-op syscall.
 *
 * The file is opened by dw3_nx_log_open() once the SD card is mounted; before
 * that (and if the mount failed) the other two still get everything.
 *
 * P5.4 (crash report): the file is a raw libnx FsFile, not a stdio FILE, so
 * the exception handler can append its `CRASH` marker after the last line
 * without touching a stdio lock (dw3_nx_log_crash_append). Each line is one
 * fsFileWrite at the tracked end offset, which is what fwrite + fflush did
 * through fsdev before: the data is in the FS service when psyz_log returns,
 * so a crash cannot take it with it. Every formatted line also goes into a
 * small ring (nx_log_ring.h) that the handler copies into crash_report.txt.
 */
#include "nx.h"
#include "nx_log_ring.h"

#include <stdarg.h>
#include <stdio.h>

#include <switch.h>

/* Device-relative path inside the "sdmc" FsFileSystem. */
#define DW3_NX_LOG_FS_PATH "/switch/dw3/dw3.log"
#define DW3_NX_LOG_MAX 1024

static FsFile s_log_file;
static volatile int s_log_ok;
static volatile s64 s_log_off; /* end of the last complete write */
static Mutex s_log_mutex;
static Dw3LogRing s_log_ring;

void dw3_nx_log_open(void) {
    FsFileSystem *fs;

    if (s_log_ok) {
        return;
    }
    fs = fsdevGetDeviceFileSystem("sdmc");
    if (!fs) {
        return;
    }
    /* Create if missing (AlreadyExists is fine), then truncate: one run per
     * file, so the tail is always the latest boot. Append mode lets each
     * write at the end offset grow the file. */
    fsFsCreateFile(fs, DW3_NX_LOG_FS_PATH, 0, 0);
    if (R_FAILED(fsFsOpenFile(fs, DW3_NX_LOG_FS_PATH,
                              FsOpenMode_Write | FsOpenMode_Append,
                              &s_log_file))) {
        return;
    }
    if (R_FAILED(fsFileSetSize(&s_log_file, 0))) {
        fsFileClose(&s_log_file);
        return;
    }
    mutexInit(&s_log_mutex);
    s_log_off = 0;
    s_log_ok = 1;
}

/* Lock-free: the exception handler may have interrupted a psyz_log holding
 * the mutex. Writing at the last complete end offset can at worst overwrite
 * that one in-flight line (the ring still holds it). */
void dw3_nx_log_crash_append(const char *buf, int len) {
    s64 off;

    if (!s_log_ok || len <= 0) {
        return;
    }
    off = s_log_off;
    if (R_SUCCEEDED(fsFileWrite(&s_log_file, off, buf, (u64)len,
                                FsWriteOption_Flush))) {
        s_log_off = off + len;
    }
}

unsigned dw3_nx_log_ring_walk(void (*emit)(void *ctx, const char *line,
                                           unsigned len),
                              void *ctx, unsigned *skipped) {
    return dw3_log_ring_walk(&s_log_ring, emit, ctx, skipped);
}

void psyz_log(unsigned int level, const char *file, unsigned int line,
              const char *func, const char *fmt, ...) {
    static const char levels[] = "DIWE";
    char buf[DW3_NX_LOG_MAX];
    char body[DW3_NX_LOG_MAX];
    va_list args;
    int len;

    if (level >= sizeof(levels) - 1) {
        return;
    }

    /* One formatted line, then fan it out: the three sinks cannot drift apart,
     * and svcOutputDebugString gets a length instead of a strlen. */
    va_start(args, fmt);
    vsnprintf(body, sizeof(body), fmt, args);
    va_end(args);

    len = snprintf(buf, sizeof(buf), "[%c][%s:%d][%s] %s\n", levels[level],
                   file, (int)line, func, body);
    if (len <= 0) {
        return;
    }
    if (len >= (int)sizeof(buf)) {
        len = (int)sizeof(buf) - 1; /* truncated; report what was written */
    }

    dw3_log_ring_push(&s_log_ring, buf, len);
    if (s_log_ok) {
        /* Unbuffered: a crash must not take the reason with it. This does not
         * defeat the emulator's own caching of sdmc: — see the header. */
        mutexLock(&s_log_mutex);
        if (R_SUCCEEDED(fsFileWrite(&s_log_file, s_log_off, buf, (u64)len,
                                    FsWriteOption_None))) {
            s_log_off += len;
        }
        mutexUnlock(&s_log_mutex);
    }
    fwrite(buf, 1, (size_t)len, stderr);
    fflush(stderr);
    svcOutputDebugString(buf, (u64)len);
}
