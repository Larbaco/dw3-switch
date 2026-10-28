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
} MapViewState;

static MapViewState s;
static char g_cache_dir[480];
/* The scaler the next open starts with: sharp bilinear (the design's default),
 * then whatever the player left it on. */
static int g_scaler = MV_SCALER_SHARP;
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
#if defined(__SWITCH__)
    snprintf(g_cache_dir, sizeof(g_cache_dir), "sdmc:/switch/dw3/cache");
    (void)config_dir;
#else
    if (!config_dir || !config_dir[0]) {
        g_cache_dir[0] = 0;
        return;
    }
    snprintf(g_cache_dir, sizeof(g_cache_dir), "%s/cache", config_dir);
#endif
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

static void build_texture(int scaler) {
    MvImage out;
    char path[600];
    double t0 = now_ms(), ms = 0.0;

    out.rgba = NULL;
    out.w = out.h = 0;
    s.built_ms = 0;
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
    } else {
        int stale = 0;

        cache_path(path, sizeof(path), s.half, scaler);
        if (g_cache_dir[0] && dw3_mv_cache_read(path, &out) == 0 &&
            out.w == s.base.w * MV_SCALE && out.h == s.base.h * MV_SCALE &&
            out.reveal == s.base.reveal) {
            s.built_ms = -1;
            fprintf(stderr, "dw3-map: scaler %s loaded from cache in %d ms "
                    "(%dx%d, cache hit %s, reveal hash %08x)\n",
                    dw3_mv_scaler_name(scaler), (int)(now_ms() - t0), out.w,
                    out.h, path, (unsigned)out.reveal);
        } else {
            /* a file made for other pieces (the bare map the precompute
             * builds, or an older state of the player's exploration) is
             * not shown: the picture would lack the reveal layer */
            stale = out.rgba != NULL && out.w == s.base.w * MV_SCALE &&
                    out.reveal != s.base.reveal;
            if (stale) {
                fprintf(stderr, "dw3-map: cache stale %s (reveal hash %08x, "
                        "now %08x)\n", path, (unsigned)out.reveal,
                        (unsigned)s.base.reveal);
            }
            dw3_mv_image_free(&out);
            fprintf(stderr, "dw3-map: cache miss %s\n",
                    g_cache_dir[0] ? path : "(no cache folder)");
            if (dw3_mv_scale(scaler, &s.base, &out, &ms) != 0) {
                fprintf(stderr, "dw3-map: scaler %s FAILED (no image built)\n",
                        dw3_mv_scaler_name(scaler));
                return;
            }
            s.built_ms = (int)(ms + 0.5);
            fprintf(stderr, "dw3-map: scaler %s built in %d ms (%dx%d, reveal "
                    "hash %08x)\n", dw3_mv_scaler_name(scaler), s.built_ms,
                    out.w, out.h, (unsigned)out.reveal);
            if (g_cache_dir[0]) {
                int wrc = dw3_mv_cache_write(path, &out);

                fprintf(stderr, "dw3-map: cache write %s: %s\n", path,
                        wrc == 0 ? "ok" : "FAILED");
            }
        }
    }
    s.scaler = scaler;
    g_scaler = scaler;
    set_texture(&out, dw3_mv_scaler_linear(scaler));
    fprintf(stderr, "dw3-map: texture %dx%d queued for the GPU (gen %u, scaler "
            "%s, %s)\n", s.tex.w, s.tex.h, s.tex_gen, dw3_mv_scaler_name(scaler),
            s.tex_linear ? "linear" : "nearest");
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
    if (s.home_x >= 0) {
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
    hud_text(6, 4, line, 0xFFFFFFFFu);
    /* the build time is in the log, not on screen: it varies per run and
     * would make every capture differ */
    snprintf(line, sizeof(line), "SCALER %d/%d: %s", s.scaler + 1,
             MV_SCALER_COUNT, dw3_mv_scaler_name(s.scaler));
    hud_text(6, 16, line, 0xFFE070FFu);
    hud_rect(0, DW3_MV_HUD_H - 16, DW3_MV_HUD_W, DW3_MV_HUD_H, 0x000000B0u);
    hud_text(6, DW3_MV_HUD_H - 12,
             "R-STICK/L1/R1 ZOOM  PAD PAN  L2/R2 SCALER  CIRCLE HUD  TRIANGLE BACK",
             0xC0FFC0FFu);
}

static void hud_touch(void) {
    ++s.hud_gen;
    hud_build();
}

/* --- open / tick / close ---------------------------------------------------*/

static void close_view(void) {
    s.base_reveal_last = s.base.reveal;
    dw3_mv_image_free(&s.base);
    dw3_mv_image_free(&s.tex);
    free(s.hud);
    s.hud = NULL;
    s.active = 0;
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
    if (s.source == 0 || npieces > 0 || getenv("DW3_MAP_VERIFY")) {
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
    build_texture(g_scaler);
    hud_touch();
}

int dw3_port_map_view_active(void) { return s.active; }

int dw3_port_map_view_frame(const void *sheet, int sheet_file, int half,
                            int home_x, int home_y, const int *visited,
                            const int *spot_x, const int *spot_y,
                            unsigned held, unsigned pressed, const void *sheet1,
                            const void *sheet2, const int *pieces,
                            int npieces) {
    float step;
    int moved = 0, zoomed = 0;

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
        return s.active;
    }
    ++s.frame;
    if (pressed & (BIT(DW3_MV_PAD_TRIANGLE) | BIT(DW3_MV_PAD_SQUARE))) {
        close_view();
        return 1; /* this frame's Triangle must not close the game's screen */
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

        build_texture(dw3_mv_scaler_step(s.scaler, dir));
        moved = 1;
    }
    if (pressed & BIT(DW3_MV_PAD_CIRCLE)) {
        s.show_hud = !s.show_hud;
        moved = 1;
    }
    /* pan: a constant speed on screen, 150 px/s at the 720 line */
    step = 2.5f * 3.0f / dw3_mv_view_cur_scale(&s.view, s.base.w, s.base.h, VW, VH);
    if (held & BIT(DW3_MV_PAD_LEFT)) { s.view.cx -= step; moved = 1; }
    if (held & BIT(DW3_MV_PAD_RIGHT)) { s.view.cx += step; moved = 1; }
    if (held & BIT(DW3_MV_PAD_UP)) { s.view.cy -= step; moved = 1; }
    if (held & BIT(DW3_MV_PAD_DOWN)) { s.view.cy += step; moved = 1; }
    if (moved) dw3_mv_view_clamp(&s.view, s.base.w, s.base.h, VW, VH);
    /* the home ring pulses, so the HUD changes every 20 frames anyway */
    if (moved || (s.frame % 20u) == 0u) hud_touch();
#undef BIT
    return 1;
}

/* --- host side -------------------------------------------------------------*/

void dw3_map_view_get_draw(Dw3MapViewDraw *d) {
    memset(d, 0, sizeof(*d));
    d->active = s.active;
    d->tex_gen = s.tex_gen;
    d->tex_rgba = s.tex.rgba;
    d->tex_w = s.tex.w;
    d->tex_h = s.tex.h;
    d->linear = s.tex_linear;
    d->hud_gen = s.hud_gen;
    d->hud_rgba = s.hud;
    if (s.active && s.base.w > 0) {
        float x0, y0, x1, y1;

        dw3_mv_view_rect(&s.view, s.base.w, s.base.h, VW, VH, &x0, &y0, &x1, &y1);
        d->x0 = x0 / (float)VW;
        d->y0 = y0 / (float)VH;
        d->x1 = x1 / (float)VW;
        d->y1 = y1 / (float)VH;
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
