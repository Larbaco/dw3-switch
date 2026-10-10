/*
 * US overlay table for title + field + battle + menus + cardgame + STAGSLCT
 * (P2.6–P3.8).
 *
 * Linked overlays: STDWTITL, FIELDSTG, STPLNMET, WSTAG260/780/785/790,
 * P3.8 batch1 WSTAG200/201/206/210/212/220/225/230/235/240/245/250/255,
 * P3.8 batch2 remaining early Central/Asuka + Amaterasu twins,
 * P3.8 batch3 Amaterasu 270-325 twins + Central/East/Seiryu/South,
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
#define DW3_FILE_WSTAG202 0x1FD /* 509 */
#define DW3_FILE_WSTAG203 0x1FE /* 510 */
#define DW3_FILE_WSTAG205 0x1C4 /* 452 */
#define DW3_FILE_WSTAG218 0x225 /* 549 */
#define DW3_FILE_WSTAG219 0x23F /* 575 */
#define DW3_FILE_WSTAG232 0x244 /* 580 */
#define DW3_FILE_WSTAG237 0x28C /* 652 */
#define DW3_FILE_WSTAG270 0x1CF /* 463 */
#define DW3_FILE_WSTAG275 0x1D0 /* 464 */
#define DW3_FILE_WSTAG280 0x1D1 /* 465 */
#define DW3_FILE_WSTAG285 0x2A7 /* 679 */
#define DW3_FILE_WSTAG290 0x29E /* 670 */
#define DW3_FILE_WSTAG295 0x35B /* 859 */
#define DW3_FILE_WSTAG300 0x341 /* 833 */
#define DW3_FILE_WSTAG305 0x33D /* 829 */
#define DW3_FILE_WSTAG310 0x335 /* 821 */
#define DW3_FILE_WSTAG315 0x3F6 /* 1014 */
#define DW3_FILE_WSTAG320 0x296 /* 662 */
#define DW3_FILE_WSTAG325 0x43C /* 1084 */
#define DW3_FILE_WSTAG211 0x209 /* 521 */
#define DW3_FILE_WSTAG221 0x241 /* 577 */
#define DW3_FILE_WSTAG226 0x242 /* 578 */
#define DW3_FILE_WSTAG231 0x243 /* 579 */
#define DW3_FILE_WSTAG233 0x261 /* 609 */
#define DW3_FILE_WSTAG236 0x270 /* 624 */
#define DW3_FILE_WSTAG238 0x291 /* 657 */
#define DW3_FILE_WSTAG241 0x2AA /* 682 */
#define DW3_FILE_WSTAG246 0x2AE /* 686 */
#define DW3_FILE_WSTAG251 0x2AF /* 687 */
#define DW3_FILE_WSTAG256 0x283 /* 643 */
#define DW3_FILE_WSTAG261 0x2B8 /* 696 */
#define DW3_FILE_WSTAG271 0x31a /* 794 */
#define DW3_FILE_WSTAG276 0x31b /* 795 */
#define DW3_FILE_WSTAG281 0x32b /* 811 */
#define DW3_FILE_WSTAG286 0x32f /* 815 */
#define DW3_FILE_WSTAG291 0x339 /* 825 */
#define DW3_FILE_WSTAG296 0x35c /* 860 */
#define DW3_FILE_WSTAG301 0x35d /* 861 */
#define DW3_FILE_WSTAG306 0x398 /* 920 */
#define DW3_FILE_WSTAG311 0x399 /* 921 */
#define DW3_FILE_WSTAG316 0x3f7 /* 1015 */
#define DW3_FILE_WSTAG321 0x425 /* 1061 */
#define DW3_FILE_WSTAG326 0x450 /* 1104 */
#define DW3_FILE_WSTAG330 0x1d2 /* 466 */
#define DW3_FILE_WSTAG335 0x1d3 /* 467 */
#define DW3_FILE_WSTAG340 0x393 /* 915 */
#define DW3_FILE_WSTAG345 0x397 /* 919 */
#define DW3_FILE_WSTAG350 0x1d4 /* 468 */
#define DW3_FILE_WSTAG355 0x1d5 /* 469 */
#define DW3_FILE_WSTAG360 0x1e0 /* 480 */
#define DW3_FILE_WSTAG365 0x20e /* 526 */
#define DW3_FILE_WSTAG370 0x1d6 /* 470 */
#define DW3_FILE_WSTAG375 0x1d7 /* 471 */
#define DW3_FILE_WSTAG380 0x2ad /* 685 */
#define DW3_FILE_WSTAG385 0x4c4 /* 1220 */
#define DW3_FILE_WSTAG395 0x1de /* 478 */
#define DW3_FILE_WSTAG400 0x3fb /* 1019 */
#define DW3_FILE_WSTAG405 0x22e /* 558 */
#define DW3_FILE_WSTAG410 0x38e /* 910 */
#define DW3_FILE_WSTAG415 0x4dd /* 1245 */
#define DW3_FILE_WSTAG420 0x4e9 /* 1257 */
#define DW3_FILE_WSTAG425 0x227 /* 551 */
#define DW3_FILE_WSTAG430 0x1e1 /* 481 */
#define DW3_FILE_WSTAG435 0x1d8 /* 472 */
#define DW3_FILE_WSTAG440 0x36e /* 878 */
#define DW3_FILE_WSTAG445 0x502 /* 1282 */
#define DW3_FILE_WSTAG450 0x38a /* 906 */
#define DW3_FILE_WSTAG455 0x382 /* 898 */
#define DW3_FILE_WSTAG475 0x1e2 /* 482 */
#define DW3_FILE_WSTAG465 0x1d9 /* 473 */
#define DW3_FILE_WSTAG470 0x206 /* 518 */
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
void dw3_overlay_bind_p38_batch2a_sections(
    Dw3OverlayDesc *wstag202,
    Dw3OverlayDesc *wstag203,
    Dw3OverlayDesc *wstag205,
    Dw3OverlayDesc *wstag218,
    Dw3OverlayDesc *wstag219,
    Dw3OverlayDesc *wstag232,
    Dw3OverlayDesc *wstag237,
    Dw3OverlayDesc *wstag270,
    Dw3OverlayDesc *wstag275,
    Dw3OverlayDesc *wstag280,
    Dw3OverlayDesc *wstag285,
    Dw3OverlayDesc *wstag290,
    Dw3OverlayDesc *wstag295,
    Dw3OverlayDesc *wstag300,
    Dw3OverlayDesc *wstag305,
    Dw3OverlayDesc *wstag310,
    Dw3OverlayDesc *wstag315,
    Dw3OverlayDesc *wstag320,
    Dw3OverlayDesc *wstag325);
void dw3_overlay_bind_p38_batch2b_sections(
    Dw3OverlayDesc *wstag211,
    Dw3OverlayDesc *wstag221,
    Dw3OverlayDesc *wstag226,
    Dw3OverlayDesc *wstag231,
    Dw3OverlayDesc *wstag233,
    Dw3OverlayDesc *wstag236,
    Dw3OverlayDesc *wstag238,
    Dw3OverlayDesc *wstag241,
    Dw3OverlayDesc *wstag246,
    Dw3OverlayDesc *wstag251,
    Dw3OverlayDesc *wstag256,
    Dw3OverlayDesc *wstag261);
void dw3_overlay_bind_p38_batch3a_sections(
    Dw3OverlayDesc *wstag271,
    Dw3OverlayDesc *wstag276,
    Dw3OverlayDesc *wstag281,
    Dw3OverlayDesc *wstag286,
    Dw3OverlayDesc *wstag291,
    Dw3OverlayDesc *wstag296,
    Dw3OverlayDesc *wstag301,
    Dw3OverlayDesc *wstag306,
    Dw3OverlayDesc *wstag311,
    Dw3OverlayDesc *wstag316,
    Dw3OverlayDesc *wstag321,
    Dw3OverlayDesc *wstag326);
void dw3_overlay_bind_p38_batch3b_sections(
    Dw3OverlayDesc *wstag330,
    Dw3OverlayDesc *wstag335,
    Dw3OverlayDesc *wstag340,
    Dw3OverlayDesc *wstag345,
    Dw3OverlayDesc *wstag350,
    Dw3OverlayDesc *wstag355,
    Dw3OverlayDesc *wstag360,
    Dw3OverlayDesc *wstag365,
    Dw3OverlayDesc *wstag370,
    Dw3OverlayDesc *wstag375,
    Dw3OverlayDesc *wstag380,
    Dw3OverlayDesc *wstag385,
    Dw3OverlayDesc *wstag395,
    Dw3OverlayDesc *wstag400,
    Dw3OverlayDesc *wstag405,
    Dw3OverlayDesc *wstag410,
    Dw3OverlayDesc *wstag415,
    Dw3OverlayDesc *wstag420);
void dw3_overlay_bind_p38_batch3c_sections(
    Dw3OverlayDesc *wstag425,
    Dw3OverlayDesc *wstag430,
    Dw3OverlayDesc *wstag435,
    Dw3OverlayDesc *wstag440,
    Dw3OverlayDesc *wstag445,
    Dw3OverlayDesc *wstag450,
    Dw3OverlayDesc *wstag455,
    Dw3OverlayDesc *wstag475,
    Dw3OverlayDesc *wstag465,
    Dw3OverlayDesc *wstag470);
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
    OVL_WSTAG202,
    OVL_WSTAG203,
    OVL_WSTAG205,
    OVL_WSTAG218,
    OVL_WSTAG219,
    OVL_WSTAG232,
    OVL_WSTAG237,
    OVL_WSTAG270,
    OVL_WSTAG275,
    OVL_WSTAG280,
    OVL_WSTAG285,
    OVL_WSTAG290,
    OVL_WSTAG295,
    OVL_WSTAG300,
    OVL_WSTAG305,
    OVL_WSTAG310,
    OVL_WSTAG315,
    OVL_WSTAG320,
    OVL_WSTAG325,
    OVL_WSTAG211,
    OVL_WSTAG221,
    OVL_WSTAG226,
    OVL_WSTAG231,
    OVL_WSTAG233,
    OVL_WSTAG236,
    OVL_WSTAG238,
    OVL_WSTAG241,
    OVL_WSTAG246,
    OVL_WSTAG251,
    OVL_WSTAG256,
    OVL_WSTAG261,
    OVL_WSTAG271,
    OVL_WSTAG276,
    OVL_WSTAG281,
    OVL_WSTAG286,
    OVL_WSTAG291,
    OVL_WSTAG296,
    OVL_WSTAG301,
    OVL_WSTAG306,
    OVL_WSTAG311,
    OVL_WSTAG316,
    OVL_WSTAG321,
    OVL_WSTAG326,
    OVL_WSTAG330,
    OVL_WSTAG335,
    OVL_WSTAG340,
    OVL_WSTAG345,
    OVL_WSTAG350,
    OVL_WSTAG355,
    OVL_WSTAG360,
    OVL_WSTAG365,
    OVL_WSTAG370,
    OVL_WSTAG375,
    OVL_WSTAG380,
    OVL_WSTAG385,
    OVL_WSTAG395,
    OVL_WSTAG400,
    OVL_WSTAG405,
    OVL_WSTAG410,
    OVL_WSTAG415,
    OVL_WSTAG420,
    OVL_WSTAG425,
    OVL_WSTAG430,
    OVL_WSTAG435,
    OVL_WSTAG440,
    OVL_WSTAG445,
    OVL_WSTAG450,
    OVL_WSTAG455,
    OVL_WSTAG475,
    OVL_WSTAG465,
    OVL_WSTAG470,
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
    {DW3_FILE_WSTAG202, NULL, NULL, NULL, 0, NULL, 0},
    {DW3_FILE_WSTAG203, NULL, NULL, NULL, 0, NULL, 0},
    {DW3_FILE_WSTAG205, NULL, NULL, NULL, 0, NULL, 0},
    {DW3_FILE_WSTAG218, NULL, NULL, NULL, 0, NULL, 0},
    {DW3_FILE_WSTAG219, NULL, NULL, NULL, 0, NULL, 0},
    {DW3_FILE_WSTAG232, NULL, NULL, NULL, 0, NULL, 0},
    {DW3_FILE_WSTAG237, NULL, NULL, NULL, 0, NULL, 0},
    {DW3_FILE_WSTAG270, NULL, NULL, NULL, 0, NULL, 0},
    {DW3_FILE_WSTAG275, NULL, NULL, NULL, 0, NULL, 0},
    {DW3_FILE_WSTAG280, NULL, NULL, NULL, 0, NULL, 0},
    {DW3_FILE_WSTAG285, NULL, NULL, NULL, 0, NULL, 0},
    {DW3_FILE_WSTAG290, NULL, NULL, NULL, 0, NULL, 0},
    {DW3_FILE_WSTAG295, NULL, NULL, NULL, 0, NULL, 0},
    {DW3_FILE_WSTAG300, NULL, NULL, NULL, 0, NULL, 0},
    {DW3_FILE_WSTAG305, NULL, NULL, NULL, 0, NULL, 0},
    {DW3_FILE_WSTAG310, NULL, NULL, NULL, 0, NULL, 0},
    {DW3_FILE_WSTAG315, NULL, NULL, NULL, 0, NULL, 0},
    {DW3_FILE_WSTAG320, NULL, NULL, NULL, 0, NULL, 0},
    {DW3_FILE_WSTAG325, NULL, NULL, NULL, 0, NULL, 0},
    {DW3_FILE_WSTAG211, NULL, NULL, NULL, 0, NULL, 0},
    {DW3_FILE_WSTAG221, NULL, NULL, NULL, 0, NULL, 0},
    {DW3_FILE_WSTAG226, NULL, NULL, NULL, 0, NULL, 0},
    {DW3_FILE_WSTAG231, NULL, NULL, NULL, 0, NULL, 0},
    {DW3_FILE_WSTAG233, NULL, NULL, NULL, 0, NULL, 0},
    {DW3_FILE_WSTAG236, NULL, NULL, NULL, 0, NULL, 0},
    {DW3_FILE_WSTAG238, NULL, NULL, NULL, 0, NULL, 0},
    {DW3_FILE_WSTAG241, NULL, NULL, NULL, 0, NULL, 0},
    {DW3_FILE_WSTAG246, NULL, NULL, NULL, 0, NULL, 0},
    {DW3_FILE_WSTAG251, NULL, NULL, NULL, 0, NULL, 0},
    {DW3_FILE_WSTAG256, NULL, NULL, NULL, 0, NULL, 0},
    {DW3_FILE_WSTAG261, NULL, NULL, NULL, 0, NULL, 0},
    {DW3_FILE_WSTAG271, NULL, NULL, NULL, 0, NULL, 0},
    {DW3_FILE_WSTAG276, NULL, NULL, NULL, 0, NULL, 0},
    {DW3_FILE_WSTAG281, NULL, NULL, NULL, 0, NULL, 0},
    {DW3_FILE_WSTAG286, NULL, NULL, NULL, 0, NULL, 0},
    {DW3_FILE_WSTAG291, NULL, NULL, NULL, 0, NULL, 0},
    {DW3_FILE_WSTAG296, NULL, NULL, NULL, 0, NULL, 0},
    {DW3_FILE_WSTAG301, NULL, NULL, NULL, 0, NULL, 0},
    {DW3_FILE_WSTAG306, NULL, NULL, NULL, 0, NULL, 0},
    {DW3_FILE_WSTAG311, NULL, NULL, NULL, 0, NULL, 0},
    {DW3_FILE_WSTAG316, NULL, NULL, NULL, 0, NULL, 0},
    {DW3_FILE_WSTAG321, NULL, NULL, NULL, 0, NULL, 0},
    {DW3_FILE_WSTAG326, NULL, NULL, NULL, 0, NULL, 0},
    {DW3_FILE_WSTAG330, NULL, NULL, NULL, 0, NULL, 0},
    {DW3_FILE_WSTAG335, NULL, NULL, NULL, 0, NULL, 0},
    {DW3_FILE_WSTAG340, NULL, NULL, NULL, 0, NULL, 0},
    {DW3_FILE_WSTAG345, NULL, NULL, NULL, 0, NULL, 0},
    {DW3_FILE_WSTAG350, NULL, NULL, NULL, 0, NULL, 0},
    {DW3_FILE_WSTAG355, NULL, NULL, NULL, 0, NULL, 0},
    {DW3_FILE_WSTAG360, NULL, NULL, NULL, 0, NULL, 0},
    {DW3_FILE_WSTAG365, NULL, NULL, NULL, 0, NULL, 0},
    {DW3_FILE_WSTAG370, NULL, NULL, NULL, 0, NULL, 0},
    {DW3_FILE_WSTAG375, NULL, NULL, NULL, 0, NULL, 0},
    {DW3_FILE_WSTAG380, NULL, NULL, NULL, 0, NULL, 0},
    {DW3_FILE_WSTAG385, NULL, NULL, NULL, 0, NULL, 0},
    {DW3_FILE_WSTAG395, NULL, NULL, NULL, 0, NULL, 0},
    {DW3_FILE_WSTAG400, NULL, NULL, NULL, 0, NULL, 0},
    {DW3_FILE_WSTAG405, NULL, NULL, NULL, 0, NULL, 0},
    {DW3_FILE_WSTAG410, NULL, NULL, NULL, 0, NULL, 0},
    {DW3_FILE_WSTAG415, NULL, NULL, NULL, 0, NULL, 0},
    {DW3_FILE_WSTAG420, NULL, NULL, NULL, 0, NULL, 0},
    {DW3_FILE_WSTAG425, NULL, NULL, NULL, 0, NULL, 0},
    {DW3_FILE_WSTAG430, NULL, NULL, NULL, 0, NULL, 0},
    {DW3_FILE_WSTAG435, NULL, NULL, NULL, 0, NULL, 0},
    {DW3_FILE_WSTAG440, NULL, NULL, NULL, 0, NULL, 0},
    {DW3_FILE_WSTAG445, NULL, NULL, NULL, 0, NULL, 0},
    {DW3_FILE_WSTAG450, NULL, NULL, NULL, 0, NULL, 0},
    {DW3_FILE_WSTAG455, NULL, NULL, NULL, 0, NULL, 0},
    {DW3_FILE_WSTAG475, NULL, NULL, NULL, 0, NULL, 0},
    {DW3_FILE_WSTAG465, NULL, NULL, NULL, 0, NULL, 0},
    {DW3_FILE_WSTAG470, NULL, NULL, NULL, 0, NULL, 0},
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
    g_us_overlays[OVL_WSTAG202].entry = NULL;
    g_us_overlays[OVL_WSTAG203].entry = NULL;
    g_us_overlays[OVL_WSTAG205].entry = NULL;
    g_us_overlays[OVL_WSTAG218].entry = NULL;
    g_us_overlays[OVL_WSTAG219].entry = NULL;
    g_us_overlays[OVL_WSTAG232].entry = NULL;
    g_us_overlays[OVL_WSTAG237].entry = NULL;
    g_us_overlays[OVL_WSTAG270].entry = NULL;
    g_us_overlays[OVL_WSTAG275].entry = NULL;
    g_us_overlays[OVL_WSTAG280].entry = NULL;
    g_us_overlays[OVL_WSTAG285].entry = NULL;
    g_us_overlays[OVL_WSTAG290].entry = NULL;
    g_us_overlays[OVL_WSTAG295].entry = NULL;
    g_us_overlays[OVL_WSTAG300].entry = NULL;
    g_us_overlays[OVL_WSTAG305].entry = NULL;
    g_us_overlays[OVL_WSTAG310].entry = NULL;
    g_us_overlays[OVL_WSTAG315].entry = NULL;
    g_us_overlays[OVL_WSTAG320].entry = NULL;
    g_us_overlays[OVL_WSTAG325].entry = NULL;
    g_us_overlays[OVL_WSTAG211].entry = NULL;
    g_us_overlays[OVL_WSTAG221].entry = NULL;
    g_us_overlays[OVL_WSTAG226].entry = NULL;
    g_us_overlays[OVL_WSTAG231].entry = NULL;
    g_us_overlays[OVL_WSTAG233].entry = NULL;
    g_us_overlays[OVL_WSTAG236].entry = NULL;
    g_us_overlays[OVL_WSTAG238].entry = NULL;
    g_us_overlays[OVL_WSTAG241].entry = NULL;
    g_us_overlays[OVL_WSTAG246].entry = NULL;
    g_us_overlays[OVL_WSTAG251].entry = NULL;
    g_us_overlays[OVL_WSTAG256].entry = NULL;
    g_us_overlays[OVL_WSTAG261].entry = NULL;
    g_us_overlays[OVL_WSTAG271].entry = NULL;
    g_us_overlays[OVL_WSTAG276].entry = NULL;
    g_us_overlays[OVL_WSTAG281].entry = NULL;
    g_us_overlays[OVL_WSTAG286].entry = NULL;
    g_us_overlays[OVL_WSTAG291].entry = NULL;
    g_us_overlays[OVL_WSTAG296].entry = NULL;
    g_us_overlays[OVL_WSTAG301].entry = NULL;
    g_us_overlays[OVL_WSTAG306].entry = NULL;
    g_us_overlays[OVL_WSTAG311].entry = NULL;
    g_us_overlays[OVL_WSTAG316].entry = NULL;
    g_us_overlays[OVL_WSTAG321].entry = NULL;
    g_us_overlays[OVL_WSTAG326].entry = NULL;
    g_us_overlays[OVL_WSTAG330].entry = NULL;
    g_us_overlays[OVL_WSTAG335].entry = NULL;
    g_us_overlays[OVL_WSTAG340].entry = NULL;
    g_us_overlays[OVL_WSTAG345].entry = NULL;
    g_us_overlays[OVL_WSTAG350].entry = NULL;
    g_us_overlays[OVL_WSTAG355].entry = NULL;
    g_us_overlays[OVL_WSTAG360].entry = NULL;
    g_us_overlays[OVL_WSTAG365].entry = NULL;
    g_us_overlays[OVL_WSTAG370].entry = NULL;
    g_us_overlays[OVL_WSTAG375].entry = NULL;
    g_us_overlays[OVL_WSTAG380].entry = NULL;
    g_us_overlays[OVL_WSTAG385].entry = NULL;
    g_us_overlays[OVL_WSTAG395].entry = NULL;
    g_us_overlays[OVL_WSTAG400].entry = NULL;
    g_us_overlays[OVL_WSTAG405].entry = NULL;
    g_us_overlays[OVL_WSTAG410].entry = NULL;
    g_us_overlays[OVL_WSTAG415].entry = NULL;
    g_us_overlays[OVL_WSTAG420].entry = NULL;
    g_us_overlays[OVL_WSTAG425].entry = NULL;
    g_us_overlays[OVL_WSTAG430].entry = NULL;
    g_us_overlays[OVL_WSTAG435].entry = NULL;
    g_us_overlays[OVL_WSTAG440].entry = NULL;
    g_us_overlays[OVL_WSTAG445].entry = NULL;
    g_us_overlays[OVL_WSTAG450].entry = NULL;
    g_us_overlays[OVL_WSTAG455].entry = NULL;
    g_us_overlays[OVL_WSTAG475].entry = NULL;
    g_us_overlays[OVL_WSTAG465].entry = NULL;
    g_us_overlays[OVL_WSTAG470].entry = NULL;

    dw3_overlay_bind_field_sections(&g_us_overlays[OVL_FIELDSTG], &g_us_overlays[OVL_STPLNMET],
                                    &g_us_overlays[OVL_WSTAG260], &g_us_overlays[OVL_WSTAG780],
                                    &g_us_overlays[OVL_WSTAG785], &g_us_overlays[OVL_WSTAG790]);
    dw3_overlay_bind_p38_batch1_sections(
        &g_us_overlays[OVL_WSTAG200], &g_us_overlays[OVL_WSTAG201], &g_us_overlays[OVL_WSTAG206],
        &g_us_overlays[OVL_WSTAG210], &g_us_overlays[OVL_WSTAG212], &g_us_overlays[OVL_WSTAG220],
        &g_us_overlays[OVL_WSTAG225], &g_us_overlays[OVL_WSTAG230], &g_us_overlays[OVL_WSTAG235],
        &g_us_overlays[OVL_WSTAG240], &g_us_overlays[OVL_WSTAG245], &g_us_overlays[OVL_WSTAG250],
        &g_us_overlays[OVL_WSTAG255]);
    dw3_overlay_bind_p38_batch2a_sections(
        &g_us_overlays[OVL_WSTAG202], &g_us_overlays[OVL_WSTAG203], &g_us_overlays[OVL_WSTAG205],
        &g_us_overlays[OVL_WSTAG218], &g_us_overlays[OVL_WSTAG219], &g_us_overlays[OVL_WSTAG232],
        &g_us_overlays[OVL_WSTAG237], &g_us_overlays[OVL_WSTAG270], &g_us_overlays[OVL_WSTAG275],
        &g_us_overlays[OVL_WSTAG280], &g_us_overlays[OVL_WSTAG285], &g_us_overlays[OVL_WSTAG290],
        &g_us_overlays[OVL_WSTAG295], &g_us_overlays[OVL_WSTAG300], &g_us_overlays[OVL_WSTAG305],
        &g_us_overlays[OVL_WSTAG310], &g_us_overlays[OVL_WSTAG315], &g_us_overlays[OVL_WSTAG320],
        &g_us_overlays[OVL_WSTAG325]);
    dw3_overlay_bind_p38_batch2b_sections(
        &g_us_overlays[OVL_WSTAG211], &g_us_overlays[OVL_WSTAG221], &g_us_overlays[OVL_WSTAG226],
        &g_us_overlays[OVL_WSTAG231], &g_us_overlays[OVL_WSTAG233], &g_us_overlays[OVL_WSTAG236],
        &g_us_overlays[OVL_WSTAG238], &g_us_overlays[OVL_WSTAG241], &g_us_overlays[OVL_WSTAG246],
        &g_us_overlays[OVL_WSTAG251], &g_us_overlays[OVL_WSTAG256], &g_us_overlays[OVL_WSTAG261]);
    dw3_overlay_bind_p38_batch3a_sections(
        &g_us_overlays[OVL_WSTAG271],
        &g_us_overlays[OVL_WSTAG276],
        &g_us_overlays[OVL_WSTAG281],
        &g_us_overlays[OVL_WSTAG286],
        &g_us_overlays[OVL_WSTAG291],
        &g_us_overlays[OVL_WSTAG296],
        &g_us_overlays[OVL_WSTAG301],
        &g_us_overlays[OVL_WSTAG306],
        &g_us_overlays[OVL_WSTAG311],
        &g_us_overlays[OVL_WSTAG316],
        &g_us_overlays[OVL_WSTAG321],
        &g_us_overlays[OVL_WSTAG326]);
    dw3_overlay_bind_p38_batch3b_sections(
        &g_us_overlays[OVL_WSTAG330],
        &g_us_overlays[OVL_WSTAG335],
        &g_us_overlays[OVL_WSTAG340],
        &g_us_overlays[OVL_WSTAG345],
        &g_us_overlays[OVL_WSTAG350],
        &g_us_overlays[OVL_WSTAG355],
        &g_us_overlays[OVL_WSTAG360],
        &g_us_overlays[OVL_WSTAG365],
        &g_us_overlays[OVL_WSTAG370],
        &g_us_overlays[OVL_WSTAG375],
        &g_us_overlays[OVL_WSTAG380],
        &g_us_overlays[OVL_WSTAG385],
        &g_us_overlays[OVL_WSTAG395],
        &g_us_overlays[OVL_WSTAG400],
        &g_us_overlays[OVL_WSTAG405],
        &g_us_overlays[OVL_WSTAG410],
        &g_us_overlays[OVL_WSTAG415],
        &g_us_overlays[OVL_WSTAG420]);
    dw3_overlay_bind_p38_batch3c_sections(
        &g_us_overlays[OVL_WSTAG425],
        &g_us_overlays[OVL_WSTAG430],
        &g_us_overlays[OVL_WSTAG435],
        &g_us_overlays[OVL_WSTAG440],
        &g_us_overlays[OVL_WSTAG445],
        &g_us_overlays[OVL_WSTAG450],
        &g_us_overlays[OVL_WSTAG455],
        &g_us_overlays[OVL_WSTAG475],
        &g_us_overlays[OVL_WSTAG465],
        &g_us_overlays[OVL_WSTAG470]);
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
            "WSTAG260/780/785/790 + P3.8 batch1+2+3 stages/"
            "FIGHTSTG/WFIGHTMN/STFGTREP/"
            "STSTATUS/STITSHOP/STGTRAIN/STGDGLAB/STGMCARD/STDGNAME/"
            "CARDGAME/STCRDDEK/STCRDSHP/STCRDABM/STAGSLCT "
            "count=%d\n",
            OVL_COUNT);
}
