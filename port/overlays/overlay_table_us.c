/*
 * US overlay table for title + field bring-up (P2.6 / P2.8 / P3.2).
 *
 * Linked overlays: STDWTITL, FIELDSTG, STPLNMET, WSTAG260 (scripts),
 * WSTAG780 (Kusanagi street / MODE_NEW_GAME), WSTAG785 (Online Center).
 * .data/.bss snapshots are bound from named sections at register time.
 */
#ifndef PORT_HOST
#error "port/overlays/overlay_table_us.c is host-only"
#endif

#include "overlay_host.h"

#include <stdio.h>

/* US file ids from include/engine/overlay.h / FIELDSTG_stages */
#define DW3_FILE_STDWTITL 0x1C1
#define DW3_FILE_FIELDSTG 0x158
#define DW3_FILE_STPLNMET 0x1F8
#define DW3_FILE_WSTAG260 0x1CE /* US stage file id 462 = 0x1CE */
#define DW3_FILE_WSTAG780 0x1DA /* 474 */
#define DW3_FILE_WSTAG785 0x1DB /* 475 */

void *STDWTITL_start(void);
void *FIELDSTG_start(void);
void *STPLNMET_start(void);

void dw3_overlay_bind_field_sections(Dw3OverlayDesc *fieldstg, Dw3OverlayDesc *stplnmet,
                                     Dw3OverlayDesc *wstag260, Dw3OverlayDesc *wstag780,
                                     Dw3OverlayDesc *wstag785);

static Dw3OverlayDesc g_us_overlays[] = {
    {DW3_FILE_STDWTITL, NULL, NULL, NULL, 0, NULL, 0},
    {DW3_FILE_FIELDSTG, NULL, NULL, NULL, 0, NULL, 0},
    {DW3_FILE_STPLNMET, NULL, NULL, NULL, 0, NULL, 0},
    {DW3_FILE_WSTAG260, NULL, NULL, NULL, 0, NULL, 0},
    {DW3_FILE_WSTAG780, NULL, NULL, NULL, 0, NULL, 0},
    {DW3_FILE_WSTAG785, NULL, NULL, NULL, 0, NULL, 0},
};

void dw3_overlay_register_us_title(void) {
    g_us_overlays[0].entry = (Dw3OverlayEntry)STDWTITL_start;
    g_us_overlays[1].entry = (Dw3OverlayEntry)FIELDSTG_start;
    g_us_overlays[2].entry = (Dw3OverlayEntry)STPLNMET_start;
    /* Sub-overlays: entry unused (loadSubOverlay by file id only). */
    g_us_overlays[3].entry = NULL;
    g_us_overlays[4].entry = NULL;
    g_us_overlays[5].entry = NULL;

    dw3_overlay_bind_field_sections(&g_us_overlays[1], &g_us_overlays[2], &g_us_overlays[3],
                                    &g_us_overlays[4], &g_us_overlays[5]);

    dw3_overlay_set_table(g_us_overlays,
                          sizeof(g_us_overlays) / sizeof(g_us_overlays[0]));
    fprintf(stderr,
            "dw3: registered US overlays STDWTITL/FIELDSTG/STPLNMET/"
            "WSTAG260/780/785 entries=%p/%p/%p\n",
            (void *)STDWTITL_start, (void *)FIELDSTG_start, (void *)STPLNMET_start);
}
