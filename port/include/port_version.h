/* P6.37: the port's version as the player and the logs see it
 * (docs/P6.37_VERSION.md). One label, fed by tools/dw3_nro_version.py through
 * the generated dw3_version_gen.h (the same source as the NACP version):
 *   dw3-switch v0.6.5-preview.57 (962462d) US
 * The region tag is the running disc: US, EU, or US+EU in a combined image
 * before the region is picked. Host side (PC executable, Switch image): the
 * Options footer and the title corner (platforms/pc/src/title_overlay_art.c),
 * the `dw3-version:` log line and the PC window title read it.
 *
 * DW3_VERSION_OVERRIDE=<text> (environment, PC) replaces the label so replay
 * pictures do not change with every commit; the Switch replay build
 * (DW3_NX_REPLAY) always shows "replay". */
#ifndef DW3_PORT_VERSION_H
#define DW3_PORT_VERSION_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* "dw3-switch v0.6.5-preview.57 (962462d) US" into buf (always terminated). */
void dw3_version_text(char *buf, size_t cap);
/* The build date, YYYY-MM-DD (commit date of a clean tree). */
const char *dw3_version_build_date(void);
/* "dw3-version: <text> build=<date>": the one log line, same on every host. */
void dw3_version_log_line(char *buf, size_t cap);

/* Logs the `dw3-version:` line (psyz_log: stderr on PC, dw3.log on Switch)
 * when the text differs from the last one logged, and on PC puts it in the
 * window title. The PC host calls it before main(); the Switch boot code
 * after the log is open, and both again when a combined image has picked its
 * region, so a combined image logs two lines (US+EU, then US or EU). */
void dw3_version_announce(void);

#ifdef __cplusplus
}
#endif

#endif /* DW3_PORT_VERSION_H */
