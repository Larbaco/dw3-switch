/* P6.33 Items menu usability (docs/P6.33_ITEMS_MENU.md). Policy for the
 * equip panel hook of patch 0128 (partner cycle). On by default, a file-only
 * key of port_options.cfg. */
#include "port_items_menu.h"

#include "port_options.h"

/* not <stdlib.h>: engine/game.h declares its own random() and atoi() */
extern char *getenv(const char *name);

#ifdef DW3_ITEMS_MENU_TEST
#include <stdio.h>
#define IM_LOG(...)                    \
    do {                               \
        fprintf(stderr, __VA_ARGS__);  \
        fputc('\n', stderr);           \
    } while (0)
extern int dw3_test_items_total;
extern int dw3_test_equipped_total;
#define IM_ITEMS_TOTAL() dw3_test_items_total
#define IM_EQUIPPED_TOTAL() dw3_test_equipped_total
#else
#include "engine/game.h"
#include <psyz/log.h>
/* psyz_log, not stderr: on Switch stderr only reaches nxlink, and these lines
 * are the check in sdmc:/switch/dw3/dw3.log. */
#define IM_LOG(...) INFOF(__VA_ARGS__)
static int im_sum(const s8 *counts) {
    int total = 0;
    int i;

    for (i = 0; i < 0x193; i++) {
        total += counts[i];
    }
    return total;
}
#define IM_ITEMS_TOTAL() im_sum(GAME.items)
#define IM_EQUIPPED_TOTAL() im_sum(GAME.equippedItems)
#endif

/* 1 unless the environment variable `env` or the file key `key` says 0. */
static int im_flag(const char *env, const char *key) {
    const char *v = getenv(env);

    if (v != NULL && v[0] != '\0') {
        return v[0] != '0';
    }
    return dw3_port_options_file_int(key, 1) != 0;
}

int dw3_port_items_partner_switch_enabled(void) {
    return im_flag("DW3_ITEMS_PARTNER_SWITCH", DW3_ITEMS_PARTNER_SWITCH_KEY);
}

int dw3_port_items_partner_next(int member, int count, int dir) {
    if (count < 2 || member < 0 || member >= count) {
        return member;
    }
    member += dir < 0 ? -1 : 1;
    if (member < 0) {
        member = count - 1;
    } else if (member >= count) {
        member = 0;
    }
    return member;
}

void dw3_port_items_partner_switched(int from, int to, int partner) {
    IM_LOG("dw3-items-menu: partner switch member %d -> %d (partner %d) bag=%d worn=%d",
           from, to, partner, IM_ITEMS_TOTAL(), IM_EQUIPPED_TOTAL());
}
