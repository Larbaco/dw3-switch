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

/* Restore .data from data_init and zero .bss.
 * Returns 0 on success, -1 if file_id is unknown or file_id == 0. */
int dw3_overlay_restore(int32_t file_id);

/* Title-path overlay tables (STDWTITL + field/battle/menus/…). Call once. */
void dw3_overlay_register_us_title(void);
void dw3_overlay_register_eu_title(void);

#ifdef __cplusplus
}
#endif

#endif /* DW3_OVERLAY_HOST_H */

/* P3.4: latch so DW3_FORCE_MENU fires once per process. */
int dw3_host_menu_force_once(void);

