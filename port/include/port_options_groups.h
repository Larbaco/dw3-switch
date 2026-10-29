#ifndef DW3_PORT_OPTIONS_GROUPS_H
#define DW3_PORT_OPTIONS_GROUPS_H
/* P9.9: the Options screen is a list of groups; a group opens a sub-screen
 * with its rows (docs/PORT_OPTIONS.md "Groups and sub-screens"). This is the
 * one table of it, read by the logic (port/options/port_options.c) and by the
 * native overlay art (platforms/pc/src/title_overlay_art.c), so they cannot
 * disagree. Pure data and inline helpers: no registry state here.
 *
 * The rows are registry indexes (DW3_PORT_OPT_*, which do not change; the
 * port_options.cfg keys are keyed by them). Their numbers are repeated here
 * because the enum lives in port_options.c; a _Static_assert there proves it. */

enum {
    DW3_OPTGRP_DISPLAY = 0,
    DW3_OPTGRP_SOUND = 1,
    DW3_OPTGRP_GAMEPLAY = 2,
    DW3_OPTGRP_SAVES = 3,
    DW3_OPTGRP_CHEATS = 4,      /* P6.20 Cheats: a sub-screen of 10 rows (scrolls) */
    DW3_OPTGRP_CONTROLS = 5,    /* P9.8 Button layout, P6.12 Customize buttons and Reset */
    /* Not built yet: hidden until the feature lands, then flip `built`. */
    DW3_OPTGRP_HELP = 6,        /* P6.30 technical help (built: a screen of its own) */
    DW3_OPTGRP_WALKTHROUGH = 7, /* P6.35 walkthrough help */
    /* P6.12: the Customize buttons sub-screen of Controls. Never listed (built
     * = 0): Controls opens it. Its ten rows are the PS buttons in map order
     * (port_button_map.h), not registry rows; a row's value is its source. */
    DW3_OPTGRP_CUSTOM = 8,
    DW3_OPTGRP_COUNT = 9,
    DW3_OPTGRP_MAX_ROWS = 10
};

/* P51 item 4: the Region at start row (registry row 10) exists in the combined
 * image only: the host (DW3_BOTH) and each game copy (DW3_REGION_IMAGE) see it. A
 * plain US or EU image has no other region to start. */
#if (defined(DW3_BOTH) && DW3_BOTH) || (defined(DW3_REGION_IMAGE) && DW3_REGION_IMAGE)
#define DW3_OPT_REGION_ROW 1
#else
#define DW3_OPT_REGION_ROW 0
#endif

typedef struct Dw3OptGroup {
    int built;                       /* 0 = a placeholder, never listed */
    int nrows;                       /* Cheats: 10 cheat rows (port_cheats.h), not registry rows */
    int rows[DW3_OPTGRP_MAX_ROWS];   /* registry indexes, in screen order */
} Dw3OptGroup;

/* Registry indexes: 0 Widescreen, 1 Fast-forward, 2 Restart hotkey, 3 Music
 * volume, 4 SFX volume, 5 Encounter rate, 6 Text speed, 7 Battle cursor,
 * 8 Auto-save, 9 Button layout, 10 Region at start (combined image only),
 * 11 Customize buttons, 12 Reset to default (P6.12), 13 Turbo music (P6.36,
 * Sound group). Restart hotkey stays in
 * Gameplay (moving it would change every Gameplay reference for no gain). */
static const Dw3OptGroup dw3_optgroups[DW3_OPTGRP_COUNT] = {
    /* Display  */ {1, 1, {0}},
    /* Sound    */ {1, 3, {3, 4, 13}},
    /* Gameplay */ {1, 5 + DW3_OPT_REGION_ROW, {1, 2, 5, 6, 7, 10}},
    /* Saves    */ {1, 1, {8}},
    /* Cheats   */ {1, 10, {0, 1, 2, 3, 4, 5, 6, 7, 8, 9}},
    /* Controls */ {1, 3, {9, 11, 12}},
    /* Help     */ {1, 0, {0}}, /* P6.30: no rows, a screen (port/help/help.c) */
    /* Walk     */ {0, 0, {0}},
    /* Customize*/ {0, 10, {0, 1, 2, 3, 4, 5, 6, 7, 8, 9}},
};

/* Visible rows of a list screen; longer lists scroll. */
enum { DW3_OPTLIST_VISIBLE = 6 };

/* Whether the Cheats group is on the list: whenever the feature is built in
 * (DW3_CHEATS), whatever the master switch says (P51 item 5: with the group
 * hidden until the switch was on, the only way in was the Square / Y shortcut,
 * which the list did not show). At the title it opens the Cheats screen where
 * the master switch is; in game it is greyed (DW3_PORT_OPTIONS_LOCK_CHEATS). */
#ifdef DW3_CHEATS
#define DW3_OPTGRP_CHEATS_LISTED 1
#else
#define DW3_OPTGRP_CHEATS_LISTED 0
#endif

/* The groups listed now, in order (placeholders are skipped; Cheats is listed
 * when `cheats_listed`: pass DW3_OPTGRP_CHEATS_LISTED, not the master switch).
 * Returns the count, `out` holds DW3_OPTGRP_COUNT entries at most. */
static inline int dw3_optgroups_visible(int cheats_listed, int *out) {
    int n = 0;
    int g;

    for (g = 0; g < DW3_OPTGRP_COUNT; g++) {
        if (!dw3_optgroups[g].built) {
            continue;
        }
        if (g == DW3_OPTGRP_CHEATS && !cheats_listed) {
            continue;
        }
        out[n++] = g;
    }
    return n;
}

/* Items of a list screen: the visible groups (group < 0) or the rows of one. */
static inline int dw3_optlist_count(int group, int cheats_listed) {
    int tmp[DW3_OPTGRP_COUNT];

    return group < 0 ? dw3_optgroups_visible(cheats_listed, tmp)
                     : dw3_optgroups[group].nrows;
}

/* First visible item so that `cursor` is on screen, from the old `scroll`. */
static inline int dw3_optlist_scroll_for(int scroll, int cursor, int count) {
    if (cursor < scroll) {
        scroll = cursor;
    }
    if (cursor >= scroll + DW3_OPTLIST_VISIBLE) {
        scroll = cursor - DW3_OPTLIST_VISIBLE + 1;
    }
    if (scroll > count - DW3_OPTLIST_VISIBLE) {
        scroll = count - DW3_OPTLIST_VISIBLE;
    }
    return scroll < 0 ? 0 : scroll;
}

/* The overlay's page code of a list screen: group < 0 is the group list. The
 * code also carries the scroll, so a scrolled list is its own cached layer.
 * The group list at scroll 0 is page 0 (the Options page since P9.2). */
enum { DW3_OPTPAGE_LIST_BASE = 16 };
static inline int dw3_optlist_page(int group, int scroll) {
    return group < 0 && scroll == 0
               ? 0 : DW3_OPTPAGE_LIST_BASE + (group + 1) * 16 + scroll;
}
/* Inverse: 1 and *group / *scroll set when `page` is a list page. */
static inline int dw3_optlist_decode(int page, int *group, int *scroll) {
    int v;

    if (page == 0) {
        *group = -1;
        *scroll = 0;
        return 1;
    }
    v = page - DW3_OPTPAGE_LIST_BASE;
    if (v < 0 || v / 16 > DW3_OPTGRP_COUNT) {
        return 0;
    }
    *group = v / 16 - 1;
    *scroll = v % 16;
    return 1;
}

#endif /* DW3_PORT_OPTIONS_GROUPS_H */
