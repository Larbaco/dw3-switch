/* Host proofs of the P6.32 map view core (port/map_view/map_view_core.c):
 * compose, disc TIM load, scalers, view maths and the cache file. Plain C,
 * no game code.
 *   map_view_core_test TMPDIR                  the self test
 *   map_view_core_test disc SHEET IMAGES OUT   composes the real map from two
 *       files of the owner's runtime and writes OUT (PPM) plus timings (a
 *       manual check, not part of the test run). */
#include "map_view_core.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int g_fail;
#define CHECK(c, ...) do { if (!(c)) { printf("FAIL %s:%d ", __FILE__, __LINE__); \
    printf(__VA_ARGS__); printf("\n"); g_fail = 1; } } while (0)

static void put32(uint8_t *p, uint32_t v) {
    p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8);
    p[2] = (uint8_t)(v >> 16); p[3] = (uint8_t)(v >> 24);
}
static void put16(uint8_t *p, unsigned v) { p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8); }

/* A sheet with one frame of two 8-bit parts side by side, 4x2 texels each. */
static size_t build_sheet(uint8_t *s) {
    memset(s, 0, 256);
    put32(s + 0, 64);        /* parts */
    put32(s + 4, 32);        /* ids */
    put32(s + 8, 128);       /* frame 0 */
    s[32] = 0; s[33] = 0xff;
    put16(s + 64 + 0, 0); put16(s + 64 + 2, 0); put16(s + 64 + 4, 4);
    put16(s + 64 + 6, 2); put16(s + 64 + 8, 0); put16(s + 64 + 10, 256);
    put16(s + 64 + 12, 1);
    put16(s + 78 + 0, 4); put16(s + 78 + 2, 0); put16(s + 78 + 4, 4);
    put16(s + 78 + 6, 2); put16(s + 78 + 8, 0); put16(s + 78 + 10, 256);
    put16(s + 78 + 12, 1);
    put16(s + 128, 2); put16(s + 130, 0); put16(s + 132, 0xffff);
    put16(s + 134, 0); put16(s + 136, 10); put16(s + 138, 20);   /* part 0 at 10,20 */
    put16(s + 140, 1); put16(s + 142, 14); put16(s + 144, 20);   /* part 1 at 14,20 */
    return 256;
}

static void test_compose(void) {
    static uint8_t sheet[256];
    uint16_t *vram = (uint16_t *)calloc((size_t)MV_VRAM_W * MV_VRAM_H, 2);
    MvImage img;
    MvBox box;
    size_t len = build_sheet(sheet);
    int x, y;

    /* 8-bit texels at page 0x280: texel x has index x + 1 (two per halfword);
     * palette row 256 at x 0x140 */
    for (y = 0; y < 2; ++y)
        for (x = 0; x < 4; ++x)
            vram[y * MV_VRAM_W + 0x280 + x] =
                (uint16_t)((2 * x + 1) | ((2 * x + 2) << 8));
    for (x = 0; x < 9; ++x)
        vram[256 * MV_VRAM_W + 0x140 + x] = (uint16_t)((x * 3) | ((x * 3) << 5));
    CHECK(dw3_mv_sheet_frame_box((const int32_t *)sheet, len, 0, &box) == 0, "box");
    CHECK(box.x0 == 10 && box.y0 == 20 && box.x1 == 18 && box.y1 == 22,
          "box %d %d %d %d", box.x0, box.y0, box.x1, box.y1);
    CHECK(dw3_mv_compose(vram, (const int32_t *)sheet, len, 0, 0x280, 0, 0x140,
                         0x100, &img, &box) == 0, "compose");
    CHECK(img.w == 8 && img.h == 2, "size %dx%d", img.w, img.h);
    for (x = 0; x < 8 && img.rgba; ++x) {
        unsigned v = (unsigned)(x + 1) * 3u;
        unsigned want = (v * 255u + 15u) / 31u;

        CHECK(img.rgba[x * 4] == want && img.rgba[x * 4 + 1] == want &&
              img.rgba[x * 4 + 2] == 0 && img.rgba[x * 4 + 3] == 255,
              "pixel %d: %u %u want %u", x, img.rgba[x * 4], img.rgba[x * 4 + 1], want);
    }
    CHECK(dw3_mv_sheet_frame_box((const int32_t *)sheet, len, 5, &box) != 0, "bad frame");
    dw3_mv_image_free(&img);
    free(vram);
}

/* The reveal layer: a frame blitted over a base keeps the base where the cell
 * is transparent (VRAM colour 0), honours the origin and clips. */
static void test_blit(void) {
    static uint8_t sheet[256];
    uint16_t *vram = (uint16_t *)calloc((size_t)MV_VRAM_W * MV_VRAM_H, 2);
    MvImage base;
    uint8_t keep[16 * 8 * 4];
    size_t len = build_sheet(sheet);
    int x, y;

    for (y = 0; y < 2; ++y)
        for (x = 0; x < 4; ++x)
            vram[y * MV_VRAM_W + 0x280 + x] =
                (uint16_t)((2 * x + 1) | ((2 * x + 2) << 8));
    for (x = 0; x < 9; ++x)
        vram[256 * MV_VRAM_W + 0x140 + x] = (uint16_t)((x * 3) | ((x * 3) << 5));
    vram[256 * MV_VRAM_W + 0x140 + 3] = 0; /* texel index 3: transparent */
    base.rgba = (uint8_t *)malloc(16 * 8 * 4);
    base.w = 16; base.h = 8; base.ox = base.oy = 0; base.reveal = 0;
    memset(base.rgba, 0x55, 16 * 8 * 4);
    memcpy(keep, base.rgba, sizeof(keep));
    /* the sheet's frame 0: two cells at (10,20) and (14,20), 4x2 texels each
     * (8 px wide in all); the origin goes on pixel (2 - 10, 3 - 20) so the
     * first texel lands on pixel (2, 3) */
    CHECK(dw3_mv_blit_frame(vram, (const int32_t *)sheet, len, 0, 0x280, 0,
                            0x140, 0x100, &base, 2 - 10, 3 - 20) == 0, "blit");
    for (x = 0; x < 16; ++x) {
        const uint8_t *d = base.rgba + ((size_t)3 * 16 + (size_t)x) * 4;
        int cell = x - 2;

        if (cell >= 0 && cell < 8 && cell != 2) {
            unsigned v = (unsigned)(cell + 1) * 3u;
            unsigned want = (v * 255u + 15u) / 31u;

            CHECK(d[0] == want && d[1] == want, "blit pixel %d: %u want %u", x,
                  d[0], want);
        } else {
            CHECK(memcmp(d, keep, 4) == 0, "blit leaves pixel %d alone", x);
        }
    }
    CHECK(memcmp(base.rgba, keep, 16 * 3 * 4) == 0, "rows above untouched");
    CHECK(dw3_mv_blit_frame(vram, (const int32_t *)sheet, len, 9, 0x280, 0,
                            0x140, 0x100, &base, 0, 0) != 0, "bad frame");
    memcpy(keep, base.rgba, sizeof(keep));
    CHECK(dw3_mv_blit_frame(vram, (const int32_t *)sheet, len, 0, 0x280, 0,
                            0x140, 0x100, &base, 500, -500) == 0, "clip");
    CHECK(memcmp(base.rgba, keep, sizeof(keep)) == 0, "clipped blit is a no-op");
    dw3_mv_image_free(&base);
    free(vram);
}

static void test_hash(void) {
    int a[8] = {1, 0, 171, 65, 2, 3, 60, 53};
    int b[8] = {1, 0, 171, 65, 2, 3, 60, 54};
    uint32_t ha = dw3_mv_hash(a, sizeof(a), 0), hb = dw3_mv_hash(b, sizeof(b), 0);

    CHECK(ha != hb, "one coordinate changes the hash");
    CHECK(ha == dw3_mv_hash(a, sizeof(a), 0), "stable");
    CHECK(ha != 0, "non-zero");
}

static void test_smooth_zoom(void) {
    MvView v;
    float z, fit;
    int i;

    v.step = 1; v.zoom = 0.0f; v.cx = 196; v.cy = 176;
    fit = dw3_mv_view_scale(0, 392, 352, 1280, 720) * 240.0f / 720.0f;
    z = dw3_mv_view_cur_zoom(&v, 392, 352, 1280, 720);
    CHECK(fabsf(z - 1.0f) < 1e-4f, "step 1 is zoom 1.0 (%f)", z);
    dw3_mv_view_zoom_by(&v, 1.1f, 392, 352, 1280, 720);
    z = dw3_mv_view_cur_zoom(&v, 392, 352, 1280, 720);
    CHECK(fabsf(z - 1.1f) < 1e-4f && v.zoom > 0.0f && v.step == 1,
          "smooth zoom 1.1 keeps a non-integer value (%f, step %d)", z, v.step);
    dw3_mv_view_zoom(&v, 1, 392, 352, 1280, 720);
    CHECK(v.zoom == 0.0f && v.step == 2, "button from 1.1 goes to 1.25 (step %d)",
          v.step);
    dw3_mv_view_zoom_by(&v, 1.1f, 392, 352, 1280, 720);
    dw3_mv_view_zoom(&v, -1, 392, 352, 1280, 720);
    CHECK(v.zoom == 0.0f && v.step == 2, "button from 1.375 goes down to 1.25 (step %d)",
          v.step);
    for (i = 0; i < 100; ++i) dw3_mv_view_zoom_by(&v, 1.2f, 392, 352, 1280, 720);
    z = dw3_mv_view_cur_zoom(&v, 392, 352, 1280, 720);
    CHECK(fabsf(z - 3.0f) < 1e-4f, "smooth zoom is capped at 3.0 (%f)", z);
    for (i = 0; i < 100; ++i) dw3_mv_view_zoom_by(&v, 0.8f, 392, 352, 1280, 720);
    CHECK(v.step == 0 && v.zoom == 0.0f, "smooth zoom out ends on FIT");
    z = dw3_mv_view_cur_zoom(&v, 392, 352, 1280, 720);
    CHECK(fabsf(z - fit) < 1e-4f, "FIT zoom %f vs %f", z, fit);
}

/* The name box must not flip while the stick zooms and the view follows the
 * cursor (owner OLED report, preview.47/48). The cursor stays on a location;
 * the view re-centres as the zoom changes, moving the cursor's screen row
 * across the middle of the picture. */
static void test_namebox_stick_zoom(void) {
    const int mw = 392, mh = 352, vw = 1280, vh = 720;
    MvView v;
    MvNameBox nb;
    int i, flips = 0, first = -1, last = -1, pass;
    float cur_sy = 176.0f; /* the cursor, a source pixel row (map middle) */

    memset(&nb, 0, sizeof(nb));
    v.step = 0; v.zoom = 0.0f; v.cx = 196; v.cy = 176;
    for (pass = 0; pass < 2; ++pass) {
        for (i = 0; i < 400; ++i) {
            float f = pass ? 0.99f : 1.01f, sc, vy;
            int b;

            dw3_mv_view_zoom_by(&v, f, mw, mh, vw, vh);
            /* follow_cursor: keep the cursor inside the middle 40 % */
            sc = dw3_mv_view_cur_scale(&v, mw, mh, vw, vh);
            vy = (float)vh / 2.0f + (cur_sy - v.cy) * sc;
            if (vy < 0.3f * vh) v.cy -= (0.3f * vh - vy) / sc;
            if (vy > 0.7f * vh) v.cy += (vy - 0.7f * vh) / sc;
            dw3_mv_view_clamp(&v, mw, mh, vw, vh);
            vy = (float)vh / 2.0f + (cur_sy - v.cy) * sc;
            b = dw3_mv_namebox_bottom(&nb, 7, vy / (float)vh * 240.0f);
            if (first < 0) first = b;
            if (b != last && last >= 0) ++flips;
            last = b;
        }
    }
    CHECK(flips == 0, "name box flipped %d times during a stick zoom", flips);
    /* leaving and entering again re-decides */
    dw3_mv_namebox_bottom(&nb, 0, 200.0f);
    CHECK(dw3_mv_namebox_bottom(&nb, 3, 50.0f) == 1, "entry in the top half -> bottom");
    dw3_mv_namebox_bottom(&nb, 0, 50.0f);
    CHECK(dw3_mv_namebox_bottom(&nb, 3, 200.0f) == 0, "entry in the bottom half -> top");
    CHECK(dw3_mv_namebox_bottom(&nb, 4, 10.0f) == 0, "location to location keeps it (as the game)");
}

/* P6.32 (owner OLED, preview.48): the cursor and the balloon keep one
 * on-screen size while the map zooms; their anchor follows the map. */
static void test_fixed_sprites(void) {
    const int mw = 392, mh = 352, vw = 1280, vh = 720;
    const float ax = 100.0f, ay = 120.0f; /* anchor in map pixels */
    const int ox = -8, oy = -8, w = 16, h = 16; /* a 16x16 sprite centred on it */
    float zooms[4] = {1.0f, 1.5f, 2.0f, 3.0f};
    float x0, y0, x1, y1, rx0, ry0, rx1, ry1, k, sc, w1 = 0.0f;
    int i;
    MvView v;

    memset(&v, 0, sizeof(v));
    for (i = 0; i < 4; ++i) {
        v.step = 1;
        v.zoom = zooms[i] == 1.0f ? 0.0f : zooms[i];
        v.cx = ax; v.cy = ay; /* view centred on the anchor, then clamped */
        dw3_mv_view_clamp(&v, mw, mh, vw, vh);
        sc = dw3_mv_view_cur_scale(&v, mw, mh, vw, vh);
        k = dw3_mv_sprite_factor(&v, mw, mh, vw, vh);
        CHECK(fabsf(k - 1.0f / zooms[i]) < 1e-4f, "k = 1/Z at %.1f: %f", (double)zooms[i], (double)k);
        dw3_mv_sprite_rect(&v, mw, mh, vw, vh, ax, ay, ox, oy, w, h, 1, &x0, &y0, &x1, &y1);
        dw3_mv_view_rect(&v, mw, mh, vw, vh, &rx0, &ry0, &rx1, &ry1);
        /* the sprite's centre is the anchor's place on the map picture */
        CHECK(fabsf((x0 + x1) * 0.5f - (rx0 + sc * ax)) < 1e-2f, "x follows the map");
        CHECK(fabsf((y0 + y1) * 0.5f - (ry0 + sc * ay)) < 1e-2f, "y follows the map");
        /* one size at every zoom: 16 source px at the zoom 1.0 scale */
        if (i == 0) w1 = x1 - x0;
        CHECK(fabsf((x1 - x0) - w1) < 1e-2f && fabsf((y1 - y0) - w1) < 1e-2f,
              "size is constant: %f vs %f", (double)(x1 - x0), (double)w1);
        CHECK(fabsf(w1 - 16.0f * dw3_mv_view_scale(1, mw, mh, vw, vh)) < 1e-2f, "size at 1.0");
        /* off: the sprite scales with the map */
        dw3_mv_sprite_rect(&v, mw, mh, vw, vh, ax, ay, ox, oy, w, h, 0, &x0, &y0, &x1, &y1);
        CHECK(fabsf((x1 - x0) - 16.0f * sc) < 1e-2f, "unfixed scales with the map");
    }
    /* FIT: the factor is 1/Z with Z < 1, so the sprite is as big as at 1.0 */
    memset(&v, 0, sizeof(v));
    v.cx = ax; v.cy = ay;
    dw3_mv_view_clamp(&v, mw, mh, vw, vh);
    dw3_mv_sprite_rect(&v, mw, mh, vw, vh, ax, ay, ox, oy, w, h, 1, &x0, &y0, &x1, &y1);
    CHECK(fabsf((x1 - x0) - w1) < 1e-2f, "size at FIT equals size at 1.0");
}

static void test_disc(void) {
    static uint8_t file[4096];
    uint16_t *vram = (uint16_t *)calloc((size_t)MV_VRAM_W * MV_VRAM_H, 2);
    uint8_t *a = file + 64;
    uint8_t *t0 = a + 16, *t1;
    uint8_t raw[32];
    uint8_t *d;
    size_t i;

    memset(file, 0, sizeof(file));
    put32(file, 64);
    put32(a, 16);                  /* TIM 0 at +16: 8-bit with a CLUT */
    put32(t0, 0x10); put32(t0 + 4, 9);
    put32(t0 + 8, 12 + 4); put16(t0 + 12, 0); put16(t0 + 14, 0);
    put16(t0 + 16, 2); put16(t0 + 18, 1); put16(t0 + 20, 0x1111); put16(t0 + 22, 0x2222);
    put32(t0 + 24, 12 + 8); put16(t0 + 28, 0); put16(t0 + 30, 0);
    put16(t0 + 32, 2); put16(t0 + 34, 2);
    put16(t0 + 36, 0xAAAA); put16(t0 + 38, 0xBBBB);
    put16(t0 + 40, 0xCCCC); put16(t0 + 42, 0xDDDD);
    t1 = t0 + 44 + 4;
    put32(a + 4, (uint32_t)(t1 - a));  /* TIM 1: RLEN, no CLUT */
    put32(t1, 0x4E454C52); put32(t1 + 4, 0);
    memset(raw, 0, sizeof(raw));
    put32(raw, 0x10); put32(raw + 4, 1);
    put32(raw + 8, 12 + 2); put16(raw + 12, 0); put16(raw + 14, 0);
    put16(raw + 16, 1); put16(raw + 18, 1); put16(raw + 20, 0x7777);
    d = t1 + 8;
    *d++ = 22;
    for (i = 0; i < 22; ++i) *d++ = raw[i];
    *d++ = 0;
    put32(a + 8, 0);
    CHECK(dw3_mv_disc_load_map_vram(file, sizeof(file), vram) == 2, "two TIMs");
    CHECK(vram[0 * MV_VRAM_W + 0x280] == 0xAAAA && vram[0 * MV_VRAM_W + 0x281] == 0xBBBB &&
          vram[1 * MV_VRAM_W + 0x280] == 0xCCCC, "image 0");
    CHECK(vram[0x100 * MV_VRAM_W + 0x140] == 0x1111 &&
          vram[0x100 * MV_VRAM_W + 0x141] == 0x2222, "clut 0");
    CHECK(vram[0 * MV_VRAM_W + 0x2C0] == 0x7777, "image 1 at +0x40 (RLEN)");
    free(vram);
}

static void make_image(MvImage *img, int w, int h, int pattern) {
    int x, y;

    img->w = w; img->h = h; img->ox = img->oy = 0;
    img->rgba = (uint8_t *)malloc((size_t)w * h * 4);
    for (y = 0; y < h; ++y) for (x = 0; x < w; ++x) {
        uint8_t *p = img->rgba + ((size_t)y * w + x) * 4;
        unsigned v = pattern == 0 ? 120u : (((x / 4 + y / 4) & 1) ? 230u : 20u);
        p[0] = (uint8_t)v; p[1] = (uint8_t)(v / 2 + 10); p[2] = (uint8_t)(255 - v);
        p[3] = 255;
    }
}

static void test_scalers(void) {
    MvImage src, dst;
    int s, i, bad;
    double ms;

    make_image(&src, 64, 48, 0);
    for (s = 0; s < MV_SCALER_COUNT; ++s) {
        int want_w = s == MV_SCALER_NEAREST ? 64 : 64 * MV_SCALE;
        int want_h = s == MV_SCALER_NEAREST ? 48 : 48 * MV_SCALE;

        CHECK(dw3_mv_scale(s, &src, &dst, &ms) == 0, "scale %d", s);
        CHECK(dst.w == want_w && dst.h == want_h, "scaler %d size %dx%d", s, dst.w, dst.h);
        bad = 0;
        for (i = 0; i < dst.w * dst.h && dst.rgba; ++i) {
            const uint8_t *p = dst.rgba + (size_t)i * 4;
            if (abs(p[0] - 120) > 1 || abs(p[1] - 70) > 1 || abs(p[2] - 135) > 1 || p[3] != 255) bad++;
        }
        CHECK(bad == 0, "scaler %s: flat colour changed in %d px", dw3_mv_scaler_name(s), bad);
        dw3_mv_image_free(&dst);
    }
    dw3_mv_image_free(&src);

    /* a checkerboard keeps (about) its range through every scaler */
    make_image(&src, 64, 48, 1);
    for (s = 1; s < MV_SCALER_COUNT; ++s) {
        int lo = 255, hi = 0;

        CHECK(dw3_mv_scale(s, &src, &dst, &ms) == 0, "scale cb %d", s);
        for (i = 0; i < dst.w * dst.h && dst.rgba; ++i) {
            int v = dst.rgba[(size_t)i * 4];
            if (v < lo) lo = v;
            if (v > hi) hi = v;
        }
        CHECK(lo <= 40 && hi >= 210, "scaler %d range %d..%d", s, lo, hi);
        dw3_mv_image_free(&dst);
    }
    dw3_mv_image_free(&src);
}

static void test_view(void) {
    MvView v;
    float x0, y0, x1, y1, s;

    v.step = 0; v.cx = 0; v.cy = 0;
    dw3_mv_view_clamp(&v, 392, 320, 1280, 720);
    s = dw3_mv_view_scale(0, 392, 320, 1280, 720);
    CHECK(fabsf(s - 2.25f) < 1e-4f, "fit scale %f", s);
    CHECK(v.cx == 196.0f && v.cy == 160.0f, "fit centred %f %f", v.cx, v.cy);
    dw3_mv_view_rect(&v, 392, 320, 1280, 720, &x0, &y0, &x1, &y1);
    CHECK(fabsf(y0) < 0.01f && fabsf(y1 - 720.0f) < 0.01f &&
          fabsf((x0 + x1) / 2 - 640.0f) < 0.01f,
          "fit rect %f %f %f %f", x0, y0, x1, y1);
    dw3_mv_view_zoom(&v, 1, 392, 320, 1280, 720);        /* 1.0 */
    CHECK(v.step == 1, "step 1");
    s = dw3_mv_view_scale(1, 392, 320, 1280, 720);
    CHECK(fabsf(s - 3.0f) < 1e-4f, "1.0 scale %f", s);
    v.cx = -50; v.cy = 9999;
    dw3_mv_view_clamp(&v, 392, 320, 1280, 720);
    CHECK(v.cx == 196.0f, "392 px at 3 px each is narrower than 1280: centred, %f", v.cx);
    CHECK(fabsf(v.cy - 200.0f) < 0.1f, "clamp y %f", v.cy);
    dw3_mv_view_rect(&v, 392, 320, 1280, 720, &x0, &y0, &x1, &y1);
    CHECK(fabsf(x0 - 52.0f) < 0.01f && fabsf(x1 - 1228.0f) < 0.01f &&
          y0 <= 0.01f && y1 >= 719.99f,
          "map at 1.0: 52 px bars, fills the height: %f %f %f %f", x0, y0, x1, y1);
    for (s = 0; s < 6; ++s) dw3_mv_view_zoom(&v, 1, 392, 320, 1280, 720);
    CHECK(v.step == MV_ZOOM_STEPS - 1, "zoom capped at %d", v.step);
    dw3_mv_view_zoom(&v, -1, 392, 320, 1280, 720);
    CHECK(v.step == MV_ZOOM_STEPS - 2, "zoom out");
    CHECK(dw3_mv_scaler_step(MV_SCALER_FSR, 1) == MV_SCALER_NEAREST, "wrap up");
    CHECK(dw3_mv_scaler_step(MV_SCALER_NEAREST, -1) == MV_SCALER_FSR, "wrap down");
}

static void test_cache(const char *dir) {
    MvImage a, b;
    char path[512];
    FILE *f;
    uint8_t junk[4] = {1, 2, 3, 4};

    make_image(&a, 17, 9, 1);
    a.ox = 3;
    a.oy = -4;
    a.reveal = 0xC0FFEE11u;
    dw3_mv_cache_name(path, sizeof(path), dir, "us", 1, MV_SCALER_FSR);
    CHECK(strstr(path, "map_us_h1_s3.rgba") != NULL, "cache name %s", path);
    CHECK(dw3_mv_cache_write(path, &a) == 0, "cache write");
    CHECK(dw3_mv_cache_read(path, &b) == 0, "cache read");
    CHECK(b.w == 17 && b.h == 9 && b.ox == 3 && b.oy == -4 && b.reveal == 0xC0FFEE11u && b.rgba && memcmp(a.rgba, b.rgba, 17 * 9 * 4) == 0,
          "cache round trip");
    dw3_mv_image_free(&b);
    f = fopen(path, "wb");
    if (f) { fwrite(junk, 1, 4, f); fclose(f); }
    CHECK(dw3_mv_cache_read(path, &b) != 0, "a bad cache file is refused");
    remove(path);
    dw3_mv_image_free(&a);
}

static int disc_mode(const char *sheet_path, const char *img_path, const char *out_path) {
    FILE *f;
    long n1, n2;
    uint8_t *sheet, *imgs;
    uint16_t *vram = (uint16_t *)calloc((size_t)MV_VRAM_W * MV_VRAM_H, 2);
    MvImage base, scaled;
    MvBox box;
    int placed, s, x;
    double ms;

    f = fopen(sheet_path, "rb"); if (!f) return 2;
    fseek(f, 0, SEEK_END); n1 = ftell(f); fseek(f, 0, SEEK_SET);
    sheet = (uint8_t *)malloc((size_t)n1); n1 = (long)fread(sheet, 1, (size_t)n1, f); fclose(f);
    f = fopen(img_path, "rb"); if (!f) return 2;
    fseek(f, 0, SEEK_END); n2 = ftell(f); fseek(f, 0, SEEK_SET);
    imgs = (uint8_t *)malloc((size_t)n2); n2 = (long)fread(imgs, 1, (size_t)n2, f); fclose(f);
    placed = dw3_mv_disc_load_map_vram(imgs, (size_t)n2, vram);
    printf("disc: %d TIMs placed (%ld + %ld bytes)\n", placed, n1, n2);
    {
        /* entry 0 of the sheet file is the map's sheet */
        const int32_t *table = (const int32_t *)sheet;
        const int32_t *entry = (const int32_t *)(sheet + table[0]);
        size_t len = (size_t)n1 - (size_t)table[0];

        if (dw3_mv_compose(vram, entry, len, 0, 0x280, 0, 0x140, 0x100, &base, &box) != 0) {
            printf("compose failed\n");
            return 3;
        }
    }
    printf("map: %dx%d, box %d,%d - %d,%d\n", base.w, base.h, box.x0, box.y0, box.x1, box.y1);
    for (s = 1; s < MV_SCALER_COUNT; ++s) {
        if (dw3_mv_scale(s, &base, &scaled, &ms) != 0) return 4;
        printf("scaler %s: %.1f ms (%dx%d)\n", dw3_mv_scaler_name(s), ms, scaled.w, scaled.h);
        if (s == MV_SCALER_FSR || getenv("MV_DUMP_ALL")) {
            char name[600];
            snprintf(name, sizeof(name), "%s.%d.ppm", out_path, s);
            f = fopen(name, "wb");
            fprintf(f, "P6\n%d %d\n255\n", scaled.w, scaled.h);
            for (x = 0; x < scaled.w * scaled.h; ++x) fwrite(scaled.rgba + (size_t)x * 4, 1, 3, f);
            fclose(f);
        }
        dw3_mv_image_free(&scaled);
    }
    f = fopen(out_path, "wb");
    fprintf(f, "P6\n%d %d\n255\n", base.w, base.h);
    for (x = 0; x < base.w * base.h; ++x) fwrite(base.rgba + (size_t)x * 4, 1, 3, f);
    fclose(f);
    return 0;
}

int main(int argc, char **argv) {
    if (argc >= 5 && strcmp(argv[1], "disc") == 0) return disc_mode(argv[2], argv[3], argv[4]);
    test_compose();
    test_disc();
    test_scalers();
    test_view();
    test_blit();
    test_hash();
    test_smooth_zoom();
    test_namebox_stick_zoom();
    test_fixed_sprites();
    test_cache(argc > 1 ? argv[1] : "/mnt/cabal/tmp");
    if (!g_fail) printf("OK: map view core\n");
    return g_fail;
}
