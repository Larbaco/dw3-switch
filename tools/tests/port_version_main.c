/* P6.37 test of port/version/port_version.c: prints the label, the log line
 * and what announce does twice (logs once). Host build, no game. */
#include <stdio.h>
#include <string.h>

#include "port_version.h"
#include "dw3_region_rt.h"

static int g_logged;
static char g_title[256];

void psyz_log(unsigned int level, const char *file, unsigned int line,
              const char *func, const char *fmt, ...) {
    (void)level; (void)file; (void)line; (void)func; (void)fmt;
    g_logged++;
}
void Psyz_SetTitle(const char *s) { snprintf(g_title, sizeof(g_title), "%s", s); }

#if defined(DW3_BOTH) && DW3_BOTH
static Dw3Region g_region = DW3_REGION_NONE;
Dw3Region dw3_region(void) { return g_region; }
#endif

int main(void) {
    char text[160], line[256];

    dw3_version_text(text, sizeof(text));
    dw3_version_log_line(line, sizeof(line));
    printf("text=%s\nline=%s\n", text, line);
    dw3_version_announce();
    dw3_version_announce();
    printf("logged=%d title=%s\n", g_logged, g_title);
#if defined(DW3_BOTH) && DW3_BOTH
    g_region = DW3_REGION_EU;
    dw3_version_text(text, sizeof(text));
    dw3_version_announce();
    printf("picked=%s logged=%d\n", text, g_logged);
#endif
    return 0;
}
