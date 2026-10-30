/* P6.22 pass 4 GPU check: the 16-bit present against the RGBA8888 pack.
 *
 * Runs the real shaders of platforms/switch/psyz_backend/nx_present16.h on an
 * OpenGL ES 3 context (Mesa llvmpipe through surfaceless EGL, no display) and
 * compares what they draw with what the old path draws from the CPU pack
 * (nx_vram_pack.c), pixel by pixel:
 *
 *   1. all 65 536 halfwords, one pixel each, 1:1: the shader's RGB equals the
 *      pack's RGB for every value, and alpha is 255.
 *   2. the same halfwords through the overlay (UI) program, with three u_keep
 *      boxes (none, the left half, everything): a pixel is discarded exactly
 *      when the old path discards it, and kept ones are equal.
 *   3. a random VRAM rectangle at several viewport scales, including the
 *      non-integer 4.5x of the docked 1080p picture, through both programs: the
 *      16-bit image is byte-identical to the old image.
 *   4. the display between its gutters (the wide texture, uploaded in three
 *      pieces) against dw3_nx_pack_rgb555_wide.
 *
 * Everything runs for both 16-bit upload modes of nx_gpu.c (DW3_PRESENT_16=1,
 * rows straight from VRAM; 2, rows copied tight first). `--bench` also times a
 * present of each.
 *
 * Exit 0 = all equal, 1 = a difference, 77 = no usable GL ES 3 context (the
 * pytest wrapper skips). Build: see tools/tests/test_present16_gpu.py.
 */
#define GL_GLEXT_PROTOTYPES 1
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES3/gl3.h>

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "nx_present16.h"
#include "nx_vram_pack.h"

#define SKIP 77
#define STRIDE 64 /* DW3_NX_GUTTER_STRIDE */
#define WIDE_W 512
#define WIDE_H 512

static uint16_t s_vram[DW3_NX_VRAM_W * DW3_NX_VRAM_H];
static uint16_t s_left[DW3_NX_VRAM_H * STRIDE];
static uint16_t s_right[DW3_NX_VRAM_H * STRIDE];
static uint8_t s_pack[DW3_NX_VRAM_W * DW3_NX_VRAM_H * 4];

static uint32_t s_rng = 0x12345678u;

static uint32_t rnd(void) {
    s_rng ^= s_rng << 13;
    s_rng ^= s_rng >> 17;
    s_rng ^= s_rng << 5;
    return s_rng;
}

static GLuint compile(GLenum type, const char *src) {
    GLuint sh = glCreateShader(type);
    GLint ok = 0;

    glShaderSource(sh, 1, &src, NULL);
    glCompileShader(sh);
    glGetShaderiv(sh, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[1024];

        glGetShaderInfoLog(sh, sizeof(log), NULL, log);
        fprintf(stderr, "shader failed: %s\n", log);
        exit(1);
    }
    return sh;
}

typedef struct {
    GLuint prog;
    GLint pos, t, uv0, uv1, tex, keep;
} Prog;

static Prog link_prog(const char *vs_src, const char *fs_src) {
    Prog p;
    GLuint vs = compile(GL_VERTEX_SHADER, vs_src);
    GLuint fs = compile(GL_FRAGMENT_SHADER, fs_src);
    GLint ok = 0;

    p.prog = glCreateProgram();
    glAttachShader(p.prog, vs);
    glAttachShader(p.prog, fs);
    glLinkProgram(p.prog);
    glGetProgramiv(p.prog, GL_LINK_STATUS, &ok);
    if (!ok) {
        char log[1024];

        glGetProgramInfoLog(p.prog, sizeof(log), NULL, log);
        fprintf(stderr, "link failed: %s\n", log);
        exit(1);
    }
    p.pos = glGetAttribLocation(p.prog, "a_pos");
    p.t = glGetAttribLocation(p.prog, "a_t");
    p.uv0 = glGetUniformLocation(p.prog, "u_uv0");
    p.uv1 = glGetUniformLocation(p.prog, "u_uv1");
    p.tex = glGetUniformLocation(p.prog, "u_tex");
    p.keep = glGetUniformLocation(p.prog, "u_keep");
    return p;
}

static GLuint s_vbo;

static void quad_init(void) {
    static const float quad[] = {
        -1.0f, -1.0f, 0.0f, 1.0f, 1.0f,  -1.0f, 1.0f, 1.0f,
        -1.0f, 1.0f,  0.0f, 0.0f, 1.0f,  1.0f,  1.0f, 0.0f,
    };

    glGenBuffers(1, &s_vbo);
    glBindBuffer(GL_ARRAY_BUFFER, s_vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quad), quad, GL_STATIC_DRAW);
}

static GLuint make_tex(int w, int h, int p16) {
    GLuint tex;

    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    if (p16) {
        glTexImage2D(GL_TEXTURE_2D, 0, GL_R16UI, w, h, 0, GL_RED_INTEGER,
                     GL_UNSIGNED_SHORT, NULL);
    } else {
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA,
                     GL_UNSIGNED_BYTE, NULL);
    }
    return tex;
}

/* Draw one quad of the program into the bound FBO, as nx_gpu.c does: viewport
 * (0, 0, vw, vh), the sampled rectangle uv0..uv1 of `tex`. */
static void draw(const Prog *p, GLuint tex, int vw, int vh, const float uv0[2],
                 const float uv1[2], const float keep[4]) {
    glViewport(0, 0, vw, vh);
    glUseProgram(p->prog);
    glUniform2f(p->uv0, uv0[0], uv0[1]);
    glUniform2f(p->uv1, uv1[0], uv1[1]);
    if (p->keep >= 0 && keep) {
        glUniform4f(p->keep, keep[0], keep[1], keep[2], keep[3]);
    }
    glUniform1i(p->tex, 0);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, tex);
    glBindBuffer(GL_ARRAY_BUFFER, s_vbo);
    glEnableVertexAttribArray((GLuint)p->pos);
    glVertexAttribPointer((GLuint)p->pos, 2, GL_FLOAT, GL_FALSE,
                          sizeof(float) * 4, (const void *)0);
    glEnableVertexAttribArray((GLuint)p->t);
    glVertexAttribPointer((GLuint)p->t, 2, GL_FLOAT, GL_FALSE,
                          sizeof(float) * 4, (const void *)(sizeof(float) * 2));
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
}

static const uint8_t SENTINEL[4] = {0x31, 0x62, 0x93, 0xC4};

static void clear_sentinel(void) {
    glDisable(GL_SCISSOR_TEST);
    glDisable(GL_BLEND);
    glClearColor(SENTINEL[0] / 255.0f, SENTINEL[1] / 255.0f,
                 SENTINEL[2] / 255.0f, SENTINEL[3] / 255.0f);
    glClear(GL_COLOR_BUFFER_BIT);
}

static uint8_t *readback(int w, int h) {
    uint8_t *img = malloc((size_t)w * (size_t)h * 4);

    if (!img) {
        exit(1);
    }
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadPixels(0, 0, w, h, GL_RGBA, GL_UNSIGNED_BYTE, img);
    return img;
}

static int g_fail;

/* DW3_PRESENT_16 modes of nx_gpu.c: 1 = rows straight from VRAM with
 * GL_UNPACK_ROW_LENGTH, 2 = rows copied tight first. */
static int g_mode = 1;
static uint16_t s_tight[DW3_NX_VRAM_W * DW3_NX_VRAM_H];

static void upload16(int px, int py, int pw, int ph, int wide, int gutter,
                     int dx, int dy, int dw) {
    if (g_mode == 2) {
        if (wide) {
            dw3_nx_copy16_wide(s_vram, s_left, s_right, STRIDE, gutter, dx, dy,
                               dw, ph, s_tight);
        } else {
            dw3_nx_copy16_rect(s_vram, px, py, pw, ph, s_tight);
        }
        dw3_nx_p16_upload_tight(s_tight, px, py, pw, ph);
    } else {
        dw3_nx_p16_upload(s_vram, s_left, s_right, STRIDE, px, py, pw, ph,
                          wide, gutter, dx, dy, dw);
    }
}

static void note_diff(const char *what, size_t px, const uint8_t *a,
                      const uint8_t *b) {
    if (g_fail++ < 10) {
        fprintf(stderr,
                "DIFF %s pixel %zu: 16-bit %02x %02x %02x %02x, old %02x %02x "
                "%02x %02x\n",
                what, px, a[px * 4], a[px * 4 + 1], a[px * 4 + 2],
                a[px * 4 + 3], b[px * 4], b[px * 4 + 1], b[px * 4 + 2],
                b[px * 4 + 3]);
    }
}

static int compare_images(const char *what, const uint8_t *a, const uint8_t *b,
                          int w, int h) {
    size_t i;
    size_t n = (size_t)w * (size_t)h;
    int bad = 0;

    for (i = 0; i < n; i++) {
        if (memcmp(a + i * 4, b + i * 4, 4) != 0) {
            note_diff(what, i, a, b);
            bad++;
        }
    }
    return bad;
}

static void upload32(GLuint tex, const uint8_t *rgba, int x, int y, int w,
                     int h) {
    glBindTexture(GL_TEXTURE_2D, tex);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
    glTexSubImage2D(GL_TEXTURE_2D, 0, x, y, w, h, GL_RGBA, GL_UNSIGNED_BYTE,
                    rgba);
}

/* 1 and 2: every halfword once. VRAM rows 0..63 hold 0..65535 in order. */
static void test_all_values(const Prog *q32, const Prog *u32, const Prog *q16,
                            const Prog *u16, GLuint t32, GLuint t16) {
    static const float keeps[3][4] = {
        {2.0f, 2.0f, -1.0f, -1.0f}, /* none */
        {0.0f, 0.0f, 0.5f, 1.0f},   /* left half */
        {-1.0f, -1.0f, 2.0f, 2.0f}, /* all */
    };
    const int W = 1024;
    const int H = 64;
    const float uv0[2] = {0.0f, 0.0f};
    const float uv1[2] = {(float)W / DW3_NX_VRAM_W, (float)H / DW3_NX_VRAM_H};
    int ox, oy, ow, oh;
    uint8_t *a;
    uint8_t *b;
    size_t i;
    int k;
    int bad;

    for (i = 0; i < 65536u; i++) {
        s_vram[i] = (uint16_t)i;
    }
    if (dw3_nx_pack_rgb555_rect(s_vram, 0, 0, W, H, s_pack, &ox, &oy, &ow,
                                &oh) != (size_t)W * H) {
        fprintf(stderr, "pack failed\n");
        exit(1);
    }
    upload32(t32, s_pack, 0, 0, W, H);
    glBindTexture(GL_TEXTURE_2D, t16);
    upload16(0, 0, W, H, 0, 0, 0, 0, 0);

    /* 1: the opaque program, against the pack itself and the old shader. */
    clear_sentinel();
    draw(q16, t16, W, H, uv0, uv1, NULL);
    a = readback(W, H);
    clear_sentinel();
    draw(q32, t32, W, H, uv0, uv1, NULL);
    b = readback(W, H);
    bad = compare_images("all values, opaque, old shader", a, b, W, H);
    for (i = 0; i < 65536u; i++) {
        /* glReadPixels starts at the bottom row; the quad puts VRAM row 0 on
         * top. */
        size_t at = ((size_t)(H - 1) - i / W) * W + i % W;

        if (a[at * 4] != s_pack[i * 4] || a[at * 4 + 1] != s_pack[i * 4 + 1] ||
            a[at * 4 + 2] != s_pack[i * 4 + 2] || a[at * 4 + 3] != 0xFF) {
            if (g_fail++ < 10) {
                fprintf(stderr,
                        "DIFF value %04zx: shader %02x %02x %02x %02x, pack "
                        "%02x %02x %02x\n",
                        i, a[at * 4], a[at * 4 + 1], a[at * 4 + 2],
                        a[at * 4 + 3], s_pack[i * 4], s_pack[i * 4 + 1],
                        s_pack[i * 4 + 2]);
            }
            bad++;
        }
    }
    free(a);
    free(b);
    printf("%s: all 65536 halfwords, opaque program, shader vs pack and vs "
           "old shader\n",
           bad ? "FAIL" : "OK");

    /* 2: the overlay program with the three keep boxes. */
    for (k = 0; k < 3; k++) {
        size_t discarded = 0;

        clear_sentinel();
        draw(u16, t16, W, H, uv0, uv1, keeps[k]);
        a = readback(W, H);
        clear_sentinel();
        draw(u32, t32, W, H, uv0, uv1, keeps[k]);
        b = readback(W, H);
        bad = compare_images("all values, overlay", a, b, W, H);
        for (i = 0; i < 65536u; i++) {
            if (memcmp(a + i * 4, SENTINEL, 4) == 0) {
                discarded++;
            }
        }
        free(a);
        free(b);
        printf("%s: all 65536 halfwords, overlay program, keep box %d "
               "(%zu discarded)\n",
               bad ? "FAIL" : "OK", k, discarded);
    }
}

/* 3: a random rectangle at several scales, both programs. */
static void test_scaled(const Prog *q32, const Prog *u32, const Prog *q16,
                        const Prog *u16, GLuint t32, GLuint t16) {
    static const int sizes[][2] = {
        {320, 240},   /* 1x */
        {960, 720},   /* 3x, handheld 720p */
        {1440, 1080}, /* 4.5x, docked 1080p */
        {1280, 720},  /* 4x by 3x: the picture stretched */
        {1000, 777},  /* arbitrary */
    };
    const int dx = 17;
    const int dy = 9;
    const int dw = 320;
    const int dh = 240;
    const float uv0[2] = {(float)dx / DW3_NX_VRAM_W, (float)dy / DW3_NX_VRAM_H};
    const float uv1[2] = {(float)(dx + dw) / DW3_NX_VRAM_W,
                          (float)(dy + dh) / DW3_NX_VRAM_H};
    const float keep[4] = {0.25f, 0.25f, 0.75f, 0.75f};
    int ox, oy, ow, oh;
    size_t i;
    size_t n;
    int s;

    /* Random content with many black and mask-black pixels. */
    for (i = 0; i < sizeof(s_vram) / sizeof(s_vram[0]); i++) {
        uint32_t r = rnd();

        s_vram[i] = (uint16_t)r;
        if ((r >> 20) % 5 == 0) {
            s_vram[i] = (uint16_t)((r >> 16) & 0x8000u); /* 0 or 0x8000 */
        }
    }
    n = dw3_nx_pack_rgb555_rect(s_vram, dx, dy, dw, dh, s_pack, &ox, &oy, &ow,
                                &oh);
    if (n != (size_t)dw * dh) {
        fprintf(stderr, "pack failed\n");
        exit(1);
    }
    upload32(t32, s_pack, ox, oy, ow, oh);
    glBindTexture(GL_TEXTURE_2D, t16);
    upload16(ox, oy, ow, oh, 0, 0, dx, dy, dw);

    for (s = 0; s < (int)(sizeof(sizes) / sizeof(sizes[0])); s++) {
        int w = sizes[s][0];
        int h = sizes[s][1];
        int pass;

        for (pass = 0; pass < 2; pass++) {
            const Prog *p16 = pass ? u16 : q16;
            const Prog *p32 = pass ? u32 : q32;
            uint8_t *a;
            uint8_t *b;
            int bad;
            char what[64];

            clear_sentinel();
            draw(p16, t16, w, h, uv0, uv1, pass ? keep : NULL);
            a = readback(w, h);
            clear_sentinel();
            draw(p32, t32, w, h, uv0, uv1, pass ? keep : NULL);
            b = readback(w, h);
            snprintf(what, sizeof(what), "scaled %dx%d %s", w, h,
                     pass ? "overlay" : "opaque");
            bad = compare_images(what, a, b, w, h);
            free(a);
            free(b);
            printf("%s: %s\n", bad ? "FAIL" : "OK", what);
        }
    }
}

/* 4: the display between its gutters in the wide texture. */
static void test_wide(const Prog *q32, const Prog *u32, const Prog *q16,
                      const Prog *u16, GLuint w32, GLuint w16) {
    const int dx = 3;
    const int dy = 5;
    const int dw = 320;
    const int dh = 240;
    const int gutter = 37;
    const int pw = dw + 2 * gutter;
    const float uv0[2] = {0.0f, 0.0f};
    const float uv1[2] = {(float)pw / WIDE_W, (float)dh / WIDE_H};
    const float keep[4] = {0.1f, 0.1f, 0.6f, 0.9f};
    size_t i;
    int pass;

    for (i = 0; i < sizeof(s_left) / sizeof(s_left[0]); i++) {
        uint32_t r = rnd();

        s_left[i] = (uint16_t)r;
        s_right[i] = (uint16_t)(r >> 11);
        if ((r >> 24) % 4 == 0) {
            s_left[i] &= 0x8000u;
            s_right[i] = (uint16_t)(s_right[i] & 0x8000u);
        }
    }
    if (dw3_nx_pack_rgb555_wide(s_vram, s_left, s_right, STRIDE, gutter, dx,
                                dy, dw, dh, s_pack) != (size_t)pw * dh) {
        fprintf(stderr, "wide pack failed\n");
        exit(1);
    }
    upload32(w32, s_pack, 0, 0, pw, dh);
    glBindTexture(GL_TEXTURE_2D, w16);
    upload16(0, 0, pw, dh, 1, gutter, dx, dy, dw);

    for (pass = 0; pass < 2; pass++) {
        const Prog *p16 = pass ? u16 : q16;
        const Prog *p32 = pass ? u32 : q32;
        int w = 1440;
        int h = 1080;
        uint8_t *a;
        uint8_t *b;
        int bad;

        clear_sentinel();
        draw(p16, w16, w, h, uv0, uv1, pass ? keep : NULL);
        a = readback(w, h);
        clear_sentinel();
        draw(p32, w32, w, h, uv0, uv1, pass ? keep : NULL);
        b = readback(w, h);
        bad = compare_images("wide", a, b, w, h);
        free(a);
        free(b);
        printf("%s: wide texture with %d-pixel gutters, %s program, 1440x1080\n",
               bad ? "FAIL" : "OK", gutter, pass ? "overlay" : "opaque");
    }
}


/* --bench: what a present costs on this host's GL (llvmpipe: the shader work is
 * on the CPU here, so the drawing numbers are an upper bound for it; they say
 * nothing about the Switch's GPU). Old path: CPU pack, RGBA8888 upload, draw.
 * New path: 16-bit upload from VRAM, draw. Interleaved blocks; the median of
 * each block, then the median and the spread of the blocks. */
static double now_us(void) {
    struct timespec ts;

    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec * 1e6 + (double)ts.tv_nsec / 1e3;
}

static int cmp_d(const void *a, const void *b) {
    double x = *(const double *)a;
    double y = *(const double *)b;

    return x < y ? -1 : x > y;
}

static double med(double *v, int n) {
    qsort(v, (size_t)n, sizeof(double), cmp_d);
    return v[n / 2];
}

static void report(const char *name, double blocks[][3], int nb, int col) {
    double v[64];
    int i;

    for (i = 0; i < nb; i++) {
        v[i] = blocks[i][col];
    }
    {
        double m = med(v, nb);

        printf("  %-22s median %7.1f us  (blocks %7.1f .. %7.1f)\n", name, m,
               v[0], v[nb - 1]);
    }
}

static void bench(const Prog *q32, const Prog *q16, GLuint t32, GLuint t16,
                  GLuint w32, GLuint w16) {
    enum { BLOCKS = 9, ITERS = 120 };
    const int vw = 1280;
    const int vh = 720;
    int wide;

    for (wide = 0; wide < 2; wide++) {
        const int dx = 0;
        const int dy = 0;
        const int dw = 320;
        const int dh = 240;
        const int gutter = wide ? 64 : 0;
        const int pw = dw + 2 * gutter;
        const float uv0[2] = {wide ? 0.0f : 0.0f, 0.0f};
        const float uv1[2] = {wide ? (float)pw / WIDE_W : (float)dw / 1024,
                              wide ? (float)dh / WIDE_H : (float)dh / 512};
        double old_pack[BLOCKS][3], old_up[BLOCKS][3], old_draw[BLOCKS][3];
        double new_up[BLOCKS][3], new_draw[BLOCKS][3];
        double old_tot[BLOCKS][3], new_tot[BLOCKS][3];
        int b;
        int it;
        size_t i;

        for (i = 0; i < sizeof(s_vram) / sizeof(s_vram[0]); i++) {
            s_vram[i] = (uint16_t)rnd();
        }
        for (b = 0; b < BLOCKS; b++) {
            double a_pack[ITERS], a_up[ITERS], a_draw[ITERS], a_tot[ITERS];
            double n_up[ITERS], n_draw[ITERS], n_tot[ITERS];

            for (it = 0; it < ITERS; it++) {
                double t0, t1, t2, t3;

                /* old */
                t0 = now_us();
                if (wide) {
                    dw3_nx_pack_rgb555_wide(s_vram, s_left, s_right, STRIDE,
                                            gutter, dx, dy, dw, dh, s_pack);
                } else {
                    int ox, oy, ow, oh;

                    dw3_nx_pack_rgb555_rect(s_vram, dx, dy, dw, dh, s_pack,
                                            &ox, &oy, &ow, &oh);
                }
                t1 = now_us();
                upload32(wide ? w32 : t32, s_pack, 0, 0, wide ? pw : dw, dh);
                t2 = now_us();
                draw(q32, wide ? w32 : t32, vw, vh, uv0, uv1, NULL);
                glFinish();
                t3 = now_us();
                a_pack[it] = t1 - t0;
                a_up[it] = t2 - t1;
                a_draw[it] = t3 - t2;
                a_tot[it] = t3 - t0;

                /* new */
                t0 = now_us();
                glBindTexture(GL_TEXTURE_2D, wide ? w16 : t16);
                upload16(0, 0, wide ? pw : dw, dh, wide, gutter, dx, dy, dw);
                t1 = now_us();
                draw(q16, wide ? w16 : t16, vw, vh, uv0, uv1, NULL);
                glFinish();
                t2 = now_us();
                n_up[it] = t1 - t0;
                n_draw[it] = t2 - t1;
                n_tot[it] = t2 - t0;
            }
            old_pack[b][0] = med(a_pack, ITERS);
            old_up[b][0] = med(a_up, ITERS);
            old_draw[b][0] = med(a_draw, ITERS);
            old_tot[b][0] = med(a_tot, ITERS);
            new_up[b][0] = med(n_up, ITERS);
            new_draw[b][0] = med(n_draw, ITERS);
            new_tot[b][0] = med(n_tot, ITERS);
        }
        printf("bench %s %dx%d -> %dx%d (%d blocks x %d presents):\n",
               wide ? "wide (gutters 64)" : "rect", dw, dh, vw, vh, BLOCKS,
               ITERS);
        report("old: CPU pack", old_pack, BLOCKS, 0);
        report("old: RGBA8888 upload", old_up, BLOCKS, 0);
        report("old: draw (llvmpipe)", old_draw, BLOCKS, 0);
        report("old: total", old_tot, BLOCKS, 0);
        report("new: 16-bit upload", new_up, BLOCKS, 0);
        report("new: draw (llvmpipe)", new_draw, BLOCKS, 0);
        report("new: total", new_tot, BLOCKS, 0);
    }
}

int main(int argc, char **argv) {
    EGLDisplay dpy;
    EGLConfig cfg;
    EGLint n = 0;
    EGLContext ctx;
    PFNEGLGETPLATFORMDISPLAYEXTPROC get_platform_display;
    static const EGLint cfg_attr[] = {
        EGL_RENDERABLE_TYPE, EGL_OPENGL_ES3_BIT, EGL_SURFACE_TYPE,
        EGL_PBUFFER_BIT,     EGL_RED_SIZE,       8,
        EGL_GREEN_SIZE,      8,                  EGL_BLUE_SIZE,
        8,                   EGL_ALPHA_SIZE,     8,
        EGL_NONE,
    };
    static const EGLint ctx_attr[] = {EGL_CONTEXT_CLIENT_VERSION, 3, EGL_NONE};
    GLuint fbo, rb;
    GLuint t32, t16, w32, w16;
    Prog q32, u32, q16, u16;
    const char *ver;

    get_platform_display = (PFNEGLGETPLATFORMDISPLAYEXTPROC)eglGetProcAddress(
        "eglGetPlatformDisplayEXT");
    dpy = EGL_NO_DISPLAY;
    if (get_platform_display) {
        dpy = get_platform_display(EGL_PLATFORM_SURFACELESS_MESA,
                                   EGL_DEFAULT_DISPLAY, NULL);
    }
    if (dpy == EGL_NO_DISPLAY) {
        dpy = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    }
    if (dpy == EGL_NO_DISPLAY || !eglInitialize(dpy, NULL, NULL) ||
        !eglBindAPI(EGL_OPENGL_ES_API) ||
        !eglChooseConfig(dpy, cfg_attr, &cfg, 1, &n) || n < 1) {
        fprintf(stderr, "SKIP: no EGL display with an ES 3 config\n");
        return SKIP;
    }
    ctx = eglCreateContext(dpy, cfg, EGL_NO_CONTEXT, ctx_attr);
    if (ctx == EGL_NO_CONTEXT ||
        !eglMakeCurrent(dpy, EGL_NO_SURFACE, EGL_NO_SURFACE, ctx)) {
        fprintf(stderr, "SKIP: no surfaceless ES 3 context (0x%x)\n",
                eglGetError());
        return SKIP;
    }
    ver = (const char *)glGetString(GL_VERSION);
    printf("GL: %s | %s\n", ver ? ver : "?",
           (const char *)glGetString(GL_RENDERER));

    glGenFramebuffers(1, &fbo);
    glGenRenderbuffers(1, &rb);
    glBindRenderbuffer(GL_RENDERBUFFER, rb);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_RGBA8, 1440, 1080);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                              GL_RENDERBUFFER, rb);
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        fprintf(stderr, "SKIP: RGBA8 renderbuffer is not complete\n");
        return SKIP;
    }

    quad_init();
    q32 = link_prog(DW3_NX_P32_VS, DW3_NX_P32_QUAD_FS);
    u32 = link_prog(DW3_NX_P32_VS, DW3_NX_P32_UI_FS);
    q16 = link_prog(DW3_NX_P16_VS, DW3_NX_P16_QUAD_FS);
    u16 = link_prog(DW3_NX_P16_VS, DW3_NX_P16_UI_FS);
    t32 = make_tex(DW3_NX_VRAM_W, DW3_NX_VRAM_H, 0);
    t16 = make_tex(DW3_NX_VRAM_W, DW3_NX_VRAM_H, 1);
    w32 = make_tex(WIDE_W, WIDE_H, 0);
    w16 = make_tex(WIDE_W, WIDE_H, 1);
    if (glGetError() != GL_NO_ERROR) {
        fprintf(stderr, "GL error while setting up\n");
        return 1;
    }

    for (g_mode = 1; g_mode <= 2; g_mode++) {
        printf("-- DW3_PRESENT_16=%d (%s)\n", g_mode,
               g_mode == 2 ? "rows copied tight" : "rows straight from VRAM");
        s_rng = 0x12345678u;
        test_all_values(&q32, &u32, &q16, &u16, t32, t16);
        test_scaled(&q32, &u32, &q16, &u16, t32, t16);
        test_wide(&q32, &u32, &q16, &u16, w32, w16);
    }

    if (glGetError() != GL_NO_ERROR) {
        fprintf(stderr, "GL error during the run\n");
        return 1;
    }
    if (g_fail) {
        printf("FAIL: %d differing pixels\n", g_fail);
        return 1;
    }
    printf("OK: 16-bit present is pixel-identical to the RGBA8888 pack\n");
    if (argc > 1 && strcmp(argv[1], "--bench") == 0) {
        for (g_mode = 1; g_mode <= 2; g_mode++) {
            printf("== bench, DW3_PRESENT_16=%d\n", g_mode);
            bench(&q32, &q16, t32, t16, w32, w16);
        }
    }
    return 0;
}
