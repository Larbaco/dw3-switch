/* P6.1 EU language select confirm (patch 0187), no game, no disc. */
#include "port_lang_select.h"

#include <stdio.h>

static int g_failed;
#define CHECK(cond)                                                       \
    do {                                                                  \
        if (!(cond)) {                                                    \
            printf("FAIL line %d: %s\n", __LINE__, #cond);                \
            g_failed++;                                                   \
        }                                                                 \
    } while (0)

int main(void) {
    CHECK(dw3_port_lang_select_confirm(0, 0) == DW3_LANG_CONFIRM_NONE);
    CHECK(dw3_port_lang_select_confirm(1, 0) == DW3_LANG_CONFIRM_START);
    CHECK(dw3_port_lang_select_confirm(0, 1) == DW3_LANG_CONFIRM_CROSS);
    /* Start stays the original button when both arrive together */
    CHECK(dw3_port_lang_select_confirm(1, 1) == DW3_LANG_CONFIRM_START);
    dw3_port_lang_select_chosen(3, DW3_LANG_CONFIRM_CROSS);
    dw3_port_lang_select_chosen(2, DW3_LANG_CONFIRM_START);
    if (g_failed) {
        return 1;
    }
    printf("OK: language select confirm\n");
    return 0;
}
