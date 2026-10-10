/*
 * US overlay table for title + field + battle bring-up (P2.6 / P2.8 / P3.2 / P3.3).
 *
 * Linked overlays: STDWTITL, FIELDSTG, STPLNMET, WSTAG260/780/785/790,
 * FIGHTSTG, WFIGHTMN, STFGTREP.
 * .data/.bss snapshots are bound from named sections at register time.
 */
#ifndef PORT_HOST
#error "port/overlays/overlay_table_us.c is host-only"
#endif

#include "overlay_host.h"

#include <stdio.h>

/* US file ids from include/engine/overlay.h / battle_mode.h / FIELDSTG_stages */
#define DW3_FILE_STDWTITL 0x1C1
#define DW3_FILE_FIELDSTG 0x158
#define DW3_FILE_STPLNMET 0x1F8
#define DW3_FILE_WSTAG260 0x1CE /* US stage file id 462 = 0x1CE */
#define DW3_FILE_WSTAG780 0x1DA /* 474 */
#define DW3_FILE_WSTAG785 0x1DB /* 475 */
#define DW3_FILE_WSTAG790 0x1DC /* 476 */
#define DW3_FILE_FIGHTSTG 0x159
#define DW3_FILE_WFIGHTMN 0x1FA
#define DW3_FILE_STFGTREP 0x1C2

void *STDWTITL_start(void);
void *FIELDSTG_start(void);
void *STPLNMET_start(void);
void *FIGHTSTG_start(void);
void *STFGTREP_start(void);

void dw3_overlay_bind_field_sections(Dw3OverlayDesc *fieldstg, Dw3OverlayDesc *stplnmet,
                                     Dw3OverlayDesc *wstag260, Dw3OverlayDesc *wstag780,
                                     Dw3OverlayDesc *wstag785, Dw3OverlayDesc *wstag790);
void dw3_overlay_bind_battle_sections(Dw3OverlayDesc *fightstg, Dw3OverlayDesc *wfightmn,
                                      Dw3OverlayDesc *stfgtrep);

static Dw3OverlayDesc g_us_overlays[] = {
    {DW3_FILE_STDWTITL, NULL, NULL, NULL, 0, NULL, 0},
    {DW3_FILE_FIELDSTG, NULL, NULL, NULL, 0, NULL, 0},
    {DW3_FILE_STPLNMET, NULL, NULL, NULL, 0, NULL, 0},
    {DW3_FILE_WSTAG260, NULL, NULL, NULL, 0, NULL, 0},
    {DW3_FILE_WSTAG780, NULL, NULL, NULL, 0, NULL, 0},
    {DW3_FILE_WSTAG785, NULL, NULL, NULL, 0, NULL, 0},
    {DW3_FILE_WSTAG790, NULL, NULL, NULL, 0, NULL, 0},
    {DW3_FILE_FIGHTSTG, NULL, NULL, NULL, 0, NULL, 0},
    {DW3_FILE_WFIGHTMN, NULL, NULL, NULL, 0, NULL, 0},
    {DW3_FILE_STFGTREP, NULL, NULL, NULL, 0, NULL, 0},
};

void dw3_overlay_register_us_title(void) {
    g_us_overlays[0].entry = (Dw3OverlayEntry)STDWTITL_start;
    g_us_overlays[1].entry = (Dw3OverlayEntry)FIELDSTG_start;
    g_us_overlays[2].entry = (Dw3OverlayEntry)STPLNMET_start;
    /* Sub-overlays: entry unused (loadSubOverlay by file id only). */
    g_us_overlays[3].entry = NULL;
    g_us_overlays[4].entry = NULL;
    g_us_overlays[5].entry = NULL;
    g_us_overlays[6].entry = NULL;
    g_us_overlays[7].entry = (Dw3OverlayEntry)FIGHTSTG_start;
    g_us_overlays[8].entry = NULL;
    g_us_overlays[9].entry = (Dw3OverlayEntry)STFGTREP_start;

    dw3_overlay_bind_field_sections(&g_us_overlays[1], &g_us_overlays[2], &g_us_overlays[3],
                                    &g_us_overlays[4], &g_us_overlays[5], &g_us_overlays[6]);
    dw3_overlay_bind_battle_sections(&g_us_overlays[7], &g_us_overlays[8], &g_us_overlays[9]);

    dw3_overlay_set_table(g_us_overlays,
                          sizeof(g_us_overlays) / sizeof(g_us_overlays[0]));
    fprintf(stderr,
            "dw3: registered US overlays STDWTITL/FIELDSTG/STPLNMET/"
            "WSTAG260/780/785/790/FIGHTSTG/WFIGHTMN/STFGTREP "
            "entries=%p/%p/%p/%p/%p\n",
            (void *)STDWTITL_start, (void *)FIELDSTG_start, (void *)STPLNMET_start,
            (void *)FIGHTSTG_start, (void *)STFGTREP_start);
}
