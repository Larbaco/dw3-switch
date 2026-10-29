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
static int g_file_equip = -1;
static int g_file_stays = -1;
static int g_file_bag = -1;
static int g_file_use = -1;

int dw3_port_options_file_int(const char *key, int def) {
    if (strcmp(key, DW3_ITEMS_USE_STAYS_KEY) == 0) {
        return g_file_use >= 0 ? g_file_use : def;
    }
    if (strcmp(key, DW3_ITEMS_BAG_VIEW_KEY) == 0) {
        return g_file_bag >= 0 ? g_file_bag : def;
    }
    if (strcmp(key, DW3_ITEMS_EQUIP_STAYS_KEY) == 0) {
        return g_file_stays >= 0 ? g_file_stays : def;
    }
    if (strcmp(key, DW3_ITEMS_EQUIP_FROM_ITEMS_KEY) == 0) {
        return g_file_equip >= 0 ? g_file_equip : def;
    }
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

    /* P6.34: the slot rule, every kind */
    {
        const char *rule = NULL;
        int none[6] = {0, 0, 0, 0, 0, 0};
        int g0[6] = {-1, -1, -1, -1, -1, -1};
        int full[6] = {10, 11, 12, 13, 14, 15};
        int gfull[6] = {-1, -1, -1, -1, 3, 4};
        int right_only[6] = {0, 0, 12, 0, 0, 0};
        int acc4[6] = {0, 0, 0, 0, 14, 0};
        int gacc4[6] = {-1, -1, -1, -1, 3, -1};
        int acc5[6] = {0, 0, 0, 0, 0, 15};
        int gacc5[6] = {-1, -1, -1, -1, -1, 4};

        CHECK(dw3_port_items_equip_slot(DW3_EQUIP_HEAD, 0, full, gfull, &rule) == 0 && !strcmp(rule, "head"));
        CHECK(dw3_port_items_equip_slot(DW3_EQUIP_BODY, 0, full, gfull, &rule) == 1 && !strcmp(rule, "body"));
        CHECK(dw3_port_items_equip_slot(DW3_EQUIP_RIGHT_HAND, 0, none, g0, &rule) == 2 && !strcmp(rule, "hand-empty"));
        CHECK(dw3_port_items_equip_slot(DW3_EQUIP_RIGHT_HAND, 0, full, gfull, &rule) == 2 && !strcmp(rule, "hand-replace"));
        CHECK(dw3_port_items_equip_slot(DW3_EQUIP_LEFT_HAND, 0, none, g0, &rule) == 3 && !strcmp(rule, "hand-empty"));
        CHECK(dw3_port_items_equip_slot(DW3_EQUIP_LEFT_HAND, 0, full, gfull, &rule) == 3 && !strcmp(rule, "hand-replace"));
        CHECK(dw3_port_items_equip_slot(DW3_EQUIP_EITHER_HAND, 0, none, g0, &rule) == 2 && !strcmp(rule, "hand-empty"));
        CHECK(dw3_port_items_equip_slot(DW3_EQUIP_EITHER_HAND, 0, right_only, g0, &rule) == 3 && !strcmp(rule, "hand-empty"));
        CHECK(dw3_port_items_equip_slot(DW3_EQUIP_EITHER_HAND, 0, full, gfull, &rule) == 2 && !strcmp(rule, "hand-replace"));
        CHECK(dw3_port_items_equip_slot(DW3_EQUIP_BOTH_HANDS, 0, full, gfull, &rule) == 2 && !strcmp(rule, "both-hands"));
        CHECK(dw3_port_items_equip_slot(DW3_EQUIP_ACCESSORY, 0, none, g0, &rule) == 4 && !strcmp(rule, "acc-empty"));
        CHECK(dw3_port_items_equip_slot(DW3_EQUIP_ACCESSORY, 0, acc4, gacc4, &rule) == 5 && !strcmp(rule, "acc-empty"));
        CHECK(dw3_port_items_equip_slot(DW3_EQUIP_ACCESSORY, 0, full, gfull, &rule) == 4 && !strcmp(rule, "acc-replace"));
        CHECK(dw3_port_items_equip_slot(DW3_EQUIP_GROUP_ACCESSORY, 3, full, gfull, &rule) == 4 && !strcmp(rule, "group"));
        CHECK(dw3_port_items_equip_slot(DW3_EQUIP_GROUP_ACCESSORY, 4, full, gfull, &rule) == 5 && !strcmp(rule, "group"));
        CHECK(dw3_port_items_equip_slot(DW3_EQUIP_GROUP_ACCESSORY, 9, acc5, gacc5, &rule) == 4 && !strcmp(rule, "acc-empty"));
        CHECK(dw3_port_items_equip_slot(DW3_EQUIP_GROUP_ACCESSORY, 9, full, gfull, &rule) == 4 && !strcmp(rule, "acc-replace"));
        CHECK(dw3_port_items_equip_slot(0, 0, none, g0, &rule) == -1 && !strcmp(rule, "unmapped"));
        CHECK(dw3_port_items_equip_slot(9, 0, none, g0, &rule) == -1);
    }

    /* the key: on by default, the file and the environment switch it off */
    unsetenv("DW3_ITEMS_EQUIP_FROM_ITEMS");
    CHECK(dw3_port_items_equip_enabled() == 1);
    g_file_equip = 0;
    CHECK(dw3_port_items_equip_enabled() == 0);
    CHECK(dw3_port_items_equip_open(1, 92, DW3_EQUIP_EITHER_HAND, 3) == 0);
    g_file_equip = 1;
    setenv("DW3_ITEMS_EQUIP_FROM_ITEMS", "0", 1);
    CHECK(dw3_port_items_equip_enabled() == 0);
    unsetenv("DW3_ITEMS_EQUIP_FROM_ITEMS");
    CHECK(dw3_port_items_equip_open(1, 92, DW3_EQUIP_EITHER_HAND, 3) == 1);
    CHECK(dw3_port_items_equip_open(1, 92, DW3_EQUIP_EITHER_HAND, 0) == 0); /* worn only */
    CHECK(dw3_port_items_equip_open(1, 92, 0, 3) == 0);                      /* unmapped kind */

    /* the result lines: game-font bytes, every language */
    {
        char buf[200];
        int n;
        const char *item = "\x1c\x2a";

        n = dw3_port_items_equip_message(buf, sizeof(buf), 2, DW3_ITEMS_MSG_DONE, item, item, NULL);
        CHECK(n > 0 && n == (int)strlen(buf));
        CHECK(strstr(buf, "\x05") != NULL); /* ends with the period icon */
        n = dw3_port_items_equip_message(buf, sizeof(buf), 2, DW3_ITEMS_MSG_DONE, item, item, item);
        CHECK(strstr(buf, "\x02\x01") != NULL); /* a second line */
        for (int lang = 2; lang <= 6; lang++) {
            n = dw3_port_items_equip_message(buf, sizeof(buf), lang, DW3_ITEMS_MSG_REFUSED, item, NULL, NULL);
            CHECK(n > 4);
        }
        /* a tiny buffer never overflows and stays terminated */
        n = dw3_port_items_equip_message(buf, 6, 3, DW3_ITEMS_MSG_DONE, item, item, item);
        CHECK(n <= 5 && buf[n] == 0);
    }
    dw3_port_items_equip_begin();
    dw3_port_items_equip_refused(6, 92, "partner-mask");
    dw3_port_items_equip_chosen(2, "hand-replace", 92, 0);
    dw3_port_items_equip_done(0, 2, 96);
    dw3_port_items_equip_list_rebuilt(123, 4);
    dw3_port_items_equip_cancelled();

    dw3_port_items_partner_switched(1, 2, 4);
    dw3_port_items_menu_selected(1, 12, 1, 3);
    dw3_port_items_menu_selected(0, 40, 0, 0);
    /* P6.34 phase 2: equip stays. Row of the worn item, page of the list, key */
    {
        short items[12] = {-1, 5, 9, 12, 20, 21, 22, 23, 24, 25, 26, 27};

        CHECK(dw3_port_items_stays_row(items, 12, 12) == 3);
        CHECK(dw3_port_items_stays_row(items, 12, 27) == 11);
        CHECK(dw3_port_items_stays_row(items, 12, 99) == 0); /* no spare copy */
        CHECK(dw3_port_items_stays_row(items, 12, 0) == 0);  /* nothing worn */
        CHECK(dw3_port_items_stays_row(items, 12, -1) == 0);
        CHECK(dw3_port_items_stays_row(items, 1, 5) == 0);   /* only "none" */
        CHECK(dw3_port_items_stays_row(NULL, 12, 5) == 0);
    }
    CHECK(dw3_port_items_stays_scroll(1, 20) == 0);
    CHECK(dw3_port_items_stays_scroll(7, 20) == 0);
    CHECK(dw3_port_items_stays_scroll(8, 20) == 5);
    CHECK(dw3_port_items_stays_scroll(18, 20) == 12); /* last page: 20 - 8 */
    CHECK(dw3_port_items_stays_scroll(19, 20) == 12);
    CHECK(dw3_port_items_stays_scroll(5, 8) == 0);
    CHECK(dw3_port_items_stays_scroll(9, 9) == 1);
    unsetenv("DW3_ITEMS_EQUIP_STAYS");
    g_file_stays = -1;
    CHECK(dw3_port_items_equip_stays_enabled() == 1);
    g_file_stays = 0;
    CHECK(dw3_port_items_equip_stays_enabled() == 0);
    g_file_stays = 1;
    setenv("DW3_ITEMS_EQUIP_STAYS", "0", 1);
    CHECK(dw3_port_items_equip_stays_enabled() == 0);
    setenv("DW3_ITEMS_EQUIP_STAYS", "1", 1);
    g_file_stays = 0;
    CHECK(dw3_port_items_equip_stays_enabled() == 1);
    unsetenv("DW3_ITEMS_EQUIP_STAYS");
    /* P6.34 phase 3: bag view. Filter cycle, slot fit, key, texts */
    CHECK(dw3_port_items_bag_next(0) == 1);
    CHECK(dw3_port_items_bag_next(1) == 2);
    CHECK(dw3_port_items_bag_next(2) == 3);
    CHECK(dw3_port_items_bag_next(3) == 0);
    CHECK(dw3_port_items_bag_next(-1) == 0);
    CHECK(dw3_port_items_bag_next(9) == 0);
    for (m = 1; m <= 8; m++) {
        int slot;

        for (slot = 0; slot < 6; slot++) {
            int want = (slot == 0 && m == DW3_EQUIP_HEAD) || (slot == 1 && m == DW3_EQUIP_BODY) ||
                       ((slot == 2 || slot == 3) && (m == 1 || m == 2 || m == 3 || m == 7)) ||
                       ((slot == 4 || slot == 5) && (m == 6 || m == 8));
            CHECK(dw3_port_items_bag_fits(slot, m) == want);
        }
    }
    CHECK(dw3_port_items_bag_fits(6, 4) == 0);
    CHECK(dw3_port_items_bag_fits(-1, 4) == 0);
    unsetenv("DW3_ITEMS_BAG_VIEW");
    g_file_bag = -1;
    CHECK(dw3_port_items_bag_view_enabled() == 1);
    g_file_bag = 0;
    CHECK(dw3_port_items_bag_view_enabled() == 0);
    setenv("DW3_ITEMS_BAG_VIEW", "1", 1);
    CHECK(dw3_port_items_bag_view_enabled() == 1);
    unsetenv("DW3_ITEMS_BAG_VIEW");
    {
        char buf[96];
        int lang;
        int f;

        /* every language and filter fits a text window (about 30 glyphs) and has no stray byte */
        for (lang = 2; lang <= 6; lang++) {
            for (f = 1; f <= 3; f++) {
                int n = dw3_port_items_bag_title(buf, sizeof(buf), lang, f, 12);

                CHECK(n > 3 && n < 60);
                CHECK(buf[n - 1] == 4 + 2 && buf[n - 2] == 4 + 1); /* "12" */
            }
            CHECK(dw3_port_items_bag_note(buf, sizeof(buf), lang, "AB") > 4);
        }
        CHECK(dw3_port_items_bag_title(buf, sizeof(buf), 2, 0, 3) == 0);
        CHECK(dw3_port_items_bag_title(buf, sizeof(buf), 2, 1, 105) > 0);
        CHECK(dw3_port_items_bag_title(buf, 1, 2, 1, 3) == 0);
        CHECK(dw3_port_items_bag_title(buf, 6, 2, 1, 3) == 5); /* clipped, never past the cap */
    }
    dw3_port_items_bag_viewed(3, 2, 2, 2);
    dw3_port_items_bag_move_begin();
    dw3_port_items_bag_moved(77, 4, 2, 5, 2);

    /* phase 4: the key, the keep rule, the log lines */
    unsetenv("DW3_ITEMS_USE_STAYS");
    g_file_use = -1;
    CHECK(dw3_port_items_use_stays_enabled() == 1);
    g_file_use = 0;
    CHECK(dw3_port_items_use_stays_enabled() == 0);
    setenv("DW3_ITEMS_USE_STAYS", "1", 1);
    CHECK(dw3_port_items_use_stays_enabled() == 1);
    setenv("DW3_ITEMS_USE_STAYS", "0", 1);
    CHECK(dw3_port_items_use_stays_enabled() == 0);
    unsetenv("DW3_ITEMS_USE_STAYS");
    CHECK(dw3_port_items_use_stays_keeps(0) == 0);
    CHECK(dw3_port_items_use_stays_keeps(1) == 0);
    CHECK(dw3_port_items_use_stays_keeps(2) == 1);
    CHECK(dw3_port_items_use_stays_keeps(99) == 1);
    dw3_port_items_use_done(5, 1, 0, 3, 2, 4, 1);
    dw3_port_items_use_done(5, 2, 1, 2, 2, 4, 1);
    dw3_port_items_use_left(5);
    dw3_port_items_use_left(5);
    dw3_port_items_use_done(5, 2, 1, 1, 0, 4, 0);

    if (g_failed) {
        return 1;
    }
    printf("OK: items menu policy\n");
    return 0;
}
