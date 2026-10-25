/* P6.24 slice 1: run-time region choice (docs/SINGLE_BUILD.md, "As built").
 *
 * Platform independent (stdio / stat only) so the PC launcher, the Switch
 * boot code and tools/tests share it. It knows nothing about the game: it
 *   - finds which region trees exist under a base folder,
 *   - decides "boot X", "ask", or "no data" from the forced region, the stored
 *     preference, the last region and the hold-Minus request,
 *   - runs the boot picker as a pure state machine (3 s countdown, D-pad
 *     cancels it, A boots, Y toggles "always start this one"),
 *   - stores `region_default` / `last_region` in the global port_options.cfg.
 *
 * Nothing here draws or reads a pad; the host feeds it button edges and a
 * millisecond delta and shows what it reports. */
#ifndef DW3_REGION_H
#define DW3_REGION_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum Dw3Region {
    DW3_REGION_NONE = 0, /* unset / ask / not found */
    DW3_REGION_US = 1,
    DW3_REGION_EU = 2
} Dw3Region;

#define DW3_REGION_KEY_DEFAULT "region_default" /* ask | us | eu */
#define DW3_REGION_KEY_LAST "last_region"       /* us | eu */
/* Seconds the picker counts down before booting the highlighted region
 * (owner decision 2026-10-24). */
#define DW3_PICKER_TIMEOUT_MS 3000

/* "us" / "eu" (any case); anything else is DW3_REGION_NONE. */
Dw3Region dw3_region_parse(const char *s);
/* "us", "eu" or "none". */
const char *dw3_region_name(Dw3Region r);
/* Folder and executable names of a region: "us" / "SLUS_014.36",
 * "eu" / "SLES_039.36". NULL for NONE. */
const char *dw3_region_dir(Dw3Region r);

typedef struct Dw3RegionProbe {
    int has_us;
    int has_eu;
} Dw3RegionProbe;

/* A region is present when `<base>/<dir>/` holds its file map
 * (`files_<dir>.json`) and its executable. Disc images are not looked at in
 * this slice (docs/SINGLE_BUILD.md, "What remains"). */
void dw3_region_probe(const char *base, Dw3RegionProbe *out);

/* The same test for one runtime folder (`<root>/files_us.json` ...). */
int dw3_region_runtime_complete(const char *root, Dw3Region r);

typedef enum Dw3BootAction {
    DW3_BOOT_NO_DATA = 0, /* nothing found: show the setup message */
    DW3_BOOT_REGION = 1,  /* boot `region`, no picker */
    DW3_BOOT_PICK = 2     /* show the picker; `region` is the default entry */
} Dw3BootAction;

typedef struct Dw3RegionInput {
    Dw3Region forced;       /* --region / DW3_REGION / replay: wins over all */
    Dw3Region pref;         /* region_default: NONE means "ask" */
    Dw3Region last;         /* last_region */
    int minus_held;         /* the player holds Minus at launch */
    Dw3RegionProbe probe;
} Dw3RegionInput;

typedef struct Dw3BootDecision {
    Dw3BootAction action;
    Dw3Region region;
    const char *why; /* short reason for the log */
} Dw3BootDecision;

/* The decision table of docs/SINGLE_BUILD.md, "Boot picker":
 *   forced                      -> boot it, no probe, no picker
 *   exactly one region present  -> boot it (preference and Minus ignored)
 *   none present                -> no data
 *   both, Minus held            -> picker
 *   both, region_default us|eu  -> boot it
 *   both, region_default ask    -> picker
 * The picker's default entry is last_region, else US. */
Dw3BootDecision dw3_region_decide(const Dw3RegionInput *in);

/* ---- Picker state machine ---- */

enum { /* button edges (pressed this step) */
    DW3_PK_LEFT = 1,
    DW3_PK_RIGHT = 2,
    DW3_PK_A = 4,
    DW3_PK_Y = 8,
    DW3_PK_PLUS = 16
};

typedef enum Dw3PickerState {
    DW3_PICKER_RUNNING = 0,
    DW3_PICKER_CHOSEN = 1, /* boot `chosen` */
    DW3_PICKER_QUIT = 2    /* + : leave without starting */
} Dw3PickerState;

typedef struct Dw3Picker {
    Dw3Region sel;      /* highlighted entry */
    int always;         /* "always start this one" toggled with Y */
    int cancelled;      /* a D-pad / Y press stopped the countdown */
    unsigned elapsed_ms;
    Dw3PickerState state;
    Dw3Region chosen;   /* valid once state == CHOSEN */
    int timed_out;      /* chosen by the countdown, not by the player */
} Dw3Picker;

void dw3_picker_init(Dw3Picker *p, Dw3Region default_sel);
/* One step: `pressed` are the button edges since the last step, `dt_ms` the
 * time since it. Does nothing once state != RUNNING. */
Dw3PickerState dw3_picker_step(Dw3Picker *p, unsigned pressed, unsigned dt_ms);
/* Whole seconds left for the "Starting X in N..." line (3, 2, 1), or -1 once
 * the countdown was cancelled. */
int dw3_picker_seconds_left(const Dw3Picker *p);

/* ---- Stored choice (global port_options.cfg) ---- */

/* Reads region_default and last_region from `<dir>/port_options.cfg`; a
 * missing or unreadable file or a bad value leaves NONE (= ask / unknown). */
void dw3_region_cfg_read(const char *dir, Dw3Region *pref, Dw3Region *last);
/* Writes last_region always, and region_default = `pref_value` ("ask", "us"
 * or "eu") when `set_pref` is nonzero. Other keys in the file are kept.
 * Returns 0 or -1. */
int dw3_region_cfg_write(const char *dir, Dw3Region last, int set_pref,
                         Dw3Region pref_value);

#ifdef __cplusplus
}
#endif

#endif /* DW3_REGION_H */
