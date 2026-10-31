/* D1: the Map buttons screen's logic. See port_map_buttons.h. */
#include "port_map_buttons.h"

#include "map_view_core.h"
#include "port_button_map.h"
#include "port_options.h"

#include <stdio.h>
#include <string.h>

#ifdef DW3_MAPBTN_TEST
#define MB_INFO(...) (fprintf(stderr, __VA_ARGS__), fputc('\n', stderr))
#else
#include <psyz/log.h>
#define MB_INFO(...) INFOF(__VA_ARGS__)
#endif

/* The table now in force: the file's values resolved the way the view does it
 * (a bad value takes its default, two view actions on one button put every
 * action back to the defaults), so the screen shows what the view will do. */
static void mb_load(MvBindings *b) {
    const char *vals[MV_ACT_COUNT];
    int a;

    for (a = 0; a < MV_ACT_COUNT; ++a) {
        vals[a] = dw3_port_options_file_str(dw3_mv_act_key(a));
    }
    (void)dw3_mv_bindings_resolve(b, vals, NULL, 0);
    (void)dw3_mv_field_bind_resolve(b, dw3_port_options_file_str(dw3_mv_act_key(MV_ACT_FIELD_OPEN)),
                                    NULL, 0);
}

int dw3_mapbtn_bit(int act) {
    MvBindings b;

    if (act < 0 || act >= MV_ACT_TOTAL) {
        return -1;
    }
    mb_load(&b);
    return b.bit[act];
}

int dw3_mapbtn_value(int row) {
    MvBindings b;
    int ps;
    int src;

    if (row < 0 || row >= MV_ACT_TOTAL) {
        return 0;
    }
    mb_load(&b);
    ps = dw3_mv_bind_ps_button(b.bit[row]);
    if (ps < 0) {
        return 10; /* Select: no console button of the map */
    }
    src = dw3_button_map_current()[ps];
    return ps | ((src + 1) << 4);
}

static int mb_store_all(const MvBindings *b) {
    int a;
    int rc = 0;

    for (a = 0; a < MV_ACT_TOTAL; ++a) {
        if (dw3_port_options_store_str(dw3_mv_act_key(a), dw3_mv_bind_word(b->bit[a])) != 0) {
            rc = -1;
        }
    }
    return rc;
}

/* 1 when `bit` may be bound to action `act`. */
static int mb_allowed(int act, int bit) {
    return act != MV_ACT_FIELD_OPEN || dw3_mv_field_bind_ok(bit);
}

int dw3_mapbtn_step(int row, int dir) {
    MvBindings b;
    const int n = dw3_mv_bindable_count();
    int cur = -1;
    int next;
    int old_bit;
    int new_bit = -1;
    int other;
    int i;

    if (row < 0 || row >= MV_ACT_TOTAL) {
        return 0;
    }
    mb_load(&b);
    old_bit = b.bit[row];
    for (i = 0; i < n; ++i) {
        if (dw3_mv_bindable_bit(i) == old_bit) {
            cur = i;
        }
    }
    if (cur < 0) {
        cur = 0;
    }
    next = cur;
    for (i = 0; i < n; ++i) {
        next = (next + (dir < 0 ? n - 1 : 1)) % n;
        if (mb_allowed(row, dw3_mv_bindable_bit(next))) {
            new_bit = dw3_mv_bindable_bit(next);
            break;
        }
    }
    if (new_bit < 0 || new_bit == old_bit) {
        return 0;
    }
    other = row < MV_ACT_COUNT ? dw3_mv_bindings_action(&b, new_bit) : -1;
    b.bit[row] = new_bit;
    if (other >= 0 && other != row) {
        b.bit[other] = old_bit; /* the swap: the six view actions always differ */
    }
    if (other >= 0 && other != row) {
        MB_INFO("dw3-options: map button %s %s->%s (swapped: %s %s->%s)",
                dw3_mv_act_key(row), dw3_mv_bind_word(old_bit), dw3_mv_bind_word(new_bit),
                dw3_mv_act_key(other), dw3_mv_bind_word(new_bit), dw3_mv_bind_word(old_bit));
    } else {
        MB_INFO("dw3-options: map button %s %s->%s", dw3_mv_act_key(row),
                dw3_mv_bind_word(old_bit), dw3_mv_bind_word(new_bit));
    }
    (void)mb_store_all(&b);
    return 1;
}

void dw3_mapbtn_reset(void) {
    MvBindings b;

    dw3_mv_bindings_default(&b);
    MB_INFO("dw3-options: map buttons reset to the defaults (open=%s zoom_out=%s zoom_in=%s "
            "centre=%s scaler_prev=%s scaler_next=%s field_open=%s)",
            dw3_mv_bind_word(b.bit[0]), dw3_mv_bind_word(b.bit[1]), dw3_mv_bind_word(b.bit[2]),
            dw3_mv_bind_word(b.bit[3]), dw3_mv_bind_word(b.bit[4]), dw3_mv_bind_word(b.bit[5]),
            dw3_mv_bind_word(b.bit[6]));
    (void)mb_store_all(&b);
}
