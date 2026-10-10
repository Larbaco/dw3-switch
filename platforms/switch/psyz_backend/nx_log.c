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
 */
#include "nx.h"

#include <stdarg.h>
#include <stdio.h>

#include <switch.h>

#define DW3_NX_LOG_PATH "sdmc:/switch/dw3/dw3.log"
#define DW3_NX_LOG_MAX 1024

static FILE *s_log;

void dw3_nx_log_open(void) {
    if (s_log) {
        return;
    }
    /* Truncate: one run per file, so the tail is always the latest boot. */
    s_log = fopen(DW3_NX_LOG_PATH, "w");
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

    if (s_log) {
        fwrite(buf, 1, (size_t)len, s_log);
        /* Unbuffered: a crash must not take the reason with it. This does not
         * defeat the emulator's own caching of sdmc: — see the header. */
        fflush(s_log);
    }
    fwrite(buf, 1, (size_t)len, stderr);
    fflush(stderr);
    svcOutputDebugString(buf, (u64)len);
}
