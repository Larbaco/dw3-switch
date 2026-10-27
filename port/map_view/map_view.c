/* P6.32 phase 0 spike: the new full-screen map screen. See map_view.h. */
#include "engine/gfx.h"

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
    MvImage base;        /* the captured map, kept while open */
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

static int base_from_vram(const void *sheet, int sheet_file) {
    RECT rect;
    u_long *vram;
    uint32_t len;
    int rc;

    rect.x = 0;
    rect.y = 0;
    rect.w = MV_VRAM_W;
    rect.h = MV_VRAM_H;
    vram = (u_long *)calloc((size_t)MV_VRAM_W * MV_VRAM_H, 2);
    if (!vram) return -1;
    Draw_StoreImage(&rect, vram);
    len = cdfs_file_size((uint32_t)sheet_file);
    if (len == (uint32_t)-1 || len > 0x10000) len = 0x1000;
    rc = dw3_mv_compose((const uint16_t *)vram, (const int32_t *)sheet, len, 0,
                        0x280, 0, 0x140, 0x100, &s.base, &s.box);
    free(vram);
    return rc;
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
        cache_path(path, sizeof(path), s.half, scaler);
        if (g_cache_dir[0] && dw3_mv_cache_read(path, &out) == 0 &&
            out.w == s.base.w * MV_SCALE && out.h == s.base.h * MV_SCALE) {
            s.built_ms = -1;
            fprintf(stderr, "dw3-map: scaler %s loaded from cache in %d ms "
                    "(%dx%d, cache hit %s)\n", dw3_mv_scaler_name(scaler),
                    (int)(now_ms() - t0), out.w, out.h, path);
        } else {
            dw3_mv_image_free(&out);
            fprintf(stderr, "dw3-map: cache miss %s\n",
                    g_cache_dir[0] ? path : "(no cache folder)");
            if (dw3_mv_scale(scaler, &s.base, &out, &ms) != 0) {
                fprintf(stderr, "dw3-map: scaler %s FAILED (no image built)\n",
                        dw3_mv_scaler_name(scaler));
                return;
            }
            s.built_ms = (int)(ms + 0.5);
            fprintf(stderr, "dw3-map: scaler %s built in %d ms (%dx%d)\n",
                    dw3_mv_scaler_name(scaler), s.built_ms, out.w, out.h);
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
    float zoom = MV_ZOOM_TABLE[s.view.step];

    if (!s.hud) return;
    memset(s.hud, 0, (size_t)DW3_MV_HUD_W * DW3_MV_HUD_H * 4);
    /* markers: visited spots (small crosses) and the current area (ring) */
    for (i = 0; i < s.nspots; ++i) {
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
    if (s.view.step == 0) snprintf(line, sizeof(line), "MAP  ZOOM FIT");
    else snprintf(line, sizeof(line), "MAP  ZOOM %.2fX", (double)zoom);
    hud_text(6, 4, line, 0xFFFFFFFFu);
    /* the build time is in the log, not on screen: it varies per run and
     * would make every capture differ */
    snprintf(line, sizeof(line), "SCALER %d/%d: %s", s.scaler + 1,
             MV_SCALER_COUNT, dw3_mv_scaler_name(s.scaler));
    hud_text(6, 16, line, 0xFFE070FFu);
    hud_rect(0, DW3_MV_HUD_H - 16, DW3_MV_HUD_W, DW3_MV_HUD_H, 0x000000B0u);
    hud_text(6, DW3_MV_HUD_H - 12,
             "L1/R1 ZOOM  PAD PAN  L2/R2 SCALER  CIRCLE HUD  TRIANGLE BACK",
             0xC0FFC0FFu);
}

static void hud_touch(void) {
    ++s.hud_gen;
    hud_build();
}

/* --- open / tick / close ---------------------------------------------------*/

static void close_view(void) {
    dw3_mv_image_free(&s.base);
    dw3_mv_image_free(&s.tex);
    free(s.hud);
    s.hud = NULL;
    s.active = 0;
    ++s.tex_gen; /* the host drops its texture after a few idle frames */
    fprintf(stderr, "dw3-map: close after %u frames\n", s.frame);
}

static void open_view(const void *sheet, int sheet_file, int half, int home_x,
                      int home_y, const int *visited, const int *spot_x,
                      const int *spot_y) {
    char path[600];
    double t0 = now_ms();
    int i;

    memset(&s.base, 0, sizeof(s.base));
    memset(&s.tex, 0, sizeof(s.tex));
    s.half = half;
    s.source = 0;
    if (g_cache_dir[0]) {
        cache_path(path, sizeof(path), half, MV_SCALER_NEAREST);
        if (dw3_mv_cache_read(path, &s.base) == 0) s.source = 1;
        fprintf(stderr, "dw3-map: base image cache %s: %s\n",
                s.source == 1 ? "hit" : "miss", path);
    }
    if (s.source == 0 || getenv("DW3_MAP_VERIFY")) {
        MvImage live;
        int rc;

        live.rgba = NULL;
        live.w = live.h = 0;
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
            rc = base_from_vram(sheet, sheet_file);
            live = s.base;
            s.base = cached;
            fprintf(stderr, "dw3-map: verify cache vs vram: %d differing bytes "
                    "(%dx%d vs %dx%d)\n", rc == 0 ? image_diff(&cached, &live) : -2,
                    cached.w, cached.h, live.w, live.h);
            dw3_mv_image_free(&live);
        } else {
            int rc = base_from_vram(sheet, sheet_file);

            fprintf(stderr, "dw3-map: compose from vram rc=%d image=%dx%d\n",
                    rc, s.base.w, s.base.h);
            if (rc != 0) {
                fprintf(stderr, "dw3-map: capture failed, screen not opened\n");
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
                cache_path(path, sizeof(path), half, MV_SCALER_NEAREST);
                dw3_mv_cache_write(path, &s.base);
            }
        }
    }
    if (!s.base.rgba) {
        fprintf(stderr, "dw3-map: no base image, screen not opened\n");
        return;
    }
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
                            unsigned held, unsigned pressed) {
    float step;
    int moved = 0;

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
                  spot_y);
        return s.active;
    }
    ++s.frame;
    if (pressed & (BIT(DW3_MV_PAD_TRIANGLE) | BIT(DW3_MV_PAD_SQUARE))) {
        close_view();
        return 1; /* this frame's Triangle must not close the game's screen */
    }
    if (pressed & BIT(DW3_MV_PAD_L1)) {
        dw3_mv_view_zoom(&s.view, -1, s.base.w, s.base.h, VW, VH);
        moved = 1;
    }
    if (pressed & BIT(DW3_MV_PAD_R1)) {
        dw3_mv_view_zoom(&s.view, 1, s.base.w, s.base.h, VW, VH);
        moved = 1;
    }
    if (moved) {
        float rx0, ry0, rx1, ry1;

        dw3_mv_view_rect(&s.view, s.base.w, s.base.h, VW, VH, &rx0, &ry0, &rx1,
                         &ry1);
        fprintf(stderr, "dw3-map: zoom step %d (%s) centre %.1f,%.1f draw rect "
                "%.0f,%.0f-%.0f,%.0f of %dx%d\n", s.view.step,
                s.view.step == 0 ? "fit" : "x", (double)s.view.cx,
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
    step = 2.5f * 3.0f / dw3_mv_view_scale(s.view.step, s.base.w, s.base.h, VW, VH);
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
