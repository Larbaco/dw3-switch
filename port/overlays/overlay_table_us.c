/*
 * US overlay table for title + field + battle + menus + cardgame + STAGSLCT
 * (P2.6–P3.8).
 *
 * Linked overlays: STDWTITL, FIELDSTG, STPLNMET, WSTAG260/780/785/790,
 * FIGHTSTG, WFIGHTMN, STFGTREP, STSTATUS, STITSHOP, STGTRAIN, STGDGLAB,
 * STGMCARD, STDGNAME, CARDGAME, STCRDDEK, STCRDSHP, STCRDABM, STAGSLCT.
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
#define DW3_FILE_STSTATUS 0x1F9
#define DW3_FILE_STITSHOP 0x1F7
#define DW3_FILE_STGTRAIN 0x1F6
#define DW3_FILE_STGDGLAB 0x1DF
#define DW3_FILE_STGMCARD 0x1E3
#define DW3_FILE_STDGNAME 0x154
#define DW3_FILE_CARDGAME 0x153
#define DW3_FILE_STCRDDEK 0x1BC
#define DW3_FILE_STCRDSHP 0x1C0
#define DW3_FILE_STCRDABM 0x177
#define DW3_FILE_STAGSLCT 0x176

void *STDWTITL_start(void);
void *FIELDSTG_start(void);
void *STPLNMET_start(void);
void *FIGHTSTG_start(void);
void *STFGTREP_start(void);
void *STSTATUS_start(void);
void *STITSHOP_start(void);
void *STGTRAIN_start(void);
void *STGDGLAB_createScene(void);
void *STGMCARD_start(void);
void *STDGNAME_start(void);
void *CARDGAME_start(void);
void *STCRDDEK_start(void);
void *STCRDSHP_start(void);
void *STCRDABM_start(void);
void *STAGSLCT_start(void);

void dw3_overlay_bind_field_sections(Dw3OverlayDesc *fieldstg, Dw3OverlayDesc *stplnmet,
                                     Dw3OverlayDesc *wstag260, Dw3OverlayDesc *wstag780,
                                     Dw3OverlayDesc *wstag785, Dw3OverlayDesc *wstag790);
void dw3_overlay_bind_battle_sections(Dw3OverlayDesc *fightstg, Dw3OverlayDesc *wfightmn,
                                      Dw3OverlayDesc *stfgtrep);
void dw3_overlay_bind_menu_sections(Dw3OverlayDesc *ststatus, Dw3OverlayDesc *stitshop,
                                    Dw3OverlayDesc *stgtrain, Dw3OverlayDesc *stgdglab,
                                    Dw3OverlayDesc *stgmcard, Dw3OverlayDesc *stdgname);
void dw3_overlay_bind_cardgame_sections(Dw3OverlayDesc *cardgame, Dw3OverlayDesc *stcrddek,
                                        Dw3OverlayDesc *stcrdshp, Dw3OverlayDesc *stcrdabm);
void dw3_overlay_bind_stagslct_sections(Dw3OverlayDesc *stagslct);

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
    {DW3_FILE_STSTATUS, NULL, NULL, NULL, 0, NULL, 0},
    {DW3_FILE_STITSHOP, NULL, NULL, NULL, 0, NULL, 0},
    {DW3_FILE_STGTRAIN, NULL, NULL, NULL, 0, NULL, 0},
    {DW3_FILE_STGDGLAB, NULL, NULL, NULL, 0, NULL, 0},
    {DW3_FILE_STGMCARD, NULL, NULL, NULL, 0, NULL, 0},
    {DW3_FILE_STDGNAME, NULL, NULL, NULL, 0, NULL, 0},
    {DW3_FILE_CARDGAME, NULL, NULL, NULL, 0, NULL, 0},
    {DW3_FILE_STCRDDEK, NULL, NULL, NULL, 0, NULL, 0},
    {DW3_FILE_STCRDSHP, NULL, NULL, NULL, 0, NULL, 0},
    {DW3_FILE_STCRDABM, NULL, NULL, NULL, 0, NULL, 0},
    {DW3_FILE_STAGSLCT, NULL, NULL, NULL, 0, NULL, 0},
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
    g_us_overlays[10].entry = (Dw3OverlayEntry)STSTATUS_start;
    g_us_overlays[11].entry = (Dw3OverlayEntry)STITSHOP_start;
    g_us_overlays[12].entry = (Dw3OverlayEntry)STGTRAIN_start;
    g_us_overlays[13].entry = (Dw3OverlayEntry)STGDGLAB_createScene;
    g_us_overlays[14].entry = (Dw3OverlayEntry)STGMCARD_start;
    g_us_overlays[15].entry = (Dw3OverlayEntry)STDGNAME_start;
    g_us_overlays[16].entry = (Dw3OverlayEntry)CARDGAME_start;
    g_us_overlays[17].entry = (Dw3OverlayEntry)STCRDDEK_start;
    g_us_overlays[18].entry = (Dw3OverlayEntry)STCRDSHP_start;
    g_us_overlays[19].entry = (Dw3OverlayEntry)STCRDABM_start;
    g_us_overlays[20].entry = (Dw3OverlayEntry)STAGSLCT_start;

    dw3_overlay_bind_field_sections(&g_us_overlays[1], &g_us_overlays[2], &g_us_overlays[3],
                                    &g_us_overlays[4], &g_us_overlays[5], &g_us_overlays[6]);
    dw3_overlay_bind_battle_sections(&g_us_overlays[7], &g_us_overlays[8], &g_us_overlays[9]);
    dw3_overlay_bind_menu_sections(&g_us_overlays[10], &g_us_overlays[11], &g_us_overlays[12],
                                   &g_us_overlays[13], &g_us_overlays[14], &g_us_overlays[15]);
    dw3_overlay_bind_cardgame_sections(&g_us_overlays[16], &g_us_overlays[17], &g_us_overlays[18],
                                       &g_us_overlays[19]);
    dw3_overlay_bind_stagslct_sections(&g_us_overlays[20]);

    dw3_overlay_set_table(g_us_overlays,
                          sizeof(g_us_overlays) / sizeof(g_us_overlays[0]));
    fprintf(stderr,
            "dw3: registered US overlays STDWTITL/FIELDSTG/STPLNMET/"
            "WSTAG260/780/785/790/FIGHTSTG/WFIGHTMN/STFGTREP/"
            "STSTATUS/STITSHOP/STGTRAIN/STGDGLAB/STGMCARD/STDGNAME/"
            "CARDGAME/STCRDDEK/STCRDSHP/STCRDABM/STAGSLCT "
            "entries=%p/%p/%p/%p/%p/%p/%p/%p/%p/%p/%p/%p/%p/%p/%p/%p\n",
            (void *)STDWTITL_start, (void *)FIELDSTG_start, (void *)STPLNMET_start,
            (void *)FIGHTSTG_start, (void *)STFGTREP_start, (void *)STSTATUS_start,
            (void *)STITSHOP_start, (void *)STGTRAIN_start, (void *)STGDGLAB_createScene,
            (void *)STGMCARD_start, (void *)STDGNAME_start, (void *)CARDGAME_start,
            (void *)STCRDDEK_start, (void *)STCRDSHP_start, (void *)STCRDABM_start,
            (void *)STAGSLCT_start);
}
