/* P6.24: the boot flow of the combined image, platform independent.
 *
 * Probes which game data is on the card, reads the stored choice, applies the
 * decision table of docs/SINGLE_BUILD.md ("Boot picker"), runs the picker when
 * it has to ask, stores the choice, and logs every decision (AGENTS.md rule
 * 12: the log answers the test plan). It draws nothing and reads no pad: the
 * platform passes those in as callbacks (platforms/switch/psyz_backend/
 * nx_region_boot.c), so the whole flow is testable on the host with fakes
 * (tools/tests/region_boot_main.c).
 *
 * Lines it logs, all starting `dw3-region: `:
 *   data found us=<0|1> eu=<0|1> under <base> [(us: disc image, needs extraction) ...]
 *   stored region_default=<ask|us|eu> last_region=<us|eu|none> minus_held=<0|1|n/a>
 *   decision <boot|pick|no-data> region=<us|eu|none>: <why>
 *   picker shown (default entry <r>, timeout 3000 ms)
 *   picker result <r> by <countdown|button A> after <N> ms always=<0|1>
 *   picker quit by + after <N> ms
 *   saved last_region=<r> [region_default=<r>]   |   could not store the choice
 *   forced <r> by <source>: no probe, no picker, nothing written
 * The caller's dw3_region_select() adds `running <r> (<source>)`. */
#ifndef DW3_REGION_BOOT_H
#define DW3_REGION_BOOT_H

#include "dw3_region.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct Dw3RegionBootIO {
    void *ud;
    /* One log line (no newline). Required. */
    void (*log)(void *ud, const char *line);
    /* Samples the pad for the hold-Minus request (about 200 ms); 1 = held.
     * Only called when the answer can change the decision. NULL = never held. */
    int (*sample_minus)(void *ud);
    /* Runs the picker: steps `p` with the pad and the clock until its state is
     * no longer RUNNING, drawing it. Returns the milliseconds it ran. Required
     * when the table can ask. */
    unsigned (*run_picker)(void *ud, Dw3Picker *p);
    /* The "no game data" screen, until the player dismisses it. */
    void (*show_no_data)(void *ud, const char *base);
    /* 1 when `<base>/<r>/` has no complete tree but holds a disc image that the
     * runtime extraction can unpack (the region then counts as present). NULL = no. */
    int (*disc_waiting)(void *ud, const char *base, Dw3Region r);
} Dw3RegionBootIO;

typedef struct Dw3RegionBootIn {
    const char *base;     /* folder holding us/ and eu/ */
    Dw3Region forced;     /* NONE unless forced (replay build, DW3_REGION, DW3_RUNTIME) */
    const char *forced_by;/* for the log */
} Dw3RegionBootIn;

typedef enum Dw3RegionBootResult {
    DW3_RB_RUN = 0,     /* boot `region` */
    DW3_RB_NO_DATA = 1, /* nothing found; the setup screen was shown */
    DW3_RB_QUIT = 2     /* the player left the picker with + */
} Dw3RegionBootResult;

typedef struct Dw3RegionBootOut {
    Dw3RegionBootResult result;
    Dw3Region region;
    const char *source; /* "forced", "only the US data is present", "region_default",
                           "picker (button A)", "picker (countdown)", ... */
} Dw3RegionBootOut;

void dw3_region_boot(const Dw3RegionBootIn *in, const Dw3RegionBootIO *io,
                     Dw3RegionBootOut *out);

#ifdef __cplusplus
}
#endif

#endif /* DW3_REGION_BOOT_H */
