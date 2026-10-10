/* PSY-Z platform logging for the Switch host (P4.5).
 *
 * Two destinations, because neither one alone is enough:
 *
 *   - stderr, which nxlink forwards to the host when the .nro is launched that
 *     way (the workflow platforms/switch/README.md documents for hardware);
 *   - sdmc:/switch/dw3/dw3.log, because Ryubing loads the .nro directly and has
 *     no nxlink, so guest stderr is simply invisible there. That file is what
 *     makes an emulator smoke run diagnosable at all.
 *
 * The file is opened by dw3_nx_log_open() once the SD card is mounted; before
 * that (and if the mount failed) only stderr gets anything.
 */
#include "nx.h"

#include <stdarg.h>
#include <stdio.h>

#define DW3_NX_LOG_PATH "sdmc:/switch/dw3/dw3.log"

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
    FILE *dsts[2];
    int i;

    if (level >= sizeof(levels) - 1) {
        return;
    }

    dsts[0] = s_log;
    dsts[1] = stderr;
    for (i = 0; i < 2; i++) {
        va_list args;
        FILE *dst = dsts[i];
        if (!dst) {
            continue;
        }
        fprintf(dst, "[%c][%s:%d][%s] ", levels[level], file, (int)line, func);
        va_start(args, fmt);
        vfprintf(dst, fmt, args);
        va_end(args);
        fputc('\n', dst);
        /* Unbuffered: a crash must not take the reason with it. */
        fflush(dst);
    }
}
