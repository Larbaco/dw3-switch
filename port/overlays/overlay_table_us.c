/*
 * US overlay table for title + field + battle + menus + cardgame + STAGSLCT
 * (P2.6–P3.8).
 *
 * Linked overlays: STDWTITL, FIELDSTG, STPLNMET, WSTAG260/780/785/790,
 * P3.8 batch1 WSTAG200/201/206/210/212/220/225/230/235/240/245/250/255,
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
#define DW3_FILE_WSTAG200 0x1C3 /* 451 */
#define DW3_FILE_WSTAG201 0x1FC /* 508 */
#define DW3_FILE_WSTAG206 0x208 /* 520 */
#define DW3_FILE_WSTAG210 0x1C5 /* 453 */
#define DW3_FILE_WSTAG212 0x221 /* 545 */
#define DW3_FILE_WSTAG220 0x1C6 /* 454 */
#define DW3_FILE_WSTAG225 0x1C7 /* 455 */
#define DW3_FILE_WSTAG230 0x1C8 /* 456 */
#define DW3_FILE_WSTAG235 0x1C9 /* 457 */
#define DW3_FILE_WSTAG240 0x1CA /* 458 */
#define DW3_FILE_WSTAG245 0x1CB /* 459 */
#define DW3_FILE_WSTAG250 0x1CC /* 460 */
#define DW3_FILE_WSTAG255 0x1CD /* 461 */
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
void dw3_overlay_bind_p38_batch1_sections(
    Dw3OverlayDesc *wstag200, Dw3OverlayDesc *wstag201, Dw3OverlayDesc *wstag206,
    Dw3OverlayDesc *wstag210, Dw3OverlayDesc *wstag212, Dw3OverlayDesc *wstag220,
    Dw3OverlayDesc *wstag225, Dw3OverlayDesc *wstag230, Dw3OverlayDesc *wstag235,
    Dw3OverlayDesc *wstag240, Dw3OverlayDesc *wstag245, Dw3OverlayDesc *wstag250,
    Dw3OverlayDesc *wstag255);
void dw3_overlay_bind_battle_sections(Dw3OverlayDesc *fightstg, Dw3OverlayDesc *wfightmn,
                                      Dw3OverlayDesc *stfgtrep);
void dw3_overlay_bind_menu_sections(Dw3OverlayDesc *ststatus, Dw3OverlayDesc *stitshop,
                                    Dw3OverlayDesc *stgtrain, Dw3OverlayDesc *stgdglab,
                                    Dw3OverlayDesc *stgmcard, Dw3OverlayDesc *stdgname);
void dw3_overlay_bind_cardgame_sections(Dw3OverlayDesc *cardgame, Dw3OverlayDesc *stcrddek,
                                        Dw3OverlayDesc *stcrdshp, Dw3OverlayDesc *stcrdabm);
void dw3_overlay_bind_stagslct_sections(Dw3OverlayDesc *stagslct);

enum {
    OVL_STDWTITL = 0,
    OVL_FIELDSTG,
    OVL_STPLNMET,
    OVL_WSTAG260,
    OVL_WSTAG780,
    OVL_WSTAG785,
    OVL_WSTAG790,
    OVL_FIGHTSTG,
    OVL_WFIGHTMN,
    OVL_STFGTREP,
    OVL_STSTATUS,
    OVL_STITSHOP,
    OVL_STGTRAIN,
    OVL_STGDGLAB,
    OVL_STGMCARD,
    OVL_STDGNAME,
    OVL_CARDGAME,
    OVL_STCRDDEK,
    OVL_STCRDSHP,
    OVL_STCRDABM,
    OVL_STAGSLCT,
    OVL_WSTAG200,
    OVL_WSTAG201,
    OVL_WSTAG206,
    OVL_WSTAG210,
    OVL_WSTAG212,
    OVL_WSTAG220,
    OVL_WSTAG225,
    OVL_WSTAG230,
    OVL_WSTAG235,
    OVL_WSTAG240,
    OVL_WSTAG245,
    OVL_WSTAG250,
    OVL_WSTAG255,
    OVL_COUNT
};

static Dw3OverlayDesc g_us_overlays[OVL_COUNT] = {
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
    {DW3_FILE_WSTAG200, NULL, NULL, NULL, 0, NULL, 0},
    {DW3_FILE_WSTAG201, NULL, NULL, NULL, 0, NULL, 0},
    {DW3_FILE_WSTAG206, NULL, NULL, NULL, 0, NULL, 0},
    {DW3_FILE_WSTAG210, NULL, NULL, NULL, 0, NULL, 0},
    {DW3_FILE_WSTAG212, NULL, NULL, NULL, 0, NULL, 0},
    {DW3_FILE_WSTAG220, NULL, NULL, NULL, 0, NULL, 0},
    {DW3_FILE_WSTAG225, NULL, NULL, NULL, 0, NULL, 0},
    {DW3_FILE_WSTAG230, NULL, NULL, NULL, 0, NULL, 0},
    {DW3_FILE_WSTAG235, NULL, NULL, NULL, 0, NULL, 0},
    {DW3_FILE_WSTAG240, NULL, NULL, NULL, 0, NULL, 0},
    {DW3_FILE_WSTAG245, NULL, NULL, NULL, 0, NULL, 0},
    {DW3_FILE_WSTAG250, NULL, NULL, NULL, 0, NULL, 0},
    {DW3_FILE_WSTAG255, NULL, NULL, NULL, 0, NULL, 0},
};

void dw3_overlay_register_us_title(void) {
    g_us_overlays[OVL_STDWTITL].entry = (Dw3OverlayEntry)STDWTITL_start;
    g_us_overlays[OVL_FIELDSTG].entry = (Dw3OverlayEntry)FIELDSTG_start;
    g_us_overlays[OVL_STPLNMET].entry = (Dw3OverlayEntry)STPLNMET_start;
    /* Sub-overlays: entry unused (loadSubOverlay by file id only). */
    g_us_overlays[OVL_WSTAG260].entry = NULL;
    g_us_overlays[OVL_WSTAG780].entry = NULL;
    g_us_overlays[OVL_WSTAG785].entry = NULL;
    g_us_overlays[OVL_WSTAG790].entry = NULL;
    g_us_overlays[OVL_FIGHTSTG].entry = (Dw3OverlayEntry)FIGHTSTG_start;
    g_us_overlays[OVL_WFIGHTMN].entry = NULL;
    g_us_overlays[OVL_STFGTREP].entry = (Dw3OverlayEntry)STFGTREP_start;
    g_us_overlays[OVL_STSTATUS].entry = (Dw3OverlayEntry)STSTATUS_start;
    g_us_overlays[OVL_STITSHOP].entry = (Dw3OverlayEntry)STITSHOP_start;
    g_us_overlays[OVL_STGTRAIN].entry = (Dw3OverlayEntry)STGTRAIN_start;
    g_us_overlays[OVL_STGDGLAB].entry = (Dw3OverlayEntry)STGDGLAB_createScene;
    g_us_overlays[OVL_STGMCARD].entry = (Dw3OverlayEntry)STGMCARD_start;
    g_us_overlays[OVL_STDGNAME].entry = (Dw3OverlayEntry)STDGNAME_start;
    g_us_overlays[OVL_CARDGAME].entry = (Dw3OverlayEntry)CARDGAME_start;
    g_us_overlays[OVL_STCRDDEK].entry = (Dw3OverlayEntry)STCRDDEK_start;
    g_us_overlays[OVL_STCRDSHP].entry = (Dw3OverlayEntry)STCRDSHP_start;
    g_us_overlays[OVL_STCRDABM].entry = (Dw3OverlayEntry)STCRDABM_start;
    g_us_overlays[OVL_STAGSLCT].entry = (Dw3OverlayEntry)STAGSLCT_start;
    g_us_overlays[OVL_WSTAG200].entry = NULL;
    g_us_overlays[OVL_WSTAG201].entry = NULL;
    g_us_overlays[OVL_WSTAG206].entry = NULL;
    g_us_overlays[OVL_WSTAG210].entry = NULL;
    g_us_overlays[OVL_WSTAG212].entry = NULL;
    g_us_overlays[OVL_WSTAG220].entry = NULL;
    g_us_overlays[OVL_WSTAG225].entry = NULL;
    g_us_overlays[OVL_WSTAG230].entry = NULL;
    g_us_overlays[OVL_WSTAG235].entry = NULL;
    g_us_overlays[OVL_WSTAG240].entry = NULL;
    g_us_overlays[OVL_WSTAG245].entry = NULL;
    g_us_overlays[OVL_WSTAG250].entry = NULL;
    g_us_overlays[OVL_WSTAG255].entry = NULL;

    dw3_overlay_bind_field_sections(&g_us_overlays[OVL_FIELDSTG], &g_us_overlays[OVL_STPLNMET],
                                    &g_us_overlays[OVL_WSTAG260], &g_us_overlays[OVL_WSTAG780],
                                    &g_us_overlays[OVL_WSTAG785], &g_us_overlays[OVL_WSTAG790]);
    dw3_overlay_bind_p38_batch1_sections(
        &g_us_overlays[OVL_WSTAG200], &g_us_overlays[OVL_WSTAG201], &g_us_overlays[OVL_WSTAG206],
        &g_us_overlays[OVL_WSTAG210], &g_us_overlays[OVL_WSTAG212], &g_us_overlays[OVL_WSTAG220],
        &g_us_overlays[OVL_WSTAG225], &g_us_overlays[OVL_WSTAG230], &g_us_overlays[OVL_WSTAG235],
        &g_us_overlays[OVL_WSTAG240], &g_us_overlays[OVL_WSTAG245], &g_us_overlays[OVL_WSTAG250],
        &g_us_overlays[OVL_WSTAG255]);
    dw3_overlay_bind_battle_sections(&g_us_overlays[OVL_FIGHTSTG], &g_us_overlays[OVL_WFIGHTMN],
                                     &g_us_overlays[OVL_STFGTREP]);
    dw3_overlay_bind_menu_sections(&g_us_overlays[OVL_STSTATUS], &g_us_overlays[OVL_STITSHOP],
                                   &g_us_overlays[OVL_STGTRAIN], &g_us_overlays[OVL_STGDGLAB],
                                   &g_us_overlays[OVL_STGMCARD], &g_us_overlays[OVL_STDGNAME]);
    dw3_overlay_bind_cardgame_sections(&g_us_overlays[OVL_CARDGAME], &g_us_overlays[OVL_STCRDDEK],
                                       &g_us_overlays[OVL_STCRDSHP], &g_us_overlays[OVL_STCRDABM]);
    dw3_overlay_bind_stagslct_sections(&g_us_overlays[OVL_STAGSLCT]);

    dw3_overlay_set_table(g_us_overlays, OVL_COUNT);
    fprintf(stderr,
            "dw3: registered US overlays STDWTITL/FIELDSTG/STPLNMET/"
            "WSTAG260/780/785/790 + P3.8 batch1 "
            "(200/201/206/210/212/220/225/230/235/240/245/250/255)/"
            "FIGHTSTG/WFIGHTMN/STFGTREP/"
            "STSTATUS/STITSHOP/STGTRAIN/STGDGLAB/STGMCARD/STDGNAME/"
            "CARDGAME/STCRDDEK/STCRDSHP/STCRDABM/STAGSLCT "
            "count=%d\n",
            OVL_COUNT);
}
