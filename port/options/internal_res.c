/* P6.11 internal resolution: the policy behind the Options rows Internal
 * resolution and Native res while turbo (docs/P6.11_INTERNAL_RES.md). Platform
 * independent. The host draws; this decides at which multiplier, once per game
 * frame, and says so in the log:
 *
 *   dw3-res: set 2x (handheld)
 *   dw3-res: clamp 3x -> 2x (handheld max)
 *   dw3-res: turbo on -> 1x
 *   dw3-res: turbo off -> 2x
 *
 * The multiplier changes at the frame boundary only (the turbo poll runs before
 * the frame's first draw), and the host keeps the targets of both multipliers
 * allocated, so the switch is a variable flip and nothing else. */
#include "port_internal_res.h"

#include "port_options.h"
#include "port_turbo.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef DW3_PORT_INTERNAL_RES_TEST
#define RES_LOG(...) fprintf(stderr, __VA_ARGS__)
#else
#include <psyz/log.h>
/* psyz_log, not stderr: on the Switch the log file is the check. */
#define RES_LOG(...) INFOF(__VA_ARGS__)
#endif

static int g_active;       /* last handed to the host, 0 = nothing yet */
static int g_reserve;
static int g_target_seen;  /* last target logged, 0 = none */
static int g_clamp_key;    /* the clamp the log line was last printed for */
static char g_mode_seen[16];
static int g_turbo_on;     /* fast-forward is running (speed >= 2) */

static int res_clamp(int n) {
    return n < 1 ? 1 : (n > DW3_INTERNAL_RES_MAX ? DW3_INTERNAL_RES_MAX : n);
}

/* What the row asks for (1..3) and what the machine allows. */
static int res_requested(void) {
    return res_clamp(1 + dw3_port_options_level(DW3_PORT_OPT_INTERNAL_RES));
}

static int res_limit(void) {
    return res_clamp(dw3_platform_max_internal_res());
}

int dw3_port_internal_res_target(void) {
    const int want = res_requested();
    const int limit = res_limit();

    return want > limit ? limit : want;
}

static int res_native_turbo(void) {
    return dw3_port_options_level(DW3_PORT_OPT_INTERNAL_RES_TURBO) != 0;
}

int dw3_port_internal_res_active(void) {
    return g_turbo_on && res_native_turbo() ? 1 : dw3_port_internal_res_target();
}

/* Pushes the state to the host when it changed, and logs what the player and a
 * test need to see: once per event, never per frame. */
static void res_apply(void) {
    const int want = res_requested();
    const int target = dw3_port_internal_res_target();
    const char *mode = dw3_platform_res_mode();
    const int native = res_native_turbo();
    const int active = g_turbo_on && native ? 1 : target;
    /* The Nx targets stay allocated only while a switch to 1x can happen. */
    const int reserve = target > 1 && native ? target : 1;

    if (target != g_target_seen || strcmp(mode, g_mode_seen) != 0) {
        const int first = g_target_seen == 0;

        g_target_seen = target;
        snprintf(g_mode_seen, sizeof(g_mode_seen), "%s", mode);
        if (!first || target > 1) {
            RES_LOG("dw3-res: set %dx (%s)", target, mode);
        }
    }
    if (want > target) {
        /* once per (request, limit, mode), not per frame */
        const int key = want * 100 + target * 10 + (int)(unsigned char)mode[0];

        if (key != g_clamp_key) {
            g_clamp_key = key;
            RES_LOG("dw3-res: clamp %dx -> %dx (%s max)", want, target, mode);
        }
    } else {
        g_clamp_key = 0;
    }
    if (active != g_active || reserve != g_reserve) {
        g_active = active;
        g_reserve = reserve;
        dw3_platform_apply_internal_res(active, reserve);
    }
}

void dw3_port_internal_res_sync(void) {
    res_apply();
}

void dw3_port_internal_res_boot(void) {
    const char *v = getenv("DW3_INTERNAL_RES");
    int n = 0;

    if (v == NULL || v[0] == '\0') {
        /* The PC experiment's variable, kept for one release (it took 3 only). */
        v = getenv("DW3_PC_INTERNAL_RES");
        if (v != NULL && v[0] != '\0') {
            RES_LOG("dw3-res: DW3_PC_INTERNAL_RES is now DW3_INTERNAL_RES "
                    "(1..3) and the Options row Internal resolution");
        }
    }
    if (v != NULL && v[0] != '\0') {
        char *end = NULL;
        const long x = strtol(v, &end, 10);

        if (end != v && *end == '\0' && x >= 1 && x <= DW3_INTERNAL_RES_MAX) {
            n = (int)x;
        } else {
            RES_LOG("dw3-res: DW3_INTERNAL_RES=%s is not 1, 2 or 3; ignored", v);
        }
    }
    if (n != 0) {
        dw3_port_options_apply_env(DW3_PORT_OPT_INTERNAL_RES, n - 1);
    }
    res_apply();
}

void dw3_port_internal_res_poll(void) {
    const int turbo = dw3_port_turbo_speed() >= DW3_TURBO_SPEED_MIN;
    const int native = res_native_turbo();
    int skip;

    if (turbo != g_turbo_on) {
        g_turbo_on = turbo;
        /* The edge of fast-forward matters when it changes the multiplier. */
        if (native && dw3_port_internal_res_target() > 1) {
            if (turbo) {
                RES_LOG("dw3-res: turbo on -> 1x");
            } else {
                RES_LOG("dw3-res: turbo off -> %dx", dw3_port_internal_res_target());
            }
        }
    }
    res_apply();
    /* Frames turbo will not show draw no mesh (a host that cannot skip ignores
     * it). Only under Native res while turbo; hold, the menus and the title
     * run at 1x speed, so they never skip. */
    skip = turbo && native && dw3_port_turbo_frame_will_skip();
    dw3_platform_set_mesh_skip(skip);
}
