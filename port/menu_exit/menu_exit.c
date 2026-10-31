/* Instant menu exit: policy, the frame step and the log line for patch 0200
 * (see port/include/port_menu_exit.h). */
#include "port_menu_exit.h"

#include "port_options.h"

/* not <stdlib.h>: engine/game.h declares its own random() and atoi() */
extern char *getenv(const char *name);

#ifdef DW3_MENU_EXIT_TEST
#include <stdio.h>
#define ME_LOG(...)                    \
    do {                               \
        fprintf(stderr, __VA_ARGS__);  \
        fputc('\n', stderr);           \
    } while (0)
#else
#include "engine/game.h"
#include <psyz/log.h>
/* psyz_log, not stderr: on Switch stderr only reaches nxlink, and the line is
 * the check in sdmc:/switch/dw3/dw3.log. */
#define ME_LOG(...) INFOF(__VA_ARGS__)
#endif

int dw3_port_menu_instant_exit_enabled(void) {
    const char *v = getenv("DW3_MENU_INSTANT_EXIT");

    if (v != NULL && v[0] != '\0') {
        return v[0] != '0';
    }
    return dw3_port_options_file_int(DW3_MENU_INSTANT_EXIT_KEY, 1) != 0;
}

#ifndef DW3_MENU_EXIT_TEST
/* The same additions as vsyncCallback (engine/gfx/display.c: the time counters
 * and the play time by the vsync step; the EU disc's 50 Hz step unless
 * NTSC_MODE), drawFrame (frame counter, the derived times, the play time
 * clock) and the main loop (RANDOM.next()). Not done: the pad, the file cache,
 * the sound loader and the music ticks, which only matter while the menu is
 * on screen. */
void dw3_port_menu_exit_frame(void) {
    s32 step = VSYNC_STEP_NTSC;

#if VERSION_EU
    if (!NTSC_MODE) {
        step = VSYNC_STEP_PAL;
    }
#endif
    GFX.timeCounter += step;
    GFX.frameTimeCounter += step;
    GAME.playTime.frames += step;
    GFX.frameCounter += step;
    GFX.frameCount = GFX.frameCounter >> 8;
    GFX.time = GFX.timeCounter >> 8;
    GFX.frameTime = GFX.frameTimeCounter >> 8;
    GFX.frameTimeCounter &= 0xFF;
    GAME.funcs.updatePlayTime();
    RANDOM.next();
}
#endif

const char *dw3_port_menu_exit_screen_name(int option, int extra) {
    /* STSTATUS_screens (menus/field_menu/helpers.c): Items, Sort, Map,
     * Techniques, Status, then Card folders when the list has the extra row,
     * then the party page. */
    static const char *const names[] = {"items", "sort", "map", "techniques", "status"};

    if (option >= 0 && option < 5) {
        return names[option];
    }
    if (extra != 0 && option == 5) {
        return "cards";
    }
    return "other";
}

/* The clocks and the RNG the exit leaves behind. The instant and the original
 * paths log them on the frame that requests the field, so two runs of the same
 * recording (key on, key off) can be compared: they must be equal. */
static void me_state(int *time, int *play, int *frames, int *rng) {
#ifdef DW3_MENU_EXIT_TEST
    *time = 0;
    *play = 0;
    *frames = 0;
    *rng = 0;
#else
    *time = (int)GFX.time;
    *play = (int)(GAME.playTime.seconds + 60 * (GAME.playTime.minutes + 60 * GAME.playTime.hours));
    *frames = (int)GAME.playTime.frames;
    *rng = (int)RANDOM.index;
#endif
}

void dw3_port_menu_exit_report(int instant, int skipped, int option, int extra) {
    const char *from = dw3_port_menu_exit_screen_name(option, extra);
    int time;
    int play;
    int frames;
    int rng;

    me_state(&time, &play, &frames, &rng);
    if (instant) {
        ME_LOG("dw3-menu: exit to field instant (skipped %d transition frames, from %s) time=%d play=%ds+%d rng=%d",
               skipped, from, time, play, frames, rng);
    } else {
        ME_LOG("dw3-menu: exit to field with the original transition (instant exit off, from %s) time=%d play=%ds+%d rng=%d",
               from, time, play, frames, rng);
    }
}
