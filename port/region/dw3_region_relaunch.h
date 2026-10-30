/* P6.38: restart into the chosen region (docs/PORT_OPTIONS.md, "Restart into
 * the chosen region").
 *
 * The combined image runs one game copy per process (dw3_region_select refuses
 * a second region: the two copies keep separate statics, overlays and a GPU
 * state that cannot be swapped under a running game), so changing region means a
 * new process. This file is the platform independent part of that:
 *   - the override argument the new process is started with,
 *   - the argument string / argv each host builds for it,
 *   - whether the target region's data is on the card.
 * The two hosts do the actual relaunch (platforms/switch/source/boot_main.c:
 * libnx envSetNextLoad; platforms/pc/src/main.c: execv). Tested on the host
 * with tools/tests/region_relaunch_main.c. */
#ifndef DW3_REGION_RELAUNCH_H
#define DW3_REGION_RELAUNCH_H

#include <stddef.h>

#include "dw3_region.h"

#ifdef __cplusplus
extern "C" {
#endif

/* `--relaunch-region eu` or `--relaunch-region=eu`: the region the new process
 * starts, whatever region_default says (and, on the PC, whatever DW3_REGION /
 * --region / DW3_RUNTIME the first process was given). */
#define DW3_RELAUNCH_ARG "--relaunch-region"

/* The region named by the argument in argv, or DW3_REGION_NONE when it is
 * absent or does not name us / eu. */
Dw3Region dw3_relaunch_arg_region(int argc, char **argv);

/* The argument string for libnx envSetNextLoad: the NRO path (quoted when it
 * has a space) then `--relaunch-region <r>`. Returns the length, or -1 when it
 * does not fit in `cap` or `nro_path` is empty. */
int dw3_relaunch_nx_args(const char *nro_path, Dw3Region target, char *out, size_t cap);

/* The argv for execv on the PC: the old arguments without --region /
 * --runtime / --relaunch-region (and their values: the new process takes its
 * region and runtime folder from the base folder), then the override. A
 * malloc'd NULL-terminated array; the strings are the old argv's (and static
 * ones), so free only the array. NULL when out of memory. */
char **dw3_relaunch_pc_argv(int argc, char **argv, Dw3Region target);

/* 1 when the data of `target` is on the card under `base`: a complete runtime
 * tree or a disc image the extraction can read (the same test as the boot
 * flow's "data found"). */
int dw3_relaunch_target_present(const char *base, Dw3Region target);

/* Why a relaunch cannot be offered (the value of the host hook below). */
enum {
    DW3_RELAUNCH_OK = 0,
    DW3_RELAUNCH_NO_DATA = 1, /* the target region has no data under the base folder */
    DW3_RELAUNCH_NO_HOST = 2  /* the host cannot start another process (hbmenu limits ...) */
};

/* ---- Host hooks (defined by platforms/pc and platforms/switch; the game copy
 * calls them like dw3_host_request_quit). ---- */

/* Can the host relaunch into `target` now? Returns DW3_RELAUNCH_OK, or a
 * reason; `why` always gets one short line for the log. */
int dw3_host_relaunch_check(Dw3Region target, char *why, size_t cap);

/* Arranges the relaunch and asks the host to quit (the normal quit path: the
 * game loop returns, audio and saves close, the log is flushed); the new
 * process starts after that. Returns 0, or -1 with the reason in `why` (the
 * game then goes on running). */
int dw3_host_relaunch_request(Dw3Region target, char *why, size_t cap);

#ifdef __cplusplus
}
#endif

#endif /* DW3_REGION_RELAUNCH_H */
