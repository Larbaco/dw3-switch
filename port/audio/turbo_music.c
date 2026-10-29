#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L /* clock_gettime under -std=c11 */
#endif
/* P6.36 music at normal speed while turbo is on (docs/P6.36_TURBO_MUSIC.md).
 *
 * The game ticks libsnd by hand: vsyncCallback calls SsSeqCalledTbyT once per
 * game frame (patch 0191 puts this gate in front of it), so turbo, which runs
 * more game frames per second, plays the music faster. The gate decides how
 * many ticks that frame owes (0, 1 or 2). With the Turbo music row on `1x` and
 * fast-forward running, the music gets the ticks of a game that runs at 1x;
 * otherwise (row on `Fast`, turbo off) every frame owes exactly one tick and
 * nothing is logged or changed. Everything stays on the game thread (libsnd
 * is not thread-safe).
 *
 * Two clocks:
 *   frames  deterministic: a credit counter, one tick per rate_x2 / 2 game
 *           frames. Replays and the PC outside a wall-clock run.
 *   wall    one tick per vblank period of real time (60 NTSC, 50 PAL), counted
 *           with CLOCK_MONOTONIC, at most 2 per game frame. The Switch and the
 *           PC outside replays: the real turbo speed there is "up to" the
 *           setting, and a frame clock would play the music under 1x. */
#include "port_audio.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "port_options.h"
#include "port_turbo.h"

#ifndef DW3_TURBO_MUSIC_TEST
#include <psyz/log.h>
#define TM_LOG(...) INFOF(__VA_ARGS__)
#if VERSION_EU
extern int32_t NTSC_MODE; /* the EU disc's 50 / 60 Hz switch (engine/gfx.h) */
#endif
#else
#define TM_LOG(...) (fprintf(stderr, __VA_ARGS__), fputc('\n', stderr))
#endif

/* Registry value of the row: 0 = 1x (the default), 1 = Fast (today's sound). */
enum { TM_OPT_1X = 0, TM_OPT_FAST = 1 };
enum { TM_SRC_GAME = 0, TM_SRC_FRAMES, TM_SRC_WALL };
enum {
    TM_MAX_PER_FRAME = 2,
    TM_IDLE_RESET_US = 250000,   /* a longer gap between frames is a pause */
    TM_WINDOW_US = 1000000,      /* wall window */
    TM_WINDOW_FRAMES = 60        /* frames window */
};

static int g_clock_forced = -1;  /* TM_SRC_*, or -1 = not read yet / auto */
static int g_clock_read;
static int g_src = TM_SRC_GAME;  /* what the last tick call decided */
static int g_rate_x2 = 2;        /* rate the line was logged for */
static int g_credit_x2;          /* frames: game frames owed, in halves */
static int64_t g_acc_us;         /* wall: time owed */
static int64_t g_last_us;
static int g_started;            /* g_last_us is valid */
static unsigned g_win_frames, g_win_ticks;
static int64_t g_win_start_us;
static unsigned g_dropped_logs;
static unsigned long long g_total_ticks, g_total_frames;

#ifdef DW3_TURBO_MUSIC_TEST
static int g_test_opt, g_test_rate = 2, g_test_ntsc = 1;
static int64_t g_test_now_us;
static int g_test_replay;
void dw3_tm_test_set(int opt, int rate_x2, int ntsc, int64_t now_us,
                     int replay) {
    g_test_opt = opt;
    g_test_rate = rate_x2;
    g_test_ntsc = ntsc;
    g_test_now_us = now_us;
    g_test_replay = replay;
}
void dw3_tm_test_reset(void) {
    g_clock_read = 0;
    g_clock_forced = -1;
    g_src = TM_SRC_GAME;
    g_rate_x2 = 2;
    g_credit_x2 = 0;
    g_acc_us = 0;
    g_started = 0;
    g_win_frames = g_win_ticks = 0;
    g_dropped_logs = 0;
    g_total_ticks = g_total_frames = 0;
}
static int tm_option(void) { return g_test_opt; }
static int tm_rate_x2(void) { return g_test_rate; }
static int tm_ntsc(void) { return g_test_ntsc; }
static int64_t tm_now_us(void) { return g_test_now_us; }
static int tm_in_replay(void) { return g_test_replay; }
#else
static int tm_option(void) {
    return dw3_port_options_level(DW3_PORT_OPT_TURBO_MUSIC);
}
static int tm_rate_x2(void) { return dw3_port_turbo_rate_x2(); }
static int tm_ntsc(void) {
#if VERSION_EU
    return NTSC_MODE != 0;
#else
    return 1;
#endif
}
static int64_t tm_now_us(void) {
    struct timespec ts;

    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (int64_t)ts.tv_sec * 1000000 + ts.tv_nsec / 1000;
}
static int tm_in_replay(void) {
    const char *e = getenv("DW3_REPLAY");

    return e != NULL && e[0] != '\0';
}
#endif

/* DW3_TURBO_MUSIC_CLOCK=frames|wall pins the clock (tests). Unset: a replay
 * always uses frames (a wall clock must never run there), anything else wall. */
static int tm_clock(void) {
    if (!g_clock_read) {
        const char *e = getenv("DW3_TURBO_MUSIC_CLOCK");

        g_clock_read = 1;
        if (e != NULL && strcmp(e, "frames") == 0) {
            g_clock_forced = TM_SRC_FRAMES;
        } else if (e != NULL && strcmp(e, "wall") == 0 && !tm_in_replay()) {
            g_clock_forced = TM_SRC_WALL;
        } else {
            g_clock_forced = tm_in_replay() ? TM_SRC_FRAMES : TM_SRC_WALL;
        }
    }
    return g_clock_forced;
}

static int tm_period_us(void) { return tm_ntsc() ? 16683 : 20000; }
static int tm_hz(void) { return tm_ntsc() ? 60 : 50; }

static const char *tm_rate_name(int rate_x2) {
    switch (rate_x2) {
    case 2: return "1x";
    case 3: return "1.5x";
    case 4: return "2x";
    case 6: return "3x";
    case 8: return "4x";
    default: return "?x";
    }
}

static const char *tm_src_name(int src) {
    return src == TM_SRC_WALL ? "wall" : src == TM_SRC_FRAMES ? "frames"
                                                              : "game";
}

/* The gate is on: the row says 1x and the game runs faster than 1x. */
static int tm_gate_on(int *rate_x2) {
    int rate;

    if (tm_option() != TM_OPT_1X) {
        return 0;
    }
    rate = tm_rate_x2();
    if (rate <= 2) {
        return 0;
    }
    *rate_x2 = rate;
    return 1;
}

static void tm_window_flush(int64_t now_us) {
    TM_LOG("dw3-audio: music tick window frames=%u ticks=%u wall_ms=%lld",
           g_win_frames, g_win_ticks,
           (long long)((now_us - g_win_start_us) / 1000));
    g_win_frames = g_win_ticks = 0;
    g_win_start_us = now_us;
}

/* Edge lines: turbo on / off, the speed or the menu rate changed. `rate` in
 * the line is the ticks per second the music gets: the vblank rate on the wall
 * clock, and on the frame clock the ticks per 60 (50) game frames. */
static void tm_note_state(int src, int rate_x2, int64_t now_us) {
    if (src == g_src && (src == TM_SRC_GAME || rate_x2 == g_rate_x2)) {
        return;
    }
    if (g_src != TM_SRC_GAME && g_win_frames > 0) {
        tm_window_flush(now_us);
    }
    if (src == TM_SRC_GAME) {
        TM_LOG("dw3-audio: music tick source=game rate=%d/s game_rate=1x "
               "(gate off; %llu ticks in %llu game frames while it was on)",
               tm_hz(), g_total_ticks, g_total_frames);
    } else {
        /* tenths of a tick per second (EU frames: 50 * 2 / 8 = 12.5) */
        int r10 = src == TM_SRC_WALL ? tm_hz() * 10
                                     : tm_hz() * 20 / rate_x2;

        if (r10 % 10 == 0) {
            TM_LOG("dw3-audio: music tick source=%s rate=%d/s game_rate=%s",
                   tm_src_name(src), r10 / 10, tm_rate_name(rate_x2));
        } else {
            TM_LOG("dw3-audio: music tick source=%s rate=%d.%d/s game_rate=%s",
                   tm_src_name(src), r10 / 10, r10 % 10,
                   tm_rate_name(rate_x2));
        }
    }
    if (g_src == TM_SRC_GAME) {
        g_win_frames = g_win_ticks = 0;
        g_win_start_us = now_us;
    }
    g_src = src;
    g_rate_x2 = rate_x2;
}

int dw3_port_audio_ticks_due(void) {
    int rate;
    int src;
    int n = 0;
    int64_t now = 0;

    if (!tm_gate_on(&rate)) {
        if (g_src != TM_SRC_GAME) {
            tm_note_state(TM_SRC_GAME, 2, tm_now_us());
            g_started = 0;
            g_credit_x2 = 0;
            g_acc_us = 0;
        }
        return 1;
    }
    src = tm_clock();
    now = tm_now_us();
    if (g_src == TM_SRC_GAME) {
        /* Entering: no catch-up burst. */
        g_credit_x2 = 0;
        g_acc_us = 0;
        g_started = 0;
        g_total_ticks = g_total_frames = 0;
    }
    tm_note_state(src, rate, now);

    if (src == TM_SRC_FRAMES) {
        g_credit_x2 += 2;
        if (g_credit_x2 >= rate) {
            g_credit_x2 -= rate;
            n = 1;
        }
    } else {
        int64_t period = tm_period_us();

        if (!g_started) {
            g_started = 1;
            g_last_us = now;
        } else {
            int64_t dt = now - g_last_us;

            g_last_us = now;
            if (dt > TM_IDLE_RESET_US) {
                /* A pause (Options over a held battle, a suspend): no debt. */
                if (g_dropped_logs++ < 8) {
                    TM_LOG("dw3-audio: music tick debt dropped (%lld) after "
                           "an idle gap of %lld ms",
                           (long long)(dt / period), (long long)(dt / 1000));
                }
                g_acc_us = 0;
            } else {
                g_acc_us += dt;
            }
        }
        while (g_acc_us >= period && n < TM_MAX_PER_FRAME) {
            g_acc_us -= period;
            n++;
        }
        if (g_acc_us >= period) {
            if (g_dropped_logs++ < 8) {
                TM_LOG("dw3-audio: music tick debt dropped (%lld)",
                       (long long)(g_acc_us / period));
            }
            g_acc_us = 0;
        }
    }
    g_win_frames++;
    g_win_ticks += (unsigned)n;
    g_total_frames++;
    g_total_ticks += (unsigned)n;
    if (src == TM_SRC_FRAMES ? g_win_frames >= TM_WINDOW_FRAMES
                             : now - g_win_start_us >= TM_WINDOW_US) {
        tm_window_flush(now);
    }
    return n;
}

/* The ticks of a fade (fadeOutSound, patch 0191): the fade spans the same game
 * time as at 1x, so the count is scaled by the game rate. */
int dw3_port_audio_fade_ticks(int ticks) {
    int rate;
    int scaled;

    if (!tm_gate_on(&rate)) {
        return ticks;
    }
    scaled = (ticks * 2 + rate / 2) / rate;
    if (scaled < 1) {
        scaled = 1;
    }
    TM_LOG("dw3-audio: fade scaled %d -> %d ticks (game_rate=%s)", ticks,
           scaled, tm_rate_name(rate));
    return scaled;
}
