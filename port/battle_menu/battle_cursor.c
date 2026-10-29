/* P51 item 1: log of the battle lists' selection bar (see
 * port/include/port_battle_cursor.h). Nothing here draws or changes state. */
#include "port_battle_cursor.h"

#include <stddef.h>
#include <psyz/log.h>

#include "port_turbo.h"
#include "port_widescreen.h"

/* psyz_missing.c: ContinueDraw() calls that carried the bar's ordering table */
extern unsigned dw3_battle_bar_submits;
/* Switch only (nx_battle_mode.c): soft | soft+bbox | gpu-solid | gpu-tex.
 * Weak, so the PC host, which has no mesh renderer, links without it. */
extern const char *dw3_nx_battle_mode_token(void) __attribute__((weak));

enum { BC_SLOTS = 4 };

typedef struct {
    const void *task;
    const char *list;
    unsigned submits;
} BcSlot;

static BcSlot g_slot[BC_SLOTS];

void dw3_port_battle_cursor_open(const void *task, const char *list) {
    int i;
    int use = 0;
    const char *mode = dw3_nx_battle_mode_token ? dw3_nx_battle_mode_token() : "pc";

    for (i = 0; i < BC_SLOTS; i++) {
        if (g_slot[i].task == NULL) {
            use = i;
            break;
        }
    }
    g_slot[use].task = task;
    g_slot[use].list = list;
    g_slot[use].submits = dw3_battle_bar_submits;
    INFOF("dw3-battle-cursor: open list=%s mode=%s widescreen=%d turbo=%d", list,
          mode ? mode : "?", dw3_port_widescreen_enabled() ? 1 : 0, dw3_port_turbo_speed());
}

void dw3_port_battle_cursor_close(const void *task, int count, int sel) {
    int i;

    for (i = 0; i < BC_SLOTS; i++) {
        if (g_slot[i].task == task && task != NULL) {
            unsigned n = dw3_battle_bar_submits - g_slot[i].submits;

            INFOF("dw3-battle-cursor: close list=%s count=%d sel=%d bar=%s vsyncs=%u", g_slot[i].list,
                  count, sel, n > 0 ? "submitted" : "dropped", n);
            g_slot[i].task = NULL;
            return;
        }
    }
}
