/* P6.33 items menu policy (port/items_menu), no game, no disc. The option file
 * reader is a stand-in that returns g_file. */
#include "port_items_menu.h"
#include "port_options.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int dw3_test_items_total = 120;
int dw3_test_equipped_total = 7;
static int g_file = -1; /* -1: key absent */

int dw3_port_options_file_int(const char *key, int def) {
    return strcmp(key, DW3_ITEMS_PARTNER_SWITCH_KEY) == 0 && g_file >= 0 ? g_file : def;
}

static int g_failed;
#define CHECK(cond)                                                       \
    do {                                                                  \
        if (!(cond)) {                                                    \
            printf("FAIL line %d: %s\n", __LINE__, #cond);                \
            g_failed++;                                                   \
        }                                                                 \
    } while (0)

int main(void) {
    int m;
    int i;

    /* wrap, both directions, parties of 2 and 3 */
    CHECK(dw3_port_items_partner_next(0, 3, 1) == 1);
    CHECK(dw3_port_items_partner_next(2, 3, 1) == 0);
    CHECK(dw3_port_items_partner_next(0, 3, -1) == 2);
    CHECK(dw3_port_items_partner_next(1, 3, -1) == 0);
    CHECK(dw3_port_items_partner_next(1, 2, 1) == 0);
    CHECK(dw3_port_items_partner_next(0, 2, -1) == 1);
    /* one partner or a bad member: no change */
    CHECK(dw3_port_items_partner_next(0, 1, 1) == 0);
    CHECK(dw3_port_items_partner_next(0, 0, -1) == 0);
    CHECK(dw3_port_items_partner_next(5, 3, 1) == 5);
    CHECK(dw3_port_items_partner_next(-1, 3, 1) == -1);
    /* a full lap returns to the start */
    m = 1;
    for (i = 0; i < 3; i++) {
        m = dw3_port_items_partner_next(m, 3, 1);
    }
    CHECK(m == 1);

    /* on by default; the file and the environment switch it off */
    unsetenv("DW3_ITEMS_PARTNER_SWITCH");
    g_file = -1;
    CHECK(dw3_port_items_partner_switch_enabled() == 1);
    g_file = 0;
    CHECK(dw3_port_items_partner_switch_enabled() == 0);
    g_file = 1;
    CHECK(dw3_port_items_partner_switch_enabled() == 1);
    setenv("DW3_ITEMS_PARTNER_SWITCH", "0", 1);
    CHECK(dw3_port_items_partner_switch_enabled() == 0);
    setenv("DW3_ITEMS_PARTNER_SWITCH", "1", 1);
    g_file = 0;
    CHECK(dw3_port_items_partner_switch_enabled() == 1);

    dw3_port_items_partner_switched(1, 2, 4);
    dw3_port_items_menu_selected(1, 12, 1, 3);
    dw3_port_items_menu_selected(0, 40, 0, 0);
    if (g_failed) {
        return 1;
    }
    printf("OK: items menu policy\n");
    return 0;
}
