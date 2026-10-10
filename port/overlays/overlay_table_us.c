/*
 * US title-path overlay table (P2.6 / P2.8).
 *
 * Phase 2 boot links engine + STDWTITL. US skips country select, so
 * CNTY_SEL is not registered here.
 *
 * .data / .bss snapshots remain NULL for now (restore is a no-op); the
 * title overlay is entered once at boot.
 */
#ifndef PORT_HOST
#error "port/overlays/overlay_table_us.c is host-only"
#endif

#include "overlay_host.h"

#include <stdio.h>

/* US FILE_STDWTITL from include/engine/overlay.h */
#define DW3_FILE_STDWTITL 0x1C1

void *STDWTITL_start(void);

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
    g_us_title[0].entry = (Dw3OverlayEntry)STDWTITL_start;
    dw3_overlay_set_table(g_us_title,
                          sizeof(g_us_title) / sizeof(g_us_title[0]));
    fprintf(stderr, "dw3: registered STDWTITL (file 0x%x) entry=%p\n",
            DW3_FILE_STDWTITL, (void *)STDWTITL_start);
}
