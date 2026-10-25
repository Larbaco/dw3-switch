/* Host check for the deferred drawing of PSY-Z's CPU rasterizer
 * (sdl3_software_defer.inc, P6.22 pass 2). The same pseudo-random game-like
 * stream of drawing ops (two display buffers, a clear at the start of each
 * frame, textures that sometimes lie in a buffer, image ops, gutter row clears,
 * Store/Move reads) runs once with immediate drawing and once with deferral on;
 * every "present" (the display rectangle through SoftwareRasterizer_SyncRect)
 * and every StoreImage must give the same pixels. A few fixed scenes also check
 * that deferral really skips a covered frame. The longer comparison with a
 * reference revision and the footprint self-check is tools/soft_raster_bench.sh
 * (--fuzz-defer --check). */
#include <psyz.h>
#include "../internal.h"
#include "rasterizer.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

PsyzDitherMode Psyz_VideoGetDitheringMode(void) { return PSYZ_DITHER_AUTO; }
void psyz_log(unsigned int level, const char *file, unsigned int line,
              const char *func, const char *fmt, ...) {
    (void)level;
    (void)file;
    (void)line;
    (void)func;
    (void)fmt;
}

static int failures;

static void check(int cond, const char *what) {
    if (!cond) {
        fprintf(stderr, "FAIL: %s\n", what);
        failures++;
    }
}

static uint64_t g_rng;
static uint32_t rnd(void) {
    g_rng ^= g_rng << 13;
    g_rng ^= g_rng >> 7;
    g_rng ^= g_rng << 17;
    return (uint32_t)(g_rng >> 11);
}
static int rr(int lo, int hi) { return lo + (int)(rnd() % (uint32_t)(hi - lo + 1)); }

static uint64_t fnv(uint64_t h, const void *data, size_t n) {
    const unsigned char *p = data;

    for (size_t i = 0; i < n; i++) {
        h = (h ^ p[i]) * 1099511628211ull;
    }
    return h;
}

/* hash of a VRAM rectangle and, with gutter, its gutter plane rows */
static uint64_t hash_rect(int x, int y, int w, int h, int gutter) {
    const u16 *v = SoftwareRasterizer_GetVramRaw();
    uint64_t hh = 1469598103934665603ull;
    int x1 = x + w, y1 = y + h;

    if (x < 0) x = 0;
    if (y < 0) y = 0;
    if (x1 > 1024) x1 = 1024;
    if (y1 > 512) y1 = 512;
    for (int yy = y; yy < y1; yy++) {
        if (x1 > x) {
            hh = fnv(hh, &v[yy * 1024 + x], (size_t)(x1 - x) * 2);
        }
        if (gutter) {
            for (int s = 0; s < 2; s++) {
                hh = fnv(hh, &SoftwareRasterizer_GetGutterPlaneRaw(s)[yy * PSYZ_GUTTER_MAX],
                         PSYZ_GUTTER_MAX * 2);
            }
        }
    }
    return hh;
}

static void observe(int defer, uint64_t *obs, int *n, int max, int x, int y,
                    int w, int h, int gutter) {
    if (defer) {
        SoftwareRasterizer_SyncRect(x, y, w, h, gutter);
    }
    if (*n < max) {
        obs[(*n)++] = hash_rect(x, y, w, h, gutter);
    }
}

static unsigned xy(int x, int y) {
    return ((unsigned)(y & 0xFFFF) << 16) | (unsigned)(x & 0xFFFF);
}

static unsigned dxy(int ay) {
    if (rnd() % 8 == 0) {
        return rnd();
    }
    return xy(rr(-60, 380), ay + rr(-40, 280));
}

static int gen_prim(u_long *w, int ay, int tile_cover) {
    int kind = tile_cover ? 4 : (int)(rnd() % 10);
    unsigned color = rnd() & 0xFFFFFF;
    int n = 0;

    if (rnd() % 3 == 0) {
        color = 0x808080;
    }
    if (kind < 3) {
        int code = 0x20 | (int)(rnd() & 0x1F);
        int tex = (code & 4) != 0, gour = (code & 0x10) != 0;
        int cnt = (code & 8) ? 4 : 3;

        w[n++] = ((u_long)code << 24) | color;
        for (int v = 0; v < cnt; v++) {
            if (v && gour) {
                w[n++] = rnd() & 0xFFFFFF;
            }
            w[n++] = dxy(ay);
            if (tex) {
                unsigned hi = rnd() & 0xFFFF;
                if (v == 1) {
                    hi = (hi & 0xFE00) | (rnd() & 0x1FF);
                }
                w[n++] = (hi << 16) | (rnd() & 0xFFFF);
            }
        }
    } else if (kind == 3) {
        int code = 0x44 | (int)(rnd() & 0x12);

        w[n++] = ((u_long)code << 24) | color;
        w[n++] = dxy(ay);
        if (code & 0x10) {
            w[n++] = rnd() & 0xFFFFFF;
        }
        w[n++] = dxy(ay);
        w[n++] = 0x55555555;
    } else {
        int code = 0x60 | (int)(rnd() & 0x1F);
        int tex;

        if (tile_cover) {
            code &= ~0x1E;
        } else if (rnd() % 3 == 0) {
            code |= 0x04;
        }
        tex = (code & 4) != 0;
        w[n++] = ((u_long)code << 24) | color;
        w[n++] = tile_cover ? xy(0, ay) : dxy(ay);
        if (tex) {
            w[n++] = rnd();
        }
        if ((code & 0x18) == 0) {
            int ww = tile_cover ? 320 : (rnd() % 4 ? rr(1, 140) : rr(0, 400));
            int hh = tile_cover ? 240 : (rnd() % 4 ? rr(1, 140) : rr(0, 300));

            w[n++] = xy(ww, hh);
        }
    }
    return n;
}

static u16 g_img[1024 * 512];

/* one run of the stream; returns the number of observations */
static int run(uint64_t seed, int iters, int defer, int gutter_on,
               uint64_t *obs, int max_obs, unsigned long *dropped) {
    u_long w[16];
    static u16 store[1024 * 512];
    int n_obs = 0;
    int buf = 0, cur_y0 = 0;
    int defer_on = 1;

    g_rng = seed * 2654435761u + 88172645463325252ull;
    SoftwareRasterizer_Reset();
    SoftwareRasterizer_SetDefer(0);
    for (int i = 0; i < 1024 * 512; i++) {
        g_img[i] = (u16)rnd();
        if (rnd() % 5 == 0) {
            g_img[i] = 0;
        }
    }
    {
        PS1_RECT all = {0, 0, 1024, 512};
        software_rasterizer.load_image(&all, (u_long *)g_img);
    }
    if (gutter_on) {
        SoftwareRasterizer_SetGutter(53, 0, 320, 240);
    }
    SoftwareRasterizer_SetDefer(defer);
    for (int it = 0; it < iters; it++) {
        int pick = rr(0, 99);
        int ay = cur_y0;

        if (pick < 5) {
            buf ^= (rnd() % 8 != 0);
            cur_y0 = buf ? 256 : 0;
            ay = cur_y0;
            software_rasterizer.area_start(0, cur_y0);
            software_rasterizer.area_end(319, cur_y0 + (rnd() % 10 ? 239 : rr(0, 239)));
            software_rasterizer.offset(0, 0);
            if (rnd() % 6 == 0) {
                software_rasterizer.mask(rnd() % 3 == 0, rnd() % 6 == 0);
            }
            if (rnd() % 10 < 8) {
                int n = gen_prim(w, cur_y0, 1);
                software_rasterizer.push_prim(w, n);
            }
            if (gutter_on && rnd() % 2) {
                int disp_y = buf ? 0 : 256;
                SoftwareRasterizer_ClearGutterRows(0, disp_y);
                SoftwareRasterizer_ClearGutterRows(disp_y + 240, 512);
            }
        } else if (pick < 9) {
            u_long mode = rnd() & 0x3FF;
            software_rasterizer.texpage_mode((ParamDrawTexpageMode *)&mode);
        } else if (pick < 10) {
            software_rasterizer.texture_window(rnd() % 3 ? 0 : rnd() & 31,
                                               rnd() % 3 ? 0 : rnd() & 31,
                                               rnd() % 3 ? 0 : rnd() & 31,
                                               rnd() % 3 ? 0 : rnd() & 31);
        } else if (pick < 11) {
            software_rasterizer.mask(rnd() % 5 == 0, rnd() % 5 == 0);
        } else if (pick < 12) {
            software_rasterizer.offset(rnd() % 2 ? 0 : rr(0, 0x7FF),
                                       rnd() % 2 ? 0 : rr(0, 0x7FF));
        } else if (pick < 14) {
            int x0 = rr(0, 300), y0 = cur_y0 + rr(0, 200);
            software_rasterizer.area_start(x0, y0);
            software_rasterizer.area_end(x0 + rr(0, 319 - x0), y0 + rr(0, 239));
        } else if (pick < 17) {
            PS1_RECT r = {(short)rr(-8, 1000), (short)rr(-8, 500),
                          (short)(rnd() % 4 ? rr(1, 64) : rr(1, 400)),
                          (short)(rnd() % 4 ? rr(1, 64) : rr(1, 260))};
            software_rasterizer.load_image(&r, (u_long *)g_img);
        } else if (pick < 19) {
            int sb = rnd() % 2 ? 256 : 0;
            PS1_RECT r = {(short)(rnd() % 2 ? 0 : rr(0, 700)),
                          (short)(rnd() % 2 ? sb : rr(0, 400)),
                          (short)(rnd() % 2 ? 320 : rr(1, 400)),
                          (short)(rnd() % 2 ? 240 : rr(1, 300))};
            software_rasterizer.move_image(&r, (unsigned)(rnd() % 2 ? 0 : rr(0, 700)),
                                           (unsigned)(rnd() % 2 ? 256 - sb : rr(0, 400)));
        } else if (pick < 21) {
            int exact = rnd() % 2;
            PS1_RECT r = {(short)(exact || rnd() % 2 ? 0 : rr(-10, 800)),
                          (short)(exact ? cur_y0 : rr(-10, 450)),
                          (short)(exact || rnd() % 2 ? 320 : rr(0, 400)),
                          (short)(exact || rnd() % 2 ? 240 : rr(0, 300))};
            software_rasterizer.clear_image(&r, (u_char)rnd(), (u_char)rnd(), (u_char)rnd());
        } else if (pick < 23) {
            PS1_RECT r = {(short)(rnd() % 2 ? 0 : rr(0, 900)),
                          (short)(rnd() % 2 ? (rnd() % 2 ? 0 : 256) : rr(0, 450)),
                          (short)(rnd() % 2 ? 320 : rr(1, 120)),
                          (short)(rnd() % 2 ? 240 : rr(1, 60))};
            if (r.x + r.w > 1024 || r.y + r.h > 512) {
                continue;
            }
            software_rasterizer.store_image(&r, (u_long *)store);
            if (n_obs < max_obs) {
                obs[n_obs++] = fnv(1469598103934665603ull, store,
                                   (size_t)r.w * (size_t)r.h * 2);
            }
        } else if (pick < 24 && gutter_on) {
            int y0 = rr(0, 511), y1 = rr(0, 512);
            SoftwareRasterizer_ClearGutterRows(y0, y1);
        } else if (pick < 25 && rnd() % 10 == 0) {
            defer_on = !defer_on;
            SoftwareRasterizer_SetDefer(defer && defer_on);
        } else if (pick < 33) {
            int m = (int)(rnd() % 10);
            int x, y, ww, hh;
            int gut = rnd() % 4 != 0;

            if (m < 5) {
                x = 0; y = rnd() % 2 ? 0 : 256; ww = 320; hh = 240;
            } else if (m < 8) {
                x = rr(0, 900); y = rr(0, 450); ww = rr(1, 200); hh = rr(1, 100);
            } else {
                x = rr(0, 1000); y = rr(0, 500); ww = rr(1, 30); hh = rr(1, 30);
            }
            observe(defer, obs, &n_obs, max_obs, x, y, ww, hh, gut && gutter_on);
        } else {
            int n = gen_prim(w, ay, 0);
            software_rasterizer.push_prim(w, n);
        }
    }
    if (defer) {
        unsigned long q, dr, ru, sl, ol;

        SoftwareRasterizer_SyncAll();
        SoftwareRasterizer_GetDeferStats(&q, &dr, &ru, &sl, &ol, 1);
        *dropped = dr;
    }
    observe(0, obs, &n_obs, max_obs, 0, 0, 1024, 512, 1);
    return n_obs;
}

static void test_fuzz(void) {
    static uint64_t a[60000], b[60000];

    for (uint64_t seed = 1; seed <= 4; seed++) {
        int gutter_on = (int)(seed & 1);
        unsigned long d0 = 0, d1 = 0;
        int na = run(seed, 30000, 0, gutter_on, a, 60000, &d0);
        int nb = run(seed, 30000, 1, gutter_on, b, 60000, &d1);
        char msg[96];

        snprintf(msg, sizeof(msg), "seed %llu: same number of observations",
                 (unsigned long long)seed);
        check(na == nb, msg);
        snprintf(msg, sizeof(msg), "seed %llu (gutters %d): presents and reads match",
                 (unsigned long long)seed, gutter_on);
        check(na == nb && memcmp(a, b, (size_t)na * sizeof(a[0])) == 0, msg);
        snprintf(msg, sizeof(msg), "seed %llu: deferral dropped primitives",
                 (unsigned long long)seed);
        check(d1 > 0, msg);
    }
}

/* ---- fixed scenes ---------------------------------------------------------- */

static void area(int y0) {
    software_rasterizer.area_start(0, y0);
    software_rasterizer.area_end(319, y0 + 239);
    software_rasterizer.offset(0, 0);
}

static void tile(int x, int y, int w, int h, unsigned bgr) {
    u_long p[3] = {((u_long)0x60 << 24) | bgr, xy(x, y), xy(w, h)};
    software_rasterizer.push_prim(p, 3);
}

static void quad(int x0, int y0, int x1, int y1, unsigned bgr) {
    u_long p[5] = {((u_long)0x28 << 24) | bgr, xy(x0, y0), xy(x1, y0),
                   xy(x0, y1), xy(x1, y1)};
    software_rasterizer.push_prim(p, 5);
}

/* frames into two buffers; only the shown one is read */
static uint64_t scene_frames(int defer, unsigned long *dropped) {
    uint64_t h = 0;
    unsigned long q, dr, ru, sl, ol;

    SoftwareRasterizer_Reset();
    SoftwareRasterizer_SetDefer(defer);
    for (int f = 0; f < 8; f++) {
        int y0 = (f & 1) ? 256 : 0;

        area(y0);
        tile(0, y0, 320, 240, 0x101010 + (unsigned)f);
        quad(10 + f, y0 + 10, 200, y0 + 100, 0x30A0F0 + (unsigned)f * 7);
        quad(50, y0 + 60, 300 - f, y0 + 200, 0x80F030);
        if (f % 4 == 3) { /* a present of the frame just drawn */
            if (defer) {
                SoftwareRasterizer_SyncRect(0, y0, 320, 240, 0);
            }
            h = h * 31 + hash_rect(0, y0, 320, 240, 0);
        }
    }
    if (defer) {
        SoftwareRasterizer_SyncAll();
    }
    h ^= hash_rect(0, 0, 1024, 512, 0);
    SoftwareRasterizer_GetDeferStats(&q, &dr, &ru, &sl, &ol, 1);
    *dropped = dr;
    return h;
}

/* a texture rewritten after the primitives that sample it were queued: they
 * still draw the old texels */
static uint64_t scene_texture_rewrite(int defer) {
    u16 tex[16 * 16];
    PS1_RECT r = {512, 0, 16, 16};
    u_long mode;
    uint64_t h;

    SoftwareRasterizer_Reset();
    SoftwareRasterizer_SetDefer(defer);
    area(0);
    mode = (8u) | (2u << 7); /* page x 512, y 0, 15-bit */
    software_rasterizer.texpage_mode((ParamDrawTexpageMode *)&mode);
    for (int i = 0; i < 256; i++) {
        tex[i] = (u16)(0x8000 | (0x001F + i));
    }
    software_rasterizer.load_image(&r, (u_long *)tex);
    {
        u_long p[4] = {((u_long)0x64 << 24) | 0x808080, xy(20, 20),
                       (u_long)0, xy(16, 16)};
        /* textured rectangle, 15-bit page at (512,0), u = v = 0 */
        software_rasterizer.push_prim(p, 4);
    }
    for (int i = 0; i < 256; i++) {
        tex[i] = (u16)(0x8000 | 0x7C00 | i);
    }
    software_rasterizer.load_image(&r, (u_long *)tex); /* after the sprite */
    if (defer) {
        SoftwareRasterizer_SyncAll();
    }
    h = hash_rect(0, 0, 320, 240, 0);
    return h;
}

/* MoveImage out of a buffer that was never drawn gets the picture */
static uint64_t scene_move(int defer) {
    PS1_RECT src = {0, 0, 320, 240};
    uint64_t h;

    SoftwareRasterizer_Reset();
    SoftwareRasterizer_SetDefer(defer);
    area(0);
    tile(0, 0, 320, 240, 0x204060);
    quad(30, 30, 120, 90, 0x55AA55);
    software_rasterizer.move_image(&src, 0, 256);
    if (defer) {
        SoftwareRasterizer_SyncAll();
    }
    h = hash_rect(0, 256, 320, 240, 0);
    return h;
}

static void test_scenes(void) {
    unsigned long d0, d1;
    uint64_t a = scene_frames(0, &d0);
    uint64_t b = scene_frames(1, &d1);

    check(a == b, "frames: deferred and immediate pictures match");
    check(d1 >= 8, "frames: the covered frames were dropped, not drawn");
    check(scene_texture_rewrite(0) == scene_texture_rewrite(1),
          "texture rewrite keeps the old texels for queued primitives");
    check(scene_move(0) == scene_move(1), "MoveImage reads a deferred frame");
}

int main(void) {
    test_scenes();
    test_fuzz();
    SoftwareRasterizer_SetDefer(0);
    if (failures) {
        fprintf(stderr, "%d failure(s)\n", failures);
        return 1;
    }
    printf("OK: software rasterizer deferred drawing\n");
    return 0;
}
