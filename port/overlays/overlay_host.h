/* Host static-overlay restore (P2.6).
 *
 * On PS1, loadModeOverlay / loadSubOverlay memcpy a .PRO into fixed VRAM
 * only when the tracked id changes. On host, every overlay is linked into
 * one image; "load" means restore that overlay's initial .data and zero
 * its .bss. Same id → no-op (globals preserved).
 */
#ifndef DW3_OVERLAY_HOST_H
#define DW3_OVERLAY_HOST_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef void *(*Dw3OverlayEntry)(void);

typedef struct Dw3OverlayDesc {
    int32_t file_id;
    Dw3OverlayEntry entry; /* mode entry, or NULL for sub-overlays */
    void *data;
    const void *data_init; /* snapshot of initial .data (may equal a RODATA copy) */
    size_t data_size;
    void *bss;
    size_t bss_size;
} Dw3OverlayDesc;

/* Install the active descriptor table (fixtures or US title table). */
void dw3_overlay_set_table(const Dw3OverlayDesc *table, size_t count);

const Dw3OverlayDesc *dw3_overlay_find(int32_t file_id);

/* Optional overlay names, parallel to the table last given to
 * dw3_overlay_set_table (which clears them). dw3_overlay_file_name returns
 * the name of a known file id ("STSTATUS", "WSTAG355"), or NULL. Lock-free
 * and allocation-free: the Switch crash handler calls it (P5.4). */
void dw3_overlay_set_names(const char *const *names);
const char *dw3_overlay_file_name(int32_t file_id);

/* Restore .data from data_init and zero .bss.
 * Returns 0 on success, -1 if file_id is unknown or file_id == 0. A failure
 * logs the stable `kind=unknown UNKNOWN` marker plus a `dw3: ERROR` line, and
 * aborts the process in strict mode (see dw3_overlay_strict). */
int dw3_overlay_restore(int32_t file_id);

/* Strict mode: a failed restore is a hard failure (abort) instead of a log
 * line. On by default when DW3_REPLAY is set (P6.15 replay runs);
 * DW3_OVERLAY_STRICT=1/0 forces it; dw3_overlay_set_strict overrides both
 * (the Switch replay build, tests). */
int dw3_overlay_strict(void);
void dw3_overlay_set_strict(int on);

/* The host halves of loadModeOverlay / loadSubOverlay (patch 0008, 0061).
 * loaded_mode / loaded_sub are OVERLAY_LOADER.mode / .subOverlay.
 * Same id as loaded: no-op, returns 1 (globals preserved, as the PS1 skips
 * the memcpy). Otherwise restores and records the id, returns 0. A mode
 * slot without a file (file_id 0) is recorded and returns 1. On a failed
 * restore the id is NOT recorded: the latch is set to -1 so the next load
 * of any id restores again, and -1 is returned. */
int dw3_overlay_load_mode(int32_t *loaded_mode, int32_t *loaded_sub, int32_t mode_index,
                          int32_t file_id);
int dw3_overlay_load_sub(int32_t *loaded_sub, int32_t file_id);

/* Last restored mode overlay (desc->entry != NULL). Sub-overlays do not
 * update this. Used by the Switch mesh gate (FIGHTSTG only). 0 = none. */
int32_t dw3_overlay_current_mode_file(void);

/* Last restored sub-overlay (desc->entry == NULL), e.g. field stageFile.
 * 0 = none yet. Used by the Switch crash report (P5.4). */
int32_t dw3_overlay_current_sub_file(void);

/* Title-path overlay tables (STDWTITL + field/battle/menus/…). Call once. */
void dw3_overlay_register_us_title(void);
void dw3_overlay_register_eu_title(void);

#ifdef __cplusplus
}
#endif

#endif /* DW3_OVERLAY_HOST_H */

/* P3.4: latch so DW3_FORCE_MENU fires once per process. */
int dw3_host_menu_force_once(void);

