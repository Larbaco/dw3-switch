/* P6.32 phase 0 spike: the new full-screen map screen. See map_view.h. */
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
#include "mod_pack.h"
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

typedef struct MapViewState {
    int active;
    int half;
    int scaler;
    int show_hud;
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
} MapViewState;

static MapViewState s;
static char g_cache_dir[480];
/* <config dir>/mods on the PC, sdmc:/switch/dw3/mods on the Switch */
static char g_mods_dir[480];
/* The scaler the next open starts with: nearest (the owner's choice after the
 * OLED look of preview.45), then whatever the player left it on. */
static int g_scaler = MV_SCALER_NEAREST;

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

static int map_view_available(void) {
    if (!MV_HOST_OK) return 0;
    if (dw3_port_options_file_int("map_screen", 1) == 0) return 0;
    /* owner decision 8: widescreen only (4:3 has no new screen) */
    return dw3_port_widescreen_menu_shift() > 0;
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
    if (!g_cache_dir[0]) return;
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
    fprintf(stderr, "dw3-map: mod: pack \"%s\" %s loaded: map_%s_h%d@%dx.png "
            "%dx%d, reveal %d pieces from the pack, %d at 1x nearest, %d "
            "skipped, %d ms%s%s%s\n", pack.man.id, pack.man.version, MV_REGION,
            s.half, scale, s.mod.w, s.mod.h, from_pack, at_1x, skipped,
            (int)(now_ms() - t0), first_bad[0] ? " (first sprite refused: " : "",
            first_bad, first_bad[0] ? ")" : "");
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
    if (scaler == MV_SCALER_NEAREST || scaler == MV_SCALER_SHARP) {
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

static void hud_text(int x, int y, const char *str, unsigned rgba) {
    for (; *str; ++str, x += 8) {
        int c = (unsigned char)*str, gy, gx;
        const uint8_t *g;

        if (c < 32 || c > 126) continue;
        g = dw3_port_font8x8[c - 32];
        for (gy = 0; gy < 8; ++gy)
            for (gx = 0; gx < 8; ++gx)
                if (g[gy] & (1u << gx)) {
                    hud_px(x + gx + 1, y + gy + 1, 0x000000FFu);
                }
        for (gy = 0; gy < 8; ++gy)
            for (gx = 0; gx < 8; ++gx)
                if (g[gy] & (1u << gx)) hud_px(x + gx, y + gy, rgba);
    }
}

static void to_hud(float ix, float iy, int *hx, int *hy) {
    float x0, y0, x1, y1;

    dw3_mv_view_rect(&s.view, s.base.w, s.base.h, VW, VH, &x0, &y0, &x1, &y1);
    *hx = (int)((x0 + (x1 - x0) * ix / (float)s.base.w) * 0.5f);
    *hy = (int)((y0 + (y1 - y0) * iy / (float)s.base.h) * 0.5f);
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
    hud_rect(0, DW3_MV_HUD_H - 16, DW3_MV_HUD_W, DW3_MV_HUD_H, 0x000000B0u);
    hud_text(6, DW3_MV_HUD_H - 12,
             "PAD/L-STICK CURSOR  R-STICK/L1/R1 ZOOM  L2/R2 SCALER  CIRCLE HUD  TRI BACK",
             0xC0FFC0FFu);
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
    s.show_hud = 1;
    s.frame = 0;
    s.hud = (uint8_t *)calloc((size_t)DW3_MV_HUD_W * DW3_MV_HUD_H, 4);
    s.view.step = 1;
    s.view.zoom = 0.0f;
    s.view.cx = (float)s.home_x;
    s.view.cy = (float)s.home_y;
    dw3_mv_view_clamp(&s.view, s.base.w, s.base.h, VW, VH);
    s.active = 1;
    fprintf(stderr, "dw3-map: open half=%d image=%dx%d source=%s in %d ms "
            "markers=%d home=%d,%d turbo=%d\n", half, s.base.w, s.base.h,
            s.source == 1 ? "cache" : "vram", (int)(now_ms() - t0), s.nspots,
            s.home_x, s.home_y, dw3_port_turbo_speed());
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

static int axis_speed(int held_frames) {
    /* the game's: 2 px a frame when the pad goes down, 3 and then 4 while it
     * is held (screen->speedX / speedY) */
    return held_frames <= 10 ? 2 : held_frames <= 20 ? 3 : 4;
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

/* The view follows the cursor: it stays inside the middle of the picture. */
static void follow_cursor(void) {
    float vx, vy, scale, mx = (float)VW * 0.30f, my = (float)VH * 0.30f;
    int x, y;

    cursor_draw_pos(&x, &y);
    sprite_to_view(x, y, &vx, &vy);
    scale = dw3_mv_view_cur_scale(&s.view, s.base.w, s.base.h, VW, VH);
    if (vx < mx) s.view.cx -= (mx - vx) / scale;
    if (vx > (float)VW - mx) s.view.cx += (vx - ((float)VW - mx)) / scale;
    if (vy < my) s.view.cy -= (my - vy) / scale;
    if (vy > (float)VH - my) s.view.cy += (vy - ((float)VH - my)) / scale;
    dw3_mv_view_clamp(&s.view, s.base.w, s.base.h, VW, VH);
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
    shown = s.hover > 0 && s.show_hud && s.name_x1 > s.name_x0;
    if (shown != s.hole || (shown && bottom != s.name_bottom)) {
        if (shown) {
            fprintf(stderr, "dw3-map: name box shown for location id=%d (name "
                    "id=%d) at the %s of the picture, logical x %d..%d y "
                    "%d..%d\n", s.hover, s.hover, bottom ? "bottom" : "top",
                    s.name_x0, s.name_x1, s.name_y0 + (bottom ? 0xB4 : 0x14),
                    s.name_y1 + (bottom ? 0xB4 : 0x14));
        } else {
            fprintf(stderr, "dw3-map: name box hidden (%s)\n",
                    s.hover > 0 ? "HUD off" : "cursor on no location");
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

int dw3_port_map_view_frame(const void *sheet, int sheet_file, int half,
                            int home_x, int home_y, const int *visited,
                            const int *spot_x, const int *spot_y,
                            unsigned held, unsigned pressed, const void *sheet1,
                            const void *sheet2, const int *pieces,
                            int npieces, Dw3MapViewIo *io) {
    int moved = 0, zoomed = 0, dx = 0, dy = 0;

#define BIT(b) (1u << (b))
    if (!s.active) {
        if (!(pressed & BIT(DW3_MV_PAD_SQUARE))) return 0;
        if (!map_view_available()) {
            fprintf(stderr, "dw3-map: Square pressed, view not available "
                    "(host=%d map_screen=%d widescreen shift=%d)\n", MV_HOST_OK,
                    dw3_port_options_file_int("map_screen", 1),
                    dw3_port_widescreen_menu_shift());
            return 0;
        }
        open_view(sheet, sheet_file, half, home_x, home_y, visited, spot_x,
                  spot_y, sheet1, sheet2, pieces, npieces);
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
    if (pressed & BIT(DW3_MV_PAD_SQUARE)) {
        fprintf(stderr, "dw3-map: Square ignored in the view (it opens the view; "
                "close with the cancel button, PS-side bit %d)\n", MV_CANCEL_BIT);
    }
    if (pressed & BIT(DW3_MV_PAD_L1)) {
        dw3_mv_view_zoom(&s.view, -1, s.base.w, s.base.h, VW, VH);
        moved = zoomed = 1;
    }
    if (pressed & BIT(DW3_MV_PAD_R1)) {
        dw3_mv_view_zoom(&s.view, 1, s.base.w, s.base.h, VW, VH);
        moved = zoomed = 1;
    }
    if (zoomed) {
        ++s.zoom_button_presses;
        s.zoom_src = 2;
    }
    if (g_zoom_stick != 0) {
        /* the right stick: up zooms in, down zooms out, smoothly (no steps) */
        float f = powf(2.0f, (float)g_zoom_stick / 127.0f *
                       MV_STICK_OCTAVES_PER_S / 60.0f);

        dw3_mv_view_zoom_by(&s.view, f, s.base.w, s.base.h, VW, VH);
        if (s.zoom_src != 1 || (s.zoom_stick_frames % 30u) == 0u) {
            fprintf(stderr, "dw3-map: zoom source=stick (axis %d) zoom %.3f "
                    "centre %.1f,%.1f\n", g_zoom_stick,
                    (double)dw3_mv_view_cur_zoom(&s.view, s.base.w, s.base.h,
                                                 VW, VH),
                    (double)s.view.cx, (double)s.view.cy);
        }
        s.zoom_src = 1;
        ++s.zoom_stick_frames;
        moved = zoomed = 1;
    }
    if (pressed & (BIT(DW3_MV_PAD_L1) | BIT(DW3_MV_PAD_R1))) {
        float rx0, ry0, rx1, ry1;

        dw3_mv_view_rect(&s.view, s.base.w, s.base.h, VW, VH, &rx0, &ry0, &rx1,
                         &ry1);
        fprintf(stderr, "dw3-map: zoom source=button step %d (%s) centre "
                "%.1f,%.1f draw rect %.0f,%.0f-%.0f,%.0f of %dx%d\n",
                s.view.step, s.view.step == 0 ? "fit" : "x", (double)s.view.cx,
                (double)s.view.cy, (double)rx0, (double)ry0, (double)rx1,
                (double)ry1, VW, VH);
    }
    if (pressed & (BIT(DW3_MV_PAD_R2) | BIT(DW3_MV_PAD_L2))) {
        int dir = (pressed & BIT(DW3_MV_PAD_R2)) ? 1 : -1;

        build_texture(dw3_mv_scaler_step(s.req_scaler, dir));
        moved = 1;
    }
    if (job_poll()) moved = 1;
    if (pressed & BIT(DW3_MV_PAD_CIRCLE)) {
        s.show_hud = !s.show_hud;
        fprintf(stderr, "dw3-map: HUD %s (name box %s)\n",
                s.show_hud ? "on" : "off", s.show_hud ? "allowed" : "hidden");
        moved = 1;
    }
    /* the cursor: the pad and the left stick (the game's pad bits), at the
     * game's speeds, over the whole map */
    if (held & BIT(DW3_MV_PAD_LEFT)) dx -= 1;
    if (held & BIT(DW3_MV_PAD_RIGHT)) dx += 1;
    if (held & BIT(DW3_MV_PAD_UP)) dy -= 1;
    if (held & BIT(DW3_MV_PAD_DOWN)) dy += 1;
    s.hold_x = dx ? s.hold_x + 1 : 0;
    s.hold_y = dy ? s.hold_y + 1 : 0;
    if (dx || dy) {
        s.cur_x += dx * axis_speed(s.hold_x);
        s.cur_y += dy * axis_speed(s.hold_y);
        clamp_cursor();
        moved = 1;
    }
    if (moved) dw3_mv_view_clamp(&s.view, s.base.w, s.base.h, VW, VH);
    update_hover();
    follow_cursor();
    update_balloon();
    update_name_box();
    if ((dx || dy) != s.was_moving) {
        s.was_moving = dx || dy;
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

    if (!g_cache_dir[0]) return;
    for (half = 0; half < 2; ++half) {
        MvImage base;
        MvBox box;
        char path[600];
        int missing = 0;

        memset(&base, 0, sizeof(base));
        for (sc = 0; sc < MV_SCALER_COUNT; ++sc) {
            FILE *f;

            if (sc == MV_SCALER_SHARP) continue; /* the sharp prescale is a
                                                  * 5 ms copy: not worth 4 MB of SD */
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
        for (sc = MV_SCALER_LANCZOS2; sc < MV_SCALER_COUNT; ++sc) {
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
