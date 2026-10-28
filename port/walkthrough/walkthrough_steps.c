/* P6.35 walkthrough: the step table, phase 1 (docs/P6.35_WALKTHROUGH.md,
 * section 2.2). Logic only; the strings are in port/walkthrough/text/<lang>.txt
 * keyed by `id`. DRAFT, owner review pending.
 *
 * Evidence (decomp, vendor/dw3_decomp; progress values confirmed by PC replay
 * at each row, tmp/evidence/p635/, see docs/P6.35_WALKTHROUGH.md "As built"):
 *   p0  kusanagi_city/wstag780.c endEvent0   (street corner, Junior/Ivy/Teddy)
 *   p1  kusanagi_city/wstag785.c endEvent1   (Online Center, Account Clerk)
 *   p2  asuka_city/wstag225.c    endEvent5   (Login Room, Account Clerk)
 *   p3  asuka_city/wstag212.c    endEvents10To12 (Main Lobby, starter pack)
 *   p4  central_sector/wstag200.c endEvent52 (Tamer Genji; flag 0x400D is set
 *       by endEvent50, and wstag200's updateStage plays event 50 while it is
 *       clear and event 52 once it is set)
 *   p5  east_sector/wstag420.c   endEvent71 (not in phase 1)
 * The map areas are STSTATUS_getArea() of the step's stage (replay-logged).
 *
 * Rows are ordered by from_progress, then order. Condition codes are the
 * game's own (WT_FLAG = FLAG(group, id)). Region: ALL in phase 1; the EU only
 * chapter (progress 0x2D) arrives in phase 3. */
#include "port_walkthrough.h"

#define AREA_KUSANAGI WT_AREA_NONE /* the real world: no map area */
#define AREA_ASUKA 0x13

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
};

const DW3WtStep *dw3_wt_steps(int *count) {
    if (count) *count = (int)(sizeof(k_steps) / sizeof(k_steps[0]));
    return k_steps;
}

void dw3_wt_resolve(const DW3WtState *st, DW3WtResult *out) {
    dw3_wt_resolve_table(k_steps, (int)(sizeof(k_steps) / sizeof(k_steps[0])), st, out);
}
