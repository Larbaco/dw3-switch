#define _POSIX_C_SOURCE 200809L
/* Headless P6.36 music tick gate (port/audio/turbo_music.c): cadence per game
 * rate on the frame clock, pass-through at 1x and with the row on Fast, the
 * wall clock's cap and idle reset, fade scaling, and the log lines. */
#include "port_audio.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

void dw3_tm_test_set(int opt, int rate_x2, int ntsc, int64_t now_us, int replay);
void dw3_tm_test_reset(void);

static int fail(const char *msg) {
    fprintf(stderr, "FAIL: %s\n", msg);
    return 1;
}

/* Ticks over n game frames, `step_us` of wall time between them. */
static int run(int n, int opt, int rate, int64_t *now, int64_t step_us) {
    int total = 0;
    int i;

    for (i = 0; i < n; i++) {
        *now += step_us;
        dw3_tm_test_set(opt, rate, 1, *now, 0);
        total += dw3_port_audio_ticks_due();
    }
    return total;
}

int main(void) {
    int64_t now = 1000000;
    int i, t;

    /* Frame clock (DW3_TURBO_MUSIC_CLOCK=frames; a replay is forced to it). */
    setenv("DW3_TURBO_MUSIC_CLOCK", "frames", 1);
    dw3_tm_test_reset();

    /* 1x game rate: always exactly one tick per frame, nothing logged. */
    if (run(600, 0, 2, &now, 16683) != 600) {
        return fail("rate 1x passes every frame through");
    }
    /* Row on Fast, turbo 4x: today's behaviour, one tick per frame. */
    if (run(600, 1, 8, &now, 4166) != 600) {
        return fail("Fast keeps one tick per game frame at 4x");
    }
    fprintf(stderr, "-- no music tick line above this one --\n");

    /* 1x row, 2x / 3x / 4x: one tick per 2 / 3 / 4 game frames. */
    if (run(600, 0, 4, &now, 8333) != 300) {
        return fail("2x: 300 ticks in 600 frames");
    }
    if (run(600, 0, 6, &now, 5555) != 200) {
        return fail("3x: 200 ticks in 600 frames");
    }
    if (run(600, 0, 8, &now, 4166) != 150) {
        return fail("4x: 150 ticks in 600 frames");
    }
    /* The menu's 1.5x: two ticks every three frames. */
    if (run(600, 0, 3, &now, 11122) != 400) {
        return fail("1.5x: 400 ticks in 600 frames");
    }
    /* 4x cadence frame by frame: 0 0 0 1. */
    dw3_tm_test_reset();
    for (i = 0; i < 12; i++) {
        now += 4166;
        dw3_tm_test_set(0, 8, 1, now, 0);
        t = dw3_port_audio_ticks_due();
        if (t != (i % 4 == 3 ? 1 : 0)) {
            return fail("4x ticks on every fourth frame");
        }
    }
    /* Back to 1x: the gate is off again, one tick per frame. */
    if (run(10, 0, 2, &now, 16683) != 10) {
        return fail("turbo off returns to one tick per frame");
    }

    /* Wall clock: one tick per vblank period of real time, at most 2 a frame. */
    setenv("DW3_TURBO_MUSIC_CLOCK", "wall", 1);
    dw3_tm_test_reset();
    now = 5000000;
    /* the test's replay flag is 0, so the pin is honoured */
    t = run(2400, 0, 8, &now, 4166); /* 10 s of game frames at 4x */
    if (t < 595 || t > 605) {
        fprintf(stderr, "wall 4x ticks in 10 s: %d\n", t);
        return fail("wall 4x gives ~60 ticks a second");
    }
    dw3_tm_test_reset();
    now = 5000000;
    t = run(300, 0, 8, &now, 33000); /* the game is slower than 1x: 2 per frame at most */
    if (t < 585 || t > 600) {
        fprintf(stderr, "wall slow: %d\n", t);
        return fail("wall: a slow frame owes two ticks, no more");
    }
    /* An idle gap (a pause) is dropped, not played back. */
    dw3_tm_test_reset();
    now = 5000000;
    run(10, 0, 8, &now, 4166);
    now += 3000000;
    dw3_tm_test_set(0, 8, 1, now, 0);
    if (dw3_port_audio_ticks_due() > 1) {
        return fail("wall: no burst after a pause");
    }
    /* PAL: 50 ticks a second. */
    dw3_tm_test_reset();
    now = 5000000;
    t = 0;
    for (i = 0; i < 2400; i++) {
        now += 4166;
        dw3_tm_test_set(0, 8, 0, now, 0);
        t += dw3_port_audio_ticks_due();
    }
    if (t < 495 || t > 505) {
        fprintf(stderr, "wall PAL: %d\n", t);
        return fail("wall PAL gives ~50 ticks a second");
    }

    /* Fades: scaled by the game rate while the gate is on. */
    dw3_tm_test_set(0, 2, 1, now, 0);
    if (dw3_port_audio_fade_ticks(60) != 60) {
        return fail("fade at 1x is unchanged");
    }
    dw3_tm_test_set(1, 8, 1, now, 0);
    if (dw3_port_audio_fade_ticks(60) != 60) {
        return fail("fade with the row on Fast is unchanged");
    }
    dw3_tm_test_set(0, 8, 1, now, 0);
    if (dw3_port_audio_fade_ticks(60) != 15) {
        return fail("fade 60 -> 15 at 4x");
    }
    if (dw3_port_audio_fade_ticks(50) != 13) {
        return fail("fade 50 -> 13 at 4x");
    }
    dw3_tm_test_set(0, 6, 1, now, 0);
    if (dw3_port_audio_fade_ticks(60) != 20) {
        return fail("fade 60 -> 20 at 3x");
    }
    dw3_tm_test_set(0, 3, 1, now, 0);
    if (dw3_port_audio_fade_ticks(60) != 40) {
        return fail("fade 60 -> 40 at 1.5x");
    }

    printf("OK: turbo music gate\n");
    return 0;
}
