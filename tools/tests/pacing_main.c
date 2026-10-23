/* P6.18 pacing policy and glue (port/pacing), no game, no disc.
 *   pacing_test            run every check
 * The glue is built with DW3_PORT_PACING_TEST (log lines on stderr) and reads
 * its two modes from the stand-in below. */
#include "port_options.h"
#include "port_pacing.h"

#include <stdio.h>
#include <string.h>

static int g_encounter;
static int g_text;

int dw3_port_options_level(int index) {
    return index == DW3_PORT_OPT_ENCOUNTER_RATE ? g_encounter : g_text;
}

static int g_failed;
#define CHECK(cond)                                                       \
    do {                                                                  \
        if (!(cond)) {                                                    \
            printf("FAIL line %d: %s\n", __LINE__, #cond);                \
            g_failed++;                                                   \
        }                                                                 \
    } while (0)

static void policy_rate(void) {
    static const int rates[] = {3, 4, 6, 9, 18};
    unsigned i;

    for (i = 0; i < sizeof(rates) / sizeof(rates[0]); i++) {
        int rate = rates[i];
        int carry = 0;
        int sum = 0;
        int n;

        /* 1x: the rate, untouched, and the remainder is never used. */
        CHECK(dw3_pacing_scale_rate(DW3_ENCOUNTER_1X, rate, &carry) == rate);
        CHECK(carry == 0);
        /* Off: the footstep does not count. */
        CHECK(dw3_pacing_scale_rate(DW3_ENCOUNTER_OFF, rate, &carry) == -1);
        /* 0.5x: exactly half in total, whatever the rate is odd or even. */
        for (n = 0; n < 1000; n++) {
            int out = dw3_pacing_scale_rate(DW3_ENCOUNTER_HALF, rate, &carry);

            CHECK(out >= rate / 2 && out <= rate / 2 + 1);
            sum += out;
        }
        CHECK(sum * 2 == rate * 1000);
        /* 1.5x: exactly one and a half times the 1x countdown fall per
         * footstep, with the odd half step carried (never lost). */
        carry = 0;
        sum = 0;
        for (n = 0; n < 1000; n++) {
            int out = dw3_pacing_scale_rate(DW3_ENCOUNTER_1_5X, rate, &carry);

            CHECK(out >= rate * 3 / 2 && out <= rate * 3 / 2 + 1);
            CHECK(carry == 0 || carry == 1);
            sum += out;
        }
        CHECK(sum * 2 == rate * 3 * 1000);
        /* ... and from an odd carry too: nothing is lost or gained. */
        carry = 1;
        sum = dw3_pacing_scale_rate(DW3_ENCOUNTER_1_5X, rate, &carry);
        CHECK(sum * 2 + carry == rate * 3 + 1);
        /* 2x: exactly double, no remainder. */
        carry = 0;
        for (n = 0; n < 1000; n++) {
            CHECK(dw3_pacing_scale_rate(DW3_ENCOUNTER_2X, rate, &carry) == rate * 2);
        }
        CHECK(carry == 0);
    }
    /* A map-forced area (rate 2000, BattleList.count 0) is never touched. */
    {
        int carry = 1;

        CHECK(dw3_pacing_scale_rate(DW3_ENCOUNTER_OFF, 2000, &carry) == 2000);
        CHECK(dw3_pacing_scale_rate(DW3_ENCOUNTER_HALF, 2000, &carry) == 2000);
        CHECK(dw3_pacing_scale_rate(DW3_ENCOUNTER_1X, 2000, &carry) == 2000);
        CHECK(dw3_pacing_scale_rate(DW3_ENCOUNTER_1_5X, 2000, &carry) == 2000);
        CHECK(dw3_pacing_scale_rate(DW3_ENCOUNTER_2X, 2000, &carry) == 2000);
        CHECK(dw3_pacing_scale_rate(DW3_ENCOUNTER_2X, DW3_PACING_FORCED_RATE, 0) ==
              DW3_PACING_FORCED_RATE);
        CHECK(dw3_pacing_scale_rate(DW3_ENCOUNTER_2X, DW3_PACING_FORCED_RATE - 1, 0) ==
              (DW3_PACING_FORCED_RATE - 1) * 2);
        CHECK(carry == 1);
        CHECK(dw3_pacing_scale_rate(DW3_ENCOUNTER_OFF, DW3_PACING_FORCED_RATE, 0) ==
              DW3_PACING_FORCED_RATE);
        CHECK(dw3_pacing_scale_rate(DW3_ENCOUNTER_OFF, DW3_PACING_FORCED_RATE - 1, 0) == -1);
    }
    /* A bad rate is returned as it is, never turned into a countdown. */
    CHECK(dw3_pacing_scale_rate(DW3_ENCOUNTER_HALF, 0, 0) == 0);
    CHECK(dw3_pacing_scale_rate(DW3_ENCOUNTER_HALF, -5, 0) == -5);
    CHECK(dw3_pacing_scale_rate(DW3_ENCOUNTER_1_5X, 0, 0) == 0);
    CHECK(dw3_pacing_scale_rate(DW3_ENCOUNTER_2X, 0, 0) == 0);
    CHECK(dw3_pacing_scale_rate(DW3_ENCOUNTER_2X, -5, 0) == -5);
    /* Without a carry slot 1.5x still returns a whole step. */
    CHECK(dw3_pacing_scale_rate(DW3_ENCOUNTER_1_5X, 3, 0) == 4);
    /* An unknown mode is 1x. */
    CHECK(dw3_pacing_scale_rate(5, 9, 0) == 9);
    CHECK(dw3_pacing_scale_rate(-1, 9, 0) == 9);
}

static void policy_text(void) {
    int mode;

    for (mode = DW3_TEXT_NORMAL; mode <= DW3_TEXT_INSTANT; mode++) {
        /* Only the dialogue delay is scaled; 0 stays 0, the banner's 5 stays. */
        CHECK(dw3_pacing_scale_delay(mode, 0) == 0);
        CHECK(dw3_pacing_scale_delay(mode, 5) == 5);
        CHECK(dw3_pacing_scale_delay(mode, 1) == 1);
        CHECK(dw3_pacing_scale_delay(mode, -3) == -3);
    }
    CHECK(dw3_pacing_scale_delay(DW3_TEXT_NORMAL, 6) == 6);
    CHECK(dw3_pacing_scale_delay(DW3_TEXT_FAST, 6) == DW3_PACING_FAST_DELAY);
    CHECK(dw3_pacing_scale_delay(DW3_TEXT_INSTANT, 6) == DW3_PACING_FAST_DELAY);
    CHECK(DW3_PACING_FAST_DELAY > 0 && DW3_PACING_FAST_DELAY < DW3_PACING_DIALOGUE_DELAY);
    /* The page reveal is Instant only, for the scaled windows only, at the
     * start of a page, and never once the text is finished. */
    CHECK(dw3_pacing_reveal_page(DW3_TEXT_INSTANT, DW3_PACING_FAST_DELAY, 0, 0));
    CHECK(dw3_pacing_reveal_page(DW3_TEXT_INSTANT, DW3_PACING_FAST_DELAY, 0, 2));
    CHECK(dw3_pacing_reveal_page(DW3_TEXT_INSTANT, DW3_PACING_FAST_DELAY, 0, -4));
    CHECK(!dw3_pacing_reveal_page(DW3_TEXT_INSTANT, DW3_PACING_FAST_DELAY, 0, 3));
    CHECK(!dw3_pacing_reveal_page(DW3_TEXT_INSTANT, DW3_PACING_FAST_DELAY, 1, 0));
    CHECK(!dw3_pacing_reveal_page(DW3_TEXT_INSTANT, 5, 0, 0));
    CHECK(!dw3_pacing_reveal_page(DW3_TEXT_INSTANT, 0, 0, 0));
    CHECK(!dw3_pacing_reveal_page(DW3_TEXT_FAST, DW3_PACING_FAST_DELAY, 0, 0));
    CHECK(!dw3_pacing_reveal_page(DW3_TEXT_NORMAL, DW3_PACING_FAST_DELAY, 0, 0));
}

static void policy_scroll(void) {
    int waited = 0;
    int n;
    int mode;

    /* Normal and Fast scroll as the game does, whatever the delay. */
    for (mode = DW3_TEXT_NORMAL; mode <= DW3_TEXT_FAST; mode++) {
        CHECK(dw3_pacing_scroll_action(mode, DW3_PACING_FAST_DELAY, &waited) == 0);
        CHECK(waited == 0);
    }
    /* Instant holds the full page DW3_PACING_SCROLL_HOLD frames, then scrolls
     * and reveals, then holds again for the next scroll. */
    for (n = 0; n < 3; n++) {
        int i;

        for (i = 0; i < DW3_PACING_SCROLL_HOLD; i++) {
            CHECK(dw3_pacing_scroll_action(DW3_TEXT_INSTANT, DW3_PACING_FAST_DELAY, &waited) == 1);
        }
        CHECK(dw3_pacing_scroll_action(DW3_TEXT_INSTANT, DW3_PACING_FAST_DELAY, &waited) == 2);
        CHECK(waited == 0);
    }
    /* A window that is not a scaled dialogue window is never held. */
    CHECK(dw3_pacing_scroll_action(DW3_TEXT_INSTANT, 5, &waited) == 0);
    CHECK(dw3_pacing_scroll_action(DW3_TEXT_INSTANT, 0, &waited) == 0);
    CHECK(dw3_pacing_scroll_action(DW3_TEXT_INSTANT, DW3_PACING_DIALOGUE_DELAY, &waited) == 0);
    CHECK(dw3_pacing_scroll_action(DW3_TEXT_INSTANT, DW3_PACING_FAST_DELAY, 0) == 0);
    CHECK(waited == 0);
}

static void glue_scroll(void) {
    int w1, w2;
    int n;
    int holds = 0;

    g_text = DW3_TEXT_INSTANT;
    CHECK(dw3_port_pacing_text_delay(&w1, 6) == DW3_PACING_FAST_DELAY);
    /* Two scrolls: DW3_PACING_SCROLL_HOLD held frames, then the scroll. */
    for (n = 0; n < 2; n++) {
        int a;

        holds = 0;
        while ((a = dw3_port_pacing_text_scroll(&w1, DW3_PACING_FAST_DELAY)) == 1) {
            holds++;
        }
        CHECK(a == 2 && holds == DW3_PACING_SCROLL_HOLD);
    }
    /* A window the port never gave a dialogue delay is left alone. */
    CHECK(dw3_port_pacing_text_scroll(&w2, DW3_PACING_FAST_DELAY) == 0);
    /* A new text in the same window starts the hold over. */
    CHECK(dw3_port_pacing_text_scroll(&w1, DW3_PACING_FAST_DELAY) == 1);
    CHECK(dw3_port_pacing_text_delay(&w1, 6) == DW3_PACING_FAST_DELAY);
    holds = 0;
    while (dw3_port_pacing_text_scroll(&w1, DW3_PACING_FAST_DELAY) == 1) {
        holds++;
    }
    CHECK(holds == DW3_PACING_SCROLL_HOLD);
    g_text = DW3_TEXT_FAST;
    CHECK(dw3_port_pacing_text_delay(&w1, 6) == DW3_PACING_FAST_DELAY);
    CHECK(dw3_port_pacing_text_scroll(&w1, DW3_PACING_FAST_DELAY) == 0);
    g_text = DW3_TEXT_NORMAL;
    CHECK(dw3_port_pacing_text_scroll(&w1, 6) == 0);
}

static void glue_encounter(void) {
    int rate;
    int steps;

    /* 1x: unchanged, every step counted. */
    g_encounter = DW3_ENCOUNTER_1X;
    rate = 6;
    CHECK(dw3_port_pacing_encounter_step(&rate) == 1 && rate == 6);
    /* A forced area is not counted and not changed in any mode. */
    for (g_encounter = 0; g_encounter <= DW3_ENCOUNTER_MODE_MAX; g_encounter++) {
        rate = 2000;
        CHECK(dw3_port_pacing_encounter_step(&rate) == 1 && rate == 2000);
    }
    /* Off: the footstep is skipped. */
    g_encounter = DW3_ENCOUNTER_OFF;
    for (steps = 0; steps < 100; steps++) {
        rate = 18;
        CHECK(dw3_port_pacing_encounter_step(&rate) == 0);
    }
    /* 0.5x: the caller subtracts the returned rate. */
    g_encounter = DW3_ENCOUNTER_HALF;
    {
        int battle_steps = 18 * 40;
        int n = 0;

        while (battle_steps > 0) {
            rate = 18;
            CHECK(dw3_port_pacing_encounter_step(&rate) == 1);
            battle_steps -= rate;
            n++;
        }
        CHECK(n == 80); /* twice the 1x footsteps */
    }
    dw3_port_pacing_encounter_started();
    /* 1.5x and 2x: the countdown falls faster, so 2/3 and 1/2 the footsteps. */
    g_encounter = DW3_ENCOUNTER_1_5X;
    {
        int battle_steps = 18 * 40;
        int n = 0;

        while (battle_steps > 0) {
            rate = 18;
            CHECK(dw3_port_pacing_encounter_step(&rate) == 1 && rate == 27);
            battle_steps -= rate;
            n++;
        }
        CHECK(n == 27); /* 720 / 27 = 26.7 */
    }
    dw3_port_pacing_encounter_started();
    g_encounter = DW3_ENCOUNTER_2X;
    {
        int battle_steps = 18 * 40;
        int n = 0;

        while (battle_steps > 0) {
            rate = 18;
            CHECK(dw3_port_pacing_encounter_step(&rate) == 1 && rate == 36);
            battle_steps -= rate;
            n++;
        }
        CHECK(n == 20); /* half the 1x footsteps */
    }
    dw3_port_pacing_encounter_started();
    /* Odd rate under 1.5x: the half step carries; a mode change drops it. */
    g_encounter = DW3_ENCOUNTER_1_5X;
    rate = 3;
    CHECK(dw3_port_pacing_encounter_step(&rate) == 1 && rate == 4);
    g_encounter = DW3_ENCOUNTER_2X;
    rate = 3;
    CHECK(dw3_port_pacing_encounter_step(&rate) == 1 && rate == 6);
    g_encounter = DW3_ENCOUNTER_1_5X;
    rate = 3;
    CHECK(dw3_port_pacing_encounter_step(&rate) == 1 && rate == 4);
    dw3_port_pacing_encounter_started();
    /* A forced area starts its battle with no counted footstep. */
    g_encounter = DW3_ENCOUNTER_OFF;
    rate = 2000;
    CHECK(dw3_port_pacing_encounter_step(&rate) == 1 && rate == 2000);
    dw3_port_pacing_encounter_started();
    /* An out-of-range mode behaves as 1x. */
    g_encounter = 7;
    rate = 9;
    CHECK(dw3_port_pacing_encounter_step(&rate) == 1 && rate == 9);
    g_encounter = -1;
    rate = 9;
    CHECK(dw3_port_pacing_encounter_step(&rate) == 1 && rate == 9);
}

static void glue_text(void) {
    int w1, w2;

    g_text = DW3_TEXT_NORMAL;
    CHECK(dw3_port_pacing_text_delay(&w1, 6) == 6);
    CHECK(dw3_port_pacing_text_delay(&w1, 5) == 5);
    g_text = DW3_TEXT_FAST;
    CHECK(dw3_port_pacing_text_delay(&w1, 6) == DW3_PACING_FAST_DELAY);
    CHECK(dw3_port_pacing_text_delay(&w2, 0) == 0);
    g_text = DW3_TEXT_INSTANT;
    CHECK(dw3_port_pacing_text_delay(&w1, 6) == DW3_PACING_FAST_DELAY);
    CHECK(dw3_port_pacing_text_reveal(DW3_PACING_FAST_DELAY, 0, 0) == 1);
    g_text = DW3_TEXT_FAST;
    CHECK(dw3_port_pacing_text_reveal(DW3_PACING_FAST_DELAY, 0, 0) == 0);
    /* The log: a page is reported when it is done, once, and only for
     * windows that were given a dialogue delay. */
    dw3_port_pacing_text_poll(&w1, 1, 0);
    dw3_port_pacing_text_poll(&w1, 1, 0);
    dw3_port_pacing_text_poll(&w1, 0, 1);   /* "typed in 3 frame(s)" */
    dw3_port_pacing_text_poll(&w1, 0, 0);   /* waiting for the button: silent */
    dw3_port_pacing_text_poll(&w1, 0, 1);   /* still waiting: no second report */
    dw3_port_pacing_text_poll(&w1, 1, 0);   /* the next page */
    dw3_port_pacing_text_poll(&w1, 0, 1);   /* "typed in 2 frame(s)" */
    dw3_port_pacing_text_poll(&w2, 1, 1);   /* never registered: silent */
    /* Instant: a long page with no scroll hold is a reveal that missed it. */
    g_text = DW3_TEXT_INSTANT;
    CHECK(dw3_port_pacing_text_delay(&w1, 6) == DW3_PACING_FAST_DELAY);
    {
        int n;

        for (n = 0; n < 30; n++) {
            dw3_port_pacing_text_poll(&w1, 1, 0);
        }
        dw3_port_pacing_text_poll(&w1, 0, 1);   /* 31 frames, no scroll */
    }
    /* ... but not when it was held for scrolls. */
    CHECK(dw3_port_pacing_text_delay(&w1, 6) == DW3_PACING_FAST_DELAY);
    {
        int n;
        int a;

        while ((a = dw3_port_pacing_text_scroll(&w1, DW3_PACING_FAST_DELAY)) == 1) {
            dw3_port_pacing_text_poll(&w1, 1, 0);
        }
        CHECK(a == 2);
        for (n = 0; n < 10; n++) {
            dw3_port_pacing_text_poll(&w1, 1, 0);
        }
        dw3_port_pacing_text_poll(&w1, 0, 1);   /* 23 frames with a scroll */
    }
}

int main(void) {
    policy_rate();
    policy_text();
    policy_scroll();
    glue_scroll();
    glue_encounter();
    glue_text();
    if (g_failed) {
        printf("%d check(s) failed\n", g_failed);
        return 1;
    }
    puts("OK: pacing policy");
    return 0;
}
