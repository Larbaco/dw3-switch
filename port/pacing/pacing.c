/* P6.18 pacing glue: reads the two Port options rows and answers the hooks of
 * patches 0062 (encounter rate) and 0063 (text speed). The policy itself is
 * in pacing_policy.c. docs/P6.18_PACING.md. */
#include "port_pacing.h"

#include "port_options.h"

#include <stddef.h>

#ifdef DW3_PORT_PACING_TEST
#include <stdio.h>
#define PACING_LOG(...)                       \
    do {                                      \
        fprintf(stderr, __VA_ARGS__);         \
        fputc('\n', stderr);                  \
    } while (0)
#else
#include <psyz/log.h>
/* psyz_log, not stderr: on Switch stderr only reaches nxlink, and these lines
 * are the check in sdmc:/switch/dw3/dw3.log. */
#define PACING_LOG(...) INFOF(__VA_ARGS__)
#endif

/* Indexed by the stored value (0 1x, 1 0.5x, 2 off, 3 1.5x, 4 2x). */
static const char *const ENCOUNTER_NAME[5] = {"1x", "0.5x", "off", "1.5x", "2x"};
static const char *const TEXT_NAME[3] = {"normal", "fast", "instant"};

static int mode_of(int row) {
    int v = dw3_port_options_level(row);
    int max = row == DW3_PORT_OPT_ENCOUNTER_RATE ? DW3_ENCOUNTER_MODE_MAX
                                                 : DW3_PACING_MODE_MAX;

    return v < 0 || v > max ? 0 : v;
}

/* --- encounter rate ------------------------------------------------------ */

static int g_steps;     /* footsteps counted in random-encounter areas since the last battle */
static int g_ignored;   /* footsteps ignored while Off */
static int g_carry;     /* 0.5x / 1.5x remainder */
static int g_carry_mode;/* the mode g_carry belongs to */

int dw3_port_pacing_encounter_step(int *rate) {
    int mode = mode_of(DW3_PORT_OPT_ENCOUNTER_RATE);
    int out;

    if (*rate >= DW3_PACING_FORCED_RATE) {
        return 1; /* map-forced battle area, not a random encounter */
    }
    if (mode != g_carry_mode) {
        g_carry = 0; /* a half step left by another mode is not this one's */
        g_carry_mode = mode;
    }
    out = dw3_pacing_scale_rate(mode, *rate, &g_carry);
    if (out < 0) {
        g_ignored++;
        if ((g_ignored & 31) == 1) {
            PACING_LOG("dw3-pacing: encounters off: %d footstep(s) in battle "
                       "areas ignored", g_ignored);
        }
        return 0;
    }
    g_steps++;
    *rate = out;
    return 1;
}

void dw3_port_pacing_encounter_started(void) {
    if (g_steps == 0) {
        /* Only a map-forced area gets here without a counted footstep. */
        PACING_LOG("dw3-pacing: map-forced area battle on the first footstep "
                   "(not affected by the encounter rate)");
        return;
    }
    PACING_LOG("dw3-pacing: random battle after %d footstep(s) (encounter "
               "rate %s)", g_steps,
               ENCOUNTER_NAME[mode_of(DW3_PORT_OPT_ENCOUNTER_RATE)]);
    g_steps = 0;
}

/* --- text speed ---------------------------------------------------------- */

enum { TRACKED = 4 };
typedef struct Tracked {
    const void *win;
    int armed;  /* a page is being typed */
    int frames; /* updates it has taken so far */
    int held;   /* frames its full page has been held before a scroll */
    int scrolls;/* scrolls of the text so far */
    int page_scrolls; /* scrolls since the current page started typing */
} Tracked;
static Tracked g_tracked[TRACKED];
static int g_next_slot;

static Tracked *find_tracked(const void *win) {
    int i;

    for (i = 0; i < TRACKED; i++) {
        if (g_tracked[i].win == win) {
            return &g_tracked[i];
        }
    }
    return NULL;
}

int dw3_port_pacing_text_delay(const void *win, int delay) {
    int mode = mode_of(DW3_PORT_OPT_TEXT_SPEED);
    Tracked *t;

    if (delay != DW3_PACING_DIALOGUE_DELAY) {
        return delay;
    }
    /* Remember the dialogue windows so the log can say how long a page took. */
    t = find_tracked(win);
    if (t == NULL) {
        t = &g_tracked[g_next_slot];
        g_next_slot = (g_next_slot + 1) % TRACKED;
    }
    t->win = win;
    t->armed = 1;
    t->frames = 0;
    t->held = 0;
    t->scrolls = 0;
    t->page_scrolls = 0;
    return dw3_pacing_scale_delay(mode, delay);
}

int dw3_port_pacing_text_reveal(int type_delay, int finished, int shown) {
    return dw3_pacing_reveal_page(mode_of(DW3_PORT_OPT_TEXT_SPEED), type_delay,
                                  finished, shown);
}

void dw3_port_pacing_text_poll(const void *win, int typing, int done) {
    Tracked *t = find_tracked(win);

    if (t == NULL) {
        return;
    }
    if (!t->armed) {
        if (!typing) {
            return; /* waiting for a button, closed, or not shown yet */
        }
        t->armed = 1; /* the next page */
        t->frames = 0;
        t->page_scrolls = 0;
    }
    if (typing || done) {
        t->frames++;
    }
    if (done) {
        PACING_LOG("dw3-pacing: text page typed in %d frame(s) (text speed %s)",
                   t->frames, TEXT_NAME[mode_of(DW3_PORT_OPT_TEXT_SPEED)]);
        if (mode_of(DW3_PORT_OPT_TEXT_SPEED) == DW3_TEXT_INSTANT &&
            t->frames > 2 * DW3_PACING_SCROLL_HOLD && t->page_scrolls == 0) {
            /* Not a scroll (those log their holds): a page that was typed
             * instead of shown. A diagnostic for the next OLED log. */
            PACING_LOG("dw3-pacing: slow Instant page with no scroll hold "
                       "(%d frames): reveal missed it", t->frames);
        }
        t->armed = 0;
    }
}

int dw3_port_pacing_text_scroll(const void *win, int type_delay) {
    Tracked *t = find_tracked(win);
    int mode = mode_of(DW3_PORT_OPT_TEXT_SPEED);
    int action;

    if (t == NULL) {
        return 0;
    }
    action = dw3_pacing_scroll_action(mode, type_delay, &t->held);
    if (action == 2) {
        t->scrolls++;
        t->page_scrolls++;
        PACING_LOG("dw3-pacing: text scroll %d after a hold of %d frame(s) "
                   "(text speed %s)", t->scrolls, DW3_PACING_SCROLL_HOLD,
                   TEXT_NAME[mode]);
    }
    return action;
}
