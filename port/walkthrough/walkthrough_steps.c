/* P6.35 walkthrough: the step table (docs/P6.35_WALKTHROUGH.md, section 2.2).
 * Logic only; the strings are in port/walkthrough/text/<lang>.txt keyed by
 * `id`. DRAFT, owner review pending.
 *
 * Evidence (decomp, vendor/dw3_decomp, src/field/stages). A step at progress N
 * is the thing to do to reach N+1, so its row is the stage whose event assigns
 * `GAME.progress = N+1` (the stage and event are listed per chapter below;
 * tools/tests/test_walkthrough.py checks every assignment, every flag setter
 * and every map area against the decomp). Progress values the first half
 * assigns: 0 to 0x29; the next one, 0x2B (FIELD_PROGRESS_MOVIE_BATTLES), is
 * written by the Control Room of the Spy Satellite (wstag820) and starts the
 * last battle (ch11); the EU disc goes on to the extra chapter at 0x2D (ch12, region EU).
 *
 * Rows are ordered by from_progress, then order (the order field grows over
 * the whole table). Several rows may share one progress value when the writer
 * stage only plays after a flag or an item that another stage sets; the
 * earlier row carries that condition as its `done`, so the resolver moves on
 * to the next row when the game says it happened. Condition codes are the
 * game's own (WT_FLAG = FLAG(group, id), WT_ITEM = ITEM(kind, id)). The map
 * areas are dw3_wt_area_of_mode() of the step's stage (the game's
 * STSTATUS_getArea). Region: ALL for every row except the EU only chapter
 * (progress 0x2D, ch12) and its optional rows.
 *
 * SIDE rows (slice 4) sit after the first row of their chapter, open at that
 * chapter's first progress value (the value the decomp's own gate opens at, see
 * the evidence per row below) and never decide the current step:
 *  ch04.x01/x02  0xF    Seabed (825..860) and its forge (845): UG Lakes 525/526, SPECIAL(0x93)
 *  ch08.x01/x02  0x1E   Circuit Board (865..895) and the Black Kingz' den (875): SPECIAL(0x94)
 *  ch12.x01/x02  0x2D   EU: Kulon Weapons (731), the extra Circuit Board (970, 971)
 *
 *  ch01  0..4    kusanagi 780/785, asuka 225/212/200 (endEvent0, 1, 5, 10 to 12, 52)
 *  ch02  5..0xA  410 East Station (90), 440 South Station (152), 480 Jungle
 *                Grave (170/171), 475 Shaman House (180, 230), 375/205/202
 *                Protocol Ruins, Asuka Bridge, Central Sector (flags
 *                0x1A18..0x1A1B), 380 Divermon's Lake (205), 320 Asuka Sewers
 *                (item 0x15), 520 Suzaku Hall (260/261)
 *  ch03  0xB..0xE 212 Main Lobby (271/273, 352), 237 Underground Path (300),
 *                305 Master Room (350), 460 Reliability Spot (370)
 *  ch04  0xF..0x11 685 Underground Cave (413), 655 Mirage Room (430/431), 605
 *                Operation Room (510/511)
 *  ch05  0x12..0x17 615 Sewers (530/531), 620 Secret Room (532/534, 540, 551),
 *                525 Suzaku UG Lake (flag 0x4003), 325 Control Room (375/376,
 *                560), 305 Master Room (660/661), 310 A.o.A Headquarters
 *                (680 to 682), 210 Main Lobby (685)
 *  ch06  0x18..0x1A 685 Underground Cave (690), 520 Suzaku Hall (695), 545 Bug
 *                Maze Pit (710/711)
 *  ch07  0x1B..0x1D 421 Qing Long City (740 to 743), 521 Zhu Que Hall
 *                (750/751), 526 Zhu Que UG Lake (760)
 *  ch08  0x1E..0x21 755 Battle Gate (780/781), 750 Chamber Room (820, 886),
 *                780 Kusanagi City (885, flag 0x4066), 305 Master Room (890,
 *                893 to 895), 295 Prison Tower (flag 0x4059)
 *  ch09  0x22..0x24 676 Bai Hu Dome (900/901), 756 Battle Gate (910/911), 206
 *                Amaterasu Bridge (921/922)
 *  ch10  0x25..0x29 311 A.o.A Headquarters Amaterasu (flag 0x4060, 981/982),
 *                226 Login Room (935), 310 A.o.A Headquarters (961), 810
 *                Gunslinger 1F (1035), 820 Control Room (1060, 1080)
 *  ch11  0x2B    820 Control Room (endEvent1080 starts the last battle); 780 Kusanagi
 *                City (event 1500) follows the ending
 *  ch12  0x2D    EU only: 925 Login Room (event 1600, flag 0x40CD), 956 Qing Long City,
 *                949 Protocol Ruins, 954 Tyranno Valley (flags 0x100D to 0x1010),
 *                926 Arena Front Desk and 927 Digimon Arena (event 1608, flag 0x100C)
 */
#include "port_walkthrough.h"

#define AREA_KUSANAGI WT_AREA_NONE /* the real world: no map area */
#define AREA_ASUKA 0x13

/* One MAIN row on one progress value, with no condition (WT_ROW), or with one
 * `done` flag / item (WT_ROW_DONE). id, chapter, area id, map area, progress,
 * order. */
#define NO_CONDS {{0, 0}, {0, 0}}
#define WT_ROW(id, ch, area, map, p, ord) \
    {id, ch, area, map, WT_MAIN, WT_CAT_NONE, WT_REGION_ALL, p, p, ord, 0, NO_CONDS, 0, NO_CONDS}
#define WT_ROW_DONE(id, ch, area, map, p, ord, code) \
    {id, ch, area, map, WT_MAIN, WT_CAT_NONE, WT_REGION_ALL, p, p, ord, 0, NO_CONDS, 1, {{code, 1}, {0, 0}}}
/* The same on the EU disc only (the extra chapter): a `done` condition, and an `unlock` and a
 * `done` condition. */
#define WT_ROW_EU_DONE(id, ch, area, map, p, ord, code) \
    {id, ch, area, map, WT_MAIN, WT_CAT_NONE, WT_REGION_EU, p, p, ord, 0, NO_CONDS, 1, {{code, 1}, {0, 0}}}
#define WT_ROW_EU_UNLOCK_DONE(id, ch, area, map, p, ord, ucode, dcode) \
    {id, ch, area, map, WT_MAIN, WT_CAT_NONE, WT_REGION_EU, p, p, ord, 1, {{ucode, 1}, {0, 0}}, 1, \
     {{dcode, 1}, {0, 0}}}

/* One SIDE (optional) row: no upper bound, shown once the chapter is reached (`from` is the first
 * progress value of the chapter, and the one the decomp's own gate opens at). id, chapter, area id,
 * map area, category, region, from, order. WT_SIDE_ROW_DONE adds a `done` code. */
#define WT_SIDE_ROW(id, ch, area, map, cat, reg, from, ord) \
    {id, ch, area, map, WT_SIDE, cat, reg, from, -1, ord, 0, NO_CONDS, 0, NO_CONDS}
#define WT_SIDE_ROW_DONE(id, ch, area, map, cat, reg, from, ord, code) \
    {id, ch, area, map, WT_SIDE, cat, reg, from, -1, ord, 0, NO_CONDS, 1, {{code, 1}, {0, 0}}}

static const DW3WtStep k_steps[] = {
    /* id, chapter, area, map_area, kind, category, region, from, to, order,
       nunlock, unlock[2], ndone, done[2] */
    {"ch01.s01", "ch01", "kusanagi", AREA_KUSANAGI, WT_MAIN, WT_CAT_NONE, WT_REGION_ALL, 0, 0, 1,
     0, {{0, 0}, {0, 0}}, 0, {{0, 0}, {0, 0}}},
    {"ch01.s02", "ch01", "asuka", AREA_ASUKA, WT_MAIN, WT_CAT_NONE, WT_REGION_ALL, 1, 1, 2,
     0, {{0, 0}, {0, 0}}, 0, {{0, 0}, {0, 0}}},
    {"ch01.s03", "ch01", "asuka", AREA_ASUKA, WT_MAIN, WT_CAT_NONE, WT_REGION_ALL, 2, 2, 3,
     0, {{0, 0}, {0, 0}}, 0, {{0, 0}, {0, 0}}},
    {"ch01.s04", "ch01", "asuka", AREA_ASUKA, WT_MAIN, WT_CAT_NONE, WT_REGION_ALL, 3, 3, 4,
     0, {{0, 0}, {0, 0}}, 1, {{WT_FLAG(0x40, 0x0D), 1}, {0, 0}}},
    {"ch01.s05", "ch01", "asuka", AREA_ASUKA, WT_MAIN, WT_CAT_NONE, WT_REGION_ALL, 3, 3, 5,
     1, {{WT_FLAG(0x40, 0x0D), 1}, {0, 0}}, 0, {{0, 0}, {0, 0}}},
    {"ch01.s06", "ch01", "asuka", AREA_ASUKA, WT_MAIN, WT_CAT_NONE, WT_REGION_ALL, 4, 4, 6,
     0, {{0, 0}, {0, 0}}, 0, {{0, 0}, {0, 0}}},
    /* optional: the town's buildings get new people at progress 4
       (actor conditions PROGRESS(4) in wstag220/235/240/250/255/280); no
       safe "done" condition, so no status */
    {"ch01.x01", "ch01", "asuka", AREA_ASUKA, WT_SIDE, WT_CAT_OTHER, WT_REGION_ALL, 4, -1, 7,
     0, {{0, 0}, {0, 0}}, 0, {{0, 0}, {0, 0}}},

    /* ---- ch02: progress 5 to 0xA, East Station to Suzaku Hall ---------------- */
    WT_ROW("ch02.s01", "ch02", "east", 0x1E, 5, 8),           /* 410 East Station: slot event, PROGRESS(5) */
    WT_ROW("ch02.s02", "ch02", "south", 0x1F, 6, 9),          /* 440 South Station: updateStage, progress 6 */
    WT_ROW("ch02.s03", "ch02", "south", 0x2C, 7, 10),         /* 480 Jungle Grave: 170 sets 0x4000, battle, 171 */
    /* progress 8: Shaman House event 180 needs flag 0x1C43 (Swamp Inn) and sets 0x1A17;
       0x1A18 (Protocol Ruins, 375), 0x1A19 (Asuka Bridge, 205) and 0x1A1A
       (Central Sector, 202) are set by talking in turn; the Lake (380) needs 0x1A1A */
    WT_ROW_DONE("ch02.s04", "ch02", "south", 0x24, 8, 11, WT_FLAG(0x1A, 0x17)),
    WT_ROW_DONE("ch02.s05", "ch02", "east", 0x0D, 8, 12, WT_FLAG(0x1A, 0x1A)),
    WT_ROW("ch02.s06", "ch02", "east", 0x16, 8, 13),          /* 380 Divermon's Lake: event 205 */
    /* progress 9: Central Sector (202, actor 43) sets 0x1A1B; the Asuka Sewers
       (320, actor 14) then gives item 0x15; the Shaman House (475) event 230 needs it */
    WT_ROW_DONE("ch02.s07", "ch02", "asuka", AREA_ASUKA, 9, 14, WT_FLAG(0x1A, 0x1B)),
    WT_ROW_DONE("ch02.s08", "ch02", "asuka", AREA_ASUKA, 9, 15, WT_ITEM(0, 0x15)),
    WT_ROW("ch02.s09", "ch02", "south", 0x24, 9, 16),         /* 475 Shaman House: event 230 */
    WT_ROW("ch02.s10", "ch02", "suzaku", 0x29, 0xA, 17),      /* 520 Suzaku Hall: 260 sets 0x4002, battle, 261 */

    /* ---- ch03: progress 0xB to 0xE, Main Lobby to Reliability Spot ----------- */
    WT_ROW("ch03.s01", "ch03", "asuka", AREA_ASUKA, 0xB, 18), /* 212 Main Lobby: 271 sets 0x4004, then 273 */
    WT_ROW("ch03.s02", "ch03", "asuka", AREA_ASUKA, 0xC, 19), /* 237 Underground Path: event 300 */
    WT_ROW_DONE("ch03.s03", "ch03", "asuka", AREA_ASUKA, 0xD, 20, WT_FLAG(0x1C, 0x0D)), /* 305 Master Room: 350 */
    WT_ROW("ch03.s04", "ch03", "asuka", AREA_ASUKA, 0xD, 21), /* 212 Main Lobby: event 352 needs 0x1C0D */
    WT_ROW("ch03.s05", "ch03", "south", 0x25, 0xE, 22),       /* 460 Reliability Spot: slot event, PROGRESS(0xE) */

    /* ---- ch04: progress 0xF to 0x11, Byakko City ---------------------------- */
    WT_ROW("ch04.s01", "ch04", "west", 0x21, 0xF, 23),        /* 685 Underground Cave: updateStage, progress 0xF */
    /* optional, slice 4: the UG Lakes (525, 526) get a warp down to the Seabed (825) behind
       SPECIAL(0x93) = PROGRESS_RANGES[0xC] = 0xF..0x63; 845 is the forge. No safe "done" */
    WT_SIDE_ROW("ch04.x01", "ch04", "south", WT_AREA_NONE, WT_CAT_OTHER, WT_REGION_ALL, 0xF, 24),
    WT_SIDE_ROW("ch04.x02", "ch04", "south", WT_AREA_NONE, WT_CAT_ITEM, WT_REGION_ALL, 0xF, 25),
    WT_ROW("ch04.s02", "ch04", "west", 0x26, 0x10, 26),       /* 655 Mirage Room: 430 sets 0x403E, battle, 431 */
    WT_ROW("ch04.s03", "ch04", "west", 0x07, 0x11, 27),       /* 605 Operation Room: 510 sets 0x40A6, battle, 511 */

    /* ---- ch05: progress 0x12 to 0x17, the West Sector rebels ---------------- */
    WT_ROW("ch05.s01", "ch05", "west", 0x07, 0x12, 28),       /* 615 Sewers: 530 sets 0x4039, battle, 531 */
    WT_ROW("ch05.s02", "ch05", "west", 0x07, 0x13, 29),       /* 620 Secret Room: 532 sets 0x403B, battle, 534 */
    WT_ROW_DONE("ch05.s03", "ch05", "west", 0x07, 0x14, 30, WT_FLAG(0x40, 0x03)), /* 620 event 540 then 525 */
    WT_ROW("ch05.s04", "ch05", "west", 0x07, 0x14, 31),       /* 620 Secret Room: event 551 needs 0x4003 */
    WT_ROW("ch05.s05", "ch05", "asuka", AREA_ASUKA, 0x15, 32), /* 325 Control Room: 375 battle, 376, 560 */
    WT_ROW("ch05.s06", "ch05", "asuka", AREA_ASUKA, 0x16, 33), /* 305 Master Room: 660, then 661 */
    WT_ROW_DONE("ch05.s07", "ch05", "asuka", AREA_ASUKA, 0x17, 34, WT_FLAG(0x40, 0x50)), /* 310: 680 to 682 */
    WT_ROW("ch05.s08", "ch05", "asuka", AREA_ASUKA, 0x17, 35), /* 210 Main Lobby: event 685 needs 0x4050 */

    /* ---- ch06: progress 0x18 to 0x1A ---------------------------------------- */
    WT_ROW("ch06.s01", "ch06", "west", 0x21, 0x18, 36),       /* 685 Underground Cave: event 690 */
    WT_ROW("ch06.s02", "ch06", "suzaku", 0x29, 0x19, 37),     /* 520 Suzaku Hall: event 695 */
    WT_ROW("ch06.s03", "ch06", "network", 0x23, 0x1A, 38),    /* 545 Bug Maze Pit: 710 battle, 711 */

    /* ---- ch07: progress 0x1B to 0x1D, the Amaterasu server ------------------- */
    WT_ROW("ch07.s01", "ch07", "east", 0x0E, 0x1B, 39),       /* 421 Qing Long City: 740 battle, 741, 743 */
    WT_ROW("ch07.s02", "ch07", "suzaku", 0x29, 0x1C, 40),     /* 521 Zhu Que Hall: 750 sets 0x4052, battle, 751 */
    WT_ROW("ch07.s03", "ch07", "suzaku", 0x29, 0x1D, 41),     /* 526 Zhu Que UG Lake: slot event, PROGRESS(0x1D) */

    /* ---- ch08: progress 0x1E to 0x21, the North Sector ---------------------- */
    WT_ROW("ch08.s01", "ch08", "north", 0x05, 0x1E, 42),      /* 755 Battle Gate: 780 sets 0x4001, battle, 781 */
    /* optional, slice 4: the sector maps get warps to the Circuit Board (865, 870) behind
       SPECIAL(0x94) = PROGRESS_RANGES[0xD] = 0x1E..0x63; 875 is the Black Kingz' den */
    WT_SIDE_ROW("ch08.x01", "ch08", "north", WT_AREA_NONE, WT_CAT_OTHER, WT_REGION_ALL, 0x1E, 43),
    WT_SIDE_ROW("ch08.x02", "ch08", "north", WT_AREA_NONE, WT_CAT_CARD, WT_REGION_ALL, 0x1E, 44),
    WT_ROW("ch08.s02", "ch08", "north", 0x08, 0x1F, 45),      /* 750 Chamber Room: event 820 */
    WT_ROW_DONE("ch08.s03", "ch08", "north", 0x08, 0x20, 46, WT_FLAG(0x40, 0x66)), /* 780 Kusanagi: 885 */
    WT_ROW("ch08.s04", "ch08", "north", 0x08, 0x20, 47),      /* 750 Chamber Room: event 886 needs 0x4066 */
    WT_ROW_DONE("ch08.s05", "ch08", "asuka", AREA_ASUKA, 0x21, 48, WT_FLAG(0x40, 0x57)), /* 305: 890 */
    WT_ROW_DONE("ch08.s06", "ch08", "asuka", AREA_ASUKA, 0x21, 49, WT_FLAG(0x40, 0x59)), /* 295 Prison Tower */
    WT_ROW("ch08.s07", "ch08", "asuka", AREA_ASUKA, 0x21, 50), /* 305 Master Room: 893 or 894, then 895 */

    /* ---- ch09: progress 0x22 to 0x24 ----------------------------------------- */
    WT_ROW("ch09.s01", "ch09", "amaterasu", 0x21, 0x22, 51),  /* 676 Bai Hu Dome: 900 sets 0x405A, battle, 901 */
    WT_ROW("ch09.s02", "ch09", "north", 0x05, 0x23, 52),      /* 756 Battle Gate: 910 sets 0x405B, battle, 911 */
    WT_ROW("ch09.s03", "ch09", "amaterasu", AREA_ASUKA, 0x24, 53), /* 206 Amaterasu Bridge: 921, then 922 */

    /* ---- ch10: progress 0x25 to 0x29, the Spy Satellite --------------------- */
    WT_ROW_DONE("ch10.s01", "ch10", "amaterasu", AREA_ASUKA, 0x25, 54, WT_FLAG(0x40, 0x60)), /* 311: 930 to 933 */
    WT_ROW("ch10.s02", "ch10", "amaterasu", AREA_ASUKA, 0x25, 55), /* 226 Login Room: event 935 needs 0x4060 */
    WT_ROW("ch10.s03", "ch10", "asuka", AREA_ASUKA, 0x26, 56), /* 310 A.o.A Headquarters: updateStage, 0x26 */
    WT_ROW("ch10.s04", "ch10", "amaterasu", AREA_ASUKA, 0x27, 57), /* 311: 981 sets 0x406C, then 982 */
    WT_ROW("ch10.s05", "ch10", "network", AREA_KUSANAGI, 0x28, 58), /* 810 Gunslinger 1F: slot event, PROGRESS(0x28) */
    WT_ROW_DONE("ch10.s06", "ch10", "network", AREA_KUSANAGI, 0x29, 59, WT_FLAG(0x40, 0x74)), /* 820: 1060 */
    WT_ROW("ch10.s07", "ch10", "network", AREA_KUSANAGI, 0x29, 60), /* 820 Control Room: 1070, then 1080 */

    /* ---- ch11: progress 0x2B, the last battle ------------------------------------
       No script writes 0x2A or 0x2C, so the table has no row for them (the resolver says "gap").
       endEvent1080 of 820 sets 0x2B and starts the last battle (battle 0x144); the ending
       leads back to Kusanagi City (MODE_NEW_GAME), where event 1500 plays at 0x2B */
    WT_ROW("ch11.s01", "ch11", "network", AREA_KUSANAGI, 0x2B, 61),

    /* ---- ch12: progress 0x2D, the EU extra chapter (region EU) --------------------
       endEvent1500 of 780 (EU only) sets 0x2D; the Chamber Room's movie then lands in the
       extra chapter's Login Room (WSTAG925, mode 0x276). Nothing writes progress inside the
       chapter, so the steps go by flags */
    WT_ROW_EU_DONE("ch12.s01", "ch12", "amaterasu", 0x13, 0x2D, 62, WT_FLAG(0x40, 0xCD)), /* 925: event 1600 */
    /* 956 (0x100D), 949 (0x100E, 0x100F), 954 (0x1010); SPECIAL(0x54) is "those four are set" */
    WT_ROW_EU_DONE("ch12.s02", "ch12", "east", 0x0E, 0x2D, 63, WT_SPECIAL(0x54)),
    WT_ROW_EU_UNLOCK_DONE("ch12.s03", "ch12", "amaterasu", 0x13, 0x2D, 64, WT_SPECIAL(0x54),
                          WT_FLAG(0x10, 0x0C)),                     /* 926 desk, 927: Kurt, event 1608 */
    /* optional, slice 4 (EU): 731 Kulon Weapons makes the five weapons SPECIAL(0x96) asks for (the
       row's done code is that set); 970 and 971 (extra Circuit Board) win back the partners */
    WT_SIDE_ROW_DONE("ch12.x01", "ch12", "north", 0x08, WT_CAT_ITEM, WT_REGION_EU, 0x2D, 65,
                     WT_SPECIAL(0x96)),
    WT_SIDE_ROW("ch12.x02", "ch12", "north", WT_AREA_NONE, WT_CAT_RECRUIT, WT_REGION_EU, 0x2D, 66),
};

const DW3WtStep *dw3_wt_steps(int *count) {
    if (count) *count = (int)(sizeof(k_steps) / sizeof(k_steps[0]));
    return k_steps;
}

void dw3_wt_resolve(const DW3WtState *st, DW3WtResult *out) {
    dw3_wt_resolve_table(k_steps, (int)(sizeof(k_steps) / sizeof(k_steps[0])), st, out);
}
