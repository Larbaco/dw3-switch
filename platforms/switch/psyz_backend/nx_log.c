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
#include <sys/iosupport.h>

/* Device-relative path inside the "sdmc" FsFileSystem. The folder keeps the
 * last DW3_NX_LOG_KEEP runs: dw3.log is the current one, dw3.1.log the run
 * before it, ... dw3.<KEEP-1>.log the oldest. */
#define DW3_NX_LOG_FS_PATH "/switch/dw3/dw3.log"
#define DW3_NX_LOG_KEEP 5
#define DW3_NX_LOG_MAX 1024

static FsFile s_log_file;
static volatile int s_log_ok;
static volatile s64 s_log_off; /* end of the last complete write */
static Mutex s_log_mutex;
static Dw3LogRing s_log_ring;
/* Set once game stderr is routed into psyz_log; psyz_log then must not write
 * stderr itself (it would loop). */
static volatile int s_stderr_captured;

/* "dw3.log" for n == 0, "dw3.<n>.log" otherwise. */
static void log_path(char *out, size_t cap, int n) {
    if (n == 0) {
        snprintf(out, cap, "%s", DW3_NX_LOG_FS_PATH);
    } else {
        snprintf(out, cap, "/switch/dw3/dw3.%d.log", n);
    }
}

/* Per-step outcome of the rotation, logged once the new file is open. */
static Result s_rot_rc[DW3_NX_LOG_KEEP]; /* [n] = dw3.<n>.log -> dw3.<n+1>.log */
static char s_rot_how[DW3_NX_LOG_KEEP];  /* r renamed, c copied, - none/failed */
static Result s_rot_del_rc;
static int s_rot_missing; /* dw3.log did not exist: nothing to move */
static int s_rotate_done; /* rotation runs exactly once per process */
static u8 s_copy_buf[16384];

/* Fallback when a rename is refused: copy `from` into `to`, then delete
 * `from`. Returns 0 on success, else the failing Result. */
static Result log_copy(FsFileSystem *fs, const char *from, const char *to) {
    FsFile in, out;
    s64 size = 0, off = 0;
    Result rc;

    rc = fsFsOpenFile(fs, from, FsOpenMode_Read, &in);
    if (R_FAILED(rc)) {
        return rc;
    }
    rc = fsFileGetSize(&in, &size);
    if (R_SUCCEEDED(rc)) {
        fsFsDeleteFile(fs, to);
        rc = fsFsCreateFile(fs, to, 0, 0);
    }
    if (R_SUCCEEDED(rc)) {
        rc = fsFsOpenFile(fs, to, FsOpenMode_Write | FsOpenMode_Append, &out);
        if (R_SUCCEEDED(rc)) {
            while (off < size) {
                u64 got = 0;

                rc = fsFileRead(&in, off, s_copy_buf, sizeof(s_copy_buf),
                                FsReadOption_None, &got);
                if (R_FAILED(rc) || got == 0) {
                    break;
                }
                rc = fsFileWrite(&out, off, s_copy_buf, got,
                                 FsWriteOption_None);
                if (R_FAILED(rc)) {
                    break;
                }
                off += (s64)got;
            }
            if (R_SUCCEEDED(rc)) {
                rc = fsFileFlush(&out);
            }
            fsFileClose(&out);
        }
    }
    fsFileClose(&in);
    if (R_SUCCEEDED(rc)) {
        fsFsDeleteFile(fs, from);
    }
    return rc;
}

/* Shift dw3.log -> dw3.1.log -> ... and drop the oldest. Rename first (cheap);
 * if the FS refuses, copy + delete so the previous run survives anyway. Every
 * failure is non-fatal (a missing older file is the normal case on the first
 * boots), but each step's Result is kept for the log line written afterwards.
 * Runs once per process: a second call would rotate the fresh file. */
static void log_rotate(FsFileSystem *fs) {
    char from[48];
    char to[48];
    int n;

    if (s_rotate_done) {
        return;
    }
    s_rotate_done = 1;
    log_path(to, sizeof(to), DW3_NX_LOG_KEEP - 1);
    s_rot_del_rc = fsFsDeleteFile(fs, to);
    for (n = DW3_NX_LOG_KEEP - 2; n >= 0; n--) {
        Result rc;

        log_path(from, sizeof(from), n);
        log_path(to, sizeof(to), n + 1);
        rc = fsFsRenameFile(fs, from, to);
        s_rot_how[n] = R_SUCCEEDED(rc) ? 'r' : '-';
        if (R_FAILED(rc)) {
            FsFile probe;

            /* Nothing there to move is not worth a copy. */
            if (R_SUCCEEDED(fsFsOpenFile(fs, from, FsOpenMode_Read, &probe))) {
                fsFileClose(&probe);
                rc = log_copy(fs, from, to);
                s_rot_how[n] = R_SUCCEEDED(rc) ? 'c' : '-';
            } else if (n == 0) {
                s_rot_missing = 1;
            } else if (n == 0) {
                s_rot_missing = 1;
            }
        }
        s_rot_rc[n] = rc;
    }
}

static void log_rotate_report(void) {
    char line[320];
    int len = 0, n, kept = 0;

    line[0] = '\0';
    for (n = 0; n < DW3_NX_LOG_KEEP - 1; n++) {
        len += snprintf(line + len, sizeof(line) - (size_t)len,
                        "%sdw3.%d.log %c rc=0x%x", n ? ", " : "", n,
                        s_rot_how[n], (unsigned)s_rot_rc[n]);
        if (s_rot_how[n] != '-') {
            kept++;
        }
    }
    psyz_log(1, __FILE__, __LINE__, __func__,
             "dw3-log: rotated (dw3.N.log -> dw3.N+1.log; r=renamed "
             "c=copied -=nothing to move or failed): %s; delete dw3.%d.log "
             "rc=0x%x; kept %d previous run(s)",
             line, DW3_NX_LOG_KEEP - 1, (unsigned)s_rot_del_rc, kept);
}

void dw3_nx_log_run_header(void) {
    u64 now = 0;
    TimeCalendarTime cal;
    TimeCalendarAdditionalInfo info;

    if (R_SUCCEEDED(timeInitialize()) &&
        R_SUCCEEDED(timeGetCurrentTime(TimeType_UserSystemClock, &now)) &&
        R_SUCCEEDED(timeToCalendarTimeWithMyRule(now, &cal, &info))) {
        psyz_log(1, __FILE__, __LINE__, __func__,
                 "dw3-run: started %04d-%02d-%02d %02d:%02d:%02d (console "
                 "clock, epoch=%llu); previous runs are dw3.1.log ... "
                 "dw3.%d.log, newest first",
                 (int)cal.year, (int)cal.month, (int)cal.day, (int)cal.hour,
                 (int)cal.minute, (int)cal.second, (unsigned long long)now,
                 DW3_NX_LOG_KEEP - 1);
    } else {
        psyz_log(1, __FILE__, __LINE__, __func__,
                 "dw3-run: started (console clock unavailable); previous "
                 "runs are dw3.1.log ... dw3.%d.log, newest first",
                 DW3_NX_LOG_KEEP - 1);
    }
}

void dw3_nx_log_open(void) {
    FsFileSystem *fs;

    if (s_log_ok) {
        return;
    }
    fs = fsdevGetDeviceFileSystem("sdmc");
    if (!fs) {
        return;
    }
    /* One run per file: move the older runs up, then start a fresh dw3.log.
     * Create (AlreadyExists is fine) then truncate in case the rename failed.
     * Append mode lets each write at the end offset grow the file. */
    log_rotate(fs);
    fsFsCreateFile(fs, DW3_NX_LOG_FS_PATH, 0, 0);
    if (R_FAILED(fsFsOpenFile(fs, DW3_NX_LOG_FS_PATH,
                              FsOpenMode_Write | FsOpenMode_Append,
                              &s_log_file))) {
        return;
    }
    mutexInit(&s_log_mutex);
    s_log_off = 0;
    if (s_rot_how[0] == 'r' || s_rot_how[0] == 'c' || s_rot_missing) {
        if (R_FAILED(fsFileSetSize(&s_log_file, 0))) {
            fsFileClose(&s_log_file);
            return;
        }
    } else {
        /* dw3.log could not be moved aside (renamed nor copied). Never
         * truncate it then: append after the old run so that run survives. */
        s64 size = 0;

        if (R_SUCCEEDED(fsFileGetSize(&s_log_file, &size))) {
            s_log_off = size;
        }
    }
    s_log_ok = 1;
    log_rotate_report();
}

/* Close the file at the end of main. The chain-loaded launcher/hbloader
 * process outlives one NRO, so a handle left open can make the next run's
 * rename of dw3.log fail. */
void dw3_nx_log_close(void) {
    if (!s_log_ok) {
        return;
    }
    mutexLock(&s_log_mutex);
    s_log_ok = 0;
    fsFileFlush(&s_log_file);
    fsFileClose(&s_log_file);
    mutexUnlock(&s_log_mutex);
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

/* Game code reports most of its "dw3-<area>: ..." lines with
 * fprintf(stderr, ...). With nothing attached that went nowhere, so those lines
 * (port-options open/close, memcard recovery, restart, map view, battle
 * options, ...) never reached dw3.log. This devoptab takes over fd 2, cuts
 * the byte stream into lines and hands each to psyz_log. */
/* Per-thread line buffers: the audio, precompute and game threads all write
 * stderr, and a piece of a line (no newline yet) from one must never be glued
 * to bytes of another. (newlib already makes one fprintf a single write.) */
static __thread char s_err_line[DW3_NX_LOG_MAX - 128];
static __thread unsigned s_err_len;

/* Safety valve: every stderr line is now a synchronous SD write, so a loop
 * that prints per frame must not be able to fill the card or stall the game.
 * More than ERR_MAX_PER_SEC lines in one second are dropped and counted; the
 * count is logged at the start of the next window. Measured need on PC
 * replays: boot burst ~300 lines, then well under 5 lines/s. */
#define ERR_MAX_PER_SEC 400
static Mutex s_err_mutex;
static u64 s_err_window_start;
static unsigned s_err_window_count, s_err_dropped;

static int err_allow(unsigned *dropped_before) {
    const u64 now = armGetSystemTick();
    int ok;

    *dropped_before = 0;
    mutexLock(&s_err_mutex);
    if (now - s_err_window_start >= armGetSystemTickFreq()) {
        *dropped_before = s_err_dropped;
        s_err_dropped = 0;
        s_err_window_start = now;
        s_err_window_count = 0;
    }
    ok = s_err_window_count < ERR_MAX_PER_SEC;
    if (ok) {
        s_err_window_count++;
    } else {
        s_err_dropped++;
    }
    mutexUnlock(&s_err_mutex);
    return ok;
}

static void err_flush_line(void) {
    unsigned dropped;

    if (s_err_len == 0) {
        return;
    }
    s_err_line[s_err_len] = '\0';
    s_err_len = 0;
    if (!err_allow(&dropped)) {
        return;
    }
    if (dropped) {
        psyz_log(2, "stderr", 0, "game",
                 "dw3-log: %u stderr lines dropped (more than %d per second)",
                 dropped, ERR_MAX_PER_SEC);
    }
    psyz_log(1, "stderr", 0, "game", "%s", s_err_line);
}

static ssize_t err_write(struct _reent *r, void *fd, const char *ptr,
                         size_t len) {
    size_t i;

    (void)r;
    (void)fd;
    for (i = 0; i < len; i++) {
        if (ptr[i] == '\n') {
            err_flush_line();
        } else if (ptr[i] != '\r') {
            if (s_err_len >= sizeof(s_err_line) - 1) {
                err_flush_line();
            }
            s_err_line[s_err_len++] = ptr[i];
        }
    }
    return (ssize_t)len;
}

static const devoptab_t s_err_devoptab = {
    .name = "dw3err",
    .structSize = 0,
    .write_r = err_write,
};

void dw3_nx_log_capture_stderr(void) {
    if (s_stderr_captured) {
        return;
    }
    mutexInit(&s_err_mutex);
    devoptab_list[STD_ERR] = &s_err_devoptab;
    setvbuf(stderr, NULL, _IONBF, 0);
    s_stderr_captured = 1;
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
    if (!s_stderr_captured) {
        fwrite(buf, 1, (size_t)len, stderr);
        fflush(stderr);
    }
    svcOutputDebugString(buf, (u64)len);
}
