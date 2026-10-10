/*
 * US title-path overlay table (P2.6 / E5).
 *
 * Phase 2 boot needs engine + STDWTITL. US skips country select, so
 * CNTY_SEL is not registered here (add only if a build path requires it).
 *
 * .data / .bss snapshots for STDWTITL are filled when that overlay is
 * linked with init images (E6 / P2.8). Until then entry is wired and
 * restore is a no-op for NULL regions.
 */
#ifndef PORT_HOST
#error "port/overlays/overlay_table_us.c is host-only"
#endif

#include "overlay_host.h"

/* US FILE_STDWTITL from include/engine/overlay.h — keep numeric to avoid
 * pulling the full decomp header into this TU before E6 links the overlay. */
#define DW3_FILE_STDWTITL 0x1C1

/* Declared by the STDWTITL overlay; weak so engine-only links succeed. */
void *STDWTITL_start(void) __attribute__((weak));

static void *stdwtitl_entry_stub(void) {
    return NULL;
}

static Dw3OverlayDesc g_us_title[] = {
    {
        DW3_FILE_STDWTITL,
        NULL, /* filled in dw3_overlay_register_us_title */
        NULL,
        NULL,
        0,
        NULL,
        0,
    },
};

void dw3_overlay_register_us_title(void) {
    if (STDWTITL_start != NULL) {
        g_us_title[0].entry = (Dw3OverlayEntry)STDWTITL_start;
    } else {
        g_us_title[0].entry = stdwtitl_entry_stub;
    }
    dw3_overlay_set_table(g_us_title,
                          sizeof(g_us_title) / sizeof(g_us_title[0]));
}
