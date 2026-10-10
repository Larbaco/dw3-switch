/*
 * US overlay table for title + field + battle + menus + cardgame + STAGSLCT
 * (P2.6–P3.8).
 *
 * Linked overlays: STDWTITL, FIELDSTG, STPLNMET, WSTAG260/780/785/790,
 * P3.8 batch1 WSTAG200/201/206/210/212/220/225/230/235/240/245/250/255,
 * P3.8 batch2 remaining early Central/Asuka + Amaterasu twins,
 * P3.8 batch3 Amaterasu 270-325 twins + Central/East/Seiryu/South,
 * P3.8 batch4 South/Suzaku/Network + West/Byakko/North (skip WSTAG460),
 * P3.8 batch5 Amaterasu twins Central→North (skip WSTAG460),
 * P3.8 batch6 undersea / spy-satellite / underground,
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
#define DW3_FILE_WSTAG480 0x358 /* 856 */
#define DW3_FILE_WSTAG485 0x3a7 /* 935 */
#define DW3_FILE_WSTAG490 0x3f0 /* 1008 */
#define DW3_FILE_WSTAG495 0x3b8 /* 952 */
#define DW3_FILE_WSTAG500 0x3ff /* 1023 */
#define DW3_FILE_WSTAG505 0x20d /* 525 */
#define DW3_FILE_WSTAG520 0x214 /* 532 */
#define DW3_FILE_WSTAG525 0x3b4 /* 948 */
#define DW3_FILE_WSTAG530 0x23e /* 574 */
#define DW3_FILE_WSTAG535 0x286 /* 646 */
#define DW3_FILE_WSTAG537 0x545 /* 1349 */
#define DW3_FILE_WSTAG540 0x572 /* 1394 */
#define DW3_FILE_WSTAG545 0x22f /* 559 */
#define DW3_FILE_WSTAG550 0x45f /* 1119 */
#define DW3_FILE_WSTAG555 0x211 /* 529 */
#define DW3_FILE_WSTAG560 0x2a2 /* 674 */
#define DW3_FILE_WSTAG565 0x50c /* 1292 */
#define DW3_FILE_WSTAG570 0x415 /* 1045 */
#define DW3_FILE_WSTAG575 0x472 /* 1138 */
#define DW3_FILE_WSTAG580 0x58b /* 1419 */
#define DW3_FILE_WSTAG585 0x4fc /* 1276 */
#define DW3_FILE_WSTAG590 0x4fd /* 1277 */
#define DW3_FILE_WSTAG595 0x4fe /* 1278 */
#define DW3_FILE_WSTAG600 0x4f2 /* 1266 */
#define DW3_FILE_WSTAG605 0x476 /* 1142 */
#define DW3_FILE_WSTAG610 0x4b1 /* 1201 */
#define DW3_FILE_WSTAG615 0x54e /* 1358 */
#define DW3_FILE_WSTAG620 0x47d /* 1149 */
#define DW3_FILE_WSTAG625 0x47e /* 1150 */
#define DW3_FILE_WSTAG630 0x485 /* 1157 */
#define DW3_FILE_WSTAG635 0x48d /* 1165 */
#define DW3_FILE_WSTAG640 0x491 /* 1169 */
#define DW3_FILE_WSTAG645 0x499 /* 1177 */
#define DW3_FILE_WSTAG650 0x53f /* 1343 */
#define DW3_FILE_WSTAG655 0x489 /* 1161 */
#define DW3_FILE_WSTAG660 0x4b8 /* 1208 */
#define DW3_FILE_WSTAG675 0x4a1 /* 1185 */
#define DW3_FILE_WSTAG680 0x5d0 /* 1488 */
#define DW3_FILE_WSTAG685 0x4b9 /* 1209 */
#define DW3_FILE_WSTAG690 0x5d8 /* 1496 */
#define DW3_FILE_WSTAG695 0x5e0 /* 1504 */
#define DW3_FILE_WSTAG700 0x5e8 /* 1512 */
#define DW3_FILE_WSTAG705 0x5f0 /* 1520 */
#define DW3_FILE_WSTAG710 0x29a /* 666 */
#define DW3_FILE_WSTAG715 0x5f8 /* 1528 */
#define DW3_FILE_WSTAG720 0x604 /* 1540 */
#define DW3_FILE_WSTAG725 0x60c /* 1548 */
#define DW3_FILE_WSTAG730 0x614 /* 1556 */
#define DW3_FILE_WSTAG735 0x643 /* 1603 */
#define DW3_FILE_WSTAG740 0x645 /* 1605 */
#define DW3_FILE_WSTAG745 0x647 /* 1607 */
#define DW3_FILE_WSTAG750 0x649 /* 1609 */
#define DW3_FILE_WSTAG755 0x64a /* 1610 */
#define DW3_FILE_WSTAG760 0x64f /* 1615 */
#define DW3_FILE_WSTAG331 0x451 /* 1105 */
#define DW3_FILE_WSTAG336 0x452 /* 1106 */
#define DW3_FILE_WSTAG341 0x453 /* 1107 */
#define DW3_FILE_WSTAG346 0x464 /* 1124 */
#define DW3_FILE_WSTAG351 0x46e /* 1134 */
#define DW3_FILE_WSTAG356 0x495 /* 1173 */
#define DW3_FILE_WSTAG361 0x49d /* 1181 */
#define DW3_FILE_WSTAG366 0x4a5 /* 1189 */
#define DW3_FILE_WSTAG371 0x4a9 /* 1193 */
#define DW3_FILE_WSTAG376 0x4bf /* 1215 */
#define DW3_FILE_WSTAG381 0x4c3 /* 1219 */
#define DW3_FILE_WSTAG386 0x4c9 /* 1225 */
#define DW3_FILE_WSTAG396 0x4cd /* 1229 */
#define DW3_FILE_WSTAG401 0x4d1 /* 1233 */
#define DW3_FILE_WSTAG406 0x4db /* 1243 */
#define DW3_FILE_WSTAG411 0x4dc /* 1244 */
#define DW3_FILE_WSTAG421 0x4ee /* 1262 */
#define DW3_FILE_WSTAG426 0x504 /* 1284 */
#define DW3_FILE_WSTAG431 0x508 /* 1288 */
#define DW3_FILE_WSTAG436 0x510 /* 1296 */
#define DW3_FILE_WSTAG441 0x514 /* 1300 */
#define DW3_FILE_WSTAG446 0x518 /* 1304 */
#define DW3_FILE_WSTAG451 0x51c /* 1308 */
#define DW3_FILE_WSTAG456 0x520 /* 1312 */
#define DW3_FILE_WSTAG466 0x524 /* 1316 */
#define DW3_FILE_WSTAG471 0x537 /* 1335 */
#define DW3_FILE_WSTAG476 0x538 /* 1336 */
#define DW3_FILE_WSTAG481 0x539 /* 1337 */
#define DW3_FILE_WSTAG486 0x53a /* 1338 */
#define DW3_FILE_WSTAG491 0x53b /* 1339 */
#define DW3_FILE_WSTAG496 0x540 /* 1344 */
#define DW3_FILE_WSTAG501 0x55a /* 1370 */
#define DW3_FILE_WSTAG506 0x55e /* 1374 */
#define DW3_FILE_WSTAG521 0x562 /* 1378 */
#define DW3_FILE_WSTAG526 0x566 /* 1382 */
#define DW3_FILE_WSTAG531 0x56a /* 1386 */
#define DW3_FILE_WSTAG538 0x56e /* 1390 */
#define DW3_FILE_WSTAG551 0x576 /* 1398 */
#define DW3_FILE_WSTAG556 0x57a /* 1402 */
#define DW3_FILE_WSTAG561 0x57e /* 1406 */
#define DW3_FILE_WSTAG566 0x582 /* 1410 */
#define DW3_FILE_WSTAG571 0x586 /* 1414 */
#define DW3_FILE_WSTAG576 0x587 /* 1415 */
#define DW3_FILE_WSTAG581 0x58f /* 1423 */
#define DW3_FILE_WSTAG586 0x593 /* 1427 */
#define DW3_FILE_WSTAG591 0x597 /* 1431 */
#define DW3_FILE_WSTAG596 0x59b /* 1435 */
#define DW3_FILE_WSTAG601 0x59f /* 1439 */
#define DW3_FILE_WSTAG606 0x5a3 /* 1443 */
#define DW3_FILE_WSTAG611 0x5a7 /* 1447 */
#define DW3_FILE_WSTAG616 0x5ab /* 1451 */
#define DW3_FILE_WSTAG621 0x5af /* 1455 */
#define DW3_FILE_WSTAG631 0x5b3 /* 1459 */
#define DW3_FILE_WSTAG636 0x5b7 /* 1463 */
#define DW3_FILE_WSTAG641 0x5bb /* 1467 */
#define DW3_FILE_WSTAG646 0x5bf /* 1471 */
#define DW3_FILE_WSTAG651 0x5c3 /* 1475 */
#define DW3_FILE_WSTAG656 0x5c7 /* 1479 */
#define DW3_FILE_WSTAG661 0x5cb /* 1483 */
#define DW3_FILE_WSTAG676 0x5cf /* 1487 */
#define DW3_FILE_WSTAG686 0x5d4 /* 1492 */
#define DW3_FILE_WSTAG691 0x5dc /* 1500 */
#define DW3_FILE_WSTAG696 0x5e4 /* 1508 */
#define DW3_FILE_WSTAG701 0x5ec /* 1516 */
#define DW3_FILE_WSTAG706 0x5f4 /* 1524 */
#define DW3_FILE_WSTAG711 0x5f7 /* 1527 */
#define DW3_FILE_WSTAG716 0x600 /* 1536 */
#define DW3_FILE_WSTAG721 0x608 /* 1544 */
#define DW3_FILE_WSTAG726 0x610 /* 1552 */
#define DW3_FILE_WSTAG731 0x624 /* 1572 */
#define DW3_FILE_WSTAG736 0x644 /* 1604 */
#define DW3_FILE_WSTAG741 0x646 /* 1606 */
#define DW3_FILE_WSTAG746 0x648 /* 1608 */
#define DW3_FILE_WSTAG756 0x64b /* 1611 */
#define DW3_FILE_WSTAG761 0x653 /* 1619 */
#define DW3_FILE_WSTAG795 0x657 /* 1623 */
#define DW3_FILE_WSTAG800 0x65b /* 1627 */
#define DW3_FILE_WSTAG805 0x65f /* 1631 */
#define DW3_FILE_WSTAG810 0x663 /* 1635 */
#define DW3_FILE_WSTAG815 0x667 /* 1639 */
#define DW3_FILE_WSTAG820 0x66b /* 1643 */
#define DW3_FILE_WSTAG825 0x671 /* 1649 */
#define DW3_FILE_WSTAG830 0x675 /* 1653 */
#define DW3_FILE_WSTAG835 0x679 /* 1657 */
#define DW3_FILE_WSTAG840 0x691 /* 1681 */
#define DW3_FILE_WSTAG845 0x695 /* 1685 */
#define DW3_FILE_WSTAG850 0x699 /* 1689 */
#define DW3_FILE_WSTAG855 0x69d /* 1693 */
#define DW3_FILE_WSTAG860 0x6a9 /* 1705 */
#define DW3_FILE_WSTAG865 0x6a1 /* 1697 */
#define DW3_FILE_WSTAG870 0x6a5 /* 1701 */
#define DW3_FILE_WSTAG875 0x67d /* 1661 */
#define DW3_FILE_WSTAG880 0x68a /* 1674 */
#define DW3_FILE_WSTAG885 0x68b /* 1675 */
#define DW3_FILE_WSTAG890 0x68c /* 1676 */
#define DW3_FILE_WSTAG895 0x68d /* 1677 */
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
void dw3_overlay_bind_p38_batch4a_sections(
    Dw3OverlayDesc *wstag480,
    Dw3OverlayDesc *wstag485,
    Dw3OverlayDesc *wstag490,
    Dw3OverlayDesc *wstag495,
    Dw3OverlayDesc *wstag500,
    Dw3OverlayDesc *wstag505,
    Dw3OverlayDesc *wstag520,
    Dw3OverlayDesc *wstag525,
    Dw3OverlayDesc *wstag530,
    Dw3OverlayDesc *wstag535,
    Dw3OverlayDesc *wstag537,
    Dw3OverlayDesc *wstag540,
    Dw3OverlayDesc *wstag545);
void dw3_overlay_bind_p38_batch4b_sections(
    Dw3OverlayDesc *wstag550,
    Dw3OverlayDesc *wstag555,
    Dw3OverlayDesc *wstag560,
    Dw3OverlayDesc *wstag565,
    Dw3OverlayDesc *wstag570,
    Dw3OverlayDesc *wstag575,
    Dw3OverlayDesc *wstag580,
    Dw3OverlayDesc *wstag585,
    Dw3OverlayDesc *wstag590,
    Dw3OverlayDesc *wstag595,
    Dw3OverlayDesc *wstag600,
    Dw3OverlayDesc *wstag605,
    Dw3OverlayDesc *wstag610,
    Dw3OverlayDesc *wstag615,
    Dw3OverlayDesc *wstag620);
void dw3_overlay_bind_p38_batch4c_sections(
    Dw3OverlayDesc *wstag625,
    Dw3OverlayDesc *wstag630,
    Dw3OverlayDesc *wstag635,
    Dw3OverlayDesc *wstag640,
    Dw3OverlayDesc *wstag645,
    Dw3OverlayDesc *wstag650,
    Dw3OverlayDesc *wstag655,
    Dw3OverlayDesc *wstag660,
    Dw3OverlayDesc *wstag675,
    Dw3OverlayDesc *wstag680,
    Dw3OverlayDesc *wstag685);
void dw3_overlay_bind_p38_batch4d_sections(
    Dw3OverlayDesc *wstag690,
    Dw3OverlayDesc *wstag695,
    Dw3OverlayDesc *wstag700,
    Dw3OverlayDesc *wstag705,
    Dw3OverlayDesc *wstag710,
    Dw3OverlayDesc *wstag715,
    Dw3OverlayDesc *wstag720,
    Dw3OverlayDesc *wstag725,
    Dw3OverlayDesc *wstag730,
    Dw3OverlayDesc *wstag735,
    Dw3OverlayDesc *wstag740,
    Dw3OverlayDesc *wstag745,
    Dw3OverlayDesc *wstag750,
    Dw3OverlayDesc *wstag755,
    Dw3OverlayDesc *wstag760);
void dw3_overlay_bind_p38_batch5a_sections(
    Dw3OverlayDesc *wstag331,
    Dw3OverlayDesc *wstag336,
    Dw3OverlayDesc *wstag341,
    Dw3OverlayDesc *wstag346,
    Dw3OverlayDesc *wstag351,
    Dw3OverlayDesc *wstag356,
    Dw3OverlayDesc *wstag361,
    Dw3OverlayDesc *wstag366,
    Dw3OverlayDesc *wstag371,
    Dw3OverlayDesc *wstag376,
    Dw3OverlayDesc *wstag381,
    Dw3OverlayDesc *wstag386,
    Dw3OverlayDesc *wstag396,
    Dw3OverlayDesc *wstag401,
    Dw3OverlayDesc *wstag406);
void dw3_overlay_bind_p38_batch5b_sections(
    Dw3OverlayDesc *wstag411,
    Dw3OverlayDesc *wstag421,
    Dw3OverlayDesc *wstag426,
    Dw3OverlayDesc *wstag431,
    Dw3OverlayDesc *wstag436,
    Dw3OverlayDesc *wstag441,
    Dw3OverlayDesc *wstag446,
    Dw3OverlayDesc *wstag451,
    Dw3OverlayDesc *wstag456,
    Dw3OverlayDesc *wstag466,
    Dw3OverlayDesc *wstag471,
    Dw3OverlayDesc *wstag476,
    Dw3OverlayDesc *wstag481,
    Dw3OverlayDesc *wstag486,
    Dw3OverlayDesc *wstag491);
void dw3_overlay_bind_p38_batch5c_sections(
    Dw3OverlayDesc *wstag496,
    Dw3OverlayDesc *wstag501,
    Dw3OverlayDesc *wstag506,
    Dw3OverlayDesc *wstag521,
    Dw3OverlayDesc *wstag526,
    Dw3OverlayDesc *wstag531,
    Dw3OverlayDesc *wstag538,
    Dw3OverlayDesc *wstag551,
    Dw3OverlayDesc *wstag556,
    Dw3OverlayDesc *wstag561,
    Dw3OverlayDesc *wstag566,
    Dw3OverlayDesc *wstag571,
    Dw3OverlayDesc *wstag576,
    Dw3OverlayDesc *wstag581,
    Dw3OverlayDesc *wstag586);
void dw3_overlay_bind_p38_batch5d_sections(
    Dw3OverlayDesc *wstag591,
    Dw3OverlayDesc *wstag596,
    Dw3OverlayDesc *wstag601,
    Dw3OverlayDesc *wstag606,
    Dw3OverlayDesc *wstag611,
    Dw3OverlayDesc *wstag616,
    Dw3OverlayDesc *wstag621,
    Dw3OverlayDesc *wstag631,
    Dw3OverlayDesc *wstag636,
    Dw3OverlayDesc *wstag641,
    Dw3OverlayDesc *wstag646,
    Dw3OverlayDesc *wstag651,
    Dw3OverlayDesc *wstag656,
    Dw3OverlayDesc *wstag661,
    Dw3OverlayDesc *wstag676);
void dw3_overlay_bind_p38_batch5e_sections(
    Dw3OverlayDesc *wstag686,
    Dw3OverlayDesc *wstag691,
    Dw3OverlayDesc *wstag696,
    Dw3OverlayDesc *wstag701,
    Dw3OverlayDesc *wstag706,
    Dw3OverlayDesc *wstag711,
    Dw3OverlayDesc *wstag716,
    Dw3OverlayDesc *wstag721,
    Dw3OverlayDesc *wstag726,
    Dw3OverlayDesc *wstag731,
    Dw3OverlayDesc *wstag736,
    Dw3OverlayDesc *wstag741,
    Dw3OverlayDesc *wstag746,
    Dw3OverlayDesc *wstag756,
    Dw3OverlayDesc *wstag761);
void dw3_overlay_bind_p38_batch6a_sections(
    Dw3OverlayDesc *wstag795,
    Dw3OverlayDesc *wstag800,
    Dw3OverlayDesc *wstag805,
    Dw3OverlayDesc *wstag810,
    Dw3OverlayDesc *wstag815,
    Dw3OverlayDesc *wstag820,
    Dw3OverlayDesc *wstag825,
    Dw3OverlayDesc *wstag830,
    Dw3OverlayDesc *wstag835,
    Dw3OverlayDesc *wstag840,
    Dw3OverlayDesc *wstag845,
    Dw3OverlayDesc *wstag850,
    Dw3OverlayDesc *wstag855,
    Dw3OverlayDesc *wstag860,
    Dw3OverlayDesc *wstag865);
void dw3_overlay_bind_p38_batch6b_sections(
    Dw3OverlayDesc *wstag870,
    Dw3OverlayDesc *wstag875,
    Dw3OverlayDesc *wstag880,
    Dw3OverlayDesc *wstag885,
    Dw3OverlayDesc *wstag890,
    Dw3OverlayDesc *wstag895);
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
    OVL_WSTAG480,
    OVL_WSTAG485,
    OVL_WSTAG490,
    OVL_WSTAG495,
    OVL_WSTAG500,
    OVL_WSTAG505,
    OVL_WSTAG520,
    OVL_WSTAG525,
    OVL_WSTAG530,
    OVL_WSTAG535,
    OVL_WSTAG537,
    OVL_WSTAG540,
    OVL_WSTAG545,
    OVL_WSTAG550,
    OVL_WSTAG555,
    OVL_WSTAG560,
    OVL_WSTAG565,
    OVL_WSTAG570,
    OVL_WSTAG575,
    OVL_WSTAG580,
    OVL_WSTAG585,
    OVL_WSTAG590,
    OVL_WSTAG595,
    OVL_WSTAG600,
    OVL_WSTAG605,
    OVL_WSTAG610,
    OVL_WSTAG615,
    OVL_WSTAG620,
    OVL_WSTAG625,
    OVL_WSTAG630,
    OVL_WSTAG635,
    OVL_WSTAG640,
    OVL_WSTAG645,
    OVL_WSTAG650,
    OVL_WSTAG655,
    OVL_WSTAG660,
    OVL_WSTAG675,
    OVL_WSTAG680,
    OVL_WSTAG685,
    OVL_WSTAG690,
    OVL_WSTAG695,
    OVL_WSTAG700,
    OVL_WSTAG705,
    OVL_WSTAG710,
    OVL_WSTAG715,
    OVL_WSTAG720,
    OVL_WSTAG725,
    OVL_WSTAG730,
    OVL_WSTAG735,
    OVL_WSTAG740,
    OVL_WSTAG745,
    OVL_WSTAG750,
    OVL_WSTAG755,
    OVL_WSTAG760,
    OVL_WSTAG331,
    OVL_WSTAG336,
    OVL_WSTAG341,
    OVL_WSTAG346,
    OVL_WSTAG351,
    OVL_WSTAG356,
    OVL_WSTAG361,
    OVL_WSTAG366,
    OVL_WSTAG371,
    OVL_WSTAG376,
    OVL_WSTAG381,
    OVL_WSTAG386,
    OVL_WSTAG396,
    OVL_WSTAG401,
    OVL_WSTAG406,
    OVL_WSTAG411,
    OVL_WSTAG421,
    OVL_WSTAG426,
    OVL_WSTAG431,
    OVL_WSTAG436,
    OVL_WSTAG441,
    OVL_WSTAG446,
    OVL_WSTAG451,
    OVL_WSTAG456,
    OVL_WSTAG466,
    OVL_WSTAG471,
    OVL_WSTAG476,
    OVL_WSTAG481,
    OVL_WSTAG486,
    OVL_WSTAG491,
    OVL_WSTAG496,
    OVL_WSTAG501,
    OVL_WSTAG506,
    OVL_WSTAG521,
    OVL_WSTAG526,
    OVL_WSTAG531,
    OVL_WSTAG538,
    OVL_WSTAG551,
    OVL_WSTAG556,
    OVL_WSTAG561,
    OVL_WSTAG566,
    OVL_WSTAG571,
    OVL_WSTAG576,
    OVL_WSTAG581,
    OVL_WSTAG586,
    OVL_WSTAG591,
    OVL_WSTAG596,
    OVL_WSTAG601,
    OVL_WSTAG606,
    OVL_WSTAG611,
    OVL_WSTAG616,
    OVL_WSTAG621,
    OVL_WSTAG631,
    OVL_WSTAG636,
    OVL_WSTAG641,
    OVL_WSTAG646,
    OVL_WSTAG651,
    OVL_WSTAG656,
    OVL_WSTAG661,
    OVL_WSTAG676,
    OVL_WSTAG686,
    OVL_WSTAG691,
    OVL_WSTAG696,
    OVL_WSTAG701,
    OVL_WSTAG706,
    OVL_WSTAG711,
    OVL_WSTAG716,
    OVL_WSTAG721,
    OVL_WSTAG726,
    OVL_WSTAG731,
    OVL_WSTAG736,
    OVL_WSTAG741,
    OVL_WSTAG746,
    OVL_WSTAG756,
    OVL_WSTAG761,
    OVL_WSTAG795,
    OVL_WSTAG800,
    OVL_WSTAG805,
    OVL_WSTAG810,
    OVL_WSTAG815,
    OVL_WSTAG820,
    OVL_WSTAG825,
    OVL_WSTAG830,
    OVL_WSTAG835,
    OVL_WSTAG840,
    OVL_WSTAG845,
    OVL_WSTAG850,
    OVL_WSTAG855,
    OVL_WSTAG860,
    OVL_WSTAG865,
    OVL_WSTAG870,
    OVL_WSTAG875,
    OVL_WSTAG880,
    OVL_WSTAG885,
    OVL_WSTAG890,
    OVL_WSTAG895,
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
    {DW3_FILE_WSTAG480, NULL, NULL, NULL, 0, NULL, 0},
    {DW3_FILE_WSTAG485, NULL, NULL, NULL, 0, NULL, 0},
    {DW3_FILE_WSTAG490, NULL, NULL, NULL, 0, NULL, 0},
    {DW3_FILE_WSTAG495, NULL, NULL, NULL, 0, NULL, 0},
    {DW3_FILE_WSTAG500, NULL, NULL, NULL, 0, NULL, 0},
    {DW3_FILE_WSTAG505, NULL, NULL, NULL, 0, NULL, 0},
    {DW3_FILE_WSTAG520, NULL, NULL, NULL, 0, NULL, 0},
    {DW3_FILE_WSTAG525, NULL, NULL, NULL, 0, NULL, 0},
    {DW3_FILE_WSTAG530, NULL, NULL, NULL, 0, NULL, 0},
    {DW3_FILE_WSTAG535, NULL, NULL, NULL, 0, NULL, 0},
    {DW3_FILE_WSTAG537, NULL, NULL, NULL, 0, NULL, 0},
    {DW3_FILE_WSTAG540, NULL, NULL, NULL, 0, NULL, 0},
    {DW3_FILE_WSTAG545, NULL, NULL, NULL, 0, NULL, 0},
    {DW3_FILE_WSTAG550, NULL, NULL, NULL, 0, NULL, 0},
    {DW3_FILE_WSTAG555, NULL, NULL, NULL, 0, NULL, 0},
    {DW3_FILE_WSTAG560, NULL, NULL, NULL, 0, NULL, 0},
    {DW3_FILE_WSTAG565, NULL, NULL, NULL, 0, NULL, 0},
    {DW3_FILE_WSTAG570, NULL, NULL, NULL, 0, NULL, 0},
    {DW3_FILE_WSTAG575, NULL, NULL, NULL, 0, NULL, 0},
    {DW3_FILE_WSTAG580, NULL, NULL, NULL, 0, NULL, 0},
    {DW3_FILE_WSTAG585, NULL, NULL, NULL, 0, NULL, 0},
    {DW3_FILE_WSTAG590, NULL, NULL, NULL, 0, NULL, 0},
    {DW3_FILE_WSTAG595, NULL, NULL, NULL, 0, NULL, 0},
    {DW3_FILE_WSTAG600, NULL, NULL, NULL, 0, NULL, 0},
    {DW3_FILE_WSTAG605, NULL, NULL, NULL, 0, NULL, 0},
    {DW3_FILE_WSTAG610, NULL, NULL, NULL, 0, NULL, 0},
    {DW3_FILE_WSTAG615, NULL, NULL, NULL, 0, NULL, 0},
    {DW3_FILE_WSTAG620, NULL, NULL, NULL, 0, NULL, 0},
    {DW3_FILE_WSTAG625, NULL, NULL, NULL, 0, NULL, 0},
    {DW3_FILE_WSTAG630, NULL, NULL, NULL, 0, NULL, 0},
    {DW3_FILE_WSTAG635, NULL, NULL, NULL, 0, NULL, 0},
    {DW3_FILE_WSTAG640, NULL, NULL, NULL, 0, NULL, 0},
    {DW3_FILE_WSTAG645, NULL, NULL, NULL, 0, NULL, 0},
    {DW3_FILE_WSTAG650, NULL, NULL, NULL, 0, NULL, 0},
    {DW3_FILE_WSTAG655, NULL, NULL, NULL, 0, NULL, 0},
    {DW3_FILE_WSTAG660, NULL, NULL, NULL, 0, NULL, 0},
    {DW3_FILE_WSTAG675, NULL, NULL, NULL, 0, NULL, 0},
    {DW3_FILE_WSTAG680, NULL, NULL, NULL, 0, NULL, 0},
    {DW3_FILE_WSTAG685, NULL, NULL, NULL, 0, NULL, 0},
    {DW3_FILE_WSTAG690, NULL, NULL, NULL, 0, NULL, 0},
    {DW3_FILE_WSTAG695, NULL, NULL, NULL, 0, NULL, 0},
    {DW3_FILE_WSTAG700, NULL, NULL, NULL, 0, NULL, 0},
    {DW3_FILE_WSTAG705, NULL, NULL, NULL, 0, NULL, 0},
    {DW3_FILE_WSTAG710, NULL, NULL, NULL, 0, NULL, 0},
    {DW3_FILE_WSTAG715, NULL, NULL, NULL, 0, NULL, 0},
    {DW3_FILE_WSTAG720, NULL, NULL, NULL, 0, NULL, 0},
    {DW3_FILE_WSTAG725, NULL, NULL, NULL, 0, NULL, 0},
    {DW3_FILE_WSTAG730, NULL, NULL, NULL, 0, NULL, 0},
    {DW3_FILE_WSTAG735, NULL, NULL, NULL, 0, NULL, 0},
    {DW3_FILE_WSTAG740, NULL, NULL, NULL, 0, NULL, 0},
    {DW3_FILE_WSTAG745, NULL, NULL, NULL, 0, NULL, 0},
    {DW3_FILE_WSTAG750, NULL, NULL, NULL, 0, NULL, 0},
    {DW3_FILE_WSTAG755, NULL, NULL, NULL, 0, NULL, 0},
    {DW3_FILE_WSTAG760, NULL, NULL, NULL, 0, NULL, 0},
    {DW3_FILE_WSTAG331, NULL, NULL, NULL, 0, NULL, 0},
    {DW3_FILE_WSTAG336, NULL, NULL, NULL, 0, NULL, 0},
    {DW3_FILE_WSTAG341, NULL, NULL, NULL, 0, NULL, 0},
    {DW3_FILE_WSTAG346, NULL, NULL, NULL, 0, NULL, 0},
    {DW3_FILE_WSTAG351, NULL, NULL, NULL, 0, NULL, 0},
    {DW3_FILE_WSTAG356, NULL, NULL, NULL, 0, NULL, 0},
    {DW3_FILE_WSTAG361, NULL, NULL, NULL, 0, NULL, 0},
    {DW3_FILE_WSTAG366, NULL, NULL, NULL, 0, NULL, 0},
    {DW3_FILE_WSTAG371, NULL, NULL, NULL, 0, NULL, 0},
    {DW3_FILE_WSTAG376, NULL, NULL, NULL, 0, NULL, 0},
    {DW3_FILE_WSTAG381, NULL, NULL, NULL, 0, NULL, 0},
    {DW3_FILE_WSTAG386, NULL, NULL, NULL, 0, NULL, 0},
    {DW3_FILE_WSTAG396, NULL, NULL, NULL, 0, NULL, 0},
    {DW3_FILE_WSTAG401, NULL, NULL, NULL, 0, NULL, 0},
    {DW3_FILE_WSTAG406, NULL, NULL, NULL, 0, NULL, 0},
    {DW3_FILE_WSTAG411, NULL, NULL, NULL, 0, NULL, 0},
    {DW3_FILE_WSTAG421, NULL, NULL, NULL, 0, NULL, 0},
    {DW3_FILE_WSTAG426, NULL, NULL, NULL, 0, NULL, 0},
    {DW3_FILE_WSTAG431, NULL, NULL, NULL, 0, NULL, 0},
    {DW3_FILE_WSTAG436, NULL, NULL, NULL, 0, NULL, 0},
    {DW3_FILE_WSTAG441, NULL, NULL, NULL, 0, NULL, 0},
    {DW3_FILE_WSTAG446, NULL, NULL, NULL, 0, NULL, 0},
    {DW3_FILE_WSTAG451, NULL, NULL, NULL, 0, NULL, 0},
    {DW3_FILE_WSTAG456, NULL, NULL, NULL, 0, NULL, 0},
    {DW3_FILE_WSTAG466, NULL, NULL, NULL, 0, NULL, 0},
    {DW3_FILE_WSTAG471, NULL, NULL, NULL, 0, NULL, 0},
    {DW3_FILE_WSTAG476, NULL, NULL, NULL, 0, NULL, 0},
    {DW3_FILE_WSTAG481, NULL, NULL, NULL, 0, NULL, 0},
    {DW3_FILE_WSTAG486, NULL, NULL, NULL, 0, NULL, 0},
    {DW3_FILE_WSTAG491, NULL, NULL, NULL, 0, NULL, 0},
    {DW3_FILE_WSTAG496, NULL, NULL, NULL, 0, NULL, 0},
    {DW3_FILE_WSTAG501, NULL, NULL, NULL, 0, NULL, 0},
    {DW3_FILE_WSTAG506, NULL, NULL, NULL, 0, NULL, 0},
    {DW3_FILE_WSTAG521, NULL, NULL, NULL, 0, NULL, 0},
    {DW3_FILE_WSTAG526, NULL, NULL, NULL, 0, NULL, 0},
    {DW3_FILE_WSTAG531, NULL, NULL, NULL, 0, NULL, 0},
    {DW3_FILE_WSTAG538, NULL, NULL, NULL, 0, NULL, 0},
    {DW3_FILE_WSTAG551, NULL, NULL, NULL, 0, NULL, 0},
    {DW3_FILE_WSTAG556, NULL, NULL, NULL, 0, NULL, 0},
    {DW3_FILE_WSTAG561, NULL, NULL, NULL, 0, NULL, 0},
    {DW3_FILE_WSTAG566, NULL, NULL, NULL, 0, NULL, 0},
    {DW3_FILE_WSTAG571, NULL, NULL, NULL, 0, NULL, 0},
    {DW3_FILE_WSTAG576, NULL, NULL, NULL, 0, NULL, 0},
    {DW3_FILE_WSTAG581, NULL, NULL, NULL, 0, NULL, 0},
    {DW3_FILE_WSTAG586, NULL, NULL, NULL, 0, NULL, 0},
    {DW3_FILE_WSTAG591, NULL, NULL, NULL, 0, NULL, 0},
    {DW3_FILE_WSTAG596, NULL, NULL, NULL, 0, NULL, 0},
    {DW3_FILE_WSTAG601, NULL, NULL, NULL, 0, NULL, 0},
    {DW3_FILE_WSTAG606, NULL, NULL, NULL, 0, NULL, 0},
    {DW3_FILE_WSTAG611, NULL, NULL, NULL, 0, NULL, 0},
    {DW3_FILE_WSTAG616, NULL, NULL, NULL, 0, NULL, 0},
    {DW3_FILE_WSTAG621, NULL, NULL, NULL, 0, NULL, 0},
    {DW3_FILE_WSTAG631, NULL, NULL, NULL, 0, NULL, 0},
    {DW3_FILE_WSTAG636, NULL, NULL, NULL, 0, NULL, 0},
    {DW3_FILE_WSTAG641, NULL, NULL, NULL, 0, NULL, 0},
    {DW3_FILE_WSTAG646, NULL, NULL, NULL, 0, NULL, 0},
    {DW3_FILE_WSTAG651, NULL, NULL, NULL, 0, NULL, 0},
    {DW3_FILE_WSTAG656, NULL, NULL, NULL, 0, NULL, 0},
    {DW3_FILE_WSTAG661, NULL, NULL, NULL, 0, NULL, 0},
    {DW3_FILE_WSTAG676, NULL, NULL, NULL, 0, NULL, 0},
    {DW3_FILE_WSTAG686, NULL, NULL, NULL, 0, NULL, 0},
    {DW3_FILE_WSTAG691, NULL, NULL, NULL, 0, NULL, 0},
    {DW3_FILE_WSTAG696, NULL, NULL, NULL, 0, NULL, 0},
    {DW3_FILE_WSTAG701, NULL, NULL, NULL, 0, NULL, 0},
    {DW3_FILE_WSTAG706, NULL, NULL, NULL, 0, NULL, 0},
    {DW3_FILE_WSTAG711, NULL, NULL, NULL, 0, NULL, 0},
    {DW3_FILE_WSTAG716, NULL, NULL, NULL, 0, NULL, 0},
    {DW3_FILE_WSTAG721, NULL, NULL, NULL, 0, NULL, 0},
    {DW3_FILE_WSTAG726, NULL, NULL, NULL, 0, NULL, 0},
    {DW3_FILE_WSTAG731, NULL, NULL, NULL, 0, NULL, 0},
    {DW3_FILE_WSTAG736, NULL, NULL, NULL, 0, NULL, 0},
    {DW3_FILE_WSTAG741, NULL, NULL, NULL, 0, NULL, 0},
    {DW3_FILE_WSTAG746, NULL, NULL, NULL, 0, NULL, 0},
    {DW3_FILE_WSTAG756, NULL, NULL, NULL, 0, NULL, 0},
    {DW3_FILE_WSTAG761, NULL, NULL, NULL, 0, NULL, 0},
    {DW3_FILE_WSTAG795, NULL, NULL, NULL, 0, NULL, 0},
    {DW3_FILE_WSTAG800, NULL, NULL, NULL, 0, NULL, 0},
    {DW3_FILE_WSTAG805, NULL, NULL, NULL, 0, NULL, 0},
    {DW3_FILE_WSTAG810, NULL, NULL, NULL, 0, NULL, 0},
    {DW3_FILE_WSTAG815, NULL, NULL, NULL, 0, NULL, 0},
    {DW3_FILE_WSTAG820, NULL, NULL, NULL, 0, NULL, 0},
    {DW3_FILE_WSTAG825, NULL, NULL, NULL, 0, NULL, 0},
    {DW3_FILE_WSTAG830, NULL, NULL, NULL, 0, NULL, 0},
    {DW3_FILE_WSTAG835, NULL, NULL, NULL, 0, NULL, 0},
    {DW3_FILE_WSTAG840, NULL, NULL, NULL, 0, NULL, 0},
    {DW3_FILE_WSTAG845, NULL, NULL, NULL, 0, NULL, 0},
    {DW3_FILE_WSTAG850, NULL, NULL, NULL, 0, NULL, 0},
    {DW3_FILE_WSTAG855, NULL, NULL, NULL, 0, NULL, 0},
    {DW3_FILE_WSTAG860, NULL, NULL, NULL, 0, NULL, 0},
    {DW3_FILE_WSTAG865, NULL, NULL, NULL, 0, NULL, 0},
    {DW3_FILE_WSTAG870, NULL, NULL, NULL, 0, NULL, 0},
    {DW3_FILE_WSTAG875, NULL, NULL, NULL, 0, NULL, 0},
    {DW3_FILE_WSTAG880, NULL, NULL, NULL, 0, NULL, 0},
    {DW3_FILE_WSTAG885, NULL, NULL, NULL, 0, NULL, 0},
    {DW3_FILE_WSTAG890, NULL, NULL, NULL, 0, NULL, 0},
    {DW3_FILE_WSTAG895, NULL, NULL, NULL, 0, NULL, 0},
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
    g_us_overlays[OVL_WSTAG480].entry = NULL;
    g_us_overlays[OVL_WSTAG485].entry = NULL;
    g_us_overlays[OVL_WSTAG490].entry = NULL;
    g_us_overlays[OVL_WSTAG495].entry = NULL;
    g_us_overlays[OVL_WSTAG500].entry = NULL;
    g_us_overlays[OVL_WSTAG505].entry = NULL;
    g_us_overlays[OVL_WSTAG520].entry = NULL;
    g_us_overlays[OVL_WSTAG525].entry = NULL;
    g_us_overlays[OVL_WSTAG530].entry = NULL;
    g_us_overlays[OVL_WSTAG535].entry = NULL;
    g_us_overlays[OVL_WSTAG537].entry = NULL;
    g_us_overlays[OVL_WSTAG540].entry = NULL;
    g_us_overlays[OVL_WSTAG545].entry = NULL;
    g_us_overlays[OVL_WSTAG550].entry = NULL;
    g_us_overlays[OVL_WSTAG555].entry = NULL;
    g_us_overlays[OVL_WSTAG560].entry = NULL;
    g_us_overlays[OVL_WSTAG565].entry = NULL;
    g_us_overlays[OVL_WSTAG570].entry = NULL;
    g_us_overlays[OVL_WSTAG575].entry = NULL;
    g_us_overlays[OVL_WSTAG580].entry = NULL;
    g_us_overlays[OVL_WSTAG585].entry = NULL;
    g_us_overlays[OVL_WSTAG590].entry = NULL;
    g_us_overlays[OVL_WSTAG595].entry = NULL;
    g_us_overlays[OVL_WSTAG600].entry = NULL;
    g_us_overlays[OVL_WSTAG605].entry = NULL;
    g_us_overlays[OVL_WSTAG610].entry = NULL;
    g_us_overlays[OVL_WSTAG615].entry = NULL;
    g_us_overlays[OVL_WSTAG620].entry = NULL;
    g_us_overlays[OVL_WSTAG625].entry = NULL;
    g_us_overlays[OVL_WSTAG630].entry = NULL;
    g_us_overlays[OVL_WSTAG635].entry = NULL;
    g_us_overlays[OVL_WSTAG640].entry = NULL;
    g_us_overlays[OVL_WSTAG645].entry = NULL;
    g_us_overlays[OVL_WSTAG650].entry = NULL;
    g_us_overlays[OVL_WSTAG655].entry = NULL;
    g_us_overlays[OVL_WSTAG660].entry = NULL;
    g_us_overlays[OVL_WSTAG675].entry = NULL;
    g_us_overlays[OVL_WSTAG680].entry = NULL;
    g_us_overlays[OVL_WSTAG685].entry = NULL;
    g_us_overlays[OVL_WSTAG690].entry = NULL;
    g_us_overlays[OVL_WSTAG695].entry = NULL;
    g_us_overlays[OVL_WSTAG700].entry = NULL;
    g_us_overlays[OVL_WSTAG705].entry = NULL;
    g_us_overlays[OVL_WSTAG710].entry = NULL;
    g_us_overlays[OVL_WSTAG715].entry = NULL;
    g_us_overlays[OVL_WSTAG720].entry = NULL;
    g_us_overlays[OVL_WSTAG725].entry = NULL;
    g_us_overlays[OVL_WSTAG730].entry = NULL;
    g_us_overlays[OVL_WSTAG735].entry = NULL;
    g_us_overlays[OVL_WSTAG740].entry = NULL;
    g_us_overlays[OVL_WSTAG745].entry = NULL;
    g_us_overlays[OVL_WSTAG750].entry = NULL;
    g_us_overlays[OVL_WSTAG755].entry = NULL;
    g_us_overlays[OVL_WSTAG760].entry = NULL;
    g_us_overlays[OVL_WSTAG331].entry = NULL;
    g_us_overlays[OVL_WSTAG336].entry = NULL;
    g_us_overlays[OVL_WSTAG341].entry = NULL;
    g_us_overlays[OVL_WSTAG346].entry = NULL;
    g_us_overlays[OVL_WSTAG351].entry = NULL;
    g_us_overlays[OVL_WSTAG356].entry = NULL;
    g_us_overlays[OVL_WSTAG361].entry = NULL;
    g_us_overlays[OVL_WSTAG366].entry = NULL;
    g_us_overlays[OVL_WSTAG371].entry = NULL;
    g_us_overlays[OVL_WSTAG376].entry = NULL;
    g_us_overlays[OVL_WSTAG381].entry = NULL;
    g_us_overlays[OVL_WSTAG386].entry = NULL;
    g_us_overlays[OVL_WSTAG396].entry = NULL;
    g_us_overlays[OVL_WSTAG401].entry = NULL;
    g_us_overlays[OVL_WSTAG406].entry = NULL;
    g_us_overlays[OVL_WSTAG411].entry = NULL;
    g_us_overlays[OVL_WSTAG421].entry = NULL;
    g_us_overlays[OVL_WSTAG426].entry = NULL;
    g_us_overlays[OVL_WSTAG431].entry = NULL;
    g_us_overlays[OVL_WSTAG436].entry = NULL;
    g_us_overlays[OVL_WSTAG441].entry = NULL;
    g_us_overlays[OVL_WSTAG446].entry = NULL;
    g_us_overlays[OVL_WSTAG451].entry = NULL;
    g_us_overlays[OVL_WSTAG456].entry = NULL;
    g_us_overlays[OVL_WSTAG466].entry = NULL;
    g_us_overlays[OVL_WSTAG471].entry = NULL;
    g_us_overlays[OVL_WSTAG476].entry = NULL;
    g_us_overlays[OVL_WSTAG481].entry = NULL;
    g_us_overlays[OVL_WSTAG486].entry = NULL;
    g_us_overlays[OVL_WSTAG491].entry = NULL;
    g_us_overlays[OVL_WSTAG496].entry = NULL;
    g_us_overlays[OVL_WSTAG501].entry = NULL;
    g_us_overlays[OVL_WSTAG506].entry = NULL;
    g_us_overlays[OVL_WSTAG521].entry = NULL;
    g_us_overlays[OVL_WSTAG526].entry = NULL;
    g_us_overlays[OVL_WSTAG531].entry = NULL;
    g_us_overlays[OVL_WSTAG538].entry = NULL;
    g_us_overlays[OVL_WSTAG551].entry = NULL;
    g_us_overlays[OVL_WSTAG556].entry = NULL;
    g_us_overlays[OVL_WSTAG561].entry = NULL;
    g_us_overlays[OVL_WSTAG566].entry = NULL;
    g_us_overlays[OVL_WSTAG571].entry = NULL;
    g_us_overlays[OVL_WSTAG576].entry = NULL;
    g_us_overlays[OVL_WSTAG581].entry = NULL;
    g_us_overlays[OVL_WSTAG586].entry = NULL;
    g_us_overlays[OVL_WSTAG591].entry = NULL;
    g_us_overlays[OVL_WSTAG596].entry = NULL;
    g_us_overlays[OVL_WSTAG601].entry = NULL;
    g_us_overlays[OVL_WSTAG606].entry = NULL;
    g_us_overlays[OVL_WSTAG611].entry = NULL;
    g_us_overlays[OVL_WSTAG616].entry = NULL;
    g_us_overlays[OVL_WSTAG621].entry = NULL;
    g_us_overlays[OVL_WSTAG631].entry = NULL;
    g_us_overlays[OVL_WSTAG636].entry = NULL;
    g_us_overlays[OVL_WSTAG641].entry = NULL;
    g_us_overlays[OVL_WSTAG646].entry = NULL;
    g_us_overlays[OVL_WSTAG651].entry = NULL;
    g_us_overlays[OVL_WSTAG656].entry = NULL;
    g_us_overlays[OVL_WSTAG661].entry = NULL;
    g_us_overlays[OVL_WSTAG676].entry = NULL;
    g_us_overlays[OVL_WSTAG686].entry = NULL;
    g_us_overlays[OVL_WSTAG691].entry = NULL;
    g_us_overlays[OVL_WSTAG696].entry = NULL;
    g_us_overlays[OVL_WSTAG701].entry = NULL;
    g_us_overlays[OVL_WSTAG706].entry = NULL;
    g_us_overlays[OVL_WSTAG711].entry = NULL;
    g_us_overlays[OVL_WSTAG716].entry = NULL;
    g_us_overlays[OVL_WSTAG721].entry = NULL;
    g_us_overlays[OVL_WSTAG726].entry = NULL;
    g_us_overlays[OVL_WSTAG731].entry = NULL;
    g_us_overlays[OVL_WSTAG736].entry = NULL;
    g_us_overlays[OVL_WSTAG741].entry = NULL;
    g_us_overlays[OVL_WSTAG746].entry = NULL;
    g_us_overlays[OVL_WSTAG756].entry = NULL;
    g_us_overlays[OVL_WSTAG761].entry = NULL;
    g_us_overlays[OVL_WSTAG795].entry = NULL;
    g_us_overlays[OVL_WSTAG800].entry = NULL;
    g_us_overlays[OVL_WSTAG805].entry = NULL;
    g_us_overlays[OVL_WSTAG810].entry = NULL;
    g_us_overlays[OVL_WSTAG815].entry = NULL;
    g_us_overlays[OVL_WSTAG820].entry = NULL;
    g_us_overlays[OVL_WSTAG825].entry = NULL;
    g_us_overlays[OVL_WSTAG830].entry = NULL;
    g_us_overlays[OVL_WSTAG835].entry = NULL;
    g_us_overlays[OVL_WSTAG840].entry = NULL;
    g_us_overlays[OVL_WSTAG845].entry = NULL;
    g_us_overlays[OVL_WSTAG850].entry = NULL;
    g_us_overlays[OVL_WSTAG855].entry = NULL;
    g_us_overlays[OVL_WSTAG860].entry = NULL;
    g_us_overlays[OVL_WSTAG865].entry = NULL;
    g_us_overlays[OVL_WSTAG870].entry = NULL;
    g_us_overlays[OVL_WSTAG875].entry = NULL;
    g_us_overlays[OVL_WSTAG880].entry = NULL;
    g_us_overlays[OVL_WSTAG885].entry = NULL;
    g_us_overlays[OVL_WSTAG890].entry = NULL;
    g_us_overlays[OVL_WSTAG895].entry = NULL;

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
    dw3_overlay_bind_p38_batch4a_sections(
        &g_us_overlays[OVL_WSTAG480],
        &g_us_overlays[OVL_WSTAG485],
        &g_us_overlays[OVL_WSTAG490],
        &g_us_overlays[OVL_WSTAG495],
        &g_us_overlays[OVL_WSTAG500],
        &g_us_overlays[OVL_WSTAG505],
        &g_us_overlays[OVL_WSTAG520],
        &g_us_overlays[OVL_WSTAG525],
        &g_us_overlays[OVL_WSTAG530],
        &g_us_overlays[OVL_WSTAG535],
        &g_us_overlays[OVL_WSTAG537],
        &g_us_overlays[OVL_WSTAG540],
        &g_us_overlays[OVL_WSTAG545]);
    dw3_overlay_bind_p38_batch4b_sections(
        &g_us_overlays[OVL_WSTAG550],
        &g_us_overlays[OVL_WSTAG555],
        &g_us_overlays[OVL_WSTAG560],
        &g_us_overlays[OVL_WSTAG565],
        &g_us_overlays[OVL_WSTAG570],
        &g_us_overlays[OVL_WSTAG575],
        &g_us_overlays[OVL_WSTAG580],
        &g_us_overlays[OVL_WSTAG585],
        &g_us_overlays[OVL_WSTAG590],
        &g_us_overlays[OVL_WSTAG595],
        &g_us_overlays[OVL_WSTAG600],
        &g_us_overlays[OVL_WSTAG605],
        &g_us_overlays[OVL_WSTAG610],
        &g_us_overlays[OVL_WSTAG615],
        &g_us_overlays[OVL_WSTAG620]);
    dw3_overlay_bind_p38_batch4c_sections(
        &g_us_overlays[OVL_WSTAG625],
        &g_us_overlays[OVL_WSTAG630],
        &g_us_overlays[OVL_WSTAG635],
        &g_us_overlays[OVL_WSTAG640],
        &g_us_overlays[OVL_WSTAG645],
        &g_us_overlays[OVL_WSTAG650],
        &g_us_overlays[OVL_WSTAG655],
        &g_us_overlays[OVL_WSTAG660],
        &g_us_overlays[OVL_WSTAG675],
        &g_us_overlays[OVL_WSTAG680],
        &g_us_overlays[OVL_WSTAG685]);
    dw3_overlay_bind_p38_batch4d_sections(
        &g_us_overlays[OVL_WSTAG690],
        &g_us_overlays[OVL_WSTAG695],
        &g_us_overlays[OVL_WSTAG700],
        &g_us_overlays[OVL_WSTAG705],
        &g_us_overlays[OVL_WSTAG710],
        &g_us_overlays[OVL_WSTAG715],
        &g_us_overlays[OVL_WSTAG720],
        &g_us_overlays[OVL_WSTAG725],
        &g_us_overlays[OVL_WSTAG730],
        &g_us_overlays[OVL_WSTAG735],
        &g_us_overlays[OVL_WSTAG740],
        &g_us_overlays[OVL_WSTAG745],
        &g_us_overlays[OVL_WSTAG750],
        &g_us_overlays[OVL_WSTAG755],
        &g_us_overlays[OVL_WSTAG760]);
    dw3_overlay_bind_p38_batch5a_sections(
        &g_us_overlays[OVL_WSTAG331],
        &g_us_overlays[OVL_WSTAG336],
        &g_us_overlays[OVL_WSTAG341],
        &g_us_overlays[OVL_WSTAG346],
        &g_us_overlays[OVL_WSTAG351],
        &g_us_overlays[OVL_WSTAG356],
        &g_us_overlays[OVL_WSTAG361],
        &g_us_overlays[OVL_WSTAG366],
        &g_us_overlays[OVL_WSTAG371],
        &g_us_overlays[OVL_WSTAG376],
        &g_us_overlays[OVL_WSTAG381],
        &g_us_overlays[OVL_WSTAG386],
        &g_us_overlays[OVL_WSTAG396],
        &g_us_overlays[OVL_WSTAG401],
        &g_us_overlays[OVL_WSTAG406]);
    dw3_overlay_bind_p38_batch5b_sections(
        &g_us_overlays[OVL_WSTAG411],
        &g_us_overlays[OVL_WSTAG421],
        &g_us_overlays[OVL_WSTAG426],
        &g_us_overlays[OVL_WSTAG431],
        &g_us_overlays[OVL_WSTAG436],
        &g_us_overlays[OVL_WSTAG441],
        &g_us_overlays[OVL_WSTAG446],
        &g_us_overlays[OVL_WSTAG451],
        &g_us_overlays[OVL_WSTAG456],
        &g_us_overlays[OVL_WSTAG466],
        &g_us_overlays[OVL_WSTAG471],
        &g_us_overlays[OVL_WSTAG476],
        &g_us_overlays[OVL_WSTAG481],
        &g_us_overlays[OVL_WSTAG486],
        &g_us_overlays[OVL_WSTAG491]);
    dw3_overlay_bind_p38_batch5c_sections(
        &g_us_overlays[OVL_WSTAG496],
        &g_us_overlays[OVL_WSTAG501],
        &g_us_overlays[OVL_WSTAG506],
        &g_us_overlays[OVL_WSTAG521],
        &g_us_overlays[OVL_WSTAG526],
        &g_us_overlays[OVL_WSTAG531],
        &g_us_overlays[OVL_WSTAG538],
        &g_us_overlays[OVL_WSTAG551],
        &g_us_overlays[OVL_WSTAG556],
        &g_us_overlays[OVL_WSTAG561],
        &g_us_overlays[OVL_WSTAG566],
        &g_us_overlays[OVL_WSTAG571],
        &g_us_overlays[OVL_WSTAG576],
        &g_us_overlays[OVL_WSTAG581],
        &g_us_overlays[OVL_WSTAG586]);
    dw3_overlay_bind_p38_batch5d_sections(
        &g_us_overlays[OVL_WSTAG591],
        &g_us_overlays[OVL_WSTAG596],
        &g_us_overlays[OVL_WSTAG601],
        &g_us_overlays[OVL_WSTAG606],
        &g_us_overlays[OVL_WSTAG611],
        &g_us_overlays[OVL_WSTAG616],
        &g_us_overlays[OVL_WSTAG621],
        &g_us_overlays[OVL_WSTAG631],
        &g_us_overlays[OVL_WSTAG636],
        &g_us_overlays[OVL_WSTAG641],
        &g_us_overlays[OVL_WSTAG646],
        &g_us_overlays[OVL_WSTAG651],
        &g_us_overlays[OVL_WSTAG656],
        &g_us_overlays[OVL_WSTAG661],
        &g_us_overlays[OVL_WSTAG676]);
    dw3_overlay_bind_p38_batch5e_sections(
        &g_us_overlays[OVL_WSTAG686],
        &g_us_overlays[OVL_WSTAG691],
        &g_us_overlays[OVL_WSTAG696],
        &g_us_overlays[OVL_WSTAG701],
        &g_us_overlays[OVL_WSTAG706],
        &g_us_overlays[OVL_WSTAG711],
        &g_us_overlays[OVL_WSTAG716],
        &g_us_overlays[OVL_WSTAG721],
        &g_us_overlays[OVL_WSTAG726],
        &g_us_overlays[OVL_WSTAG731],
        &g_us_overlays[OVL_WSTAG736],
        &g_us_overlays[OVL_WSTAG741],
        &g_us_overlays[OVL_WSTAG746],
        &g_us_overlays[OVL_WSTAG756],
        &g_us_overlays[OVL_WSTAG761]);
    dw3_overlay_bind_p38_batch6a_sections(
        &g_us_overlays[OVL_WSTAG795],
        &g_us_overlays[OVL_WSTAG800],
        &g_us_overlays[OVL_WSTAG805],
        &g_us_overlays[OVL_WSTAG810],
        &g_us_overlays[OVL_WSTAG815],
        &g_us_overlays[OVL_WSTAG820],
        &g_us_overlays[OVL_WSTAG825],
        &g_us_overlays[OVL_WSTAG830],
        &g_us_overlays[OVL_WSTAG835],
        &g_us_overlays[OVL_WSTAG840],
        &g_us_overlays[OVL_WSTAG845],
        &g_us_overlays[OVL_WSTAG850],
        &g_us_overlays[OVL_WSTAG855],
        &g_us_overlays[OVL_WSTAG860],
        &g_us_overlays[OVL_WSTAG865]);
    dw3_overlay_bind_p38_batch6b_sections(
        &g_us_overlays[OVL_WSTAG870],
        &g_us_overlays[OVL_WSTAG875],
        &g_us_overlays[OVL_WSTAG880],
        &g_us_overlays[OVL_WSTAG885],
        &g_us_overlays[OVL_WSTAG890],
        &g_us_overlays[OVL_WSTAG895]);
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
            "WSTAG260/780/785/790 + P3.8 batch1+2+3+4+5+6 stages/"
            "FIGHTSTG/WFIGHTMN/STFGTREP/"
            "STSTATUS/STITSHOP/STGTRAIN/STGDGLAB/STGMCARD/STDGNAME/"
            "CARDGAME/STCRDDEK/STCRDSHP/STCRDABM/STAGSLCT "
            "count=%d\n",
            OVL_COUNT);
}
