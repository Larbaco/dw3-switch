/* Host test of P9.7 slice 2 (port/options/battle_options.c): the entry (Select
 * at the command menu, never with a shoulder button held), the hold (what the
 * main loop sees frame by frame), the clocks given back on resume, a second
 * open, and a reset while held. The pad, the game's clocks, the sound, the mixer
 * and the overlay are stubs. Built with -DDW3_PORT_OPTIONS_TEST
 * -DDW3_FIELD_OPTIONS_TEST -DDW3_BATTLE_OPTIONS_TEST. */
#include "port_battle_options.h"
#include "port_field_options.h"
#include "port_options.h"

#include "title_overlay_art.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void dw3_port_options_test_set_pressed(unsigned mask);

enum { TRIANGLE = 14 };

typedef struct Dw3TestClocks {
    int time_counter;
    int frame_time_counter;
    int play_frames;
    int frame_count;
} Dw3TestClocks;
Dw3TestClocks g_dw3_test_clocks;

/* --- stubs --- */
static int g_confirm_available = 1;
static int g_select, g_shoulder;
static int g_state_active = -9, g_state_calls;

int dw3_port_confirm_available(void) { return g_confirm_available; }
void dw3_port_audio_apply(int music_step, int sfx_step) {
    (void)music_step;
    (void)sfx_step;
}
void dw3_pc_title_menu_set_state(int active, int selected) {
    (void)selected;
    g_state_active = active;
    g_state_calls++;
}
void dw3_pc_title_overlay_set_options_language(int lang) { (void)lang; }
void dw3_pc_title_overlay_set_options_locked(unsigned mask) { (void)mask; }
int dw3_test_select_pressed(void) { return g_select; }
int dw3_test_shoulder_held(void) { return g_shoulder; }

static int g_fail;
#define CHECK(cond)                                                        \
    do {                                                                   \
        if (!(cond)) {                                                     \
            fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #cond);        \
            g_fail = 1;                                                    \
        }                                                                  \
    } while (0)

static int g_probe_calls;
static void probe(int *events, int *event_time, int *hp) {
    g_probe_calls++;
    *events = 3;
    *event_time = 615;
    *hp = 540;
}

/* One main-loop iteration the way patch 0165 runs it: the held branch or the
 * task list and the draw (which advance the clocks as the vsync callback and
 * drawFrame do). Returns 1 when the iteration was held. */
static int iteration(int *tasks_ran) {
    int held = dw3_port_battle_options_hold_frame();

    g_dw3_test_clocks.time_counter += 256;       /* the vsync callback */
    g_dw3_test_clocks.frame_time_counter += 256;
    g_dw3_test_clocks.play_frames += 256;
    if (!held) {
        (*tasks_ran)++;
        g_dw3_test_clocks.frame_count++;         /* drawFrame */
    }
    return held;
}

static int g_tasks;

static void press_triangle(void) {
    dw3_port_options_test_set_pressed(1u << TRIANGLE);
}

int main(void) {
    int i;
    int held_count;
    int time_at_hold;
    int frame_at_hold;

    /* Not offered without the native overlay. */
    g_confirm_available = 0;
    g_select = 1;
    CHECK(!dw3_port_battle_options_enabled());
    CHECK(dw3_port_battle_options_poll(2, probe) == 0);
    CHECK(!dw3_port_battle_options_holding());
    CHECK(!dw3_port_options_is_open());
    g_confirm_available = 1;
    CHECK(dw3_port_battle_options_enabled());

    /* Nothing pressed, or a shoulder button held (the P6.4 / P6.9 / debug
     * chords): no entry. */
    g_select = 0;
    CHECK(dw3_port_battle_options_poll(2, probe) == 0);
    g_select = 1;
    g_shoulder = 1;
    CHECK(dw3_port_battle_options_poll(2, probe) == 0);
    CHECK(!dw3_port_options_is_open());
    CHECK(g_probe_calls == 0);
    g_shoulder = 0;

    /* The main loop is not held while nothing is open. */
    CHECK(dw3_port_battle_options_hold_frame() == 0);

    /* Select at the command menu: opens, in game, drawn by the overlay. The
     * frame that opened it is still a normal frame (drawn, presented). */
    g_dw3_test_clocks.frame_count = 330;
    g_dw3_test_clocks.time_counter = 86016;
    CHECK(dw3_port_battle_options_poll(2, probe) == 1);
    CHECK(dw3_port_options_is_open() && dw3_port_options_in_game());
    CHECK(!dw3_port_battle_options_holding());
    CHECK(g_state_active == 1);
    /* A second poll while open does nothing (the command menu is not reached
     * anyway, the tasks do not run). */
    CHECK(dw3_port_battle_options_poll(2, probe) == 0);
    g_select = 0;
    g_dw3_test_clocks.frame_count++;   /* the open frame's drawFrame */
    g_dw3_test_clocks.time_counter += 256;

    /* Held frames: no tasks, no draw; the vsync clocks run on. */
    g_tasks = 0;
    CHECK(iteration(&g_tasks) == 1);
    CHECK(dw3_port_battle_options_holding());
    time_at_hold = g_dw3_test_clocks.time_counter - 256;  /* saved at the top of the first held frame */
    frame_at_hold = g_dw3_test_clocks.frame_count;
    for (i = 0; i < 200; i++) {
        CHECK(iteration(&g_tasks) == 1);
    }
    CHECK(g_tasks == 0);
    CHECK(g_dw3_test_clocks.frame_count == frame_at_hold);
    CHECK(g_dw3_test_clocks.time_counter > time_at_hold + 200 * 256);
    CHECK(g_probe_calls == 2);          /* open, first held frame */

    /* Triangle closes the screen in its own tick; that iteration is still
     * held (the press must not reach the battle), the next one runs the tasks
     * with the clocks as they were. */
    press_triangle();
    CHECK(iteration(&g_tasks) == 1);
    dw3_port_options_test_set_pressed(0);
    CHECK(!dw3_port_options_is_open());
    CHECK(dw3_port_battle_options_holding());
    CHECK(g_tasks == 0);
    CHECK(iteration(&g_tasks) == 0);
    CHECK(g_tasks == 1);
    CHECK(!dw3_port_battle_options_holding());
    /* The clocks: put back to the first held frame's, then this frame's own
     * vsync (256) added by the iteration. */
    CHECK(g_dw3_test_clocks.time_counter == time_at_hold + 256);
    CHECK(g_dw3_test_clocks.frame_time_counter > 0);
    CHECK(g_dw3_test_clocks.frame_count == frame_at_hold + 1);
    CHECK(g_state_active == 0);          /* the overlay was hidden on close */
    CHECK(g_probe_calls == 3);          /* ... and resume */

    /* The same battle can open it again (the next turn's command menu). */
    g_select = 1;
    CHECK(dw3_port_battle_options_poll(2, probe) == 1);
    g_select = 0;
    held_count = 0;
    for (i = 0; i < 5; i++) {
        held_count += iteration(&g_tasks);
    }
    CHECK(held_count == 5);
    press_triangle();
    CHECK(iteration(&g_tasks) == 1);
    dw3_port_options_test_set_pressed(0);
    CHECK(iteration(&g_tasks) == 0);

    /* A restart chord while held forgets the hold and closes the screen. */
    g_select = 1;
    CHECK(dw3_port_battle_options_poll(2, probe) == 1);
    g_select = 0;
    CHECK(iteration(&g_tasks) == 1);
    CHECK(dw3_port_battle_options_holding());
    dw3_port_battle_options_reset();
    CHECK(!dw3_port_battle_options_holding());
    CHECK(!dw3_port_options_is_open());
    CHECK(dw3_port_battle_options_hold_frame() == 0);
    dw3_port_battle_options_reset();     /* a no-op when not held */

    /* The title opens the screen with nothing locked and the field's log
     * words again after a battle excursion. */
    CHECK(!dw3_port_options_in_game());

    if (g_fail) {
        return 1;
    }
    printf("OK: battle options\n");
    return 0;
}
