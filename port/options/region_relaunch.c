/* P6.38: restart into the chosen region; see port_region_relaunch.h. */
#include "port_region_relaunch.h"

#include "port_options_groups.h"

#if DW3_OPT_REGION_ROW

#include "dw3_region_relaunch.h"
#include "dw3_region_rt.h"
#include "memcard_host.h"
#include "port_confirm.h"
#include "port_options.h"

#include <stdio.h>

#if defined(DW3_REGION_RELAUNCH_TEST)
#define RR_INFO(...) (fprintf(stderr, __VA_ARGS__), fputc('\n', stderr))
#define RR_WARN(...) (fprintf(stderr, __VA_ARGS__), fputc('\n', stderr))
#else
#include <psyz/log.h>
#define RR_INFO(...) INFOF(__VA_ARGS__)
#define RR_WARN(...) WARNF(__VA_ARGS__)
#endif

/* The row was stepped since the window was last answered or skipped. Only a
 * change asks: walking the cursor over a row whose value was left on the other
 * region (a No an earlier time) must not ask again. */
static int g_changed;

static Dw3Region region_of(int value) {
    return value == 1 ? DW3_REGION_US : value == 2 ? DW3_REGION_EU : DW3_REGION_NONE;
}

void dw3_port_region_relaunch_reset(void) {
    g_changed = 0;
}

void dw3_port_region_relaunch_note_step(void) {
    g_changed = 1;
}

int dw3_port_region_relaunch_leave(int value, int in_game) {
    Dw3Region target = region_of(value);
    const char *name = dw3_region_name(target);
    char why[160];

    if (!g_changed) {
        return 0;
    }
    g_changed = 0;
    if (target == DW3_REGION_NONE) {
        RR_INFO("dw3-region: region_default ask: no restart prompt");
        return 0;
    }
    if (target == dw3_region()) {
        RR_INFO("dw3-region: region_default %s is the running region: no restart prompt", name);
        return 0;
    }
    if (!dw3_port_confirm_available()) {
        RR_INFO("dw3-region: no restart prompt for %s: this build has no window for it; takes "
                "effect at the next start", name);
        return 0;
    }
    why[0] = '\0';
    switch (dw3_host_relaunch_check(target, why, sizeof(why))) {
    case DW3_RELAUNCH_OK:
        RR_INFO("dw3-region: restart prompt for %s (%s)", name, in_game ? "in game" : "title");
        dw3_port_confirm_open_region((int)target, in_game);
        return dw3_port_confirm_is_open();
    case DW3_RELAUNCH_NO_DATA:
        /* Nothing to restart into: the next start boots what is there. */
        RR_INFO("dw3-region: no restart prompt for %s: %s; takes effect at the next start", name,
                why);
        return 0;
    default:
        RR_WARN("dw3-region: relaunch to %s not available: %s; takes effect at the next start",
                name, why);
        dw3_port_confirm_open_notice();
        return dw3_port_confirm_is_open();
    }
}

/* The relaunch could not go ahead: say so in the log and on the screen, and
 * keep today's behaviour (the choice is saved, the next start uses it). */
static void failed(const char *name, const char *why) {
    RR_WARN("dw3-region: relaunch to %s failed: %s; takes effect at the next start", name, why);
    dw3_port_confirm_open_notice();
}

void dw3_port_region_relaunch_yes(int target_value) {
    Dw3Region target = region_of(target_value);
    const char *name = dw3_region_name(target);
    const char *base = dw3_port_options_region_base();
    char why[160];

    RR_INFO("dw3-region: relaunch to %s requested", name);
    /* The same care as the restart hotkey: a save being written, or one that
     * cannot be flushed, stops the relaunch. */
    if (memcard_host_section_open_any()) {
        failed(name, "a save is being written");
        return;
    }
    if (memcard_host_flush_all() != 0) {
        failed(name, "the saves could not be flushed");
        return;
    }
    /* last_region and region_default, so the next plain start agrees too. */
    if (base != NULL && dw3_region_cfg_write(base, target, 1, target) == 0) {
        RR_INFO("dw3-region: saved last_region=%s region_default=%s before the relaunch", name,
                name);
    } else {
        RR_WARN("dw3-region: could not store the choice before the relaunch; the new process "
                "still starts %s", name);
    }
    why[0] = '\0';
    if (dw3_host_relaunch_request(target, why, sizeof(why)) != 0) {
        failed(name, why[0] != '\0' ? why : "the host could not start it");
        return;
    }
    RR_INFO("dw3-region: relaunch to %s: leaving the game now (%s)", name, why);
}

void dw3_port_region_relaunch_no(int target_value, int cancelled) {
    RR_INFO("dw3-region: relaunch declined%s; %s takes effect at the next start",
            cancelled ? " (cancelled)" : "", dw3_region_name(region_of(target_value)));
}

void dw3_port_region_relaunch_notice_done(void) {
    RR_INFO("dw3-region: restart notice closed");
}

#else /* no Region row: a plain US / EU image */

void dw3_port_region_relaunch_reset(void) {}
void dw3_port_region_relaunch_note_step(void) {}
int dw3_port_region_relaunch_leave(int value, int in_game) {
    (void)value;
    (void)in_game;
    return 0;
}
void dw3_port_region_relaunch_yes(int target) { (void)target; }
void dw3_port_region_relaunch_no(int target, int cancelled) {
    (void)target;
    (void)cancelled;
}
void dw3_port_region_relaunch_notice_done(void) {}

#endif
