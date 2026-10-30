/* P6.32: the new full-screen map screen. See map_view.h. */
#include "engine/gfx.h"

#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>

#include "cdfs.h"
#include "map_view.h"
#include "map_view_core.h"
#include "map_view_help.h"
#include "mod_pack.h"
#include "engine/pad.h"
#include "port_button_map.h"
#include "port_glyphs.h"
#include "port_options.h"
#include "port_turbo.h"
#include "port_widescreen.h"

/* PSY-Z GPU backend's VRAM readback (private to its renderer TU). */
extern void Draw_StoreImage(RECT *rect, u_long *pixels);
extern const uint8_t dw3_port_font8x8[95][8];

#if defined(VERSION_EU) && VERSION_EU
#define MV_REGION "eu"
#define MV_FILE_BASE 0x799 /* FILE_STATUS_BG: sheets; +1 images; +2, +3 second half */
#else
#define MV_REGION "us"
#define MV_FILE_BASE 0x78A
#endif

/* All view maths runs in a virtual 16:9 viewport; the hosts map the result onto
 * the real picture. */
enum { VW = 1280, VH = 720 };

/* P51 item 2: the view opens at 1.5x (MV_ZOOM_TABLE step 3), on the current
 * area. 1.0 needs a third of the map's height in panning and draws the towns
 * small; 2.0 shows only 56 % of the width at 16:9. */
enum { MAP_OPEN_STEP = 3 };

typedef struct MapViewState {
    int active;
    int half;
    int scaler;
    int show_hud;        /* the top and bottom bars (P51 item 2: off at open) */
    int show_name;       /* the game's name box over a location (always on) */
    int built_ms;        /* last CPU build, -1 = from the cache, 0 = none */
    int source;          /* 0 vram, 1 cache, 2 disc */
    MvImage base;        /* the map and its reveal layer, kept while open */
    MvImage mod;         /* the mod pack's map with its reveal layer (scale x
                          * base), kept while open; rgba NULL = no pack */
    int mod_scale;
    char mod_id[64];
    int pieces_in;       /* pieces the game sent */
    int pieces_drawn;    /* pieces composed into base */
    int towns_drawn, areas_drawn;
    int zoom_src;        /* 0 none yet, 1 stick, 2 button */
    unsigned zoom_stick_frames, zoom_button_presses;
    uint32_t base_reveal_last;
    MvBox box;
    MvImage tex;         /* waiting for the host to take it */
    unsigned tex_gen;
    int tex_linear;
    MvView view;
    /* phase 3: zoom about the cursor, pan limits, cursor speed, the backdrop */
    int pan_flags;              /* MV_PAN_* the last clamp reported (logged on change) */
    int pan_acc;                /* MV_PAN_* the clamps of this frame have reported so far */
    float pan_wx, pan_wy;       /* the centre the clamp that held the view had asked for */
    const char *pan_why;        /* and what asked */
    int cover_full;             /* the map rectangle covered the whole picture at the last check, -1 unknown */
    uint8_t backdrop[DW3_MV_BACKDROP_W * DW3_MV_BACKDROP_H * 4];
    unsigned backdrop_gen;
    int backdrop_ok;
    float acc_x, acc_y;         /* sub-pixel cursor movement carried between frames */
    int pan_src;                /* 0 idle, 1 d-pad, 2 left stick */
    int pan_frames;             /* frames the cursor has been moving in this go */
    int pan_level;              /* floor(speed) last logged */
    float pan_peak;
    int pan_from_x, pan_from_y;
    unsigned pan_goes;          /* cursor goes this open */
    unsigned centre_presses;
    unsigned zoom_events;
    unsigned hud_gen;
    uint8_t *hud;
    int home_x, home_y;  /* image pixels */
    int nspots;
    int spot_x[47], spot_y[47];
    unsigned frame;
    /* the game's own sprites on the map (bank_build) */
    MvImage bank[DW3_MV_BANK_N];
    int bank_ok;
    unsigned bank_gen;
    /* the cursor, in sprite space (the game's map space), and the locations */
    int home_sx, home_sy;       /* the home mark, sprite space */
    int home_spot;              /* the spot id of the current area, 0 = none */
    int all_x[47], all_y[47];   /* every spot, sprite space */
    int vis[47];                /* vis[i]: spot i is visited (i >= 1) */
    int cur_x, cur_y;
    int hover;                  /* spot under the cursor, 0 = none */
    int hold_x, hold_y;         /* frames each axis has been held */
    int was_moving;
    int cursor_anim, home_anim;
    int hover_x;                /* the cursor's x when it got on the location */
    int home_alt;               /* the balloon faces the other way (EU) */
    int name_x0, name_y0, name_x1, name_y1; /* the name box frame, logical */
    MvNameBox name_nb;          /* the latched placement (core) */
    int name_bottom;            /* the box is at the bottom (cursor in the top half) */
    int hole;                   /* the game's name box is shown */
    /* the CPU scalers on a worker thread */
    int req_scaler;             /* the scaler asked for (shown on the HUD) */
    struct MvJob *job;
    int pending_scaler;         /* -1, or the scaler to start when the job ends */
    unsigned job_id;
    /* the game's own Map screen underneath (phase 1: "returns unchanged") */
    int game_cx, game_cy, game_sx, game_sy; /* its cursor and scroll at the open */
    int resume_wait;            /* inactive frames to see after the close, 0 = none */
    unsigned r3_ignored;        /* R3 presses swallowed while open */
    /* phase 2: the area names and the strips the PS1 cursor cannot reach */
    int name_key;               /* the location the last name line was for, 0 = none */
    uint32_t name_hash;         /* and the hash of the text it showed */
    int strip;                  /* strip bits the cursor is in (dw3_mv_point_strip) */
    unsigned names_shown;       /* name lines logged this open */
    unsigned name_bad;          /* mismatch with the view's hit test, text not the table's, overflow */
    unsigned char named[47];    /* location i has been named by the game's own window */
    /* the test tour (DW3_MAP_TOUR=<frames per step>, PC only) */
    int tour_period, tour_n;
    unsigned tour_start;
    struct { int x, y, id, kind; } tour[160]; /* kind 1 location, 2 strip probe */
} MapViewState;

static MapViewState s;
static void log_pan(int flags, float want_cx, float want_cy, const char *why);
static void pan_note(int flags, float want_cx, float want_cy, const char *why);
static void pan_flush(void);
static char g_cache_dir[480];
/* <config dir>/mods on the PC, sdmc:/switch/dw3/mods on the Switch */
static char g_mods_dir[480];
/* The scaler the next open starts with: nearest (the owner's choice after the
 * OLED look of preview.45), then whatever the player left it on. */
static int g_scaler = MV_SCALER_NEAREST;
/* patch 0147: the marker list and the area names' file, as the game last gave them */
static Dw3MapViewData g_data;
static int g_data_ok;

/* EU LANGUAGE values (the disc's text copies), the US text is language 1 */
static const char *language_name(int language) {
    switch (language) {
    case 1: return "en (US text)";
    case 2: return "en";
    case 3: return "fr";
    case 4: return "it";
    case 5: return "de";
    case 6: return "es";
    default: return "?";
    }
}

/* 1: the cursor and the balloon keep the size they have at zoom 1.0 while the
 * map zooms (positions follow the map). DW3_MAP_SPRITES_FIXED=0 gives the old
 * look (they scale with the map), for comparisons. */
static int sprites_fixed(void) {
    const char *v = getenv("DW3_MAP_SPRITES_FIXED");

    return !(v && v[0] == '0');
}
/* The right stick's vertical axis, -127..127, up positive, with the dead zone
 * taken out; fed by the hosts every frame (dw3_map_view_set_zoom_stick). */
static int g_zoom_stick;
/* Smooth zoom speed at full tilt: octaves per second (1.0 to 3.0 is 1.58). */
#define MV_STICK_OCTAVES_PER_S 1.5f
#define MV_STICK_DEADZONE 20

static double now_ms(void) {
    struct timespec ts;

    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec * 1000.0 + (double)ts.tv_nsec / 1.0e6;
}

/* --- availability --------------------------------------------------------*/

#if defined(DW3_TITLE_NATIVE_OVERLAY) && \
    ((defined(VERSION_US) && VERSION_US) || (defined(VERSION_EU) && VERSION_EU))
#define MV_HOST_OK 1
#else
#define MV_HOST_OK 0
#endif

/* The port option: the `map_screen` row of the Options screen's Display group
 * (registry row DW3_PORT_OPT_MAP_SCREEN, key `map_screen` in port_options.cfg,
 * which the preview builds already used as a file-only key). Off by default since
 * P6.32 phase 1. It is read where it is used, so the row works at once, also in
 * game. */
#define MV_OPTION_KEY "map_screen"
#define MV_OPTION_DEFAULT 0

static int map_view_option(void) {
    return dw3_port_options_level(DW3_PORT_OPT_MAP_SCREEN) != 0;
}

static int map_view_available(void) {
    if (!MV_HOST_OK) return 0;
    if (map_view_option() == 0) return 0;
    /* owner decision 8: widescreen only (4:3 has no new screen) */
    return dw3_port_widescreen_menu_shift() > 0;
}

/* --- button bindings (phase 4) ---------------------------------------------
 * The table of map_view_core.h, read from the file-only keys of port_options.cfg
 * the first time the view is asked for anything (the file is loaded by then). A
 * binding names a logical PS button; what the console button is comes from the
 * P6.12 / P9.8 map in force, which the log line of each open (and of each change
 * of the map) says, so "which physical button opens the map" is in dw3.log. */
static MvBindings g_bind;
static int g_bind_loaded;
static char g_bind_map[DW3_BTN_COUNT + 1];  /* the map the last line was logged for */
static int g_bind_logged;
static int g_option_at_boot = -1;           /* map_screen when dw3_map_view_init ran */
static int g_option_late_logged;

#define BIND(act) (g_bind.bit[act])

static void bind_load(void) {
    const char *vals[MV_ACT_COUNT];
    char why[512];
    int a, rc;

    if (g_bind_loaded) return;
    g_bind_loaded = 1;
    for (a = 0; a < MV_ACT_COUNT; ++a) vals[a] = dw3_port_options_file_str(dw3_mv_act_key(a));
    rc = dw3_mv_bindings_resolve(&g_bind, vals, why, sizeof(why));
    if (rc != 0) {
        fprintf(stderr, "dw3-map: BINDING PROBLEM (%s): %s\n",
                rc == 2 ? "conflict, all defaults" : "bad value, that one at its default", why);
    }
    fprintf(stderr, "dw3-map: bindings read from port_options.cfg:");
    for (a = 0; a < MV_ACT_COUNT; ++a) {
        fprintf(stderr, " %s=%s (%s)", dw3_mv_act_key(a), dw3_mv_bind_bit_name(g_bind.bit[a]),
                g_bind.from_file[a] ? "file" : "default");
    }
    fprintf(stderr, "\n");
}

/* The console button that makes PS bit `bit` under the map in force: the Switch
 * button's name ("Y"), "Minus" for Select, for the log (the PC's key is the same
 * PS button's key, docs/HOTKEYS.md). */
static const char *bind_source(int bit) {
    int ps = dw3_mv_bind_ps_button(bit);

    if (ps < 0) return "Minus";
    return dw3_button_source_name(dw3_button_map_current()[ps]);
}

static void bind_map_text(char *out) {
    dw3_button_map_format(dw3_button_map_current(), out);
}

static void bind_log(const char *why) {
    char line[640];
    size_t n = 0;
    int a;
    char digits[DW3_BTN_COUNT + 1];

    bind_load();
    bind_map_text(digits);
    line[0] = 0;
    for (a = 0; a < MV_ACT_COUNT && n + 40 < sizeof(line); ++a) {
        n += (size_t)snprintf(line + n, sizeof(line) - n, "%s%s=%s(%s)", a ? " " : "",
                              dw3_mv_act_name(a), dw3_mv_bind_bit_name(BIND(a)),
                              bind_source(BIND(a)));
    }
    fprintf(stderr, "dw3-map: bindings in effect (%s): %s; fixed: close=%s(%s, the game's "
            "cancel) bars=Circle(%s) R3=ignored; PS button (console button) under button "
            "layout %s, map %s\n", why, line, "Triangle",
            dw3_button_source_name(dw3_button_map_current()[DW3_BTN_TRIANGLE]),
            dw3_button_source_name(dw3_button_map_current()[DW3_BTN_CIRCLE]),
            dw3_button_layout_name(dw3_button_map_layout()), digits);
    memcpy(g_bind_map, digits, sizeof(g_bind_map));
    g_bind_logged = 1;
}

/* Called every frame of the game's Map screen (cheap): when the button map was
 * changed (Options, Controls: layout, Customize, Reset) since the line was last
 * logged, the physical buttons of the bindings are not the ones it said. */
static void bind_note_map(void) {
    char digits[DW3_BTN_COUNT + 1];

    if (!g_bind_logged) return;
    bind_map_text(digits);
    if (memcmp(digits, g_bind_map, sizeof(digits)) != 0) {
        char was[DW3_BTN_COUNT + 1];

        memcpy(was, g_bind_map, sizeof(was));
        {
            char why[96];

            snprintf(why, sizeof(why), "the button map changed %s -> %s", was, digits);
            bind_log(why);
        }
    }
}

/* --- cache ---------------------------------------------------------------*/

void dw3_map_view_set_zoom_stick(int raw_up) {
    int a = raw_up < 0 ? -raw_up : raw_up;

    if (a > 127) a = 127;
    if (a < MV_STICK_DEADZONE) g_zoom_stick = 0;
    else {
        a = (a - MV_STICK_DEADZONE) * 127 / (127 - MV_STICK_DEADZONE);
        g_zoom_stick = raw_up < 0 ? -a : a;
    }
}

/* The left stick's raw axes for the cursor (phase 3: analog pan speed), -127..127, x
 * right and y down positive, 0 centred or on a digital pad (then the game's own d-pad
 * bits move the cursor at the fixed speeds). Read from the game's own receive buffer
 * of pad 1 (PadData.analog[2], [3] of an analog pad, which is what the game's
 * readPadButtons reads, 0x80 centred), so no host hook is needed; the Switch and the
 * PC fill it the same way. The replays have a digital pad: DW3_MAP_LSTICK=<script> (PC
 * only, scrubbed by the runner) stands in for the stick, see lstick_script. */
static int lstick_script(unsigned frame, int *x, int *y) {
    static const char *spec;
    static int parsed;
    static struct { unsigned f; int x, y; } key[64];
    static int nkey;
    int i;

    if (!parsed) {
        const char *p = getenv("DW3_MAP_LSTICK");

        parsed = 1;
        spec = p && p[0] ? p : NULL;
        /* "frame:x:y,frame:x:y,...": from view frame `frame` on, the stick is at x, y */
        while (p && *p && nkey < 64) {
            unsigned f = 0;
            int xx = 0, yy = 0, n = 0;

            if (sscanf(p, "%u:%d:%d%n", &f, &xx, &yy, &n) < 3) break;
            key[nkey].f = f;
            key[nkey].x = xx;
            key[nkey].y = yy;
            ++nkey;
            p += n;
            if (*p == ',') ++p;
        }
        if (spec) {
            fprintf(stderr, "dw3-map: DW3_MAP_LSTICK test aid: %d keyframes (the left stick "
                    "of an analog pad is read from the game's pad buffer)\n", nkey);
        }
    }
    if (!spec) return 0;
    *x = *y = 0;
    for (i = 0; i < nkey && key[i].f <= frame; ++i) {
        *x = key[i].x;
        *y = key[i].y;
    }
    return 1;
}

static void lstick_read(int *x, int *y) {
    const PadData *d = &PAD.buf[0].pad;

    *x = *y = 0;
    if (lstick_script(s.frame, x, y)) return;
    if (d->status == 0 && (d->id >> 4) == PAD_ID_ANALOG) {
        *x = (int)d->analog[2] - 0x80;
        *y = (int)d->analog[3] - 0x80;
    }
}

static void cache_path(char *out, size_t cap, int half, int scaler) {
    dw3_mv_cache_name(out, cap, g_cache_dir[0] ? g_cache_dir : NULL, MV_REGION,
                      half, scaler);
}

void dw3_map_view_init(const char *config_dir) {
    const char *env_mods = getenv("DW3_MODS_DIR");

#if defined(__SWITCH__)
    snprintf(g_cache_dir, sizeof(g_cache_dir), "sdmc:/switch/dw3/cache");
    snprintf(g_mods_dir, sizeof(g_mods_dir), "sdmc:/switch/dw3/mods");
    (void)config_dir;
#else
    g_mods_dir[0] = 0;
    g_cache_dir[0] = 0;
    if (config_dir && config_dir[0]) {
        snprintf(g_mods_dir, sizeof(g_mods_dir), "%s/mods", config_dir);
        snprintf(g_cache_dir, sizeof(g_cache_dir), "%s/cache", config_dir);
    }
#endif
    if (env_mods && env_mods[0]) { /* tests and replays */
        snprintf(g_mods_dir, sizeof(g_mods_dir), "%s", env_mods);
    }
    fprintf(stderr, "dw3-map: mods folder %s\n",
            g_mods_dir[0] ? g_mods_dir : "(none)");
    g_option_at_boot = map_view_option();
    if (!g_cache_dir[0]) return;
    if (!map_view_option()) {
        /* default: the feature costs nothing (no folder, no precompute) */
        fprintf(stderr, "dw3-map: option %s=0 (the default): the full-screen map "
                "is off, no cache folder, no precompute\n", MV_OPTION_KEY);
        return;
    }
    mkdir(g_cache_dir, 0777);
    fprintf(stderr, "dw3-map: cache folder %s\n", g_cache_dir);
}

/* --- building the base map -------------------------------------------------*/

/* The live VRAM (1024 x 512 halfwords), or NULL. */
static uint16_t *capture_vram(void) {
    RECT rect;
    u_long *vram;

    rect.x = 0;
    rect.y = 0;
    rect.w = MV_VRAM_W;
    rect.h = MV_VRAM_H;
    vram = (u_long *)calloc((size_t)MV_VRAM_W * MV_VRAM_H, 2);
    if (!vram) return NULL;
    Draw_StoreImage(&rect, vram);
    return (uint16_t *)vram;
}

static uint32_t sheet_len(int sheet_file) {
    uint32_t len = cdfs_file_size((uint32_t)sheet_file);

    if (len == (uint32_t)-1 || len > 0x10000) len = 0x1000;
    return len;
}

static int base_from_vram(const uint16_t *vram, const void *sheet,
                          int sheet_file) {
    return dw3_mv_compose(vram, (const int32_t *)sheet, sheet_len(sheet_file), 0,
                          0x280, 0, 0x140, 0x100, &s.base, &s.box);
}

/* The reveal layer: what STSTATUS_drawMapScreen draws over the dark map (the
 * towns of the progress list, then the visited area cells), composed from the
 * live VRAM with the same cells, palettes and positions as the game. The hash
 * of the piece list is the cache key of the scaled images. Returns the
 * number of pieces drawn. */
static int compose_reveal(const uint16_t *vram, int sheet_file,
                          const void *sheet1, const void *sheet2,
                          const int *pieces, int npieces) {
    uint32_t len = sheet_len(sheet_file);
    int i, drawn = 0;

    s.towns_drawn = s.areas_drawn = 0;
    s.base.reveal = 0;
    if (!vram || !pieces || npieces <= 0) return 0;
    for (i = 0; i < npieces; ++i) {
        int kind = pieces[i * 4], frame = pieces[i * 4 + 1];
        int x = pieces[i * 4 + 2] - s.base.ox, y = pieces[i * 4 + 3] - s.base.oy;
        const void *sheet = kind == 1 ? sheet1 : sheet2;
        int rc;

        if (!sheet) continue;
        rc = dw3_mv_blit_frame(vram, (const int32_t *)sheet, len, frame,
                               kind == 1 ? 0x380 : 0x300, 0x100, 0x140, 0x100,
                               &s.base, x, y);
        if (rc == 0) {
            ++drawn;
            if (kind == 1) ++s.towns_drawn;
            else ++s.areas_drawn;
        } else {
            fprintf(stderr, "dw3-map: reveal piece kind %d frame %d at %d,%d "
                    "not drawn\n", kind, frame, x, y);
        }
    }
    s.base.reveal = dw3_mv_hash(pieces, (size_t)npieces * 4 * sizeof(int), 0);
    if (s.base.reveal == 0) s.base.reveal = 1;
    return drawn;
}

/* The game's cursor and balloon: frames of the marks sheet (archive 2), drawn
 * by STSTATUS_drawMapScreen with the cells' texture (0x300,0x100): 0x32 + the
 * cursor animation frame (0, 1, 2) is the free cursor, 0x38 + it the cursor on
 * a location, 0x35 + the home frame (0, 1, 2) the balloon of the current
 * area and, on the EU version, 0x3B + it the same balloon turned the other
 * way (when the cursor is on the home mark's right). Composed once per open
 * from the live VRAM into small images, VRAM colour 0 transparent. */
enum { BK_CURSOR = 0, BK_HOVER = 3, BK_HOME = 6, BK_HOME_ALT = 9 };
static const int k_bank_frame[DW3_MV_BANK_N] = {
    0x32, 0x33, 0x34, 0x38, 0x39, 0x3A, 0x35, 0x36, 0x37, 0x3B, 0x3C, 0x3D,
};

static void bank_free(void) {
    int i;

    for (i = 0; i < DW3_MV_BANK_N; ++i) dw3_mv_image_free(&s.bank[i]);
    s.bank_ok = 0;
}

static int bank_build(const uint16_t *vram, int sheet_file, const void *sheet2) {
    uint32_t len = sheet_len(sheet_file);
    int i, built = 0;

    bank_free();
    if (!vram || !sheet2) return 0;
    for (i = 0; i < DW3_MV_BANK_N; ++i) {
        MvBox box;
        MvImage *img = &s.bank[i];

        if (dw3_mv_sheet_frame_box((const int32_t *)sheet2, len,
                                   k_bank_frame[i], &box) != 0) continue;
        img->w = box.x1 - box.x0;
        img->h = box.y1 - box.y0;
        if (img->w <= 0 || img->h <= 0 || img->w > 256 || img->h > 256) {
            img->w = img->h = 0;
            continue;
        }
        img->rgba = (uint8_t *)calloc((size_t)img->w * img->h, 4);
        if (!img->rgba) {
            img->w = img->h = 0;
            continue;
        }
        img->ox = box.x0;
        img->oy = box.y0;
        if (dw3_mv_blit_frame(vram, (const int32_t *)sheet2, len,
                              k_bank_frame[i], 0x300, 0x100, 0x140, 0x100, img,
                              -box.x0, -box.y0) != 0) {
            dw3_mv_image_free(img);
            continue;
        }
        ++built;
    }
    s.bank_ok = s.bank[BK_CURSOR].rgba && s.bank[BK_HOVER].rgba &&
                s.bank[BK_HOME].rgba;
    ++s.bank_gen;
    fprintf(stderr, "dw3-map: sprite bank: %d of %d frames built (cursor %s, "
            "hover cursor %s, balloon %s, balloon turned %s)\n", built,
            DW3_MV_BANK_N, s.bank[BK_CURSOR].rgba ? "ok" : "MISSING",
            s.bank[BK_HOVER].rgba ? "ok" : "MISSING",
            s.bank[BK_HOME].rgba ? "ok" : "MISSING",
            s.bank[BK_HOME_ALT].rgba ? "ok" : "no (US)");
    return built;
}

/* --- mod pack (docs/MOD_MAP_UPSCALE.md) ---------------------------------------*/

enum { MV_MOD_MAX_SCALE = 4, MV_MOD_SPRITES = 96 };

static void mod_free(void) {
    dw3_mv_image_free(&s.mod);
    s.mod_scale = 0;
    s.mod_id[0] = 0;
}

/* A reveal sprite of the 1x composition drawn N times larger with nearest
 * (used for a piece the pack has no valid sprite for). */
static int mod_piece_nearest(const uint16_t *vram, const void *sheet, size_t len,
                             int frame, int tex_x, const MvBox *box, int scale,
                             Dw3ModImage *out) {
    MvImage tmp;
    int x, y, k, l, rc;

    memset(&tmp, 0, sizeof(tmp));
    tmp.w = box->x1 - box->x0;
    tmp.h = box->y1 - box->y0;
    tmp.rgba = (uint8_t *)calloc((size_t)tmp.w * tmp.h, 4);
    out->rgba = (uint8_t *)calloc((size_t)tmp.w * scale * tmp.h * scale, 4);
    out->w = tmp.w * scale;
    out->h = tmp.h * scale;
    if (!tmp.rgba || !out->rgba) {
        free(tmp.rgba);
        free(out->rgba);
        out->rgba = NULL;
        return -1;
    }
    rc = dw3_mv_blit_frame(vram, (const int32_t *)sheet, len, frame, tex_x,
                           0x100, 0x140, 0x100, &tmp, -box->x0, -box->y0);
    for (y = 0; rc == 0 && y < tmp.h; ++y) {
        for (x = 0; x < tmp.w; ++x) {
            const uint8_t *p = tmp.rgba + ((size_t)y * tmp.w + x) * 4;

            if (p[3] == 0) continue;
            for (l = 0; l < scale; ++l)
                for (k = 0; k < scale; ++k)
                    memcpy(out->rgba + ((size_t)(y * scale + l) * out->w +
                                        (size_t)(x * scale + k)) * 4, p, 4);
        }
    }
    free(tmp.rgba);
    if (rc != 0) {
        free(out->rgba);
        out->rgba = NULL;
        return -1;
    }
    return 0;
}

/* Builds s.mod for this open when a valid pack exists: the pack's map at N x
 * with the reveal layer on top (the pack's sprites, or the game's own cells
 * drawn N times larger where the pack has none). Everything stays in source
 * pixels for the logic; only the texture is N times larger. Logs one line. */
static void mod_build(const uint16_t *vram, int sheet_file, const void *sheet1,
                      const void *sheet2, const int *pieces, int npieces) {
    Dw3ModPack pack;
    Dw3ModImage map, cache_img[MV_MOD_SPRITES];
    int cache_key[MV_MOD_SPRITES], cache_1x[MV_MOD_SPRITES];
    char why[DW3_MOD_WHY], first_bad[DW3_MOD_WHY];
    const char *want = getenv("DW3_MOD_PACK");
    double t0 = now_ms();
    int scale = 0, i, ncache = 0, from_pack = 0, at_1x = 0, skipped = 0;
    int shared_files = 0, region_files = 0;
    char mapfile[48];
    uint32_t len = sheet_len(sheet_file);

    mod_free();
    first_bad[0] = 0;
    if (dw3_port_options_file_int("map_mod", 1) == 0) {
        fprintf(stderr, "dw3-map: mod: original map (map_mod=0 in "
                "port_options.cfg)\n");
        return;
    }
    if (dw3_mod_pack_find(g_mods_dir, want, MV_REGION, &pack, why,
                          sizeof(why)) != 0) {
        fprintf(stderr, "dw3-map: mod: original map (%s; folder %s)\n", why,
                g_mods_dir[0] ? g_mods_dir : "none");
        return;
    }
    if (dw3_mod_load_map(&pack, MV_REGION, s.half, s.base.w, s.base.h,
                         MV_MOD_MAX_SCALE, &map, &scale, why, sizeof(why)) != 0) {
        fprintf(stderr, "dw3-map: mod: original map (pack \"%s\": %s)\n",
                pack.man.id, why);
        return;
    }
    for (i = 0; i < npieces; ++i) {
        int kind = pieces[i * 4], frame = pieces[i * 4 + 1], key = kind * 256 + frame;
        const void *sheet = kind == 1 ? sheet1 : sheet2;
        Dw3ModImage *sp = NULL;
        MvBox box;
        int j, x, y, is_1x = 0;

        if (!sheet || dw3_mv_sheet_frame_box((const int32_t *)sheet, len, frame,
                                             &box) != 0) {
            ++skipped;
            continue;
        }
        for (j = 0; j < ncache; ++j) {
            if (cache_key[j] == key) {
                sp = &cache_img[j];
                is_1x = cache_1x[j];
            }
        }
        if (!sp && ncache < MV_MOD_SPRITES) {
            sp = &cache_img[ncache];
            cache_key[ncache] = key;
            cache_1x[ncache] = 0;
            why[0] = 0;
            if (pack.man.reveal &&
                dw3_mod_load_sprite(&pack, MV_REGION, s.half,
                                    kind == 1 ? "towns" : "areas", frame, scale,
                                    box.x1 - box.x0, box.y1 - box.y0, sp, why,
                                    sizeof(why)) == 0) {
                ++ncache;
            } else if (mod_piece_nearest(vram, sheet, len, frame,
                                         kind == 1 ? 0x380 : 0x300, &box, scale,
                                         sp) == 0) {
                /* no (valid) sprite in the pack: the game's own cell, N times
                 * larger with nearest */
                if (!first_bad[0] && why[0]) {
                    snprintf(first_bad, sizeof(first_bad), "%s", why);
                }
                cache_1x[ncache] = is_1x = 1;
                ++ncache;
            } else {
                sp = NULL;
            }
        }
        if (!sp) {
            ++skipped;
            continue;
        }
        x = (pieces[i * 4 + 2] - s.base.ox + box.x0) * scale;
        y = (pieces[i * 4 + 3] - s.base.oy + box.y0) * scale;
        dw3_mod_blend(&map, sp, x, y);
        if (is_1x) ++at_1x;
        else ++from_pack;
    }
    for (i = 0; i < ncache; ++i) dw3_mod_image_free(&cache_img[i]);
    s.mod.w = map.w;
    s.mod.h = map.h;
    s.mod.rgba = map.rgba; /* ownership moves; malloc'd by stb (plain malloc) */
    s.mod.reveal = s.base.reveal;
    s.mod_scale = scale;
    snprintf(s.mod_id, sizeof(s.mod_id), "%s", pack.man.id);
    /* P51 item 3: the map and each distinct sprite came from the pack's shared
     * files (both regions) or from this region's own */
    for (i = 0; i < ncache; ++i) {
        if (!cache_1x[i]) {
            if (cache_img[i].shared) ++shared_files;
            else ++region_files;
        }
    }
    if (map.shared) ++shared_files;
    else ++region_files;
    if (map.shared) {
        snprintf(mapfile, sizeof(mapfile), "map_h%d@%dx.png", s.half, scale);
    } else {
        snprintf(mapfile, sizeof(mapfile), "map_%s_h%d@%dx.png", MV_REGION, s.half, scale);
    }
    fprintf(stderr, "dw3-map: mod: pack \"%s\" %s loaded: %s "
            "%dx%d, reveal %d pieces from the pack, %d at 1x nearest, %d "
            "skipped, %d ms%s%s%s shared=%d region=%d\n", pack.man.id, pack.man.version, mapfile,
            s.mod.w, s.mod.h, from_pack, at_1x, skipped,
            (int)(now_ms() - t0), first_bad[0] ? " (first sprite refused: " : "",
            first_bad, first_bad[0] ? ")" : "", shared_files, region_files);
}

/* The same map built from the disc files; used for the cross-check and by the
 * background precompute. */
static int base_from_disc(int half, MvImage *out, MvBox *box) {
    uint32_t sheet_id = (uint32_t)(MV_FILE_BASE + 2 * half);
    uint32_t img_id = sheet_id + 1;
    uint32_t sn = cdfs_file_size(sheet_id), in = cdfs_file_size(img_id);
    uint8_t *sheet = NULL, *imgs = NULL;
    uint16_t *vram = NULL;
    int rc = -1;

    if (sn == (uint32_t)-1 || in == (uint32_t)-1 || sn > 0x100000 ||
        in > 0x400000) return -1;
    sheet = (uint8_t *)malloc(sn);
    imgs = (uint8_t *)malloc(in);
    vram = (uint16_t *)calloc((size_t)MV_VRAM_W * MV_VRAM_H, 2);
    if (sheet && imgs && vram && cdfs_read(sheet_id, sheet, sn) == (int)sn &&
        cdfs_read(img_id, imgs, in) == (int)in &&
        dw3_mv_disc_load_map_vram(imgs, in, vram) > 0) {
        const int32_t *table = (const int32_t *)sheet;
        int off = table[0];

        if (off > 0 && (uint32_t)off < sn) {
            rc = dw3_mv_compose(vram, (const int32_t *)(sheet + off),
                                sn - (uint32_t)off, 0, 0x280, 0, 0x140, 0x100,
                                out, box);
        }
    }
    free(sheet);
    free(imgs);
    free(vram);
    return rc;
}

static int image_diff(const MvImage *a, const MvImage *b) {
    int i, n = 0;

    if (a->w != b->w || a->h != b->h) return -1;
    for (i = 0; i < a->w * a->h * 4; ++i) {
        if (a->rgba[i] != b->rgba[i]) ++n;
    }
    return n;
}

/* --- texture for the current scaler ----------------------------------------*/

static void set_texture(MvImage *img, int linear) {
    dw3_mv_image_free(&s.tex);
    s.tex = *img;
    img->rgba = NULL;
    s.tex_linear = linear;
    ++s.tex_gen;
}

static void build_texture(int scaler);
static void install_texture(int scaler, MvImage *out) {
    s.scaler = scaler;
    s.req_scaler = scaler;
    g_scaler = scaler;
    set_texture(out, dw3_mv_scaler_linear(scaler));
    fprintf(stderr, "dw3-map: texture %dx%d queued for the GPU (gen %u, scaler "
            "%s, %s)\n", s.tex.w, s.tex.h, s.tex_gen, dw3_mv_scaler_name(scaler),
            s.tex_linear ? "linear" : "nearest");
}

/* The CPU scalers: a job on a worker thread (Lanczos2 took 6.4 s and FSR 1
 * 2.1 s on the Switch, which stalled the whole game). It owns a copy of the
 * base map, so the screen can close or change under it; the picture changes
 * when it is done. The state word is the only thing both sides touch while it
 * runs: 0 running, 1 done (the game thread takes the result), 2 abandoned (the
 * worker frees the job). */
typedef struct MvJob {
    int scaler;
    unsigned id;
    MvImage src, out;
    char path[600];
    int use_cache;
    int rc, from_cache, cache_rc;
    double ms, t0;
    int state;
} MvJob;

static void job_free(MvJob *j) {
    dw3_mv_image_free(&j->src);
    dw3_mv_image_free(&j->out);
    free(j);
}

static void job_run(void *arg) {
    MvJob *j = (MvJob *)arg;

    j->rc = -1;
    j->cache_rc = -2;
    if (j->use_cache && dw3_mv_cache_read(j->path, &j->out) == 0 &&
        j->out.w == j->src.w * MV_SCALE && j->out.h == j->src.h * MV_SCALE &&
        j->out.reveal == j->src.reveal) {
        j->from_cache = 1;
        j->rc = 0;
    } else {
        int stale = j->out.rgba != NULL && j->out.w == j->src.w * MV_SCALE &&
                    j->out.reveal != j->src.reveal;

        /* a file made for other pieces (the bare map the precompute builds,
         * or an older state of the player's exploration) is not shown: the
         * picture would lack the reveal layer */
        if (stale) {
            fprintf(stderr, "dw3-map: cache stale %s (reveal hash %08x, now "
                    "%08x)\n", j->path, (unsigned)j->out.reveal,
                    (unsigned)j->src.reveal);
        }
        dw3_mv_image_free(&j->out);
        fprintf(stderr, "dw3-map: cache miss %s\n",
                j->use_cache ? j->path : "(no cache folder)");
        if (dw3_mv_scale(j->scaler, &j->src, &j->out, &j->ms) == 0 &&
            j->out.rgba) {
            j->rc = 0;
            if (j->use_cache) j->cache_rc = dw3_mv_cache_write(j->path, &j->out);
        }
    }
    if (__atomic_exchange_n(&j->state, 1, __ATOMIC_ACQ_REL) == 2) job_free(j);
}

static void job_take(void) {
    MvJob *j = s.job;
    int pending = s.pending_scaler;

    s.job = NULL;
    s.pending_scaler = -1;
    if (j->rc != 0 || !j->out.rgba) {
        fprintf(stderr, "dw3-map: scaler %s FAILED (no image built, the %s "
                "stays)\n", dw3_mv_scaler_name(j->scaler),
                dw3_mv_scaler_name(s.scaler));
        s.req_scaler = s.scaler;
    } else if (j->from_cache) {
        s.built_ms = -1;
        fprintf(stderr, "dw3-map: scaler %s loaded from cache in %d ms "
                "(%dx%d, cache hit %s, reveal hash %08x)\n",
                dw3_mv_scaler_name(j->scaler), (int)(now_ms() - j->t0),
                j->out.w, j->out.h, j->path, (unsigned)j->out.reveal);
        install_texture(j->scaler, &j->out);
    } else {
        s.built_ms = (int)(j->ms + 0.5);
        fprintf(stderr, "dw3-map: scaler %s built in %d ms (%dx%d, reveal hash "
                "%08x)\n", dw3_mv_scaler_name(j->scaler), s.built_ms, j->out.w,
                j->out.h, (unsigned)j->out.reveal);
        if (j->use_cache) {
            fprintf(stderr, "dw3-map: cache write %s: %s\n", j->path,
                    j->cache_rc == 0 ? "ok" : "FAILED");
        }
        install_texture(j->scaler, &j->out);
    }
    job_free(j);
    if (pending >= 0 && pending != s.scaler) {
        fprintf(stderr, "dw3-map: scaler %s was asked for while the last one "
                "was building: starting it now\n", dw3_mv_scaler_name(pending));
        s.req_scaler = pending;
        build_texture(pending);
    }
}

/* Every frame: take a finished job. Returns 1 when the picture changed. */
static int job_poll(void) {
    if (!s.job || __atomic_load_n(&s.job->state, __ATOMIC_ACQUIRE) != 1) return 0;
    job_take();
    return 1;
}

/* The screen closed or opened again: forget the job (it frees itself). */
static void job_abandon(void) {
    if (!s.job) return;
    if (__atomic_exchange_n(&s.job->state, 2, __ATOMIC_ACQ_REL) == 1) {
        job_free(s.job);
    } else {
        fprintf(stderr, "dw3-map: scaler %s still building at close: left to "
                "the worker\n", dw3_mv_scaler_name(s.job->scaler));
    }
    s.job = NULL;
    s.pending_scaler = -1;
}

#ifndef DW3_REGION_IMAGE
/* A game copy of a `both` image (P6.24) has none: its symbols are local, so a
 * weak definition here would shadow the host's real one. */
__attribute__((weak)) int dw3_map_view_async_start(void (*fn)(void *),
                                                   void *arg) {
    (void)fn;
    (void)arg;
    return -1;
}
#endif

static void job_start(int scaler) {
    MvJob *j = (MvJob *)calloc(1, sizeof(*j));
    int async = !getenv("DW3_MAP_SYNC");

    if (!j) return;
    j->scaler = scaler;
    j->id = ++s.job_id;
    j->t0 = now_ms();
    j->src = s.base;
    j->src.rgba = (uint8_t *)malloc((size_t)s.base.w * s.base.h * 4);
    if (!j->src.rgba) {
        free(j);
        return;
    }
    memcpy(j->src.rgba, s.base.rgba, (size_t)s.base.w * s.base.h * 4);
    cache_path(j->path, sizeof(j->path), s.half, scaler);
    j->use_cache = g_cache_dir[0] != 0;
    s.job = j;
    s.req_scaler = scaler;
    fprintf(stderr, "dw3-map: scaler %s requested (%s)\n",
            dw3_mv_scaler_name(scaler), async ? "worker thread, input stays "
            "live" : "synchronous, DW3_MAP_SYNC");
    if (async && dw3_map_view_async_start(job_run, j) == 0) return;
    if (async) {
        fprintf(stderr, "dw3-map: no worker thread, building on the game "
                "thread\n");
    }
    job_run(j);
    job_poll();
}

static void build_texture(int scaler) {
    MvImage out;
    double ms = 0.0;

    out.rgba = NULL;
    out.w = out.h = 0;
    s.built_ms = 0;
    if (s.mod.rgba) {
        /* the mod pack's map is already upscaled: no CPU scaler, only the GPU
         * filter follows the player's scaler (nearest unless he cycles it) */
        size_t n = (size_t)s.mod.w * (size_t)s.mod.h * 4;

        job_abandon();
        out = s.mod;
        out.rgba = (uint8_t *)malloc(n);
        if (out.rgba) {
            memcpy(out.rgba, s.mod.rgba, n);
            install_texture(scaler, &out);
            return;
        }
        fprintf(stderr, "dw3-map: mod: out of memory for the texture, the "
                "original map is shown\n");
        mod_free();
        out.rgba = NULL;
    }
    if (scaler == MV_SCALER_NEAREST || scaler == MV_SCALER_SHARP ||
        scaler == MV_SCALER_BILINEAR) {
        /* a copy, or a 5 ms integer prescale: not worth an SD cache file */
        if (dw3_mv_scale(scaler, &s.base, &out, &ms) != 0 || !out.rgba) {
            fprintf(stderr, "dw3-map: scaler %s FAILED (no image built, the "
                    "map picture cannot be shown)\n", dw3_mv_scaler_name(scaler));
            return;
        }
        s.built_ms = (int)(ms + 0.5);
        fprintf(stderr, "dw3-map: scaler %s built in %d ms (no cache, %dx%d)\n",
                dw3_mv_scaler_name(scaler), s.built_ms, out.w, out.h);
        job_abandon(); /* a slow one still running is no longer wanted */
        install_texture(scaler, &out);
        return;
    }
    if (s.job) {
        s.pending_scaler = scaler;
        s.req_scaler = scaler;
        fprintf(stderr, "dw3-map: scaler %s asked for while %s is building: "
                "queued\n", dw3_mv_scaler_name(scaler),
                dw3_mv_scaler_name(s.job->scaler));
        return;
    }
    job_start(scaler);
}

/* --- HUD ---------------------------------------------------------------------*/

static void hud_px(int x, int y, unsigned rgba) {
    uint8_t *p;

    if (x < 0 || y < 0 || x >= DW3_MV_HUD_W || y >= DW3_MV_HUD_H) return;
    p = s.hud + ((size_t)y * DW3_MV_HUD_W + (size_t)x) * 4;
    p[0] = (uint8_t)(rgba >> 24);
    p[1] = (uint8_t)(rgba >> 16);
    p[2] = (uint8_t)(rgba >> 8);
    p[3] = (uint8_t)rgba;
}

static void hud_rect(int x0, int y0, int x1, int y1, unsigned rgba) {
    int x, y;

    for (y = y0; y < y1; ++y)
        for (x = x0; x < x1; ++x) hud_px(x, y, rgba);
}

/* A Switch glyph of the help line (P6.32 phase 7): the art of assets/glyphs (or the
 * owner's own PNG) in the colours of the line, an open ring like the US font's icon:
 * the rim and the letter in the text colour, the shade in black, the body left to
 * the bar behind. */
static void hud_glyph(int x, int y, int glyph, unsigned rgba) {
    const unsigned char *art = dw3_glyphs_hud_art(glyph);
    int gx, gy;

    if (!art) return;
    for (gy = 0; gy < DW3_GLYPH_H; ++gy) {
        for (gx = 0; gx < DW3_GLYPH_W; ++gx) {
            switch (art[gy * DW3_GLYPH_W + gx]) {
            case DW3_ROLE_OUTLINE:
            case DW3_ROLE_LETTER: hud_px(x + gx, y + gy, rgba); break;
            case DW3_ROLE_SHADE: hud_px(x + gx, y + gy, 0x000000FFu); break;
            default: break;
            }
        }
    }
}

/* Text in the 8x8 font; a byte from MV_HELP_GLYPH0 on is a glyph (the help line),
 * 12 x 12 and centred on the text row. */
static void hud_text(int x, int y, const char *str, unsigned rgba) {
    for (; *str; ++str) {
        int c = (unsigned char)*str, gy, gx;
        const uint8_t *g;

        if (c >= MV_HELP_GLYPH0) {
            hud_glyph(x, y - 2, c - MV_HELP_GLYPH0, rgba);
            x += MV_HELP_GLYPH_ADV;
            continue;
        }
        x += MV_HELP_CHAR_W;
        if (c < 32 || c > 126) continue;
        g = dw3_port_font8x8[c - 32];
        for (gy = 0; gy < 8; ++gy)
            for (gx = 0; gx < 8; ++gx)
                if (g[gy] & (1u << gx)) {
                    hud_px(x - MV_HELP_CHAR_W + gx + 1, y + gy + 1, 0x000000FFu);
                }
        for (gy = 0; gy < 8; ++gy)
            for (gx = 0; gx < 8; ++gx)
                if (g[gy] & (1u << gx)) hud_px(x - MV_HELP_CHAR_W + gx, y + gy, rgba);
    }
}

static void to_hud(float ix, float iy, int *hx, int *hy) {
    float x0, y0, x1, y1;

    dw3_mv_view_rect(&s.view, s.base.w, s.base.h, VW, VH, &x0, &y0, &x1, &y1);
    *hx = (int)((x0 + (x1 - x0) * ix / (float)s.base.w) * 0.5f);
    *hy = (int)((y0 + (y1 - y0) * iy / (float)s.base.h) * 0.5f);
}

/* --- the help line (phase 7) ------------------------------------------------
 * The two green lines of the bars name the buttons the way the player sees them:
 * the game's button of each action (the bindings) through the button map in force,
 * as a Switch glyph (switch_glyphs=1, Switch builds only), the Switch's cap text,
 * or on the PC the PS word of the position; the words around them in the language
 * of the game's text. See map_view_help.h. */

/* PC test aid, scrubbed by the replay runner: draw the line as the Switch does
 * (the Switch's cap text, and the glyphs when switch_glyphs is on), so a replay
 * can look at what the console shows. The Switch and every release have no
 * environment. */
static void help_mode(MvHelpMode *m) {
#if defined(__SWITCH__)
    m->switch_names = 1;
#else
    static int aid = -1;

    if (aid < 0) {
        const char *v = getenv("DW3_MAP_HELP_SWITCH");

        aid = (v && v[0] && v[0] != '0') ? 1 : 0;
        if (aid) {
            fprintf(stderr, "dw3-map: DW3_MAP_HELP_SWITCH test aid: the help line is drawn as "
                    "the Switch draws it (Switch button names, glyphs with switch_glyphs=1)\n");
        }
    }
    m->switch_names = aid;
#endif
    m->glyphs = m->switch_names && dw3_port_options_level(DW3_PORT_OPT_SWITCH_GLYPHS) != 0;
}

static char g_help_logged[MV_HELP_LINES][MV_HELP_LINE_CAP];

/* The OLED check (AGENTS.md rule 12): one line per help line, when it is first
 * built in an open and whenever the text changes (a layout, a Customize change, a
 * rebind cannot happen inside the view, but the log is keyed on the text anyway). */
static void help_log(int lang, const MvHelpMode *mode, char lines[MV_HELP_LINES][MV_HELP_LINE_CAP]) {
    char digits[DW3_BTN_COUNT + 1];
    char shown[MV_HELP_LINE_CAP + 40];
    int i;

    for (i = 0; i < MV_HELP_LINES; ++i)
        if (strcmp(lines[i], g_help_logged[i]) != 0) break;
    if (i == MV_HELP_LINES) return;
    bind_map_text(digits);
    for (i = 0; i < MV_HELP_LINES; ++i) {
        const int w = dw3_mv_help_width(lines[i]);

        fprintf(stderr, "dw3-map: help line %d \"%s\" (layout=%s, map=%s, glyphs=%s, lang=%s, "
                "names=%s, %d of %d px: %s)\n", i + 1,
                dw3_mv_help_log_text(lines[i], shown, sizeof(shown)),
                dw3_button_layout_name(dw3_button_map_layout()), digits,
                mode->glyphs ? "on" : dw3_port_options_level(DW3_PORT_OPT_SWITCH_GLYPHS)
                    ? "off (switch_glyphs=on, but a PC build draws labels)" : "off",
                dw3_mv_help_lang_name(lang),
                mode->switch_names ? "Switch buttons" : "PS words of the position", w,
                MV_HELP_MAX_W, w <= MV_HELP_MAX_W ? "fits" : "OVERFLOWS the bar");
        snprintf(g_help_logged[i], MV_HELP_LINE_CAP, "%s", lines[i]);
    }
}

static void help_build(char lines[MV_HELP_LINES][MV_HELP_LINE_CAP]) {
    MvHelpMode mode;
    char names[MV_HS_COUNT][MV_HELP_NAME_CAP];
    const int lang = dw3_mv_help_lang(g_data_ok ? g_data.language : 0);
    int i;

    bind_load();
    help_mode(&mode);
    dw3_mv_help_names(dw3_button_map_current(), &g_bind, &mode, names);
    for (i = 0; i < MV_HELP_LINES; ++i) {
        if (dw3_mv_help_line(lang, i, names, lines[i], MV_HELP_LINE_CAP) < 0) {
            snprintf(lines[i], MV_HELP_LINE_CAP, "HELP LINE %d DOES NOT FIT", i + 1);
        }
    }
    help_log(lang, &mode, lines);
}

static void hud_build(void) {
    char line[96];
    int i, hx, hy;
    float zoom = dw3_mv_view_cur_zoom(&s.view, s.base.w, s.base.h, VW, VH);

    if (!s.hud) return;
    memset(s.hud, 0, (size_t)DW3_MV_HUD_W * DW3_MV_HUD_H * 4);
    /* markers: the visited areas are cells of the reveal layer (in the image);
     * only without it small crosses stand in for them. The current area is a
     * ring */
    for (i = 0; i < (s.areas_drawn > 0 ? 0 : s.nspots); ++i) {
        to_hud((float)s.spot_x[i], (float)s.spot_y[i], &hx, &hy);
        hud_rect(hx - 2, hy - 2, hx + 3, hy + 3, 0x101010E0u);
        hud_rect(hx - 1, hy - 1, hx + 2, hy + 2, 0xFFD040FFu);
    }
    if (s.home_x >= 0 && !s.bank_ok) {
        /* the game's balloon could not be composed: a ring stands in */
        int pulse = (int)((s.frame / 20u) & 1u);
        int r = 6 + pulse;

        to_hud((float)s.home_x, (float)s.home_y, &hx, &hy);
        hud_rect(hx - r, hy - r, hx + r + 1, hy + r + 1, 0x000000FFu);
        hud_rect(hx - r + 1, hy - r + 1, hx + r, hy + r, 0xFF3030FFu);
        hud_rect(hx - r + 3, hy - r + 3, hx + r - 2, hy + r - 2, 0x00000000u);
    }
    if (!s.show_hud) return;
    hud_rect(0, 0, DW3_MV_HUD_W, 38, 0x000000B0u);
    if (s.view.step == 0 && s.view.zoom <= 0.0f)
        snprintf(line, sizeof(line), "MAP  ZOOM FIT");
    else snprintf(line, sizeof(line), "MAP  ZOOM %.2fX", (double)zoom);
    if (s.mod.rgba) {
        size_t n = strlen(line);

        snprintf(line + n, sizeof(line) - n, "  MOD %dX", s.mod_scale);
    }
    hud_text(6, 4, line, 0xFFFFFFFFu);
    /* the build time is in the log, not on screen: it varies per run and
     * would make every capture differ */
    if (s.job) {
        snprintf(line, sizeof(line), "SCALER %d/%d: %s  BUILDING, %s STAYS",
                 s.req_scaler + 1, MV_SCALER_COUNT,
                 dw3_mv_scaler_name(s.req_scaler), dw3_mv_scaler_name(s.scaler));
    } else {
        snprintf(line, sizeof(line), "SCALER %d/%d: %s", s.scaler + 1,
                 MV_SCALER_COUNT, dw3_mv_scaler_name(s.scaler));
    }
    hud_text(6, 16, line, 0xFFE070FFu);
    hud_rect(0, DW3_MV_HUD_H - 28, DW3_MV_HUD_W, DW3_MV_HUD_H, 0x000000B0u);
    {
        char foot[MV_HELP_LINES][MV_HELP_LINE_CAP];

        help_build(foot);
        hud_text(6, DW3_MV_HUD_H - 26, foot[0], 0xC0FFC0FFu);
        hud_text(6, DW3_MV_HUD_H - 13, foot[1], 0xC0FFC0FFu);
    }
}

static void hud_touch(void) {
    ++s.hud_gen;
    hud_build();
}

/* --- open / tick / close ---------------------------------------------------*/

/* The game's cancel (back) button as a PS-side pad bit: STSTATUS_runMapScreen
 * closes the Map screen with PAD_TRIANGLE, and the controls remap works on the
 * raw word before the engine reads a bit, so this logical bit never moves. */
#define MV_CANCEL_BIT DW3_MV_PAD_TRIANGLE

static void close_view(void) {
    s.base_reveal_last = s.base.reveal;
    job_abandon();
    bank_free();
    mod_free();
    dw3_mv_image_free(&s.base);
    dw3_mv_image_free(&s.tex);
    free(s.hud);
    s.hud = NULL;
    s.active = 0;
    s.hole = 0;
    ++s.tex_gen; /* the host drops its texture after a few idle frames */
    fprintf(stderr, "dw3-map: close after %u frames (zoom: %u stick frames, %u "
            "button presses, last source %s; reveal hash %08x)\n", s.frame,
            s.zoom_stick_frames, s.zoom_button_presses,
            s.zoom_src == 1 ? "stick" : s.zoom_src == 2 ? "button" : "none",
            (unsigned)s.base_reveal_last);
    fprintf(stderr, "dw3-map: view session: %u zoom changes (%u button, %u stick frames), "
            "%u centre-on-area presses, %u cursor goes\n", s.zoom_events,
            s.zoom_button_presses, s.zoom_stick_frames, s.centre_presses, s.pan_goes);
    fprintf(stderr, "dw3-map: area names this open: %u shown by the game's name "
            "window, %u bad\n", s.names_shown, s.name_bad);
    fprintf(stderr, "dw3-map: pad held by the view for %u frames (the game's Map "
            "screen read none of them), R3 presses ignored %u, turbo=%d\n",
            s.frame, s.r3_ignored, dw3_port_turbo_speed());
    s.resume_wait = 2;
}

/* Phase 1 done-when: no black strips. The view's map rectangle against the whole
 * 16:9 picture. What it leaves open (FIT: the map is 392:352, not 16:9; or below
 * 1.0 on a tall window) is not black since phase 3: the backdrop of the map's own
 * colours. Logged at the open and every time the rectangle starts or stops
 * covering the picture. */
static void log_coverage(int force) {
    float x0, y0, x1, y1;
    int full, bucket;
    float l, r, t, b;

    dw3_mv_view_rect(&s.view, s.base.w, s.base.h, VW, VH, &x0, &y0, &x1, &y1);
    full = x0 <= 0.5f && y0 <= 0.5f && x1 >= (float)VW - 0.5f &&
           y1 >= (float)VH - 0.5f;
    /* the bars beside the map in viewport pixels */
    l = x0 > 0.0f ? x0 : 0.0f;
    r = x1 < (float)VW ? (float)VW - x1 : 0.0f;
    t = y0 > 0.0f ? y0 : 0.0f;
    b = y1 < (float)VH ? (float)VH - y1 : 0.0f;
    /* logged when the map starts or stops covering the picture, and while it does not,
     * every 120 px of bars (all four sides added) the bars grow or shrink: 1.0, then FIT */
    bucket = full ? -1 : (int)((l + r + t + b) / 120.0f);
    if (!force && bucket == s.cover_full) return;
    s.cover_full = bucket;
    if (full) {
        fprintf(stderr, "dw3-map: picture coverage full: map rect %.0f,%.0f-%.0f,%.0f of "
                "%dx%d (black strips: none)\n", (double)x0, (double)y0, (double)x1,
                (double)y1, VW, VH);
    } else {
        fprintf(stderr, "dw3-map: picture coverage partial: map rect %.0f,%.0f-%.0f,%.0f "
                "of %dx%d (black strips: none; the backdrop of the map's own colours "
                "(%s) fills left %.0f right %.0f top %.0f bottom %.0f px of %dx%d)\n",
                (double)x0, (double)y0, (double)x1, (double)y1, VW, VH,
                s.backdrop_ok ? "32x18 cells, dimmed to 50%, linear filter" :
                "NOT BUILT: black", (double)l, (double)r, (double)t, (double)b, VW, VH);
    }
}

/* --- phase 2: the marker list, the strips and the tour --------------------*/

/* One line for the marker list the game gave (patch 0147) and one per location or
 * town that lies in, or touches, a strip the PS1 cursor cannot reach. The towns
 * are the lit art of the reveal layer: their box is the frame's, at the town's
 * position. */
static void log_markers(const int *visited, const void *sheet1, int sheet_file) {
    int i, nvis = 0, partly = 0, outside = 0, listed = 0, strips = 0;
    uint32_t len = sheet_len(sheet_file);

    for (i = 1; i < 47; ++i) {
        int r = dw3_mv_hitbox_reach(s.all_x[i], s.all_y[i]);

        if (visited[i - 1]) ++nvis;
        if (r == 1) ++partly;
        if (r == 2) ++outside;
    }
    if (g_data_ok) {
        for (i = 1; i <= g_data.ntowns; ++i) listed += g_data.town_listed[i] != 0;
    }
    fprintf(stderr, "dw3-map: marker list: 46 locations (%d visited), %d towns "
            "(%d listed for this progress), current area %d = location id=%d; "
            "area names: text file 0x%x, language %s (%d); PS1 cursor reach "
            "x %d..%d y %d..%d: locations partly out of it %d, wholly out %d\n",
            nvis, g_data_ok ? g_data.ntowns : 0, listed, g_data_ok ? g_data.area : -1,
            s.home_spot, g_data_ok ? g_data.text_file : 0,
            language_name(g_data_ok ? g_data.language : 0),
            g_data_ok ? g_data.language : 0, MV_PS1_X0, MV_PS1_X1, MV_PS1_Y0,
            MV_PS1_Y1, partly, outside);
    for (i = 1; i < 47; ++i) {
        int r = dw3_mv_hitbox_reach(s.all_x[i], s.all_y[i]);

        if (r == 0) continue;
        fprintf(stderr, "dw3-map: location id=%d at %d,%d: PS1 cursor reach %s\n", i,
                s.all_x[i], s.all_y[i], r == 1 ? "partly" : "none");
    }
    if (!g_data_ok || !sheet1) return;
    for (i = 1; i <= g_data.ntowns; ++i) {
        MvBox box;
        int x0, y0, x1, y1, strip;

        if (dw3_mv_sheet_frame_box((const int32_t *)sheet1, len, i - 1, &box) != 0) continue;
        x0 = g_data.town_x[i] + box.x0;
        y0 = g_data.town_y[i] + box.y0;
        x1 = g_data.town_x[i] + box.x1 - 1;
        y1 = g_data.town_y[i] + box.y1 - 1;
        strip = dw3_mv_point_strip(x0, y0) | dw3_mv_point_strip(x1, y1);
        if (!strip) continue;
        ++strips;
        fprintf(stderr, "dw3-map: town id=%d at %d,%d art %d..%d,%d..%d touches the "
                "strip(s) the PS1 cursor cannot reach (%s%s%s%s), %s; towns have no "
                "name box in the game\n", i, g_data.town_x[i], g_data.town_y[i], x0,
                x1, y0, y1, strip & 1 ? "left " : "", strip & 2 ? "right " : "",
                strip & 4 ? "top " : "", strip & 8 ? "bottom" : "",
                g_data.town_listed[i] ? "listed" : "not listed yet");
    }
    fprintf(stderr, "dw3-map: towns touching a strip: %d; locations (the only things "
            "with a name) wholly or partly out of the PS1 reach: %d\n", strips,
            partly + outside);
}

/* The test tour: every visited location in id order, with a probe at the image
 * edge in front of each location that lies near an edge (so the cursor goes
 * through the strip the PS1 cursor cannot reach before it gets there), and one
 * probe at each town whose art touches a strip. */
static void tour_add(int x, int y, int id, int kind) {
    if (s.tour_n >= (int)(sizeof(s.tour) / sizeof(s.tour[0]))) return;
    s.tour[s.tour_n].x = x;
    s.tour[s.tour_n].y = y;
    s.tour[s.tour_n].id = id;
    s.tour[s.tour_n].kind = kind;
    ++s.tour_n;
}

static void tour_build(const int *visited) {
    const char *v = getenv("DW3_MAP_TOUR");
    int i, w = s.base.w, h = s.base.h;

    s.tour_period = 0;
    s.tour_n = 0;
    if (!v || atoi(v) <= 0) return;
    s.tour_period = atoi(v);
    s.tour_start = 10;
    for (i = 1; i <= (g_data_ok ? g_data.ntowns : 0); ++i) {
        if (dw3_mv_point_strip(g_data.town_x[i], g_data.town_y[i]))
            tour_add(g_data.town_x[i], g_data.town_y[i] + 8, i, 3);
    }
    for (i = 1; i < 47; ++i) {
        int cx = s.all_x[i] + 12, cy = s.all_y[i] + 12;

        if (!visited[i - 1]) continue;
        if (cx - s.box.x0 < 60) tour_add(s.box.x0, cy, i, 2);
        if (s.box.x0 + w - cx < 60) tour_add(s.box.x0 + w, cy, i, 2);
        if (cy - s.box.y0 < 70) tour_add(cx, s.box.y0, i, 2);
        if (s.box.y0 + h - cy < 100) tour_add(cx, s.box.y0 + h, i, 2);
        tour_add(cx, cy, i, 1);
    }
    fprintf(stderr, "dw3-map: tour armed (DW3_MAP_TOUR=%d): %d targets, the first at "
            "frame %u, then one every %d frames\n", s.tour_period, s.tour_n,
            s.tour_start, s.tour_period);
}

static void open_view(const void *sheet, int sheet_file, int half, int home_x,
                      int home_y, const int *visited, const int *spot_x,
                      const int *spot_y, const void *sheet1,
                      const void *sheet2, const int *pieces, int npieces) {
    char path[600];
    double t0 = now_ms();
    uint16_t *vram = NULL;
    int i;

    memset(&s.base, 0, sizeof(s.base));
    memset(&s.tex, 0, sizeof(s.tex));
    job_abandon();
    s.pending_scaler = -1;
    s.half = half;
    s.source = 0;
    s.pieces_in = npieces;
    s.pieces_drawn = s.towns_drawn = s.areas_drawn = 0;
    s.zoom_src = 0;
    s.zoom_stick_frames = s.zoom_button_presses = 0;
    if (g_cache_dir[0]) {
        cache_path(path, sizeof(path), half, MV_SCALER_NEAREST);
        if (dw3_mv_cache_read(path, &s.base) == 0) s.source = 1;
        fprintf(stderr, "dw3-map: base image cache %s: %s\n",
                s.source == 1 ? "hit" : "miss", path);
    }
    /* the live VRAM: the base when the cache has none, and always the cells of
     * the reveal layer (the towns and the visited areas the game shows) */
    if (s.source == 0 || npieces > 0 || sheet2 || getenv("DW3_MAP_VERIFY")) {
        vram = capture_vram();
        if (!vram) {
            fprintf(stderr, "dw3-map: VRAM capture failed (out of memory)\n");
            dw3_mv_image_free(&s.base);
            return;
        }
    }
    if (s.source == 0 || getenv("DW3_MAP_VERIFY")) {
        MvImage live;
        int rc;

        live.rgba = NULL;
        live.w = live.h = 0;
        live.reveal = 0;
        fprintf(stderr, "dw3-map: open half=%d region=%s: sheet file 0x%x "
                "size=%u, image file 0x%x size=%u; base from %s\n", half,
                MV_REGION, (unsigned)sheet_file,
                (unsigned)cdfs_file_size((uint32_t)sheet_file),
                (unsigned)(sheet_file + 1),
                (unsigned)cdfs_file_size((uint32_t)sheet_file + 1u),
                s.source == 1 ? "cache + vram check" : "vram capture");
        if (s.source == 1) {
            /* the cross-check: the cached (disc-built) base against the VRAM */
            MvImage cached = s.base;

            memset(&s.base, 0, sizeof(s.base));
            rc = base_from_vram(vram, sheet, sheet_file);
            live = s.base;
            s.base = cached;
            fprintf(stderr, "dw3-map: verify cache vs vram: %d differing bytes "
                    "(%dx%d vs %dx%d)\n", rc == 0 ? image_diff(&cached, &live) : -2,
                    cached.w, cached.h, live.w, live.h);
            dw3_mv_image_free(&live);
        } else {
            int rc = base_from_vram(vram, sheet, sheet_file);

            fprintf(stderr, "dw3-map: compose from vram rc=%d image=%dx%d\n",
                    rc, s.base.w, s.base.h);
            if (rc != 0) {
                fprintf(stderr, "dw3-map: capture failed, screen not opened\n");
                free(vram);
                return;
            }
            if (getenv("DW3_MAP_VERIFY")) {
                MvImage disc;
                MvBox db;

                memset(&disc, 0, sizeof(disc));
                if (base_from_disc(half, &disc, &db) == 0) {
                    fprintf(stderr, "dw3-map: verify vram vs disc: %d differing "
                            "bytes (%dx%d vs %dx%d)\n", image_diff(&s.base, &disc),
                            s.base.w, s.base.h, disc.w, disc.h);
                } else {
                    fprintf(stderr, "dw3-map: verify vram vs disc: disc build "
                            "failed\n");
                }
                dw3_mv_image_free(&disc);
            }
            if (g_cache_dir[0]) {
                /* the bare map only: the reveal layer is added below, per open */
                cache_path(path, sizeof(path), half, MV_SCALER_NEAREST);
                dw3_mv_cache_write(path, &s.base);
            }
        }
    }
    if (!s.base.rgba) {
        fprintf(stderr, "dw3-map: no base image, screen not opened\n");
        free(vram);
        return;
    }
    s.pieces_drawn = compose_reveal(vram, sheet_file, sheet1, sheet2, pieces,
                                    npieces);
    bank_build(vram, sheet_file, sheet2);
    mod_build(vram, sheet_file, sheet1, sheet2, pieces, npieces);
    free(vram);
    fprintf(stderr, "dw3-map: reveal layer: %d pieces from the game (%d towns "
            "+ %d visited areas drawn), %d drawn, hash %08x%s\n", npieces,
            s.towns_drawn, s.areas_drawn, s.pieces_drawn,
            (unsigned)s.base.reveal,
            npieces > 0 && s.pieces_drawn == 0 ? " (NOTHING DRAWN)" : "");
    s.box.x0 = s.base.ox;
    s.box.y0 = s.base.oy;
    if (getenv("DW3_MAP_VERIFY")) {
        /* P6.32 phase 0, question 4.2g: which spots lie where the PS1 cursor
         * cannot reach (its map-space reach is x 22..370, y 15 on; a spot is
         * hit from x + 7 to x + 18 and y + 7 to y + 18) */
        int outside = 0;

        for (i = 1; i < 47; ++i) {
            if (spot_x[i] + 18 < 22 || spot_x[i] + 7 > 370 || spot_y[i] + 18 < 15)
                ++outside;
        }
        fprintf(stderr, "dw3-map: spots outside the PS1 cursor reach: %d of 46\n",
                outside);
        for (i = 1; i < 47; ++i) {
            fprintf(stderr, "dw3-map: spot %d at %d,%d%s\n", i, spot_x[i],
                    spot_y[i], visited[i - 1] ? " visited" : "");
        }
    }
    s.nspots = 0;
    for (i = 0; i < 47; ++i) {
        /* spot i + 1 is visited when visited[i] is set; markers sit at the
         * hit-box middle (the sprite position + 12) */
        if (visited[i] && i + 1 < 47) {
            s.spot_x[s.nspots] = spot_x[i + 1] + 12 - s.box.x0;
            s.spot_y[s.nspots] = spot_y[i + 1] + 12 - s.box.y0;
            ++s.nspots;
        }
    }
    s.home_x = home_x - s.box.x0;
    s.home_y = home_y - s.box.y0;
    s.home_sx = home_x;
    s.home_sy = home_y;
    for (i = 0; i < 47; ++i) {
        s.all_x[i] = spot_x[i];
        s.all_y[i] = spot_y[i];
        s.vis[i] = i >= 1 && visited[i - 1] != 0;
    }
    s.home_spot = 0;
    for (i = 1; i < 47; ++i) {
        if (spot_x[i] + 0xC == home_x && spot_y[i] + 0xC == home_y) {
            s.home_spot = i;
            break;
        }
    }
    if (g_data_ok) {
        /* patch 0147: the game's own current area (its spot is area + 1) */
        if (g_data.area + 1 != s.home_spot) {
            fprintf(stderr, "dw3-map: current area %d gives location id=%d, the "
                    "home mark's position gave id=%d: using the game's\n",
                    g_data.area, g_data.area + 1, s.home_spot);
        }
        s.home_spot = g_data.area + 1;
    }
    /* the cursor starts on the current area, as the game's does */
    s.cur_x = home_x;
    s.cur_y = home_y;
    s.hover = 0;
    s.hold_x = s.hold_y = 0;
    s.was_moving = 0;
    s.home_alt = -1;
    s.name_bottom = -1;
    memset(&s.name_nb, 0, sizeof(s.name_nb));
    s.hole = 0;
    s.name_key = 0;
    s.name_hash = 0;
    s.names_shown = s.name_bad = 0;
    memset(s.named, 0, sizeof(s.named));
    s.strip = dw3_mv_point_strip(s.cur_x, s.cur_y);
    log_markers(visited, sheet1, sheet_file);
    tour_build(visited);
    s.show_hud = 0;      /* P51 item 2: the picture fills the screen; Circle brings the bars */
    memset(g_help_logged, 0, sizeof(g_help_logged)); /* phase 7: the help line is logged again per open */
    s.show_name = 1;
    s.frame = 0;
    s.r3_ignored = 0;
    s.resume_wait = 0;
    s.hud = (uint8_t *)calloc((size_t)DW3_MV_HUD_W * DW3_MV_HUD_H, 4);
    s.view.step = MAP_OPEN_STEP;  /* P51 item 2: 1.5x (the table's step 3) */
    s.view.top = s.mod.rgba ? MV_ZOOM_STEPS - 1 : MV_ZOOM_TOP_PLAIN; /* 4.0 only with a hi-res pack */
    s.view.zoom = 0.0f;
    s.pan_flags = 0;
    s.pan_acc = 0;
    s.pan_why = NULL;
    s.pan_src = 0;
    s.pan_frames = 0;
    s.acc_x = s.acc_y = 0.0f;
    s.pan_goes = s.centre_presses = s.zoom_events = 0;
    s.backdrop_ok = dw3_mv_backdrop(&s.base, s.backdrop, 50) == 0;
    ++s.backdrop_gen;
    s.view.cx = (float)s.home_x;
    s.view.cy = (float)s.home_y;
    {
        float wx, wy;
        const int fl = dw3_mv_view_clamp_ex(&s.view, s.base.w, s.base.h, VW, VH, &wx, &wy);

        s.active = 1;
        log_pan(fl, wx, wy, "the open, centred on the current area");
    }
    fprintf(stderr, "dw3-map: open half=%d image=%dx%d source=%s in %d ms "
            "markers=%d home=%d,%d turbo=%d zoom=%.2f hud=%s\n", half, s.base.w,
            s.base.h, s.source == 1 ? "cache" : "vram", (int)(now_ms() - t0),
            s.nspots, s.home_x, s.home_y, dw3_port_turbo_speed(),
            (double)dw3_mv_view_cur_zoom(&s.view, s.base.w, s.base.h, VW, VH),
            s.show_hud ? "on" : "off");
    s.cover_full = -1;
    log_coverage(1);
    {
        char ladder[160];
        size_t o = 0;
        int i;

        for (i = 1; i <= (s.view.top ? s.view.top : MV_ZOOM_TOP_PLAIN) && o + 8 < sizeof(ladder); ++i)
            o += (size_t)snprintf(ladder + o, sizeof(ladder) - o, "%s%.2f", i > 1 ? " " : "",
                                  (double)MV_ZOOM_TABLE[i]);
        fprintf(stderr, "dw3-map: zoom limits: FIT %.3f, steps %s, top %.2fx (%s); pan limit "
                "= the image box %dx%d at every zoom (no margin, centred where the whole "
                "map fits); the cursor reaches the whole image, the strips the PS1 cursor "
                "cannot reach included (x < %d, x > %d, y < %d, y > %d); Select centres on "
                "the current area; backdrop beside the map: %s\n",
                (double)(dw3_mv_view_scale(0, s.base.w, s.base.h, VW, VH) * 240.0f / (float)VH),
                ladder, (double)dw3_mv_view_top_zoom(&s.view),
                s.mod.rgba ? "4.0 because a higher-resolution pack is loaded" :
                "3.0: the source cannot show more, 4.0 needs a higher-resolution source",
                s.base.w, s.base.h, MV_PS1_X0, MV_PS1_X1, MV_PS1_Y0, MV_PS1_Y1,
                s.backdrop_ok ? "the map's own colours (32x18 cells, dim 50%)" : "black (not built)");
    }
    fprintf(stderr, "dw3-map: sprites at %s (scale 1/Z, Z = zoom, reference zoom "
            "1.0 = %.2f px per source px; the reveal layer, the lit towns and "
            "visited areas, scales with the map; the name box is the game's own "
            "window)\n", sprites_fixed() ? "fixed size" : "map scale (fixed size "
            "off: DW3_MAP_SPRITES_FIXED=0)",
            (double)dw3_mv_view_scale(1, s.base.w, s.base.h, VW, VH));
    build_texture(g_scaler);
    hud_touch();
}

int dw3_port_map_view_active(void) { return s.active; }

int dw3_port_map_view_wants(void) { return s.active || map_view_available(); }

void dw3_port_map_view_data(const Dw3MapViewData *d) {
    g_data = *d;
    g_data_ok = 1;
}

/* --- the cursor ------------------------------------------------------------*/

/* A sprite-space point (the game's map space) in the virtual 1280x720 viewport. */
static void sprite_to_view(int sx, int sy, float *vx, float *vy) {
    float x0, y0, x1, y1;

    dw3_mv_view_rect(&s.view, s.base.w, s.base.h, VW, VH, &x0, &y0, &x1, &y1);
    *vx = x0 + (x1 - x0) * (float)(sx - s.box.x0) / (float)s.base.w;
    *vy = y0 + (y1 - y0) * (float)(sy - s.box.y0) / (float)s.base.h;
}

/* The spot under the cursor, 0 = none: the game's own test
 * (STSTATUS_showMapArea) on the visited spots, in sprite space. */
static int spot_under(int x, int y) {
    int i;

    for (i = 1; i < 47; ++i) {
        if (!s.vis[i]) continue;
        if (s.all_x[i] + 6 < x && x <= s.all_x[i] + 0x12 &&
            s.all_y[i] + 6 < y && y <= s.all_y[i] + 0x12) return i;
    }
    return 0;
}

/* Where the cursor sprite is drawn: on the spot while it is on one. */
static void cursor_draw_pos(int *x, int *y) {
    if (s.hover > 0) {
        *x = s.all_x[s.hover] + 0xC;
        *y = s.all_y[s.hover] + 0xC;
    } else {
        *x = s.cur_x;
        *y = s.cur_y;
    }
}

static void update_hover(void) {
    int now = spot_under(s.cur_x, s.cur_y);

    if (now == s.hover) return;
    if (now) {
        s.hover_x = s.cur_x;
        fprintf(stderr, "dw3-map: cursor on location id=%d (name id=%d, spot "
                "at %d,%d) cursor %d,%d\n", now, now, s.all_x[now],
                s.all_y[now], s.cur_x, s.cur_y);
    } else {
        fprintf(stderr, "dw3-map: cursor left location id=%d, free at %d,%d\n",
                s.hover, s.cur_x, s.cur_y);
    }
    s.hover = now;
}

/* The cursor reaches the whole map, but its sprite (an arrow whose tip is the
 * cursor) stays on the map. */
static void clamp_cursor(void) {
    const MvImage *c = &s.bank[BK_CURSOR];
    int right = c->rgba ? c->ox + c->w : 12, down = c->rgba ? c->oy + c->h : 16;
    int left = c->rgba ? -c->ox : 0, up = c->rgba ? -c->oy : 0;

    if (s.cur_x < s.box.x0 + left) s.cur_x = s.box.x0 + left;
    if (s.cur_x > s.box.x0 + s.base.w - right) s.cur_x = s.box.x0 + s.base.w - right;
    if (s.cur_y < s.box.y0 + up) s.cur_y = s.box.y0 + up;
    if (s.cur_y > s.box.y0 + s.base.h - down) s.cur_y = s.box.y0 + s.base.h - down;
}

/* The cursor's map pixel (the image's own pixels) as the view maths take it. */
static void cursor_map_pos(float *mx, float *my) {
    int x, y;

    cursor_draw_pos(&x, &y);
    *mx = (float)(x - s.box.x0);
    *my = (float)(y - s.box.y0);
}

/* Pan limits (phase 3, 4.2g): the view never shows past the image at any zoom; where
 * it would, it is held at the edge (left, right, top, bottom). The stages of a frame
 * (zoom about the cursor, centre on the area, the view following the cursor) each
 * report what their clamp did with pan_note; at the end of the frame pan_flush says it
 * ONCE when the set changed, so a zoom that the follow then pushes against the same
 * edge every frame is one line, not one per frame. FIT and the small zooms centre the
 * axis the whole map fits on. */
static void pan_names(int flags, char *edges, size_t ecap, char *fits, size_t fcap) {
    const char *e[4] = {"left", "right", "top", "bottom"};
    int i;
    size_t o = 0;

    edges[0] = 0;
    for (i = 0; i < 4; ++i) {
        if (flags & (1 << i))
            o += (size_t)snprintf(edges + o, ecap - o, "%s%s", o ? "+" : "", e[i]);
    }
    if (!o) snprintf(edges, ecap, "none");
    snprintf(fits, fcap, "%s", (flags & MV_PAN_CENTRED_X) && (flags & MV_PAN_CENTRED_Y) ? "x+y" :
             (flags & MV_PAN_CENTRED_X) ? "x" : (flags & MV_PAN_CENTRED_Y) ? "y" : "neither axis");
}

static void log_pan(int flags, float want_cx, float want_cy, const char *why) {
    char edges[40], fits[16];
    const float sc = dw3_mv_view_cur_scale(&s.view, s.base.w, s.base.h, VW, VH);
    const int mask = MV_PAN_LEFT | MV_PAN_RIGHT | MV_PAN_TOP | MV_PAN_BOTTOM |
                     MV_PAN_CENTRED_X | MV_PAN_CENTRED_Y;

    if ((flags & mask) == (s.pan_flags & mask)) return;
    pan_names(flags, edges, sizeof(edges), fits, sizeof(fits));
    fprintf(stderr, "dw3-map: pan clamp %s: view at the image edge: %s, whole map fits on %s (%s; zoom "
            "%.3f, view centre %.1f,%.1f wanted %.1f,%.1f, map %dx%d, view covers x "
            "%.1f..%.1f y %.1f..%.1f of the image, never past it)\n",
            (flags & (MV_PAN_LEFT | MV_PAN_RIGHT | MV_PAN_TOP | MV_PAN_BOTTOM)) ? "holds" : "free",
            edges, fits, why,
            (double)dw3_mv_view_cur_zoom(&s.view, s.base.w, s.base.h, VW, VH),
            (double)s.view.cx, (double)s.view.cy, (double)want_cx, (double)want_cy,
            s.base.w, s.base.h, (double)(s.view.cx - 0.5f * (float)VW / sc),
            (double)(s.view.cx + 0.5f * (float)VW / sc),
            (double)(s.view.cy - 0.5f * (float)VH / sc),
            (double)(s.view.cy + 0.5f * (float)VH / sc));
    s.pan_flags = flags & mask;
}

static void pan_note(int flags, float want_cx, float want_cy, const char *why) {
    const int edges = MV_PAN_LEFT | MV_PAN_RIGHT | MV_PAN_TOP | MV_PAN_BOTTOM;

    if ((flags & edges) && !(s.pan_acc & edges)) {
        s.pan_wx = want_cx;
        s.pan_wy = want_cy;
        s.pan_why = why;
    } else if (!(s.pan_acc & edges) && !s.pan_why) {
        s.pan_wx = want_cx;
        s.pan_wy = want_cy;
        s.pan_why = why;
    }
    s.pan_acc |= flags;
}

static void pan_flush(void) {
    if (s.pan_why) log_pan(s.pan_acc, s.pan_wx, s.pan_wy, s.pan_why);
    s.pan_acc = 0;
    s.pan_why = NULL;
}

/* The view follows the cursor: it stays inside the middle of the picture. */
static void follow_cursor(void) {
    float mx, my, wx, wy;
    int flags;

    cursor_map_pos(&mx, &my);
    flags = dw3_mv_view_follow(&s.view, mx, my, (float)VW * 0.30f,
                               (float)VH * 0.30f, s.base.w, s.base.h, VW, VH, &wx, &wy);
    pan_note(flags, wx, wy, "the view follows the cursor");
}

static void fill_io(Dw3MapViewIo *io) {
    float vx, vy;
    int x, y, g = dw3_port_widescreen_menu_shift();

    if (!io) return;
    cursor_draw_pos(&x, &y);
    sprite_to_view(x, y, &vx, &vy);
    io->cur_x = s.cur_x;
    io->cur_y = s.cur_y;
    io->scr_x = (int)floorf(vx / (float)VW * (float)(320 + 2 * g)) - g;
    io->scr_y = (int)floorf(vy / (float)VH * 240.0f);
}

/* The name box: shown by the game while the cursor is on a location (its
 * frame and its text window; the host leaves its rectangle to the game's own
 * pixels). It sits at the bottom while the cursor is in the top half of the
 * picture, as the game's does. */
static void update_name_box(void) {
    float vx, vy;
    int x, y, bottom, shown;

    cursor_draw_pos(&x, &y);
    sprite_to_view(x, y, &vx, &vy);
    bottom = dw3_mv_namebox_bottom(&s.name_nb, s.hover,
                                   vy / (float)VH * 240.0f);
    shown = s.hover > 0 && s.show_name && s.name_x1 > s.name_x0;
    if (shown != s.hole || (shown && bottom != s.name_bottom)) {
        if (shown) {
            fprintf(stderr, "dw3-map: name box shown for location id=%d (name "
                    "id=%d) at the %s of the picture, logical x %d..%d y "
                    "%d..%d\n", s.hover, s.hover, bottom ? "bottom" : "top",
                    s.name_x0, s.name_x1, s.name_y0 + (bottom ? 0xB4 : 0x14),
                    s.name_y1 + (bottom ? 0xB4 : 0x14));
        } else {
            fprintf(stderr, "dw3-map: name box hidden (%s)\n",
                    s.hover > 0 ? "name box off" : "cursor on no location");
        }
    }
    s.hole = shown;
    if (shown && s.name_bottom >= 0 && bottom != s.name_bottom) {
        fprintf(stderr, "dw3-map: name box placement changed %s->%s at zoom "
                "%.3f (location id=%d)\n", s.name_bottom ? "bottom" : "top",
                bottom ? "bottom" : "top",
                (double)dw3_mv_view_cur_zoom(&s.view, s.base.w, s.base.h, VW,
                                             VH), s.hover);
    }
    s.name_bottom = bottom;
}

static void update_balloon(void) {
    int alt = 0;

    /* the EU balloon faces the cursor: turned when the cursor (where it got on
     * the location, while it is on one) is not to the left of the home mark */
    if (MV_REGION[0] == 'e') {
        int x = s.hover > 0 ? s.hover_x : s.cur_x;

        alt = x >= s.home_sx;
    }
    if (alt != s.home_alt) {
        fprintf(stderr, "dw3-map: balloon at sprite %d,%d (image %d,%d) on "
                "location id=%d, frame 0x%x (%s)\n", s.home_sx, s.home_sy,
                s.home_x, s.home_y, s.home_spot, alt ? 0x3B : 0x35,
                alt ? "turned" : "plain");
        s.home_alt = alt;
    }
}

/* The cursor goes through the strips the PS1 cursor cannot reach: one line per
 * entry and exit (the new cursor covers the whole image). */
static void update_strip(void) {
    int b = dw3_mv_point_strip(s.cur_x, s.cur_y);

    if (b == s.strip) return;
    if (b) {
        fprintf(stderr, "dw3-map: cursor in the strip the PS1 cursor cannot reach "
                "(%s%s%s%s) at %d,%d, location id=%d\n", b & 1 ? "left " : "",
                b & 2 ? "right " : "", b & 4 ? "top " : "", b & 8 ? "bottom" : "",
                s.cur_x, s.cur_y, s.hover);
    } else {
        fprintf(stderr, "dw3-map: cursor back inside the PS1 cursor's reach at "
                "%d,%d, location id=%d\n", s.cur_x, s.cur_y, s.hover);
    }
    s.strip = b;
}

/* The test tour (DW3_MAP_TOUR): the cursor jumps to the next target every
 * tour_period frames; what the name window shows is logged by
 * dw3_port_map_view_name, and the end counts them. */
static void tour_step(int *moved) {
    unsigned k;

    if (s.tour_period <= 0 || s.frame < s.tour_start) return;
    k = s.frame - s.tour_start;
    if (k % (unsigned)s.tour_period) return;
    k /= (unsigned)s.tour_period;
    if (k < (unsigned)s.tour_n) {
        s.cur_x = s.tour[k].x;
        s.cur_y = s.tour[k].y;
        clamp_cursor();
        *moved = 1;
        fprintf(stderr, "dw3-map: tour step %u of %d: %s id=%d, cursor to %d,%d\n",
                k + 1, s.tour_n, s.tour[k].kind == 1 ? "location" :
                s.tour[k].kind == 2 ? "edge probe before location" :
                "strip probe at town", s.tour[k].id, s.cur_x, s.cur_y);
    } else if (k == (unsigned)s.tour_n) {
        int i, visited = 0, named = 0;
        char missing[256];
        size_t o = 0;

        missing[0] = 0;
        for (i = 1; i < 47; ++i) {
            if (!s.vis[i]) continue;
            ++visited;
            if (s.named[i]) ++named;
            else if (o + 8 < sizeof(missing)) o += (size_t)snprintf(missing + o, sizeof(missing) - o, " %d", i);
        }
        fprintf(stderr, "dw3-map: tour done: %d of %d visited locations named by the "
                "game's own name window, %u bad (not the table's string, not the "
                "view's location, or wider than the box)%s%s\n", named, visited,
                s.name_bad, missing[0] ? ", not named:" : "", missing);
    }
}

/* patch 0147: what the game's name window holds after STSTATUS_showMapArea ran
 * on the view's cursor. One line per name shown (the location, the source, the
 * text, whether it fits the box) and one when it goes away. The window must hold
 * the table's string of the location the view's own hit test found. */
void dw3_port_map_view_name(const Dw3MapViewName *n) {
    int key, bad = 0;
    uint32_t hash;
    char text[160];
    int odd = 0, room;

    if (!s.active || !n) return;
    key = n->hovering ? n->spot : 0;
    hash = key ? dw3_mv_hash(n->codes, (size_t)n->len, 0) : 0;
    if (key != s.hover) {
        fprintf(stderr, "dw3-map: area name MISMATCH: the game's own hit test says "
                "location id=%d, the view's says id=%d at %d,%d\n", key, s.hover,
                s.cur_x, s.cur_y);
        ++s.name_bad;
    }
    if (key == s.name_key && hash == s.name_hash) return;
    if (key) {
        odd = dw3_mv_name_text(n->chars, n->nchars, text, sizeof(text));
        room = s.name_x1 - n->win_x - 8;
        if (!n->in_table) bad = 1;
        if (n->width > room || n->width <= 0) bad = 1;
        if (!n->win_visible) bad = 1;
        fprintf(stderr, "dw3-map: area name shown: location id=%d source=game name "
                "window (text file 0x%x language %s, string %d of the table, %d font "
                "codes, hash %08x, window text %s the table's string) text=\"%s\" "
                "(%d undecoded) width %d of %d px at window %d,%d %d lines%s\n", key,
                n->text_file, language_name(n->language), key, n->len, (unsigned)hash,
                n->in_table ? "is" : "IS NOT", text, odd, n->width, room, n->win_x,
                n->win_y, n->win_lines, bad ? ": BAD (window hidden, text not the "
                "table's or wider than the box)" : ": fits");
        if (getenv("DW3_MAP_NAME_CODES")) { /* a debugging aid: the raw font codes */
            char hex[3 * 96 + 1];
            int k;

            for (k = 0; k < n->len; ++k) snprintf(hex + 3 * k, 4, "%02x ", n->codes[k]);
            hex[3 * n->len] = 0;
            fprintf(stderr, "dw3-map: area name codes id=%d: %s\n", key, hex);
        }
        ++s.names_shown;
        if (bad) ++s.name_bad;
        else if (key < 47) s.named[key] = 1;
    } else if (s.name_key) {
        fprintf(stderr, "dw3-map: area name gone (cursor left location id=%d)\n",
                s.name_key);
    }
    s.name_key = key;
    s.name_hash = hash;
}

/* --- zoom about the cursor, centre on the area, cursor speed (phase 3) -------*/

typedef struct ZoomRec {
    float z0, z1;           /* the logical zoom before and after */
    int step0, step1;
    float mx, my;           /* the anchor: the cursor, in map pixels */
    float px0, py0, px1, py1; /* the anchor on the 1280x720 viewport before / after */
    float cx0, cy0;         /* the view centre before */
    float wx, wy;           /* the centre the anchor asked for, before the clamp */
    int flags;
} ZoomRec;

/* Zoom with the cursor as the fixed point (the PoC zoomed about the view centre):
 * the cursor stays on the spot of the picture it was on, unless the image's edge
 * (the pan limit) does not allow it, and then the log says so. `dir` steps the
 * ladder (L1 / R1), else the smooth zoom multiplies by `factor` (the right stick). */
static void zoom_about_cursor(ZoomRec *z, int dir, float factor) {
    memset(z, 0, sizeof(*z));
    z->z0 = dw3_mv_view_cur_zoom(&s.view, s.base.w, s.base.h, VW, VH);
    z->step0 = s.view.step;
    z->cx0 = s.view.cx;
    z->cy0 = s.view.cy;
    cursor_map_pos(&z->mx, &z->my);
    dw3_mv_view_point(&s.view, s.base.w, s.base.h, VW, VH, z->mx, z->my, &z->px0,
                      &z->py0);
    if (dir) dw3_mv_view_zoom(&s.view, dir, s.base.w, s.base.h, VW, VH);
    else dw3_mv_view_zoom_by(&s.view, factor, s.base.w, s.base.h, VW, VH);
    z->flags = dw3_mv_view_anchor(&s.view, z->mx, z->my, z->px0, z->py0, s.base.w,
                                  s.base.h, VW, VH, &z->wx, &z->wy);
    dw3_mv_view_point(&s.view, s.base.w, s.base.h, VW, VH, z->mx, z->my, &z->px1,
                      &z->py1);
    z->z1 = dw3_mv_view_cur_zoom(&s.view, s.base.w, s.base.h, VW, VH);
    z->step1 = s.view.step;
    ++s.zoom_events;
}

/* "anchor=cursor ..." for the zoom lines: where the cursor was and is on the
 * picture, how far the zoom moved it, the view centre before and after. */
static void zoom_text(char *out, size_t cap, const ZoomRec *z) {
    const float dxp = z->px1 - z->px0, dyp = z->py1 - z->py0;
    const float moved = sqrtf(dxp * dxp + dyp * dyp);
    /* the pan limit is named only when it displaced the anchor */
    const int g = moved > 0.05f ? (int)(z->flags & (MV_PAN_LEFT | MV_PAN_RIGHT | MV_PAN_TOP | MV_PAN_BOTTOM)) : 0;
    const int fit = (int)(z->flags & (MV_PAN_CENTRED_X | MV_PAN_CENTRED_Y));
    int sx, sy;

    cursor_draw_pos(&sx, &sy);
    snprintf(out, cap, "anchor=cursor map %d,%d (sprite %d,%d) on the picture %.0f,%.0f "
             "-> %.0f,%.0f (moved %.1f px%s%s%s%s%s%s) view centre %.1f,%.1f -> "
             "%.1f,%.1f", (int)z->mx, (int)z->my, sx, sy, (double)z->px0,
             (double)z->py0, (double)z->px1, (double)z->py1, (double)moved,
             g || fit ? ", pan limit:" : "",
             g & MV_PAN_LEFT ? " left" : "", g & MV_PAN_RIGHT ? " right" : "",
             g & MV_PAN_TOP ? " top" : "", g & MV_PAN_BOTTOM ? " bottom" : "",
             fit ? " map fits, centred" : "", (double)z->cx0, (double)z->cy0,
             (double)s.view.cx, (double)s.view.cy);
}

/* The "centre on the current area" button (Select): the cursor goes back to the
 * game's current-area mark, like the cursor at the open, and the view centres on it
 * (held by the pan limits near the image edge). The zoom stays. */
static void centre_on_home(void) {
    float wx, wy, mx, my;
    float cx0 = s.view.cx, cy0 = s.view.cy;
    int bx = s.cur_x, by = s.cur_y, flags;

    ++s.centre_presses;
    s.cur_x = s.home_sx;
    s.cur_y = s.home_sy;
    s.acc_x = s.acc_y = 0.0f;
    s.hold_x = s.hold_y = 0;
    update_hover();
    update_strip();
    cursor_map_pos(&mx, &my);
    s.view.cx = mx;
    s.view.cy = my;
    flags = dw3_mv_view_clamp_ex(&s.view, s.base.w, s.base.h, VW, VH, &wx, &wy);
    {
        /* the pan limit is named only when it displaced the centre */
        const int held = fabsf(wx - s.view.cx) > 0.05f || fabsf(wy - s.view.cy) > 0.05f ?
                         (flags & (MV_PAN_LEFT | MV_PAN_RIGHT | MV_PAN_TOP | MV_PAN_BOTTOM)) : 0;

        fprintf(stderr, "dw3-map: centre on the current area: location id=%d (area %d), "
                "home mark at sprite %d,%d, cursor %d,%d -> %d,%d, view centre %.1f,%.1f "
                "-> %.1f,%.1f wanted %.1f,%.1f%s%s%s%s%s zoom %.3f\n", s.home_spot,
                g_data_ok ? g_data.area : -1, s.home_sx, s.home_sy, bx, by, s.cur_x,
                s.cur_y, (double)cx0, (double)cy0, (double)s.view.cx, (double)s.view.cy,
                (double)wx, (double)wy, held ? ", pan limit:" : ", free",
                held & MV_PAN_LEFT ? " left" : "", held & MV_PAN_RIGHT ? " right" : "",
                held & MV_PAN_TOP ? " top" : "", held & MV_PAN_BOTTOM ? " bottom" : "",
                (double)dw3_mv_view_cur_zoom(&s.view, s.base.w, s.base.h, VW, VH));
    }
    pan_note(flags, wx, wy, "centre on the current area");
}

/* The cursor's movement for this frame: the d-pad at the game's speeds, the left
 * stick scaled by how far it is tilted (an analog axis wins over the d-pad bits
 * the game derives from the same stick), both accelerating after a long hold. The
 * cursor is a whole sprite-space pixel (the game's hit test is integer), the rest
 * of a step carries over. Returns 1 while a direction is held. */
static int pan_cursor(unsigned held, int *moved) {
    const float zoom = dw3_mv_view_cur_zoom(&s.view, s.base.w, s.base.h, VW, VH);
    int lx, ly;
    float ux, uy;
    int dxd = 0, dyd = 0, dir_x, dir_y;
    float fx, fy, rx = 0.0f, ry = 0.0f, ex, ey;
    int stepx, stepy, src;

    lstick_read(&lx, &ly);
    ux = dw3_mv_stick_unit(lx);
    uy = dw3_mv_stick_unit(ly);

#define BITP(b) (1u << (b))
    if (held & BITP(DW3_MV_PAD_LEFT)) dxd -= 1;
    if (held & BITP(DW3_MV_PAD_RIGHT)) dxd += 1;
    if (held & BITP(DW3_MV_PAD_UP)) dyd -= 1;
    if (held & BITP(DW3_MV_PAD_DOWN)) dyd += 1;
#undef BITP
    dir_x = ux != 0.0f ? (ux < 0.0f ? -1 : 1) : dxd;
    dir_y = uy != 0.0f ? (uy < 0.0f ? -1 : 1) : dyd;
    fx = ux != 0.0f ? fabsf(ux) : 1.0f;
    fy = uy != 0.0f ? fabsf(uy) : 1.0f;
    src = (ux != 0.0f || uy != 0.0f) ? 2 : 1;
    s.hold_x = dir_x ? s.hold_x + 1 : 0;
    s.hold_y = dir_y ? s.hold_y + 1 : 0;
    if (!dir_x) s.acc_x = 0.0f;
    if (!dir_y) s.acc_y = 0.0f;
    if (!dir_x && !dir_y) {
        if (s.pan_src) {
            fprintf(stderr, "dw3-map: pan end source=%s after %d frames, peak ramp %.2f "
                    "px/frame, cursor %d,%d -> %d,%d (moved %d,%d), zoom %.3f\n",
                    s.pan_src == 2 ? "left stick" : "d-pad", s.pan_frames,
                    (double)s.pan_peak, s.pan_from_x, s.pan_from_y, s.cur_x, s.cur_y,
                    s.cur_x - s.pan_from_x, s.cur_y - s.pan_from_y, (double)zoom);
        }
        s.pan_src = 0;
        return 0;
    }
    if (dir_x) rx = dw3_mv_pan_speed(s.hold_x, zoom);
    if (dir_y) ry = dw3_mv_pan_speed(s.hold_y, zoom);
    ex = (float)dir_x * fx * rx;
    ey = (float)dir_y * fy * ry;
    if (!s.pan_src) {
        s.pan_src = src;
        s.pan_frames = 0;
        s.pan_level = 0;
        s.pan_peak = 0.0f;
        s.pan_from_x = s.cur_x;
        s.pan_from_y = s.cur_y;
        ++s.pan_goes;
    }
    ++s.pan_frames;
    if ((rx > ry ? rx : ry) > s.pan_peak) s.pan_peak = rx > ry ? rx : ry;
    if ((int)(rx > ry ? rx : ry) != s.pan_level) {
        s.pan_level = (int)(rx > ry ? rx : ry);
        fprintf(stderr, "dw3-map: pan speed source=%s frame %d: ramp %.2f px/frame "
                "(zoom %.3f, cap %.2f) x tilt %.2f/%.2f = %.2f/%.2f px/frame\n",
                src == 2 ? "left stick" : "d-pad", s.pan_frames, (double)(rx > ry ? rx : ry),
                (double)zoom, (double)dw3_mv_pan_speed(100000, zoom),
                (double)(dir_x ? fx : 0.0f), (double)(dir_y ? fy : 0.0f), (double)ex,
                (double)ey);
    }
    s.acc_x += ex;
    s.acc_y += ey;
    stepx = (int)s.acc_x;
    stepy = (int)s.acc_y;
    s.acc_x -= (float)stepx;
    s.acc_y -= (float)stepy;
    s.cur_x += stepx;
    s.cur_y += stepy;
    clamp_cursor();
    *moved = 1;
    return 1;
}

int dw3_port_map_view_frame(const void *sheet, int sheet_file, int half,
                            int home_x, int home_y, const int *visited,
                            const int *spot_x, const int *spot_y,
                            unsigned held, unsigned pressed, const void *sheet1,
                            const void *sheet2, const int *pieces,
                            int npieces, Dw3MapViewIo *io) {
    int moved = 0, moving = 0, k;

#define BIT(b) (1u << (b))
    if (!s.active) {
        if (s.resume_wait > 0 && io && --s.resume_wait == 0) {
            /* the patch restores the game's cursor and scroll after the first
             * inactive call, so this one (the second) sees them back */
            fprintf(stderr, "dw3-map: game Map screen back after the close: "
                    "cursor %d,%d scroll %d,%d (at the open %d,%d / %d,%d): %s\n",
                    io->game_cx, io->game_cy, io->game_sx, io->game_sy, s.game_cx,
                    s.game_cy, s.game_sx, s.game_sy,
                    io->game_cx == s.game_cx && io->game_cy == s.game_cy &&
                    io->game_sx == s.game_sx && io->game_sy == s.game_sy ?
                    "unchanged" : "CHANGED");
        }
        bind_load();
        bind_note_map();
        if (!(pressed & BIT(BIND(MV_ACT_OPEN)))) return 0;
        if (!map_view_available()) {
            fprintf(stderr, "dw3-map: %s pressed, view not available "
                    "(host=%d map_screen=%d widescreen shift=%d)\n",
                    dw3_mv_bind_bit_name(BIND(MV_ACT_OPEN)), MV_HOST_OK,
                    map_view_option(), dw3_port_widescreen_menu_shift());
            return 0;
        }
        if (g_option_at_boot == 0 && !g_option_late_logged) {
            /* turned on from the Options screen after this run started: the boot
             * built no cache folder and ran no precompute */
            g_option_late_logged = 1;
            if (g_cache_dir[0]) mkdir(g_cache_dir, 0777);
            fprintf(stderr, "dw3-map: option %s turned on after the start of this run: "
                    "cache folder %s ensured, no precompute this run (the CPU scalers "
                    "are built when first selected)\n", MV_OPTION_KEY,
                    g_cache_dir[0] ? g_cache_dir : "(none)");
        }
        bind_log("at the open");
        open_view(sheet, sheet_file, half, home_x, home_y, visited, spot_x,
                  spot_y, sheet1, sheet2, pieces, npieces);
        if (io) {
            s.game_cx = io->game_cx;
            s.game_cy = io->game_cy;
            s.game_sx = io->game_sx;
            s.game_sy = io->game_sy;
            fprintf(stderr, "dw3-map: game Map screen frozen underneath: cursor "
                    "%d,%d scroll %d,%d\n", s.game_cx, s.game_cy, s.game_sx,
                    s.game_sy);
        }
        if (s.active && io) {
            s.name_x0 = io->name_x0;
            s.name_y0 = io->name_y0;
            s.name_x1 = io->name_x1;
            s.name_y1 = io->name_y1;
            update_hover();
            update_balloon();
            update_name_box();
            fill_io(io);
            hud_touch();
        }
        return s.active;
    }
    ++s.frame;
    if (io) {
        s.cursor_anim = io->cursor_anim;
        s.home_anim = io->home_anim;
        s.name_x0 = io->name_x0;
        s.name_y0 = io->name_y0;
        s.name_x1 = io->name_x1;
        s.name_y1 = io->name_y1;
    }
    /* Close with the game's cancel button (owner decision, 2026-10-28).
     * `pressed` is the logical (PS-side) pad that the game reads, so this
     * follows the P6.12 / P9.8 remap by itself: the game cancels with the
     * PS-side Triangle bit whatever physical button is mapped to it
     * (docs/CONTROLS_BUNDLE.md). Square, the button that opens the view, does
     * not close it. */
    if (pressed & BIT(MV_CANCEL_BIT)) {
        fprintf(stderr, "dw3-map: close requested by the cancel button (PS-side "
                "bit %d, the game's own cancel; follows a button remap)\n",
                MV_CANCEL_BIT);
        fill_io(io);
        close_view();
        return 1; /* this frame's cancel must not close the game's screen */
    }
    if (pressed & BIT(DW3_MV_PAD_R3)) {
        ++s.r3_ignored;
        fprintf(stderr, "dw3-map: R3 pressed in the view: ignored (no fast-forward "
                "here; turbo=%d)\n", dw3_port_turbo_speed());
    }
    {
        /* a bindable button nothing is bound to (the old key after a rebind): say so,
         * once per press, so "the key does nothing" has its reason in the log */
        static const int cand[] = {13, 15, 10, 11, 8, 9, 1, 0};
        unsigned i;

        for (i = 0; i < sizeof(cand) / sizeof(cand[0]); ++i) {
            if (!(pressed & BIT(cand[i])) || dw3_mv_bindings_action(&g_bind, cand[i]) >= 0) {
                continue;
            }
            fprintf(stderr, "dw3-map: %s pressed in the view: not bound to an action "
                    "(zoom out %s, zoom in %s, centre %s, scaler %s/%s, open %s)\n",
                    dw3_mv_bind_bit_name(cand[i]),
                    dw3_mv_bind_bit_name(BIND(MV_ACT_ZOOM_OUT)),
                    dw3_mv_bind_bit_name(BIND(MV_ACT_ZOOM_IN)),
                    dw3_mv_bind_bit_name(BIND(MV_ACT_CENTRE)),
                    dw3_mv_bind_bit_name(BIND(MV_ACT_SCALER_PREV)),
                    dw3_mv_bind_bit_name(BIND(MV_ACT_SCALER_NEXT)),
                    dw3_mv_bind_bit_name(BIND(MV_ACT_OPEN)));
        }
    }
    if (pressed & BIT(BIND(MV_ACT_OPEN))) {
        fprintf(stderr, "dw3-map: %s ignored in the view (it opens the view; "
                "close with the cancel button, PS-side bit %d)\n",
                dw3_mv_bind_bit_name(BIND(MV_ACT_OPEN)), MV_CANCEL_BIT);
    }
    for (k = 0; k < 2; ++k) {
        /* zoom out steps the ladder down, zoom in up; both in one frame do both */
        const int dir = k ? 1 : -1;
        ZoomRec z;
        char at[320];
        float rx0, ry0, rx1, ry1;

        if (!(pressed & BIT(BIND(k ? MV_ACT_ZOOM_IN : MV_ACT_ZOOM_OUT)))) continue;
        zoom_about_cursor(&z, dir, 1.0f);
        moved = 1;
        ++s.zoom_button_presses;
        s.zoom_src = 2;
        zoom_text(at, sizeof(at), &z);
        dw3_mv_view_rect(&s.view, s.base.w, s.base.h, VW, VH, &rx0, &ry0, &rx1,
                         &ry1);
        fprintf(stderr, "dw3-map: zoom source=button step %d (%s) %.2fx -> %.2fx "
                "%s; ladder top %.2fx; centre %.1f,%.1f draw rect %.0f,%.0f-%.0f,%.0f "
                "of %dx%d\n", s.view.step, s.view.step == 0 ? "fit" : "x",
                (double)z.z0, (double)z.z1, at,
                (double)dw3_mv_view_top_zoom(&s.view), (double)s.view.cx,
                (double)s.view.cy, (double)rx0, (double)ry0, (double)rx1,
                (double)ry1, VW, VH);
        pan_note(z.flags, z.wx, z.wy, "zoom");
        log_coverage(0);
    }
    if (g_zoom_stick != 0) {
        /* the right stick: up zooms in, down zooms out, smoothly (no steps) */
        float f = powf(2.0f, (float)g_zoom_stick / 127.0f *
                       MV_STICK_OCTAVES_PER_S / 60.0f);
        ZoomRec z;

        zoom_about_cursor(&z, 0, f);
        if (s.zoom_src != 1 || (s.zoom_stick_frames % 30u) == 0u) {
            char at[320];

            zoom_text(at, sizeof(at), &z);
            fprintf(stderr, "dw3-map: zoom source=stick (axis %d) zoom %.3f (from %.3f) "
                    "%s; ladder top %.2fx; centre %.1f,%.1f\n", g_zoom_stick,
                    (double)z.z1, (double)z.z0, at,
                    (double)dw3_mv_view_top_zoom(&s.view), (double)s.view.cx,
                    (double)s.view.cy);
        }
        pan_note(z.flags, z.wx, z.wy, "zoom");
        log_coverage(0);
        s.zoom_src = 1;
        ++s.zoom_stick_frames;
        moved = 1;
    }
    if (pressed & BIT(BIND(MV_ACT_CENTRE))) {
        centre_on_home();
        moved = 1;
    }
    if (pressed & (BIT(BIND(MV_ACT_SCALER_NEXT)) | BIT(BIND(MV_ACT_SCALER_PREV)))) {
        int dir = (pressed & BIT(BIND(MV_ACT_SCALER_NEXT))) ? 1 : -1;

        build_texture(dw3_mv_scaler_step(s.req_scaler, dir));
        moved = 1;
    }
    if (job_poll()) moved = 1;
    if (pressed & BIT(DW3_MV_PAD_CIRCLE)) {
        s.show_hud = !s.show_hud;
        fprintf(stderr, "dw3-map: HUD %s (the bars; the name box stays)\n",
                s.show_hud ? "on" : "off");
        moved = 1;
    }
    /* the cursor: the pad and the left stick, over the whole map (phase 3: the stick
     * is analog, and a long hold accelerates) */
    moving = pan_cursor(held, &moved);
    tour_step(&moved);
    if (moved) dw3_mv_view_clamp(&s.view, s.base.w, s.base.h, VW, VH);
    update_hover();
    update_strip();
    follow_cursor();
    update_balloon();
    update_name_box();
    pan_flush();
    if (moving != s.was_moving) {
        s.was_moving = moving;
        if (!s.was_moving) {
            fprintf(stderr, "dw3-map: cursor at rest at %d,%d, location "
                    "id=%d name id=%d (view centre %.1f,%.1f)\n", s.cur_x,
                    s.cur_y, s.hover, s.hover, (double)s.view.cx,
                    (double)s.view.cy);
        }
    }
    if (moved || (!s.bank_ok && (s.frame % 20u) == 0u)) hud_touch();
    fill_io(io);
#undef BIT
    return 1;
}

/* --- host side -------------------------------------------------------------*/

static void add_sprite(Dw3MapViewDraw *d, int bank, int px, int py) {
    const MvImage *b;
    float x0, y0, x1, y1;

    if (bank < 0 || bank >= DW3_MV_BANK_N || !s.bank[bank].rgba ||
        d->nspr >= DW3_MV_SPRITES) return;
    b = &s.bank[bank];
    /* anchor = the sprite's origin in map pixels; offset and size follow */
    dw3_mv_sprite_rect(&s.view, s.base.w, s.base.h, VW, VH,
                       (float)(px - s.box.x0), (float)(py - s.box.y0), b->ox,
                       b->oy, b->w, b->h, sprites_fixed(), &x0, &y0, &x1, &y1);
    d->spr[d->nspr].bank = bank;
    d->spr[d->nspr].x0 = x0 / (float)VW;
    d->spr[d->nspr].y0 = y0 / (float)VH;
    d->spr[d->nspr].x1 = x1 / (float)VW;
    d->spr[d->nspr].y1 = y1 / (float)VH;
    ++d->nspr;
}

void dw3_map_view_get_draw(Dw3MapViewDraw *d) {
    int i;

    memset(d, 0, sizeof(*d));
    d->active = s.active;
    d->tex_gen = s.tex_gen;
    d->tex_rgba = s.tex.rgba;
    d->tex_w = s.tex.w;
    d->tex_h = s.tex.h;
    d->linear = s.tex_linear;
    d->hud_gen = s.hud_gen;
    d->hud_rgba = s.hud;
    d->backdrop_gen = s.backdrop_gen;
    d->backdrop_rgba = s.active && s.backdrop_ok ? s.backdrop : NULL;
    d->bank_gen = s.bank_gen;
    for (i = 0; i < DW3_MV_BANK_N; ++i) {
        d->bank[i].rgba = s.bank[i].rgba;
        d->bank[i].w = s.bank[i].w;
        d->bank[i].h = s.bank[i].h;
    }
    if (s.active && s.base.w > 0) {
        float x0, y0, x1, y1;
        int cx, cy, a;

        dw3_mv_view_rect(&s.view, s.base.w, s.base.h, VW, VH, &x0, &y0, &x1, &y1);
        d->x0 = x0 / (float)VW;
        d->y0 = y0 / (float)VH;
        d->x1 = x1 / (float)VW;
        d->y1 = y1 / (float)VH;
        /* the cursor under the balloon, as the game's depths put them */
        cursor_draw_pos(&cx, &cy);
        a = s.cursor_anim;
        if (a < 0 || a > 2) a = 0;
        add_sprite(d, (s.hover > 0 ? BK_HOVER : BK_CURSOR) + a, cx, cy);
        a = s.home_anim;
        if (a < 0 || a > 2) a = 0;
        add_sprite(d, (s.home_alt > 0 && s.bank[BK_HOME_ALT].rgba ?
                       BK_HOME_ALT : BK_HOME) + a, s.home_sx, s.home_sy);
        if (s.hole) {
            const int g = dw3_port_widescreen_menu_shift();
            const int oy = s.name_bottom ? 0xB4 : 0x14;
            const float w = (float)(320 + 2 * g);

            d->hole = 1;
            d->hx0 = (float)(s.name_x0 + g) / w;
            d->hx1 = (float)(s.name_x1 + g) / w;
            d->hy0 = (float)(s.name_y0 + oy) / 240.0f;
            d->hy1 = (float)(s.name_y1 + oy) / 240.0f;
        }
    }
}

void dw3_map_view_tex_taken(unsigned gen) {
    if (gen == s.tex_gen) dw3_mv_image_free(&s.tex);
}

/* --- background precompute ----------------------------------------------------*/

void dw3_map_view_precompute_run(void) {
    int half, sc;
    double t_all = now_ms();
    int built = 0;

    if (!g_cache_dir[0] || !map_view_option()) return;
    for (half = 0; half < 2; ++half) {
        MvImage base;
        MvBox box;
        char path[600];
        int missing = 0;

        memset(&base, 0, sizeof(base));
        for (sc = 0; sc < MV_SCALER_COUNT; ++sc) {
            FILE *f;

            if (sc == MV_SCALER_SHARP || sc == MV_SCALER_BILINEAR)
                continue; /* the sharp prescale is a 5 ms copy and bilinear is
                           * the base itself: not worth 4 MB of SD */
            cache_path(path, sizeof(path), half, sc);
            f = fopen(path, "rb");
            if (f) fclose(f);
            else missing = 1;
        }
        if (!missing) continue;
        if (base_from_disc(half, &base, &box) != 0) {
            fprintf(stderr, "dw3-map: precompute half %d: disc read failed\n", half);
            continue;
        }
        cache_path(path, sizeof(path), half, MV_SCALER_NEAREST);
        dw3_mv_cache_write(path, &base);
        for (sc = MV_SCALER_LANCZOS2; sc <= MV_SCALER_FSR; ++sc) {
            MvImage out;
            double ms;
            FILE *f;

            cache_path(path, sizeof(path), half, sc);
            f = fopen(path, "rb");
            if (f) { fclose(f); continue; }
            if (dw3_mv_scale(sc, &base, &out, &ms) == 0) {
                dw3_mv_cache_write(path, &out);
                fprintf(stderr, "dw3-map: precompute half %d scaler %s built in "
                        "%d ms\n", half, dw3_mv_scaler_name(sc), (int)(ms + 0.5));
                ++built;
            }
            dw3_mv_image_free(&out);
        }
        dw3_mv_image_free(&base);
    }
    fprintf(stderr, "dw3-map: precompute done (%d images, %d ms, RAM freed)\n",
            built, (int)(now_ms() - t_all));
}
