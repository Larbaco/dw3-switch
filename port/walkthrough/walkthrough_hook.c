/* P6.35 walkthrough: the game-side reader (docs/P6.35_WALKTHROUGH.md, 1.4).
 * Reads GAME.progress, GAME.fieldMode and FLAGS_00.checkCondition once per
 * call, resolves the step and logs it. Read-only: no applyAction. No UI.
 *
 * Callers: the debug probe (patch 0186, DW3_CHEATS builds, env
 * DW3_WALKTHROUGH_PROBE) now; the walkthrough screen when it opens later. */
#include "port_walkthrough.h"

#include "engine/game.h"
#include <psyz/log.h>
char *getenv(const char *name); /* stdlib.h clashes with engine/game.h */

#if defined(VERSION_EU)
#define WT_THIS_REGION WT_REGION_EU
#else
#define WT_THIS_REGION WT_REGION_US
#endif

/* only codes taken from the step table reach the game's evaluator */
static int game_check(void *ctx, unsigned code, unsigned value) {
    (void)ctx;
    return FLAGS_00.checkCondition((u16)code, (u16)value) != 0;
}

/* The field map the player is in: the running mode when it is a field map,
 * else GAME.fieldMode, which the field menu and the other screens return to
 * (what STSTATUS_getArea() reads). */
static int field_mode_now(void) {
    int cur = (int)GAME.funcs.getMode();

    if (cur >= 0x200 && cur <= 0x2EF) return cur;
    return (int)GAME.fieldMode;
}

/* One read of the game state for the walkthrough screen. Read-only. */
int dw3_walkthrough_snapshot(DW3WtState *st, int *mode) {
    int m = field_mode_now();

    st->progress = (int)GAME.progress;
    st->half = dw3_wt_half_of_mode(m);
    st->area = dw3_wt_area_of_mode(m);
    st->region = WT_THIS_REGION;
    st->check = game_check;
    st->ctx = NULL;
    if (mode) *mode = m;
    return 1;
}

int dw3_walkthrough_log_state(const char *why) {
    int mode = field_mode_now();

    DW3WtState st;
    DW3WtResult r;
    char line[192];

    st.progress = (int)GAME.progress;
    st.half = dw3_wt_half_of_mode(mode);
    st.area = dw3_wt_area_of_mode(mode);
    st.region = WT_THIS_REGION;
    st.check = game_check;
    st.ctx = NULL;
    dw3_wt_resolve(&st, &r);
    dw3_wt_format_line(line, sizeof(line), &st, &r);
    INFOF("%s [%s mode 0x%x]", line, why ? why : "-", (unsigned)mode);
    return r.status;
}

void dw3_walkthrough_probe_tick(void) {
    static int enabled = -1;
    static int last_progress = -2;
    static int last_mode = -2;
    static int last_flag = -2;
    int flag;

    if (enabled < 0) {
        const char *e = getenv("DW3_WALKTHROUGH_PROBE");

        enabled = (e && e[0] && e[0] != '0') ? 1 : 0;
    }
    if (!enabled || field_mode_now() < 0x200) return; /* field maps only */
    flag = game_check(NULL, WT_FLAG(0x40, 0x0D), 1);
    if ((int)GAME.progress == last_progress && field_mode_now() == last_mode &&
        flag == last_flag) {
        return;
    }
    last_progress = (int)GAME.progress;
    last_mode = field_mode_now();
    last_flag = flag;
    (void)dw3_walkthrough_log_state("probe");
}
