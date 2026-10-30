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

    memset(&v, 0, sizeof(v));
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
    memset(&v, 0, sizeof(v));
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

/* P6.32 phase 2: the PS1 cursor's reach and the area-name text for the log. */
static void test_reach_and_text(void) {
    /* "Bulk Swamp", a line break, "42" */
    unsigned short n1[] = { 0x8261, 0x8295, 0x828C, 0x828B, 0x8140, 0x8272, 0x8297, 0x8281,
                            0x828D, 0x8290, 0x0A, 0x8253, 0x8251 };
    unsigned short n2[] = { 0x8261, 0x8001, 0x8260, 0x8005, 0x8013, 0x8051, 0x80C8, 0x1234 };
    char out[64];
    int odd;

    /* a hit box (x + 7..x + 18, y + 7..y + 18) in x 22..370 and y 15..290 */
    CHECK(dw3_mv_hitbox_reach(35, 100) == 0, "spot 35,100 inside");
    CHECK(dw3_mv_hitbox_reach(340, 105) == 0, "the rightmost real spot inside");
    CHECK(dw3_mv_hitbox_reach(15, 100) == 0, "box starts at x 22: inside");
    CHECK(dw3_mv_hitbox_reach(14, 100) == 1, "box x 21..32 starts in the strip: partly");
    CHECK(dw3_mv_hitbox_reach(-30, 100) == 2, "box ends before x 22: outside");
    CHECK(dw3_mv_hitbox_reach(363, 100) == 1, "box x 370..381: partly");
    CHECK(dw3_mv_hitbox_reach(364, 100) == 2, "box starts at x 371: outside");
    CHECK(dw3_mv_hitbox_reach(100, -12) == 2, "box y -5..6 above y 15: outside");
    CHECK(dw3_mv_hitbox_reach(100, 5) == 1, "box y 12..23 crosses y 15: partly");
    CHECK(dw3_mv_hitbox_reach(100, 272) == 0, "box ends at y 290: inside");
    CHECK(dw3_mv_hitbox_reach(100, 283) == 1, "box y 290..301: partly");
    CHECK(dw3_mv_point_strip(22, 15) == 0 && dw3_mv_point_strip(370, 290) == 0, "reach corners");
    CHECK(dw3_mv_point_strip(21, 100) == 1 && dw3_mv_point_strip(371, 100) == 2, "x strips");
    CHECK(dw3_mv_point_strip(100, 14) == 4 && dw3_mv_point_strip(100, 291) == 8, "y strips");
    CHECK(dw3_mv_point_strip(0, 0) == 5 && dw3_mv_point_strip(391, 351) == 10, "corners");
    odd = dw3_mv_name_text(n1, (int)(sizeof(n1) / sizeof(n1[0])), out, sizeof(out));
    CHECK(odd == 0 && strcmp(out, "Bulk Swamp / 42") == 0, "decoded text: %s (%d)", out, odd);
    odd = dw3_mv_name_text(n2, 8, out, sizeof(out));
    CHECK(odd == 2 && strcmp(out, "B A.'\xC3\xA9<i200><1234>") == 0, "icons and unknown codes: %s (%d)", out, odd);
    dw3_mv_name_text(n1, (int)(sizeof(n1) / sizeof(n1[0])), out, 4);
    CHECK(strlen(out) <= 3, "output is bounded");
}

/* tools/tests/test_map_names.py: `reach FILE` classifies the "x y" lines of FILE */
static int reach_mode(const char *file) {
    FILE *f = fopen(file, "r");
    int x, y, n[3] = { 0, 0, 0 };

    if (!f) return 2;
    while (fscanf(f, "%d %d", &x, &y) == 2) {
        int r = dw3_mv_hitbox_reach(x, y);

        ++n[r];
        if (r) printf("spot %d,%d reach %d\n", x, y, r);
    }
    fclose(f);
    printf("inside %d partly %d outside %d\n", n[0], n[1], n[2]);
    return 0;
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
        int copy = s == MV_SCALER_NEAREST || s == MV_SCALER_BILINEAR;
        int want_w = copy ? 64 : 64 * MV_SCALE;
        int want_h = copy ? 48 : 48 * MV_SCALE;

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

    memset(&v, 0, sizeof(v));
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
    for (s = 0; s < 8; ++s) dw3_mv_view_zoom(&v, 1, 392, 320, 1280, 720);
    CHECK(v.step == MV_ZOOM_TOP_PLAIN, "zoom capped at %d (3.0)", v.step);
    dw3_mv_view_zoom(&v, -1, 392, 320, 1280, 720);
    CHECK(v.step == MV_ZOOM_TOP_PLAIN - 1, "zoom out");
    CHECK(dw3_mv_scaler_step(MV_SCALER_BILINEAR, 1) == MV_SCALER_NEAREST, "wrap up");
    CHECK(dw3_mv_scaler_step(MV_SCALER_NEAREST, -1) == MV_SCALER_BILINEAR, "wrap down");
    CHECK(dw3_mv_scaler_step(MV_SCALER_FSR, 1) == MV_SCALER_BILINEAR, "bilinear follows FSR 1");
    CHECK(dw3_mv_scaler_linear(MV_SCALER_BILINEAR) && !dw3_mv_scaler_linear(MV_SCALER_NEAREST),
          "bilinear filters, nearest does not");
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


/* --- P6.32 phase 3 ------------------------------------------------------------ */

static const int PW = 392, PH = 352, PVW = 1280, PVH = 720;

/* Zoom about the cursor: with the cursor away from the image's edges the cursor
 * stays on the same picture pixel through every step of the ladder, up and down
 * (the PoC zoomed about the view centre); near an edge the pan limit wins and says
 * so; the view never shows past the image. */
static void test_zoom_anchor(void) {
    MvView v;
    float mx = 150.0f, my = 140.0f, px0, py0, px1, py1, wx, wy;
    int step, flags, k;

    memset(&v, 0, sizeof(v));
    v.step = 3; v.cx = 196.0f; v.cy = 176.0f;
    /* the cursor sits left of the middle: the view centre is not on it */
    dw3_mv_view_point(&v, PW, PH, PVW, PVH, mx, my, &px0, &py0);
    CHECK(px0 < PVW * 0.5f, "cursor left of the picture middle (%f)", px0);
    for (k = 0; k < 2; ++k) {
        for (step = 0; step < 4; ++step) {
            dw3_mv_view_point(&v, PW, PH, PVW, PVH, mx, my, &px0, &py0);
            dw3_mv_view_zoom(&v, k ? -1 : 1, PW, PH, PVW, PVH);
            flags = dw3_mv_view_anchor(&v, mx, my, px0, py0, PW, PH, PVW, PVH, &wx, &wy);
            dw3_mv_view_point(&v, PW, PH, PVW, PVH, mx, my, &px1, &py1);
            /* where the whole map fits an axis (1.0 is 1176 px wide in a 1280 picture)
             * the view is centred and the cursor must move: the pan limit wins */
            if (!(flags & (MV_PAN_CENTRED_X | MV_PAN_LEFT | MV_PAN_RIGHT)))
                CHECK(fabsf(px1 - px0) < 0.01f, "zoom %s step %d: cursor x moved %f -> %f (flags %d)",
                      k ? "out" : "in", v.step, px0, px1, flags);
            if (!(flags & (MV_PAN_CENTRED_Y | MV_PAN_TOP | MV_PAN_BOTTOM)))
                CHECK(fabsf(py1 - py0) < 0.01f, "zoom %s step %d: cursor y moved %f -> %f (flags %d)",
                      k ? "out" : "in", v.step, py0, py1, flags);
        }
    }
    /* near the left edge at 3.0: the anchor cannot hold, the clamp does */
    memset(&v, 0, sizeof(v));
    v.step = 3; v.cx = 196.0f; v.cy = 176.0f;
    mx = 12.0f; my = 100.0f;
    dw3_mv_view_point(&v, PW, PH, PVW, PVH, mx, my, &px0, &py0);
    v.cx = 70.0f; /* the view is as far left as it can go at 1.5 (hx = 142 -> clamp) */
    dw3_mv_view_clamp(&v, PW, PH, PVW, PVH);
    dw3_mv_view_point(&v, PW, PH, PVW, PVH, mx, my, &px0, &py0);
    dw3_mv_view_zoom(&v, 1, PW, PH, PVW, PVH);
    dw3_mv_view_zoom(&v, 1, PW, PH, PVW, PVH);
    flags = dw3_mv_view_anchor(&v, mx, my, px0, py0, PW, PH, PVW, PVH, &wx, &wy);
    dw3_mv_view_point(&v, PW, PH, PVW, PVH, mx, my, &px1, &py1);
    CHECK(px1 >= 0.0f && px1 <= (float)PVW, "cursor stays on the picture at the edge (%f)", px1);
    {
        float x0, y0, x1, y1;

        dw3_mv_view_rect(&v, PW, PH, PVW, PVH, &x0, &y0, &x1, &y1);
        CHECK(x0 <= 0.01f && x1 >= PVW - 0.01f && y0 <= 0.01f && y1 >= PVH - 0.01f,
              "the image covers the picture after the zoom (%f %f %f %f)", x0, y0, x1, y1);
    }
    (void)flags;
    /* zooming out to FIT: the map fits, the view centres, the cursor moves on screen */
    for (k = 0; k < 8; ++k) {
        dw3_mv_view_point(&v, PW, PH, PVW, PVH, mx, my, &px0, &py0);
        dw3_mv_view_zoom(&v, -1, PW, PH, PVW, PVH);
        flags = dw3_mv_view_anchor(&v, mx, my, px0, py0, PW, PH, PVW, PVH, &wx, &wy);
    }
    CHECK(v.step == 0, "FIT reached");
    CHECK((flags & MV_PAN_CENTRED_X) != 0, "at FIT the map is narrower than the picture: centred x (flags %d)", flags);
    CHECK(fabsf(v.cx - PW * 0.5f) < 0.01f, "FIT centre x %f", v.cx);
}

/* 4.2g: at every zoom, from FIT to the top, the view never shows past the image,
 * wherever the cursor is (strips included), and the flags name what held it. */
static void test_pan_limits(void) {
    int step, ix, iy;
    MvView v;

    for (step = 0; step <= MV_ZOOM_TOP_PLAIN; ++step) {
        for (ix = -2; ix <= 6; ++ix) {
            for (iy = -2; iy <= 6; ++iy) {
                float x0, y0, x1, y1, s, wx, wy;
                int flags;

                memset(&v, 0, sizeof(v));
                v.step = step;
                v.cx = (float)ix * 100.0f - 50.0f;
                v.cy = (float)iy * 100.0f - 50.0f;
                flags = dw3_mv_view_clamp_ex(&v, PW, PH, PVW, PVH, &wx, &wy);
                s = dw3_mv_view_cur_scale(&v, PW, PH, PVW, PVH);
                dw3_mv_view_rect(&v, PW, PH, PVW, PVH, &x0, &y0, &x1, &y1);
                if (!(flags & MV_PAN_CENTRED_X))
                    CHECK(x0 <= 0.01f && x1 >= PVW - 0.01f, "step %d x: rect %f..%f", step, x0, x1);
                else
                    CHECK(x0 >= -0.01f && x1 <= PVW + 0.01f && fabsf((x0 + x1) * 0.5f - PVW * 0.5f) < 0.01f,
                          "step %d x centred: rect %f..%f", step, x0, x1);
                if (!(flags & MV_PAN_CENTRED_Y))
                    CHECK(y0 <= 0.01f && y1 >= PVH - 0.01f, "step %d y: rect %f..%f", step, y0, y1);
                else
                    CHECK(y0 >= -0.01f && y1 <= PVH + 0.01f && fabsf((y0 + y1) * 0.5f - PVH * 0.5f) < 0.01f,
                          "step %d y centred: rect %f..%f", step, y0, y1);
                /* a flag names an edge the view rests on: the view's edge is the image's */
                if (flags & MV_PAN_LEFT) CHECK(fabsf(x0) < 0.01f, "left flag but the rect starts at %f", x0);
                if (flags & MV_PAN_RIGHT) CHECK(fabsf(x1 - PVW) < 0.01f || x1 >= PVW, "right flag but rect ends %f", x1);
                if (flags & MV_PAN_TOP) CHECK(fabsf(y0) < 0.01f, "top flag but the rect starts at %f", y0);
                if (flags & MV_PAN_BOTTOM) CHECK(y1 >= PVH - 0.01f, "bottom flag but rect ends %f", y1);
                /* and one the view was pushed against when it wanted to go past */
                if (wx < v.cx - 0.001f) CHECK(flags & (MV_PAN_LEFT | MV_PAN_CENTRED_X), "pushed left, no flag (%f -> %f)", wx, v.cx);
                if (wx > v.cx + 0.001f) CHECK(flags & (MV_PAN_RIGHT | MV_PAN_CENTRED_X), "pushed right, no flag (%f -> %f)", wx, v.cx);
                if (wy < v.cy - 0.001f) CHECK(flags & (MV_PAN_TOP | MV_PAN_CENTRED_Y), "pushed up, no flag (%f -> %f)", wy, v.cy);
                if (wy > v.cy + 0.001f) CHECK(flags & (MV_PAN_BOTTOM | MV_PAN_CENTRED_Y), "pushed down, no flag (%f -> %f)", wy, v.cy);
                (void)s;
            }
        }
    }
    /* FIT: the map is 392x352, the picture 16:9: bars at the sides, none above or below */
    memset(&v, 0, sizeof(v));
    v.cx = 0; v.cy = 0;
    {
        int f = dw3_mv_view_clamp_ex(&v, PW, PH, PVW, PVH, NULL, NULL);

        CHECK((f & MV_PAN_CENTRED_X) && (f & MV_PAN_CENTRED_Y),
              "FIT: the whole map fits both axes (bars at the sides; flags %d)", f);
    }
    /* a cursor in the strips: at the top zoom the follow keeps it on the picture and
     * the image's edge stops the view */
    memset(&v, 0, sizeof(v));
    v.step = MV_ZOOM_TOP_PLAIN; v.cx = 196; v.cy = 176;
    for (ix = 0; ix < 4; ++ix) {
        static const float pts[4][2] = {{2, 2}, {390, 2}, {2, 350}, {390, 350}};
        float px, py;

        v.cx = 196; v.cy = 176;
        dw3_mv_view_follow(&v, pts[ix][0], pts[ix][1], PVW * 0.3f, PVH * 0.3f, PW, PH, PVW, PVH, NULL, NULL);
        dw3_mv_view_point(&v, PW, PH, PVW, PVH, pts[ix][0], pts[ix][1], &px, &py);
        CHECK(px >= 0 && px <= PVW && py >= 0 && py <= PVH,
              "cursor in the strip corner %d is on the picture (%f,%f)", ix, px, py);
    }
}

/* 4.4: 4.0 only with a higher-resolution source. */
static void test_zoom_top(void) {
    MvView v;
    int i;

    memset(&v, 0, sizeof(v));
    v.step = 1; v.cx = 196; v.cy = 176;
    for (i = 0; i < 12; ++i) dw3_mv_view_zoom(&v, 1, PW, PH, PVW, PVH);
    CHECK(fabsf(dw3_mv_view_cur_zoom(&v, PW, PH, PVW, PVH) - 3.0f) < 1e-4f && v.step == MV_ZOOM_TOP_PLAIN,
          "no pack: the ladder stops at 3.0 (step %d)", v.step);
    CHECK(fabsf(dw3_mv_view_top_zoom(&v) - 3.0f) < 1e-4f, "top zoom 3.0");
    for (i = 0; i < 100; ++i) dw3_mv_view_zoom_by(&v, 1.2f, PW, PH, PVW, PVH);
    CHECK(fabsf(dw3_mv_view_cur_zoom(&v, PW, PH, PVW, PVH) - 3.0f) < 1e-4f, "stick zoom capped at 3.0");
    memset(&v, 0, sizeof(v));
    v.step = 1; v.cx = 196; v.cy = 176; v.top = MV_ZOOM_STEPS - 1;
    for (i = 0; i < 12; ++i) dw3_mv_view_zoom(&v, 1, PW, PH, PVW, PVH);
    CHECK(fabsf(dw3_mv_view_cur_zoom(&v, PW, PH, PVW, PVH) - 4.0f) < 1e-4f && v.step == MV_ZOOM_STEPS - 1,
          "with a pack: the ladder goes to 4.0 (step %d)", v.step);
    for (i = 0; i < 100; ++i) dw3_mv_view_zoom_by(&v, 1.2f, PW, PH, PVW, PVH);
    CHECK(fabsf(dw3_mv_view_cur_zoom(&v, PW, PH, PVW, PVH) - 4.0f) < 1e-4f, "stick zoom capped at 4.0 with a pack");
    dw3_mv_view_zoom(&v, -1, PW, PH, PVW, PVH);
    CHECK(v.step == MV_ZOOM_STEPS - 2 || v.zoom == 0.0f, "button steps down from 4.0");
}

static void test_pan_speed(void) {
    int hold;
    float prev = 0.0f, v;

    CHECK(dw3_mv_pan_speed(0, 1.5f) == 0.0f, "no speed without a hold");
    CHECK(dw3_mv_pan_speed(1, 1.5f) == 2.0f && dw3_mv_pan_speed(10, 1.5f) == 2.0f, "the game's 2 px a frame first");
    CHECK(dw3_mv_pan_speed(11, 1.5f) == 3.0f && dw3_mv_pan_speed(20, 1.5f) == 3.0f, "then 3");
    CHECK(dw3_mv_pan_speed(21, 1.5f) == 4.0f && dw3_mv_pan_speed(MV_PAN_ACCEL_AFTER, 1.5f) == 4.0f,
          "then 4 up to the acceleration (the game's three speeds are unchanged)");
    for (hold = 1; hold < 400; ++hold) {
        v = dw3_mv_pan_speed(hold, 1.0f);
        CHECK(v >= prev, "speed never drops while held (%d: %f < %f)", hold, v, prev);
        prev = v;
    }
    CHECK(fabsf(dw3_mv_pan_speed(400, 1.0f) - 10.0f) < 1e-4f, "cap at zoom 1.0 is 10 px/frame");
    CHECK(fabsf(dw3_mv_pan_speed(400, 0.5f) - 20.0f) < 1e-4f, "cap at zoom 0.5 is 20 px/frame");
    CHECK(dw3_mv_pan_speed(400, 3.0f) == 4.0f, "no acceleration where 4 px is already 12 screen px");
    CHECK(dw3_mv_pan_speed(MV_PAN_ACCEL_AFTER + 15, 1.0f) == 5.0f, "+1 px a frame every 15 frames after the ramp");
    /* the left stick */
    CHECK(dw3_mv_stick_unit(0) == 0.0f && dw3_mv_stick_unit(MV_STICK_MOVE_DEADZONE) == 0.0f &&
          dw3_mv_stick_unit(-MV_STICK_MOVE_DEADZONE) == 0.0f, "dead zone");
    CHECK(fabsf(dw3_mv_stick_unit(127) - 1.0f) < 1e-5f && fabsf(dw3_mv_stick_unit(-127) + 1.0f) < 1e-5f,
          "full tilt is 1");
    CHECK(dw3_mv_stick_unit(MV_STICK_MOVE_DEADZONE + 1) >= 0.02f, "a barely moved stick still moves the cursor");
    prev = 0.0f;
    for (hold = MV_STICK_MOVE_DEADZONE + 1; hold <= 127; ++hold) {
        v = dw3_mv_stick_unit(hold);
        CHECK(v >= prev && v <= 1.0f, "the stick curve rises to 1 (%d: %f)", hold, v);
        CHECK(fabsf(dw3_mv_stick_unit(-hold) + v) < 1e-6f, "symmetric");
        prev = v;
    }
    v = dw3_mv_stick_unit(64);
    CHECK(v > 0.2f && v < 0.5f, "half tilt moves slower than half of full (%f)", v);
}

/* Phase 4: the button bindings. */
static void test_bindings(void) {
    MvBindings b;
    const char *v[MV_ACT_COUNT];
    char why[400];
    int a, bit, rc;
    const int want_default[MV_ACT_COUNT] = {15, 10, 11, 0, 8, 9};

    /* the defaults are the PoC's buttons: Square opens, L1 / R1 zoom, Select
     * centres, L2 / R2 change the scaler */
    dw3_mv_bindings_default(&b);
    for (a = 0; a < MV_ACT_COUNT; ++a) {
        CHECK(b.bit[a] == want_default[a] && b.bit[a] == dw3_mv_bind_default_bit(a),
              "default %d is bit %d", a, b.bit[a]);
        CHECK(!b.from_file[a], "default %d is not from the file", a);
    }
    CHECK(strcmp(dw3_mv_act_key(MV_ACT_OPEN), "map_view_open") == 0, "key open");
    CHECK(strcmp(dw3_mv_act_key(MV_ACT_SCALER_NEXT), "map_view_scaler_next") == 0, "key next");
    CHECK(strcmp(dw3_mv_act_name(MV_ACT_ZOOM_OUT), "zoom_out") == 0, "name zoom_out");

    /* parsing: any case, spaces trimmed; reserved words say so */
    CHECK(dw3_mv_bind_parse("square") == 15 && dw3_mv_bind_parse("  SQUARE\r") == 15, "square");
    CHECK(dw3_mv_bind_parse("Cross") == 13 && dw3_mv_bind_parse("l1") == 10 &&
          dw3_mv_bind_parse("R1") == 11 && dw3_mv_bind_parse("l2") == 8 &&
          dw3_mv_bind_parse("r2") == 9 && dw3_mv_bind_parse("l3") == 1 &&
          dw3_mv_bind_parse("select") == 0, "the eight words");
    CHECK(dw3_mv_bind_parse("triangle") == -2 && dw3_mv_bind_parse("circle") == -2 &&
          dw3_mv_bind_parse("r3") == -2 && dw3_mv_bind_parse("start") == -2 &&
          dw3_mv_bind_parse("up") == -2, "reserved");
    CHECK(dw3_mv_bind_parse("y") == -1 && dw3_mv_bind_parse("") == -1 &&
          dw3_mv_bind_parse(NULL) == -1 && dw3_mv_bind_parse("square2") == -1, "unknown");
    for (bit = 0; bit < 16; ++bit) {
        const char *n = dw3_mv_bind_bit_name(bit);
        int back = dw3_mv_bind_parse(n);

        if (strcmp(n, "?") == 0) {
            CHECK(bit == 2 || bit == 3 || bit == 4 || bit == 5 || bit == 6 || bit == 7 ||
                  bit == 12 || bit == 14, "bit %d has no name but is bindable", bit);
        } else {
            CHECK(back == bit, "bit %d name %s parses back to %d", bit, n, back);
        }
    }
    CHECK(dw3_mv_bind_ps_button(15) == 3 && dw3_mv_bind_ps_button(10) == 4 &&
          dw3_mv_bind_ps_button(13) == 0 && dw3_mv_bind_ps_button(1) == 8 &&
          dw3_mv_bind_ps_button(0) == -1, "ps button of a bit");

    /* no keys: the defaults, no complaint */
    for (a = 0; a < MV_ACT_COUNT; ++a) v[a] = NULL;
    rc = dw3_mv_bindings_resolve(&b, v, why, sizeof(why));
    CHECK(rc == 0 && why[0] == 0, "no keys: rc %d why '%s'", rc, why);
    for (a = 0; a < MV_ACT_COUNT; ++a) CHECK(b.bit[a] == want_default[a], "no keys %d", a);

    /* a full rebind, each action on a button of its own */
    v[MV_ACT_OPEN] = "r2"; v[MV_ACT_ZOOM_OUT] = "square"; v[MV_ACT_ZOOM_IN] = "cross";
    v[MV_ACT_CENTRE] = "l3"; v[MV_ACT_SCALER_PREV] = "l1"; v[MV_ACT_SCALER_NEXT] = "r1";
    rc = dw3_mv_bindings_resolve(&b, v, why, sizeof(why));
    CHECK(rc == 0 && why[0] == 0, "rebind: rc %d why '%s'", rc, why);
    CHECK(b.bit[MV_ACT_OPEN] == 9 && b.bit[MV_ACT_ZOOM_OUT] == 15 && b.bit[MV_ACT_ZOOM_IN] == 13 &&
          b.bit[MV_ACT_CENTRE] == 1 && b.bit[MV_ACT_SCALER_PREV] == 10 &&
          b.bit[MV_ACT_SCALER_NEXT] == 11, "rebind bits");
    for (a = 0; a < MV_ACT_COUNT; ++a) CHECK(b.from_file[a] == 1, "from file %d", a);
    CHECK(dw3_mv_bindings_action(&b, 9) == MV_ACT_OPEN && dw3_mv_bindings_action(&b, 13) == MV_ACT_ZOOM_IN &&
          dw3_mv_bindings_action(&b, 0) == -1 && dw3_mv_bindings_action(&b, 14) == -1, "action of a bit");

    /* swapping two defaults is fine (a permutation of the table) */
    for (a = 0; a < MV_ACT_COUNT; ++a) v[a] = NULL;
    v[MV_ACT_ZOOM_OUT] = "r1"; v[MV_ACT_ZOOM_IN] = "l1";
    rc = dw3_mv_bindings_resolve(&b, v, why, sizeof(why));
    CHECK(rc == 0 && b.bit[MV_ACT_ZOOM_OUT] == 11 && b.bit[MV_ACT_ZOOM_IN] == 10, "swap L1 / R1: rc %d", rc);

    /* a value that cannot be bound: that action alone takes its default, the rest stand */
    for (a = 0; a < MV_ACT_COUNT; ++a) v[a] = NULL;
    v[MV_ACT_OPEN] = "triangle"; v[MV_ACT_ZOOM_IN] = "cross"; v[MV_ACT_ZOOM_OUT] = "banana";
    rc = dw3_mv_bindings_resolve(&b, v, why, sizeof(why));
    CHECK(rc == 1, "bad values: rc %d", rc);
    CHECK(b.bit[MV_ACT_OPEN] == 15 && !b.from_file[MV_ACT_OPEN], "triangle falls back to square");
    CHECK(b.bit[MV_ACT_ZOOM_OUT] == 10, "banana falls back to l1");
    CHECK(strstr(why, "map_view_open=triangle ignored (the view keeps that button for itself") != NULL &&
          strstr(why, "map_view_zoom_out=banana ignored (not a bindable button), using L1") != NULL, "why: %s", why);

    /* two actions on one button (a default included): everything back to the defaults */
    for (a = 0; a < MV_ACT_COUNT; ++a) v[a] = NULL;
    v[MV_ACT_OPEN] = "l1";
    rc = dw3_mv_bindings_resolve(&b, v, why, sizeof(why));
    CHECK(rc == 2, "open on L1 clashes with zoom_out's default: rc %d", rc);
    for (a = 0; a < MV_ACT_COUNT; ++a)
        CHECK(b.bit[a] == want_default[a] && !b.from_file[a], "conflict resets %d", a);
    CHECK(strstr(why, "map_view_open and map_view_zoom_out are both L1") != NULL, "why: %s", why);
    for (a = 0; a < MV_ACT_COUNT; ++a) v[a] = "cross";
    rc = dw3_mv_bindings_resolve(&b, v, why, 24);   /* a short buffer never overruns */
    CHECK(rc == 2 && strlen(why) < 24, "short buffer: rc %d len %d", rc, (int)strlen(why));
}

static void test_backdrop(void) {
    MvImage src;
    uint8_t out[MV_BACKDROP_W * MV_BACKDROP_H * 4];
    int i, bad = 0;

    make_image(&src, 64, 48, 0); /* flat 120, 70, 135 */
    CHECK(dw3_mv_backdrop(&src, out, 50) == 0, "backdrop built");
    for (i = 0; i < MV_BACKDROP_W * MV_BACKDROP_H; ++i) {
        const uint8_t *p = out + i * 4;
        if (abs(p[0] - 60) > 1 || abs(p[1] - 35) > 1 || abs(p[2] - 67) > 1 || p[3] != 255) ++bad;
    }
    CHECK(bad == 0, "a flat map gives a flat backdrop at half brightness (%d cells off)", bad);
    CHECK(dw3_mv_backdrop(&src, out, 100) == 0 && abs(out[0] - 120) <= 1, "dim 100 keeps the colour");
    CHECK(dw3_mv_backdrop(&src, out, 0) == 0 && out[0] == 0 && out[3] == 255, "dim 0 is black but opaque");
    /* left half red, right half blue: the backdrop keeps the layout */
    for (i = 0; i < src.w * src.h; ++i) {
        uint8_t *p = src.rgba + (size_t)i * 4;
        int x = i % src.w;

        p[0] = x < src.w / 2 ? 200 : 0;
        p[1] = 0;
        p[2] = x < src.w / 2 ? 0 : 200;
    }
    CHECK(dw3_mv_backdrop(&src, out, 100) == 0, "backdrop 2");
    CHECK(out[0] > 150 && out[2] < 20, "left cell red (%d,%d)", out[0], out[2]);
    CHECK(out[(MV_BACKDROP_W - 1) * 4] < 20 && out[(MV_BACKDROP_W - 1) * 4 + 2] > 150, "right cell blue");
    dw3_mv_image_free(&src);
    CHECK(dw3_mv_backdrop(NULL, out, 50) == -1, "no source, no backdrop");
}

int main(int argc, char **argv) {
    if (argc >= 3 && strcmp(argv[1], "reach") == 0) return reach_mode(argv[2]);
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
    test_reach_and_text();
    test_zoom_anchor();
    test_pan_limits();
    test_zoom_top();
    test_pan_speed();
    test_backdrop();
    test_bindings();
    test_cache(argc > 1 ? argv[1] : "/mnt/cabal/tmp");
    if (!g_fail) printf("OK: map view core\n");
    return g_fail;
}
