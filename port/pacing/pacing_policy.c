/* P6.18 pacing policy: pure functions, no game or host state.
 * docs/P6.18_PACING.md. */
#include "port_pacing.h"

int dw3_pacing_scale_rate(int mode, int rate, int *carry) {
    int sum;

    if (rate >= DW3_PACING_FORCED_RATE || rate <= 0) {
        return rate;
    }
    switch (mode) {
    case DW3_ENCOUNTER_OFF:
        return -1;
    case DW3_ENCOUNTER_HALF:
        sum = (carry != 0 ? *carry : 0) + rate;
        if (carry != 0) {
            *carry = sum & 1;
        }
        return sum >> 1;
    case DW3_ENCOUNTER_1_5X:
        /* 3/2 of the rate; the odd half step is carried like 0.5x does. */
        sum = (carry != 0 ? *carry : 0) + rate * 3;
        if (carry != 0) {
            *carry = sum & 1;
        }
        return sum >> 1;
    case DW3_ENCOUNTER_2X:
        return rate * 2;
    default:
        return rate;
    }
}

int dw3_pacing_scale_delay(int mode, int delay) {
    if (delay != DW3_PACING_DIALOGUE_DELAY) {
        return delay;
    }
    return mode == DW3_TEXT_FAST || mode == DW3_TEXT_INSTANT
               ? DW3_PACING_FAST_DELAY : delay;
}

int dw3_pacing_reveal_page(int mode, int type_delay, int finished, int shown) {
    return mode == DW3_TEXT_INSTANT && type_delay == DW3_PACING_FAST_DELAY &&
           finished == 0 && shown <= 2;
}

int dw3_pacing_scroll_action(int mode, int type_delay, int *waited) {
    if (mode != DW3_TEXT_INSTANT || type_delay != DW3_PACING_FAST_DELAY ||
        waited == 0) {
        return 0;
    }
    if (*waited < DW3_PACING_SCROLL_HOLD) {
        (*waited)++;
        return 1;
    }
    *waited = 0;
    return 2;
}
