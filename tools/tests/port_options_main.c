/* Headless P9.2 port-options state machine (no disc / GPU). */
#include "port_audio.h"
#include "port_options.h"
#include "port_options_groups.h"

#include <stdio.h>
#include <string.h>

void dw3_port_options_test_set_pressed(unsigned mask);
void dw3_port_options_test_goto_row(int reg);
int dw3_port_options_test_sound_count(int kind);

void dw3_port_audio_apply(int music_step, int sfx_step) {
    (void)music_step;
    (void)sfx_step;
}

enum {
    BIT_SELECT = 0,
    BIT_UP = 4,
    BIT_DOWN = 6,
    BIT_CROSS = 13,
    BIT_TRIANGLE = 14
};

static int fail(const char *msg) {
    fprintf(stderr, "FAIL: %s\n", msg);
    return 1;
}

int main(void) {
    if (dw3_port_options_is_open()) {
        return fail("starts closed");
    }
    dw3_port_options_open();
    if (!dw3_port_options_is_open()) {
        return fail("open");
    }
    dw3_port_options_open(); /* idempotent */
    if (!dw3_port_options_is_open()) {
        return fail("re-open");
    }

    if (dw3_port_options_count() != 16) {
        return fail("option count");
    }
    if (dw3_port_options_level(DW3_PORT_OPT_BATTLE_MENU) != 0 ||
        dw3_port_options_label(DW3_PORT_OPT_BATTLE_MENU) == NULL ||
        strstr(dw3_port_options_label(DW3_PORT_OPT_BATTLE_MENU),
               "Battle cursor") == NULL) {
        return fail("Battle cursor row, default Off");
    }
    if (dw3_port_options_level(DW3_PORT_OPT_ENCOUNTER_RATE) != 0 ||
        dw3_port_options_level(DW3_PORT_OPT_TEXT_SPEED) != 0) {
        return fail("pacing rows default to 1x / Normal");
    }
    if (dw3_port_options_label(5) == NULL ||
        strstr(dw3_port_options_label(5), "Encounter") == NULL ||
        dw3_port_options_label(6) == NULL ||
        strstr(dw3_port_options_label(6), "Text speed") == NULL) {
        return fail("pacing labels");
    }
    if (dw3_port_options_label(0) == NULL ||
        strstr(dw3_port_options_label(0), "Widescreen") == NULL) {
        return fail("widescreen stub label");
    }
    if (dw3_port_options_value(0) != 1) {
        return fail("widescreen default on");
    }
    if (dw3_port_options_restart_enabled() != 0 ||
        dw3_port_options_value(2) != 0) {
        return fail("restart hotkey default off");
    }

    if (dw3_port_options_label(1) == NULL ||
        strstr(dw3_port_options_label(1), "Fast-forward") == NULL) {
        return fail("fast-forward label");
    }
    if (dw3_port_options_level(1) != 2) {
        return fail("fast-forward default 2x");
    }

    /* P9.9: the screen opens on the group list. Down, Confirm opens Sound;
     * Back returns to the list with the cursor kept; Back again closes. */
    if (dw3_port_options_group() != -1 || dw3_port_options_cursor() != 0) {
        return fail("opens on the group list");
    }
    dw3_port_options_test_set_pressed(1u << BIT_DOWN);
    dw3_port_options_tick();
    dw3_port_options_test_set_pressed(1u << BIT_CROSS);
    dw3_port_options_tick();
    if (dw3_port_options_group() != DW3_OPTGRP_SOUND ||
        dw3_port_options_cursor() != 0) {
        return fail("Confirm opens the Sound group");
    }
    dw3_port_options_test_set_pressed(1u << BIT_DOWN);
    dw3_port_options_tick();
    if (dw3_port_options_cursor() != 1) {
        return fail("Down moves to SFX volume");
    }
    dw3_port_options_test_set_pressed(1u << BIT_DOWN);
    dw3_port_options_tick();
    if (dw3_port_options_cursor() != 2) {
        return fail("Down moves to Turbo music");
    }
    /* P6.36: the row holds 0 = 1x (default) and 1 = Fast; Confirm wraps. */
    if (dw3_port_options_level(13) != 0) {
        return fail("Turbo music defaults to 1x");
    }
    dw3_port_options_test_set_pressed(1u << BIT_CROSS);
    dw3_port_options_tick();
    if (dw3_port_options_level(13) != 1) {
        return fail("Confirm on Turbo music steps to Fast");
    }
    dw3_port_options_tick();
    if (dw3_port_options_level(13) != 0) {
        return fail("Confirm on Turbo music wraps to 1x");
    }
    dw3_port_options_test_set_pressed(1u << BIT_DOWN);
    dw3_port_options_tick();
    if (dw3_port_options_cursor() != 2) {
        return fail("Down stops on the last row");
    }
    dw3_port_options_test_set_pressed(1u << BIT_TRIANGLE);
    dw3_port_options_tick();
    if (!dw3_port_options_is_open() || dw3_port_options_group() != -1 ||
        dw3_port_options_cursor() != DW3_OPTGRP_SOUND) {
        return fail("Back returns to the list, cursor on Sound");
    }

    /* Fast-forward cycles 2x -> 3x -> 4x -> OFF -> 2x. */
    dw3_port_options_test_goto_row(1);
    dw3_port_options_test_set_pressed(1u << BIT_CROSS);
    dw3_port_options_tick();
    if (dw3_port_options_level(1) != 3) {
        return fail("fast-forward 3x");
    }
    dw3_port_options_tick();
    if (dw3_port_options_level(1) != 4) {
        return fail("fast-forward 4x");
    }
    dw3_port_options_tick();
    if (dw3_port_options_level(1) != 0 || dw3_port_options_value(1) != 0) {
        return fail("fast-forward off");
    }
    dw3_port_options_tick();
    if (dw3_port_options_level(1) != 2) {
        return fail("fast-forward wraps to 2x");
    }

    dw3_port_options_test_goto_row(2);
    dw3_port_options_test_set_pressed(1u << BIT_CROSS);
    dw3_port_options_tick();
    if (dw3_port_options_restart_enabled() != 1) {
        return fail("toggle restart hotkey");
    }

    dw3_port_options_test_set_pressed(1u << BIT_TRIANGLE);
    dw3_port_options_tick();
    if (!dw3_port_options_is_open() || dw3_port_options_group() != -1) {
        return fail("triangle in a group goes back to the list");
    }
    dw3_port_options_test_set_pressed(1u << BIT_TRIANGLE);
    dw3_port_options_tick();
    if (dw3_port_options_is_open()) {
        return fail("triangle on the list closes");
    }

    /* The group table: placeholders are hidden, Cheats follows its switch,
     * every registry row is in exactly one group, a long list scrolls. */
    {
        int ids[DW3_OPTGRP_COUNT];
        int seen[16] = {0};
        int g, k;

        if (dw3_optgroups_visible(0, ids) != 7 || ids[0] != DW3_OPTGRP_DISPLAY ||
            ids[3] != DW3_OPTGRP_SAVES || ids[4] != DW3_OPTGRP_CONTROLS ||
            ids[5] != DW3_OPTGRP_HELP || ids[6] != DW3_OPTGRP_WALKTHROUGH) {
            return fail("visible groups without cheats");
        }
        if (dw3_optgroups_visible(1, ids) != 8 || ids[4] != DW3_OPTGRP_CHEATS ||
            ids[5] != DW3_OPTGRP_CONTROLS || ids[6] != DW3_OPTGRP_HELP ||
            ids[7] != DW3_OPTGRP_WALKTHROUGH) {
            return fail("Cheats group follows the master switch");
        }
        for (g = 0; g < DW3_OPTGRP_COUNT; g++) {
            if (g == DW3_OPTGRP_CHEATS || g == DW3_OPTGRP_CUSTOM) continue; /* not registry rows */
            for (k = 0; k < dw3_optgroups[g].nrows; k++) {
                seen[dw3_optgroups[g].rows[k]]++;
            }
        }
        for (k = 0; k < 16; k++) {
            if (k == DW3_PORT_OPT_REGION) continue; /* combined image only */
            if (k == DW3_PORT_OPT_SWITCH_GLYPHS && !DW3_OPT_GLYPHS_ROW) continue; /* Switch only */
            if (seen[k] != 1) {
                return fail("every registry row is in exactly one group");
            }
        }
        if (dw3_optlist_scroll_for(0, 7, 8) != 2 ||
            dw3_optlist_scroll_for(2, 1, 8) != 1 ||
            dw3_optlist_scroll_for(5, 3, 5) != 0 ||
            dw3_optlist_scroll_for(0, 2, 5) != 0) {
            return fail("scroll keeps the cursor on screen");
        }
        if (dw3_optlist_page(-1, 0) != 0 ||
            dw3_optlist_page(2, 3) == dw3_optlist_page(2, 0)) {
            return fail("page codes");
        }
        {
            int grp, sc;
            if (!dw3_optlist_decode(dw3_optlist_page(2, 3), &grp, &sc) ||
                grp != 2 || sc != 3 || dw3_optlist_decode(1, &grp, &sc)) {
                return fail("page decode");
            }
        }
    }

    /* P6.32 phase 4: the Full-screen map row is the second row of the Display
     * group, off by default, a plain switch (Confirm toggles), not locked in game. */
    if (dw3_optgroups[DW3_OPTGRP_DISPLAY].nrows != 2 ||
        dw3_optgroups[DW3_OPTGRP_DISPLAY].rows[0] != 0 /* Widescreen */ ||
        dw3_optgroups[DW3_OPTGRP_DISPLAY].rows[1] != DW3_PORT_OPT_MAP_SCREEN) {
        return fail("Display group is Widescreen, then Map screen");
    }
    if (dw3_port_options_level(DW3_PORT_OPT_MAP_SCREEN) != 0 ||
        dw3_port_options_label(DW3_PORT_OPT_MAP_SCREEN) == NULL ||
        strstr(dw3_port_options_label(DW3_PORT_OPT_MAP_SCREEN), "Map screen") == NULL) {
        return fail("Map screen row defaults to off");
    }
    dw3_port_options_open();
    {
        dw3_port_options_test_set_pressed(1u << BIT_CROSS);
        dw3_port_options_tick();         /* the Display group */
        if (dw3_port_options_group() != DW3_OPTGRP_DISPLAY) {
            return fail("Confirm opens the Display group");
        }
        dw3_port_options_test_set_pressed(1u << BIT_DOWN);
        dw3_port_options_tick();
        if (dw3_port_options_cursor() != 1) {
            return fail("Display ends on the Map screen row");
        }
        dw3_port_options_test_set_pressed(1u << BIT_CROSS);
        dw3_port_options_tick();
        if (dw3_port_options_level(DW3_PORT_OPT_MAP_SCREEN) != 1) {
            return fail("Confirm turns Map screen on");
        }
        dw3_port_options_tick();
        if (dw3_port_options_level(DW3_PORT_OPT_MAP_SCREEN) != 0) {
            return fail("Confirm turns Map screen off");
        }
        dw3_port_options_test_set_pressed(1u << BIT_TRIANGLE);
        dw3_port_options_tick();
        dw3_port_options_test_set_pressed(1u << BIT_TRIANGLE);
        dw3_port_options_tick();
        if (dw3_port_options_is_open()) {
            return fail("Back twice closes");
        }
    }

    /* P10.6: the Switch glyphs row is the last row of the Controls group on the
     * Switch only (here: the build that forces it); the PC keeps three rows. */
    if (dw3_optgroups[DW3_OPTGRP_CONTROLS].nrows != 3 + DW3_OPT_GLYPHS_ROW) {
        return fail("Controls row count follows DW3_OPT_GLYPHS_ROW");
    }
    if (dw3_port_options_level(DW3_PORT_OPT_SWITCH_GLYPHS) != 0 ||
        dw3_port_options_label(DW3_PORT_OPT_SWITCH_GLYPHS) == NULL ||
        strstr(dw3_port_options_label(DW3_PORT_OPT_SWITCH_GLYPHS),
               "Switch glyphs") == NULL) {
        return fail("Switch glyphs row defaults to off");
    }
#if DW3_OPT_GLYPHS_ROW
    dw3_port_options_open();
    {
        int i;

        for (i = 0; i < 4; i++) { /* the list: Display, Sound, Gameplay, Saves, Controls */
            dw3_port_options_test_set_pressed(1u << BIT_DOWN);
            dw3_port_options_tick();
        }
        dw3_port_options_test_set_pressed(1u << BIT_CROSS);
        dw3_port_options_tick();
        if (dw3_port_options_group() != DW3_OPTGRP_CONTROLS) {
            return fail("Confirm opens the Controls group");
        }
        for (i = 0; i < 5; i++) {
            dw3_port_options_test_set_pressed(1u << BIT_DOWN);
            dw3_port_options_tick();
        }
        if (dw3_port_options_cursor() != 3) {
            return fail("Controls ends on the Switch glyphs row");
        }
        dw3_port_options_test_set_pressed(1u << BIT_CROSS);
        dw3_port_options_tick();
        if (dw3_port_options_level(DW3_PORT_OPT_SWITCH_GLYPHS) != 1) {
            return fail("Confirm turns Switch glyphs on");
        }
        dw3_port_options_tick();
        if (dw3_port_options_level(DW3_PORT_OPT_SWITCH_GLYPHS) != 0) {
            return fail("Confirm turns Switch glyphs off");
        }
        dw3_port_options_test_set_pressed(1u << BIT_TRIANGLE);
        dw3_port_options_tick();
        dw3_port_options_test_set_pressed(1u << BIT_TRIANGLE);
        dw3_port_options_tick();
        if (dw3_port_options_is_open()) {
            return fail("Back twice closes");
        }
    }
#endif

    /* P6.30: the Help group is a screen of its own (a host test only tracks that
     * it is open): Cross on it keeps the Options screen open on the list, and
     * Triangle leaves the help, not the Options screen. */
    dw3_port_options_open();
    {
        int i;

        for (i = 0; i < 5; i++) {
            dw3_port_options_test_set_pressed(1u << BIT_DOWN);
            dw3_port_options_tick();
        }
        dw3_port_options_test_set_pressed(1u << BIT_CROSS);
        dw3_port_options_tick();
        if (!dw3_port_options_is_open() || dw3_port_options_group() != -1 ||
            dw3_port_options_test_sound_count(1) < 1) {
            return fail("Help opens as a screen, the group list stays");
        }
        dw3_port_options_test_set_pressed(1u << BIT_TRIANGLE);
        dw3_port_options_tick();
        if (!dw3_port_options_is_open()) {
            return fail("Triangle in the help leaves the help only");
        }
        dw3_port_options_test_set_pressed(1u << BIT_TRIANGLE);
        dw3_port_options_tick();
        if (dw3_port_options_is_open()) {
            return fail("Triangle on the list closes");
        }
    }

    dw3_port_options_open();
    /* Select must not close — it is the title open chord. */
    dw3_port_options_test_set_pressed(1u << BIT_SELECT);
    dw3_port_options_tick();
    if (!dw3_port_options_is_open()) {
        return fail("select must not close");
    }

    dw3_port_options_close(); /* idempotent */
    printf("open=0 options=%d widescreen_default=%d restart=%d\n",
           dw3_port_options_count(), dw3_port_options_value(0),
           dw3_port_options_restart_enabled());
    printf("OK: port_options state machine\n");
    return 0;
}
