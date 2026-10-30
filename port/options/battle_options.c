/* P9.7 (slice 2): the port Options screen in battle. The screen itself is
 * port_options.c (its in-game mode, slice 1) and field_options.c (open / tick /
 * hide on the native overlay); this file is what a battle needs on top: the
 * entry, the freeze and the clocks. docs/PORT_OPTIONS.md "Options in battle".
 *
 * Entry. Select (Minus on the Switch), with no shoulder button held, while the
 * battle's command menu is waiting for the player (patch 0165 polls from
 * FIGHTSTG_updateCommandMenu's TASK_RUN). Select does nothing else in a battle
 * and the P6.4 / P6.9 / debug chords all hold a shoulder button.
 *
 * Freeze. While the screen is open the main loop does not run the task list at
 * all (dw3_port_battle_options_hold_frame), so no task, timer, event, gauge,
 * random countdown or animation moves and no task reads the pad. It also skips
 * the draw: nothing is submitted, so the display keeps the last picture. The
 * music (the vsync callback) and the loaders keep running. The game's vsync
 * callback still adds to its clocks during the hold, so they are put back on
 * the way out: the battle's messages and arrows compare absolute GFX times, and
 * the next frame's length would otherwise be the whole pause. */
#include "port_battle_options.h"

#include "port_field_options.h"
#ifndef DW3_BATTLE_OPTIONS_TEST
#include "port_map_view.h"
#endif
#include "port_options.h"

#include <stdio.h>

#ifdef DW3_BATTLE_OPTIONS_TEST
/* Host test: the pad and the game's clocks are stubs. */
extern int dw3_test_select_pressed(void);
extern int dw3_test_shoulder_held(void);
typedef struct Dw3TestClocks {
    int time_counter;
    int frame_time_counter;
    int play_frames;
    int frame_count;
} Dw3TestClocks;
extern Dw3TestClocks g_dw3_test_clocks;
#define BO_TIME_COUNTER g_dw3_test_clocks.time_counter
#define BO_FRAME_TIME_COUNTER g_dw3_test_clocks.frame_time_counter
#define BO_PLAY_FRAMES g_dw3_test_clocks.play_frames
#define BO_FRAME_COUNT g_dw3_test_clocks.frame_count
static int bo_select_pressed(void) { return dw3_test_select_pressed(); }
static int bo_shoulder_held(void) { return dw3_test_shoulder_held(); }
#else
#include "engine/game.h"
#include "engine/pad.h"
#define BO_TIME_COUNTER GFX.timeCounter
#define BO_FRAME_TIME_COUNTER GFX.frameTimeCounter
#define BO_PLAY_FRAMES GAME.playTime.frames
#define BO_FRAME_COUNT GFX.frameCount
static int bo_select_pressed(void) { return PAD_PRESSED(PAD_SELECT); }
static int bo_shoulder_held(void) {
    return PAD_HELD(PAD_L1) || PAD_HELD(PAD_R1) || PAD_HELD(PAD_L2) ||
           PAD_HELD(PAD_R2);
}
#endif

static int g_holding;        /* the screen is open over a frozen battle */
static int g_saved_time;     /* the clocks when the screen opened */
static int g_saved_frame_time;
static int g_saved_play;
static int g_hold_frames;    /* iterations held, for the log */
static Dw3BattleProbe g_probe;

int dw3_port_battle_options_enabled(void) {
    return dw3_port_field_options_enabled();
}

/* From the first iteration that skips the draw: the frame that opened the
 * screen is still drawn and presented as any other. */
int dw3_port_battle_options_holding(void) {
    return g_holding && g_hold_frames > 0;
}

static void log_state(const char *what) {
    int events = -1;
    int event_time = -1;
    int hp = -1;

    if (g_probe != NULL) {
        g_probe(&events, &event_time, &hp);
    }
    fprintf(stderr,
            "dw3-battle-options: %s gfx_frame=%d time_counter=%d events=%d "
            "event_time=%d hp=%d\n",
            what, BO_FRAME_COUNT, BO_TIME_COUNTER, events, event_time, hp);
    fflush(stderr);
}

int dw3_port_battle_options_poll(int language, Dw3BattleProbe probe) {
    if (g_holding || !dw3_port_battle_options_enabled()) {
        return 0;
    }
    if (!bo_select_pressed() || bo_shoulder_held()) {
        return 0;
    }
    g_probe = probe;
    g_hold_frames = 0;
    g_holding = 1;
    log_state("open at the command menu, battle frozen:");
    dw3_port_options_set_origin("battle command menu", "the battle");
    dw3_port_field_options_open(language);
    return 1;
}

int dw3_port_battle_options_hold_frame(void) {
    if (!g_holding) {
        return 0;
    }
    if (!dw3_port_field_options_is_open()) {
        /* Closed on the previous held frame: the pad edge that closed it has
         * gone by, so the battle can read the pad again. Give the clocks back. */
        int ran_on = BO_TIME_COUNTER - g_saved_time;

        BO_TIME_COUNTER = g_saved_time;
        BO_FRAME_TIME_COUNTER = g_saved_frame_time;
        BO_PLAY_FRAMES = g_saved_play;
        g_holding = 0;
        dw3_port_options_set_origin(NULL, NULL);
        fprintf(stderr,
                "dw3-battle-options: resume after %d held frame(s); the vsync "
                "clock ran on %d during the hold and is put back\n",
                g_hold_frames, ran_on);
        log_state("resume, battle state:");
        return 0;
    }
    g_hold_frames++;
    if (g_hold_frames == 1) {
        /* After the draw of the frame that opened it: what the battle holds,
         * and the clocks to give back. */
        g_saved_time = BO_TIME_COUNTER;
        g_saved_frame_time = BO_FRAME_TIME_COUNTER;
        g_saved_play = BO_PLAY_FRAMES;
        log_state("held, battle state:");
    }
    dw3_port_field_options_tick();
    return 1;
}

void dw3_port_battle_options_reset(void) {
#ifndef DW3_BATTLE_OPTIONS_TEST
    /* P6.32 phase 6: a restart also ends a field hotkey session (the view held the field) */
    dw3_port_map_view_field_reset();
#endif
    if (!g_holding) {
        return;
    }
    g_holding = 0;
    if (dw3_port_field_options_is_open()) {
        dw3_port_options_close();
    }
    dw3_port_options_set_origin(NULL, NULL);
    fprintf(stderr, "dw3-battle-options: reset while held\n");
    fflush(stderr);
}
