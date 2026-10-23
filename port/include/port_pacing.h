/* P6.18 pacing options (encounter rate, text speed), shared by the PC and
 * Switch game hosts. Design, decomp hooks and limits: docs/P6.18_PACING.md. */
#ifndef DW3_PORT_PACING_H
#define DW3_PORT_PACING_H

#ifdef __cplusplus
extern "C" {
#endif

/* Both Port options rows hold a mode; 0 is the original behaviour. Text speed
 * has the modes 0..DW3_PACING_MODE_MAX; Encounter rate 0..DW3_ENCOUNTER_MODE_MAX. */
#define DW3_PACING_MODE_MAX 2
#define DW3_ENCOUNTER_MODE_MAX 4

/* Encounter rate. The stored values (port_options.cfg `encounter_rate`) are
 * the ones P6.18 shipped (0 / 1 / 2) plus the two added later; they are never
 * renumbered. The order the Options screen steps through them is
 * OFF, 0.5x, 1x, 1.5x, 2x (port/options/port_options.c, docs/PORT_OPTIONS.md). */
enum {
    DW3_ENCOUNTER_1X = 0,
    DW3_ENCOUNTER_HALF = 1,
    DW3_ENCOUNTER_OFF = 2,
    DW3_ENCOUNTER_1_5X = 3,
    DW3_ENCOUNTER_2X = 4
};

/* Text speed. */
enum {
    DW3_TEXT_NORMAL = 0,
    DW3_TEXT_FAST = 1,
    DW3_TEXT_INSTANT = 2
};

/* The type delay (frames between characters, minus one) the game gives every
 * dialogue and message box (setTypeDelay(.., 6)); the area banner uses 5 and
 * is left alone. Fast and Instant type at DW3_PACING_FAST_DELAY. */
#define DW3_PACING_DIALOGUE_DELAY 6
#define DW3_PACING_FAST_DELAY 2

/* Instant mode and a text that scrolls (the window shows two lines of a three
 * line text and scrolls for the third): the page is held this many frames
 * before it scrolls, so what is on screen can be read, and the new line then
 * appears at once instead of being typed at 3 frames per character. About
 * 0.2 s at 60 Hz. Patch 0068. */
#define DW3_PACING_SCROLL_HOLD 12

/* A battle area whose countdown step is this or more (FIELDSTG_battleRates
 * index 0 is 2000, more than the countdown can ever hold) starts its battle on
 * the first step: the map forces it, it is not a random encounter, so the
 * Encounter rate row leaves it alone. The random areas use 3..18. */
#define DW3_PACING_FORCED_RATE 1000

/* Pure policy, in port/pacing/pacing_policy.c (no dependencies, unit tested). */

/* Countdown step of one footstep in a random-encounter area under `mode`.
 * `carry` is the caller's remainder (0.5x and 1.5x spend the odd half step on
 * the next footstep, so over any number of footsteps the countdown falls by
 * exactly half / one and a half times what 1x takes off; 2x is exact with no
 * remainder). Returns the step to
 * subtract from GAME.battleSteps, or -1 when the footstep must not count at
 * all (Off). A forced area (rate >= DW3_PACING_FORCED_RATE) is returned
 * unchanged in every mode. */
int dw3_pacing_scale_rate(int mode, int rate, int *carry);

/* Type delay under `mode` for a window that the game asked to type at
 * `delay`: unchanged for every window but the dialogue ones, 0 stays 0. */
int dw3_pacing_scale_delay(int mode, int delay);

/* Instant mode shows a whole page when it starts: true when a window typing
 * at `type_delay` has `shown` characters of its page on screen (see
 * updateTextWindow). */
int dw3_pacing_reveal_page(int mode, int type_delay, int finished, int shown);

/* What a dialogue window typing at `type_delay` does when its page is full and
 * would scroll, under text speed `mode`. `waited` is the window's count of
 * held frames. 0: scroll as the game does (every mode but Instant, and every
 * window that is not a dialogue window). 1: hold, leave the window as it is
 * (counts `waited` up). 2: the hold is over, scroll and show the new line
 * whole (`waited` is cleared). */
int dw3_pacing_scroll_action(int mode, int type_delay, int *waited);

/* Patch 0062 (field/field_mode/battle.c): FIELDSTG_countBattleSteps. *rate
 * is the area's countdown step; returns 1 to run the countdown with the
 * (possibly changed) *rate, 0 to skip the footstep entirely. */
int dw3_port_pacing_encounter_step(int *rate);
/* Patch 0062: a random battle is starting. Logs how many footsteps led to it. */
void dw3_port_pacing_encounter_started(void);

/* Patch 0063 (engine/text/text_window.c). `win` is only an identity.
 * text_delay: the delay to use for setTypeDelay(win, delay). text_reveal:
 * Instant mode, show the page now. text_poll runs after every window update:
 * `typing` = shown and typing (not finished), `done` = finished or waiting for
 * a button; a page is logged when `done` ends it. */
int dw3_port_pacing_text_delay(const void *win, int delay);
int dw3_port_pacing_text_reveal(int type_delay, int finished, int shown);
void dw3_port_pacing_text_poll(const void *win, int typing, int done);
/* Patch 0068 (textCodeNewLine): a full page is about to scroll. Returns the
 * dw3_pacing_scroll_action result for this window. */
int dw3_port_pacing_text_scroll(const void *win, int type_delay);

#ifdef __cplusplus
}
#endif

#endif /* DW3_PORT_PACING_H */
