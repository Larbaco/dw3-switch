/* P10.4: PadSetAct -> HID HD rumble (docs/P10.4_RUMBLE.md).
 *
 * The decomp's engine/pad drives `PAD.act[pad][0..1]` — the DualShock actuator
 * amplitudes — through libpad's PadSetAct, once pollPadState has seen
 * PadStateStable and aligned them. PSY-Z has no Psyz_PadsSetAct of its own,
 * so port/psyz_missing/psyz_pad_host.c owns __wrap_PadSetAct and forwards the
 * 2-byte array to this platform hook (weak default = idle on PC hosts).
 *
 * Port numbers are the decomp's libpad addressing: pair index in bits 4-7
 * (pad 1 = 0x0*, pad 2 = 0x10), so this backend answers pad 1 only.
 *
 * Translation (motor->band order unverified until the P10.4 hardware listen):
 *   act[0] (PS1 small motor) -> Switch high band, amp = act[0]/255, 320 Hz
 *   act[1] (PS1 big motor)   -> Switch low band,  amp = act[1]/255, 160 Hz
 * The same value goes to BOTH device handles of the selected route: the first
 * handle is the left side, the second the right (switch-examples hid/vibration).
 *
 * Routes, keyed by the libnx style set of the last padUpdate and initialized
 * on the first send for that style:
 *   NpadHandheld -> HidNpadIdType_Handheld (rails pads / Switch Lite)
 *   NpadFullKey  -> HidNpadIdType_No1      (docked Pro Controller)
 *   NpadJoyDual  -> HidNpadIdType_No1      (Joy-Con pair off the rails)
 *
 * Hardware-unverified: written against the libnx hid.h contract, gated on
 * hidIsVibrationPermitted, and silent when a device refuses the init.
 * DW3_NX_RUMBLE=0 is the manual kill-switch. A one-pulse boot diagnostic is
 * compiled in with -DDW3_NX_RUMBLE_SELFTEST.
 */
#include "nx.h"

#include <stdlib.h>
#include <string.h>

#include <psyz.h>

#include <switch.h>

/* One HidVibrationDeviceHandle per side (left / right). */
#define DW3_RUMBLE_SIDES 2

/* Style-routed device sets (s_route_ids / s_route_styles below). */
enum {
    RUMBLE_ROUTE_HANDHELD = 0,
    RUMBLE_ROUTE_FULLKEY = 1,
    RUMBLE_ROUTE_JOYDUAL = 2,
    RUMBLE_ROUTES = 3
};

/* PS1-style motor-stop frequencies: a muted band with its default frequency
 * is the documented stop shape (switch-examples hid/vibration). */
#define DW3_RUMBLE_FREQ_LOW 160.0f
#define DW3_RUMBLE_FREQ_HIGH 320.0f

static const HidNpadIdType s_route_ids[RUMBLE_ROUTES] = {
    HidNpadIdType_Handheld, HidNpadIdType_No1, HidNpadIdType_No1,
};
static const HidNpadStyleTag s_route_styles[RUMBLE_ROUTES] = {
    HidNpadStyleTag_NpadHandheld, HidNpadStyleTag_NpadFullKey,
    HidNpadStyleTag_NpadJoyDual,
};

static HidVibrationDeviceHandle s_handles[RUMBLE_ROUTES][DW3_RUMBLE_SIDES];
static bool s_route_ok[RUMBLE_ROUTES];
static bool s_route_tried[RUMBLE_ROUTES];
static bool s_inited;
static bool s_dead;
static bool s_permitted_flag; /* hidIsVibrationPermitted result */
static bool s_logged_no_style;
static bool s_logged_send_fail;
static bool s_logged_first_send[RUMBLE_ROUTES];
static unsigned s_last_sent[RUMBLE_ROUTES]; /* last act bytes sent (dedup) */

static bool rumble_env_dead(void) {
    const char *env = getenv("DW3_NX_RUMBLE");

    return env != NULL && strcmp(env, "0") == 0;
}

/* hidIsVibrationPermitted, logged once. */
static bool rumble_never_permitted(void) {
    Result rc;

    rc = hidIsVibrationPermitted(&s_permitted_flag);
    return R_FAILED(rc) || !s_permitted_flag;
}

/* Acquire the vibration device handles of one style route (first send). */
static bool rumble_route_init(unsigned route) {
    Result rc;

    if (s_route_tried[route]) {
        return s_route_ok[route];
    }
    s_route_tried[route] = 1;
    rc = hidInitializeVibrationDevices(s_handles[route], DW3_RUMBLE_SIDES,
                                       s_route_ids[route],
                                       s_route_styles[route]);
    s_route_ok[route] = R_SUCCEEDED(rc);
    if (!s_route_ok[route]) {
        psyz_log(1, "nx_rumble.c", __LINE__, __func__,
                 "route %u (style=0x%x) rc=0x%08x", route,
                 (unsigned)s_route_styles[route], (unsigned)rc);
    }
    return s_route_ok[route];
}

/* One hidSendVibrationValue for both sides; first send per route is logged. */
static void rumble_send(unsigned route, const unsigned char act[2]) {
    HidVibrationValue value;
    bool ok0;
    bool ok1;

    memset(&value, 0, sizeof(value));
    value.freq_low = DW3_RUMBLE_FREQ_LOW;
    value.freq_high = DW3_RUMBLE_FREQ_HIGH;
    /* act[0]: PS1 small motor -> high band; act[1]: PS1 big motor -> low
     * band. Band order is unverified until the P10.4 hardware listen. */
    value.amp_high = (float)act[0] / 255.0f;
    value.amp_low = (float)act[1] / 255.0f;

    ok0 = R_SUCCEEDED(hidSendVibrationValue(s_handles[route][0], &value));
    ok1 = R_SUCCEEDED(hidSendVibrationValue(s_handles[route][1], &value));
    if (!ok0 || !ok1) {
        if (!s_logged_send_fail) {
            s_logged_send_fail = 1;
            psyz_log(1, "nx_rumble.c", __LINE__, __func__,
                     "hidSendVibrationValue route=%u side0=%d side1=%d "
                     "act=0x%02x,0x%02x",
                     route, (int)ok0, (int)ok1, (unsigned)act[0],
                     (unsigned)act[1]);
        }
        return;
    }
    if (!s_logged_first_send[route]) {
        s_logged_first_send[route] = 1;
        psyz_log(1, "nx_rumble.c", __LINE__, __func__,
                 "rumble act=0x%02x,0x%02x -> route=%u (style=0x%x)",
                 (unsigned)act[0], (unsigned)act[1], route,
                 (unsigned)s_route_styles[route]);
    }
}

/* Style set -> route. Handheld wins over the No1 styles when both report. */
static unsigned rumble_route_of(u32 style) {
    if (style & HidNpadStyleTag_NpadHandheld) {
        return RUMBLE_ROUTE_HANDHELD;
    }
    if (style & HidNpadStyleTag_NpadFullKey) {
        return RUMBLE_ROUTE_FULLKEY;
    }
    return RUMBLE_ROUTE_JOYDUAL;
}

void Psyz_PadsSetAct(int port, const unsigned char *act, int len) {
    unsigned route;
    unsigned key;
    u32 style;

    /* Pair index in bits 4-7: pad 1 (0x0*) vibrates, pad 2 (0x10) does not. */
    if (((port >> 4) & 1) != 0) {
        return;
    }
    if (act == NULL || len < 2) {
        return;
    }
    if (!s_inited) {
        s_inited = 1;
        s_dead = rumble_env_dead() || rumble_never_permitted();
        if (s_dead) {
            psyz_log(1, "nx_rumble.c", __LINE__, __func__,
                     "rumble dead (env=%d permitted=%d)",
                     rumble_env_dead() ? 1 : 0,
                     s_permitted_flag ? 1 : 0);
        }
    }
    if (s_dead) {
        return;
    }

    style = dw3_nx_input_style_set();
    if (style == 0) {
        /* The pad source has not reported any style this run. */
        if (!s_logged_no_style) {
            s_logged_no_style = 1;
            psyz_log(1, "nx_rumble.c", __LINE__, __func__,
                     "no pad style yet; act ignored");
        }
        return;
    }

    route = rumble_route_of(style);
    if (!rumble_route_init(route)) {
        /* Init failed once for this style; stay quiet until it changes. */
        return;
    }

    /* Dedup: updateVibration re-sends the same bytes every vsync while a
     * motor timer runs (src/engine/pad/port.c). Only changes reach HID. */
    key = ((unsigned)act[0] << 8) | (unsigned)act[1];
    if (key != s_last_sent[route]) {
        rumble_send(route, act);
        s_last_sent[route] = key;
    }
}

#ifdef DW3_NX_RUMBLE_SELFTEST
/* Build-time diagnostic (-DDW3_NX_RUMBLE_SELFTEST): one short pulse on every
 * route that acquires devices, before dw3_game_main. Proves the device -> HID
 * path without needing the in-game SHOCKTST stage; pair with the P10.4 smoke
 * plan (docs/P10.4_RUMBLE.md). */
void dw3_nx_rumble_boot_selftest(void) {
    static const unsigned char pulse[2] = {0x40, 0x80};
    static const unsigned char stop[2] = {0x00, 0x00};
    unsigned route;

    if (rumble_env_dead()) {
        psyz_log(1, "nx_rumble.c", __LINE__, __func__,
                 "selftest skipped: DW3_NX_RUMBLE=0");
        return;
    }
    if (rumble_never_permitted()) {
        psyz_log(1, "nx_rumble.c", __LINE__, __func__,
                 "selftest skipped: vibration not permitted");
        return;
    }
    for (route = 0; route < RUMBLE_ROUTES; route++) {
        (void)rumble_route_init(route);
    }
    for (route = 0; route < RUMBLE_ROUTES; route++) {
        if (s_route_ok[route]) {
            rumble_send(route, pulse);
        }
    }
    svcSleepThread(400ULL * 1000ULL * 1000ULL);
    for (route = 0; route < RUMBLE_ROUTES; route++) {
        if (s_route_ok[route]) {
            rumble_send(route, stop);
        }
    }
    psyz_log(1, "nx_rumble.c", __LINE__, __func__,
             "boot selftest pulse done (DW3_NX_RUMBLE_SELFTEST)");
}
#endif /* DW3_NX_RUMBLE_SELFTEST */
