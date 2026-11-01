/* P6.24: the running region; see dw3_region_rt.h. */
#include "dw3_region_rt.h"
#include "dw3_disc.h"

#include <stdio.h>
#include <string.h>

#if defined(DW3_BOTH) && DW3_BOTH
#include "dw3_game_api.h"

extern const Dw3GameApi us_dw3_game_api;
extern const Dw3GameApi eu_dw3_game_api;

/* Before a region is chosen (the picker, the setup screen, an early failure, a
 * crash report) there is no game copy. The host still calls through the table in
 * a few places that run then (the backend's HUD, the crash handler), so the
 * table of "no game" is real and harmless: calls return 0 and do nothing. */
static int none_game_main(void) { return -1; }
static int none_load_boot_tim(const char *p) { (void)p; return -1; }
static void none_void(void) {}
static int none_int(void) { return 0; }
static int none_int_i(int v) { (void)v; return 0; }
static int none_int_s(const char *s) { (void)s; return -1; }
static unsigned none_mask(int v) { (void)v; return 0; }
static unsigned none_word(unsigned w) { return w; }
static void none_void_i(int v) { (void)v; }
static void none_void_s(const char *s) { (void)s; }
static void none_void_u(unsigned v) { (void)v; }
static void none_draw(struct Dw3MapViewDraw *d) { (void)d; }
static int32_t none_file(void) { return 0; }
static const char *none_name(int32_t id) { (void)id; return NULL; }
static const Dw3GameApi g_no_game = {
    .abi = DW3_GAME_API_ABI,
    .region = DW3_REGION_NONE,
    .game_main = none_game_main,
    .host_load_boot_tim = none_load_boot_tim,
    .overlay_register_title = none_void,
    .overlay_current_mode_file = none_file,
    .overlay_current_sub_file = none_file,
    .overlay_file_name = none_name,
    .overlay_set_strict = none_void_i,
    .map_view_init = none_void_s,
    .map_view_precompute_run = none_void,
    .map_view_precompute_cancel = none_void,
    .map_view_set_zoom_stick = none_void_i,
    .map_view_get_draw = none_draw,
    .map_view_tex_taken = none_void_u,
    .port_options_persist_init = none_int_s,
    .port_options_flush = none_void,
    .port_options_is_open = none_int,
    .port_options_in_game = none_int,
    .port_battle_options_holding = none_int,
    .port_options_level = none_int_i,
    .port_options_locked_mask_for = none_mask,
    .port_options_note_shown = none_void,
    .port_options_bg_phase = none_int,
    .port_screen_fade_level = none_int,
    .port_turbo_speed = none_int,
    .port_turbo_present_due = none_int,
    .port_turbo_rate_x2 = none_int,
    .button_map_apply = none_word,
};

static Dw3Region g_region = DW3_REGION_NONE;
static const Dw3GameApi *g_api = &g_no_game;

Dw3Region dw3_region(void) { return g_region; }

const Dw3GameApi *dw3_game(void) { return g_api; }

int dw3_region_select(Dw3Region r, const char *source) {
    const Dw3GameApi *api;

    if (r == DW3_REGION_US) {
        api = &us_dw3_game_api;
    } else if (r == DW3_REGION_EU) {
        api = &eu_dw3_game_api;
    } else {
        fprintf(stderr, "dw3-region: no region to select\n");
        return -1;
    }
    if (api->abi != DW3_GAME_API_ABI || api->region != r) {
        fprintf(stderr, "dw3-region: %s game copy is inconsistent (abi %u region %s)\n",
                dw3_region_name(r), api->abi, dw3_region_name(api->region));
        return -1;
    }
    if (g_region != DW3_REGION_NONE) {
        if (g_region == r) {
            return 0;
        }
        fprintf(stderr, "dw3-region: already running %s, refusing %s\n",
                dw3_region_name(g_region), dw3_region_name(r));
        return -1;
    }
    g_region = r;
    g_api = api;
    fprintf(stderr, "dw3-region: running %s (%s), one image with both game copies\n",
            dw3_region_name(r), source != NULL ? source : "?");
    return 0;
}
#endif /* DW3_BOTH */

/* "<root>/eu" or "<root>/us" (trailing slashes ignored). */
static Dw3Region region_from_name(const char *root) {
    size_t n = strlen(root);

    while (n > 0 && root[n - 1] == '/') {
        n--;
    }
    if (n >= 2 && (root[n - 2] == 'u' || root[n - 2] == 'U') &&
        (root[n - 1] == 's' || root[n - 1] == 'S') &&
        (n == 2 || root[n - 3] == '/')) {
        return DW3_REGION_US;
    }
    if (n >= 2 && (root[n - 2] == 'e' || root[n - 2] == 'E') &&
        (root[n - 1] == 'u' || root[n - 1] == 'U') &&
        (n == 2 || root[n - 3] == '/')) {
        return DW3_REGION_EU;
    }
    return DW3_REGION_NONE;
}

int dw3_region_resolve(const Dw3RegionRequest *req, Dw3RegionChoice *out) {
    const char *base = (req->base != NULL && req->base[0] != '\0') ? req->base : "runtime";
    Dw3Region pref = DW3_REGION_NONE;
    Dw3Region last = DW3_REGION_NONE;

    memset(out, 0, sizeof(*out));
    if (req->flag != NULL && req->flag[0] != '\0') {
        out->region = dw3_region_parse(req->flag);
        out->source = "flag";
        return out->region == DW3_REGION_NONE ? -1 : 0;
    }
    if (req->env != NULL && req->env[0] != '\0') {
        out->region = dw3_region_parse(req->env);
        out->source = "env";
        return out->region == DW3_REGION_NONE ? -1 : 0;
    }
    if (req->root != NULL && req->root[0] != '\0') {
        int us = dw3_region_runtime_complete(req->root, DW3_REGION_US);
        int eu = dw3_region_runtime_complete(req->root, DW3_REGION_EU);

        if (us != eu) {
            out->region = us ? DW3_REGION_US : DW3_REGION_EU;
            out->source = "runtime-root";
            return 0;
        }
        out->region = region_from_name(req->root);
        if (out->region != DW3_REGION_NONE) {
            out->source = "root-name";
            return 0;
        }
        out->region = DW3_REGION_US;
        out->source = "default";
        return 0;
    }
    dw3_region_probe(base, &out->probe);
    {
        /* a disc image for a region without a complete tree counts as present
         * (issue #322), as in the Switch boot flow (dw3_region_boot.c) */
        Dw3DiscScan scan;

        dw3_disc_scan(base, &out->probe, &scan);
        if (scan.hit[0].found) {
            out->probe.has_us = 1;
        }
        if (scan.hit[1].found) {
            out->probe.has_eu = 1;
        }
    }
    if (out->probe.has_us != out->probe.has_eu) {
        out->region = out->probe.has_us ? DW3_REGION_US : DW3_REGION_EU;
        out->source = "probe-one";
        return 0;
    }
    if (out->probe.has_us && out->probe.has_eu) {
        dw3_region_cfg_read(req->cfg_dir != NULL ? req->cfg_dir : base, &pref, &last);
        if (pref != DW3_REGION_NONE) {
            out->region = pref;
            out->source = "probe-pref";
        } else if (last != DW3_REGION_NONE) {
            out->region = last;
            out->source = "probe-last";
        } else {
            out->region = DW3_REGION_US;
            out->source = "default";
        }
        return 0;
    }
    out->region = DW3_REGION_US;
    out->source = "default";
    return 0;
}
