/* EU language select (CNTY_SEL): Start or the confirm button picks the language
 * (patch 0187). The pad word the game reads is already mapped by the host, so
 * "Cross" here is the PS-side confirm bit and follows any future button remap
 * (docs/CONTROLS_BUNDLE.md). */
#include "port_lang_select.h"

#ifdef DW3_LANG_SELECT_TEST
#include <stdio.h>
#define LS_LOG(...)                    \
    do {                               \
        fprintf(stderr, __VA_ARGS__);  \
        fputc('\n', stderr);           \
    } while (0)
#else
#include <psyz/log.h>
/* psyz log, not stderr: the line is the check in sdmc:/switch/dw3/dw3.log. */
#define LS_LOG(...) INFOF(__VA_ARGS__)
#endif

int dw3_port_lang_select_confirm(int start_pressed, int cross_pressed) {
    if (start_pressed) {
        return DW3_LANG_CONFIRM_START;
    }
    if (cross_pressed) {
        return DW3_LANG_CONFIRM_CROSS;
    }
    return DW3_LANG_CONFIRM_NONE;
}

void dw3_port_lang_select_chosen(int language, int how) {
    LS_LOG("dw3-lang-select: language %d chosen by %s", language,
           how == DW3_LANG_CONFIRM_CROSS ? "Cross (confirm)" : "Start");
}
