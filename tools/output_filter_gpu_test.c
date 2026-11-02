/* Output filter GPU check (docs/OUTPUT_FILTER.md): the real filtered shaders of
 * platforms/switch/psyz_backend (nx_output_filter.h, nx_present16.h) on an
 * OpenGL ES 3 context (Mesa llvmpipe through surfaceless EGL, no display), against
 * a CPU reference of the same arithmetic written here in double precision.
 *
 *   1. opaque program (movies, the picture without a battle mesh), bilinear and
 *      sharp, on a random rectangle of a random VRAM at several viewport sizes,
 *      among them the docked 4.5x (1440x1080 for 320x240) and the handheld 3x:
 *      every output channel is within 2 of the reference. VRAM around the rectangle
 *      is a different random, so a tap that leaked past the rectangle shows.
 *   2. sharp at an integer scale (the handheld 3x, 2x, 4x) is byte-identical to
 *      the Nearest program.
 *   3. sharp at 4.5x: the middle of each source pixel is flat (equal to the
 *      Nearest program), the seam is at most one output pixel and a half wide.
 *   4. overlay program (the CPU layer over the battle mesh): the set of discarded
 *      pixels is exactly the Nearest program's, kept colours match the reference
 *      (the weights of dropped taps are renormalised).
 *   5. mesh blit program (RGBA8 sampler): same checks as 1 on a random RGBA image.
 *
 * `--bench` times the Nearest, bilinear and sharp programs (llvmpipe: a CPU
 * rasteriser, so the numbers compare the programs, they are not the Switch GPU).
 *
 * Exit 0 = all within tolerance, 1 = a difference, 77 = no usable GL ES 3 context
 * (the pytest wrapper skips). Build: see tools/tests/test_output_filter.py.
 */
#define GL_GLEXT_PROTOTYPES 1
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES3/gl3.h>

#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define DW3_NX_VRAM_W 1024
#define DW3_NX_VRAM_H 512
#include "nx_present16.h"

#define SKIP 77
#define TOL 2

static uint16_t s_vram[DW3_NX_VRAM_W * DW3_NX_VRAM_H];
static uint8_t s_rgba[DW3_NX_VRAM_W * DW3_NX_VRAM_H * 4];
static uint32_t s_rng = 0x2468ace1u;

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
        char log[2048];

        glGetShaderInfoLog(sh, sizeof(log), NULL, log);
        fprintf(stderr, "shader failed: %s\n", log);
        exit(1);
    }
    return sh;
}

typedef struct {
    GLuint prog;
    GLint pos, t, uv0, uv1, tex, keep, flt;
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
        char log[2048];

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
    p.flt = glGetUniformLocation(p.prog, "u_flt");
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

static GLuint make_tex(int p16) {
    GLuint tex;

    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    if (p16) {
        glTexImage2D(GL_TEXTURE_2D, 0, GL_R16UI, DW3_NX_VRAM_W, DW3_NX_VRAM_H,
                     0, GL_RED_INTEGER, GL_UNSIGNED_SHORT, NULL);
    } else {
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, DW3_NX_VRAM_W, DW3_NX_VRAM_H,
                     0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    }
    return tex;
}

/* One quad, as nx_gpu.c draws it: viewport (0, 0, vw, vh), the rectangle uv0..uv1
 * of `tex`, u_flt = (mode, vw, vh) for the filtered programs. */
static void draw(const Prog *p, GLuint tex, int vw, int vh, const float uv0[2],
                 const float uv1[2], const float keep[4], int mode) {
    glViewport(0, 0, vw, vh);
    glUseProgram(p->prog);
    glUniform2f(p->uv0, uv0[0], uv0[1]);
    glUniform2f(p->uv1, uv1[0], uv1[1]);
    if (p->keep >= 0 && keep) {
        glUniform4f(p->keep, keep[0], keep[1], keep[2], keep[3]);
    }
    if (p->flt >= 0) {
        glUniform3f(p->flt, (float)mode, (float)vw, (float)vh);
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

/* Top row first, like the quad's v_t.y = 0 at the top. */
static uint8_t *readback(int w, int h) {
    uint8_t *raw = malloc((size_t)w * (size_t)h * 4);
    uint8_t *img = malloc((size_t)w * (size_t)h * 4);
    int y;

    if (!raw || !img) {
        exit(1);
    }
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadPixels(0, 0, w, h, GL_RGBA, GL_UNSIGNED_BYTE, raw);
    for (y = 0; y < h; y++) {
        memcpy(img + (size_t)y * w * 4, raw + (size_t)(h - 1 - y) * w * 4,
               (size_t)w * 4);
    }
    free(raw);
    return img;
}

static int g_fail;

static int c8(int c5) { return (c5 * 255 + 15) / 31; }

static void rgb555(uint16_t v, double out[3]) {
    out[0] = c8(v & 31);
    out[1] = c8((v >> 5) & 31);
    out[2] = c8((v >> 10) & 31);
}

/* A texel source for the reference: VRAM halfwords (p16) or RGBA8 bytes. */
typedef struct {
    int rgba;           /* 1: s_rgba, 0: s_vram */
    int x0, y0, x1, y1; /* the rectangle shown, in texels */
} Src;

static int texel(const Src *s, int x, int y, double c[3], uint16_t *raw) {
    if (x < s->x0) x = s->x0;
    if (x > s->x1 - 1) x = s->x1 - 1;
    if (y < s->y0) y = s->y0;
    if (y > s->y1 - 1) y = s->y1 - 1;
    if (s->rgba) {
        const uint8_t *p = s_rgba + ((size_t)y * DW3_NX_VRAM_W + x) * 4;

        c[0] = p[0];
        c[1] = p[1];
        c[2] = p[2];
        if (raw) *raw = 0;
    } else {
        uint16_t v = s_vram[(size_t)y * DW3_NX_VRAM_W + x];

        rgb555(v, c);
        if (raw) *raw = v;
    }
    return 0;
}

/* Where output pixel (x, y) of a vw x vh quad samples: the taps and weights of
 * flt_tap in nx_output_filter.h. */
static void taps(const Src *s, int vw, int vh, int x, int y, int mode, int *ix,
                 int *iy, double *wx, double *wy, double *tx, double *ty) {
    double vt_x = (x + 0.5) / vw;
    double vt_y = (y + 0.5) / vh;
    double sw = s->x1 - s->x0;
    double sh = s->y1 - s->y0;
    double t[2] = {s->x0 + vt_x * sw, s->y0 + vt_y * sh};
    double scale[2] = {vw / sw, vh / sh};
    double pos[2] = {t[0], t[1]};
    int a;

    if (mode == 2) {
        for (a = 0; a < 2; a++) {
            double p = floor(scale[a]);
            double f, r, cl;

            if (p < 1.0) p = 1.0;
            f = t[a] - floor(t[a]) - 0.5;
            r = 0.5 - 0.5 / p;
            cl = f < -r ? -r : f > r ? r : f;
            pos[a] = floor(t[a]) + (f - cl) * p + 0.5;
        }
    }
    *ix = (int)floor(pos[0] - 0.5);
    *iy = (int)floor(pos[1] - 0.5);
    *wx = pos[0] - 0.5 - floor(pos[0] - 0.5);
    *wy = pos[1] - 0.5 - floor(pos[1] - 0.5);
    *tx = t[0];
    *ty = t[1];
}

/* The overlay's "shown" test of a halfword (the Nearest program's discard rule). */
static int shown(uint16_t v, int inside) {
    return (v & 0x7FFF) != 0 || ((v & 0x8000) != 0 && inside);
}

/* Reference colour of output pixel (x, y); returns 0 when the overlay program
 * discards it. `overlay` applies the keep box (quad units) to p16 sources. */
static int ref_pixel(const Src *s, int vw, int vh, int x, int y, int mode,
                     int overlay, const float keep[4], double out[3]) {
    int ix, iy;
    double wx, wy, tx, ty, c[4][3], k[4];
    uint16_t raw[4];
    double vt_x = (x + 0.5) / vw, vt_y = (y + 0.5) / vh;
    int inside = overlay && vt_x >= keep[0] && vt_y >= keep[1] &&
                 vt_x < keep[2] && vt_y < keep[3];
    int n, a;
    double sum = 0;

    taps(s, vw, vh, x, y, mode, &ix, &iy, &wx, &wy, &tx, &ty);
    for (n = 0; n < 4; n++) {
        texel(s, ix + (n & 1), iy + (n >> 1), c[n], &raw[n]);
    }
    k[0] = (1 - wx) * (1 - wy);
    k[1] = wx * (1 - wy);
    k[2] = (1 - wx) * wy;
    k[3] = wx * wy;
    if (overlay) {
        uint16_t v0;
        double dummy[3];

        texel(s, (int)floor(tx), (int)floor(ty), dummy, &v0);
        if (!shown(v0, inside)) {
            return 0;
        }
        if (wx != 0.0 || wy != 0.0) {
            for (n = 0; n < 4; n++) {
                if (!shown(raw[n], inside)) k[n] = 0;
                sum += k[n];
            }
            if (sum > 0) {
                for (n = 0; n < 4; n++) k[n] /= sum;
            } else {
                for (a = 0; a < 3; a++) out[a] = dummy[a];
                return 1;
            }
        }
    }
    for (a = 0; a < 3; a++) {
        out[a] = k[0] * c[0][a] + k[1] * c[1][a] + k[2] * c[2][a] +
                 k[3] * c[3][a];
    }
    return 1;
}

/* The ideal Nearest picture: the texel under the centre of every output pixel
 * (the Nearest programs do the same arithmetic in mediump float, which a driver
 * may run in fp16, so they can pick the neighbour near a seam; the filtered
 * programs use highp and are compared with this instead). */
static uint8_t *nearest_ref(const Src *s, int vw, int vh) {
    uint8_t *out = malloc((size_t)vw * vh * 4);
    int x, y, a;

    if (!out) exit(1);
    for (y = 0; y < vh; y++) {
        for (x = 0; x < vw; x++) {
            double c[3];
            double tx = s->x0 + (x + 0.5) / vw * (s->x1 - s->x0);
            double ty = s->y0 + (y + 0.5) / vh * (s->y1 - s->y0);

            texel(s, (int)floor(tx), (int)floor(ty), c, NULL);
            for (a = 0; a < 3; a++) out[((size_t)y * vw + x) * 4 + a] = (uint8_t)c[a];
            out[((size_t)y * vw + x) * 4 + 3] = 255;
        }
    }
    return out;
}

static int compare_ref(const char *what, const uint8_t *img, const Src *s,
                       int vw, int vh, int mode, int overlay,
                       const float keep[4]) {
    int x, y, a, bad = 0;
    double maxd = 0;

    for (y = 0; y < vh; y++) {
        for (x = 0; x < vw; x++) {
            const uint8_t *p = img + ((size_t)y * vw + x) * 4;
            double ref[3];
            int kept = ref_pixel(s, vw, vh, x, y, mode, overlay, keep, ref);
            int disc = memcmp(p, SENTINEL, 4) == 0;
            double ex = s->x0 + (x + 0.5) / vw * (s->x1 - s->x0);
            double ey = s->y0 + (y + 0.5) / vh * (s->y1 - s->y0);

            /* a pixel centre exactly on a texel edge: which side wins is a
             * float rounding, in the reference as in the program */
            if (overlay && (fabs(ex - floor(ex + 0.5)) < 1e-4 ||
                            fabs(ey - floor(ey + 0.5)) < 1e-4)) {
                continue;
            }

            if (!kept) {
                if (!disc) {
                    if (g_fail++ < 10) {
                        fprintf(stderr, "DIFF %s (%d,%d): kept, reference drops\n",
                                what, x, y);
                    }
                    bad++;
                }
                continue;
            }
            if (disc) {
                if (g_fail++ < 10) {
                    fprintf(stderr, "DIFF %s (%d,%d): dropped, reference keeps\n",
                            what, x, y);
                }
                bad++;
                continue;
            }
            for (a = 0; a < 3; a++) {
                double d = fabs(p[a] - ref[a]);

                if (d > maxd) maxd = d;
                if (d > TOL) {
                    if (g_fail++ < 10) {
                        fprintf(stderr,
                                "DIFF %s (%d,%d) ch %d: gpu %d, reference %.2f\n",
                                what, x, y, a, p[a], ref[a]);
                    }
                    bad++;
                    break;
                }
            }
        }
    }
    printf("%s: %s %dx%d mode %d (max channel error %.2f)\n", bad ? "FAIL" : "OK",
           what, vw, vh, mode, maxd);
    return bad;
}

static void fill_random(void) {
    size_t i;

    for (i = 0; i < (size_t)DW3_NX_VRAM_W * DW3_NX_VRAM_H; i++) {
        uint32_t r = rnd();

        s_vram[i] = (uint16_t)r;
        s_rgba[i * 4] = (uint8_t)(r >> 3);
        s_rgba[i * 4 + 1] = (uint8_t)(r >> 11);
        s_rgba[i * 4 + 2] = (uint8_t)(r >> 19);
        s_rgba[i * 4 + 3] = 255;
    }
}

/* Smooth-ish blocks inside the rectangle (so the seams are visible in the
 * reference), loud noise outside it. */
static void fill_blocks(int x0, int y0, int w, int h, int blk) {
    int x, y;

    fill_random();
    for (y = 0; y < h; y++) {
        for (x = 0; x < w; x++) {
            uint32_t r = (uint32_t)(((x / blk) * 2654435761u) ^
                                    ((y / blk) * 40503u) ^ 0x9e3779b9u);
            size_t at = (size_t)(y0 + y) * DW3_NX_VRAM_W + (x0 + x);
            uint16_t v = (uint16_t)(r ^ (r >> 15));

            /* a third of the blocks are "transparent": black, no mask bit */
            if (((x / blk) + (y / blk) * 3) % 3 == 0) v = 0;
            if (((x / blk) + (y / blk)) % 7 == 0) v = 0x8000;
            s_vram[at] = v;
            s_rgba[at * 4] = (uint8_t)c8(v & 31);
            s_rgba[at * 4 + 1] = (uint8_t)c8((v >> 5) & 31);
            s_rgba[at * 4 + 2] = (uint8_t)c8((v >> 10) & 31);
            s_rgba[at * 4 + 3] = 255;
        }
    }
}

typedef struct {
    int rw, rh; /* source rectangle */
    int vw, vh; /* viewport */
    int fix_rx; /* 0: a random x offset in VRAM; else that one (the right half of
                 * the VRAM is where an fp16 coordinate is coarsest) */
} Case;

static const Case cases[] = {
    {320, 240, 1440, 1080, 0}, /* docked 4:3: 4.5x */
    {320, 240, 960, 720, 0},   /* handheld 4:3: 3x */
    {426, 240, 1920, 1080, 0}, /* widescreen docked */
    {320, 240, 640, 480, 0},   /* 2x */
    {256, 240, 1152, 1080, 0}, /* 4.5x, narrower mode */
    {320, 240, 400, 300, 0},   /* under 2x: plain bilinear */
    {320, 240, 1440, 1080, 640}, /* 4.5x, rectangle in the right half of VRAM */
    {320, 240, 640, 480, 704},   /* 2x, same */
};

static void upload16(GLuint tex) {
    glBindTexture(GL_TEXTURE_2D, tex);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 2);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, DW3_NX_VRAM_W, DW3_NX_VRAM_H,
                    GL_RED_INTEGER, GL_UNSIGNED_SHORT, s_vram);
}

static void upload32(GLuint tex) {
    glBindTexture(GL_TEXTURE_2D, tex);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, DW3_NX_VRAM_W, DW3_NX_VRAM_H,
                    GL_RGBA, GL_UNSIGNED_BYTE, s_rgba);
}

static const char *MESH_VS =
    "#version 300 es\n"
    "precision mediump float;\n"
    "layout(location = 0) in vec2 a_pos;\n"
    "layout(location = 1) in vec2 a_t;\n"
    "out vec2 v_t;\n"
    "void main() { v_t = a_t; gl_Position = vec4(a_pos, 0.0, 1.0); }\n";

static void test_case(const Case *c, int ci, const Prog *q, const Prog *qf,
                      const Prog *uf, const Prog *mf,
                      const Prog *mn, GLuint t16, GLuint t32) {
    int rx = c->fix_rx ? c->fix_rx : 17 + (int)(rnd() % 200);
    int ry = 9 + (int)(rnd() % 100);
    float uv0[2] = {(float)rx / DW3_NX_VRAM_W, (float)ry / DW3_NX_VRAM_H};
    float uv1[2] = {(float)(rx + c->rw) / DW3_NX_VRAM_W,
                    (float)(ry + c->rh) / DW3_NX_VRAM_H};
    float keep[4] = {0.0f, 0.0f, 0.5f, 1.0f};
    Src s16 = {0, rx, ry, rx + c->rw, ry + c->rh};
    Src s32 = {1, rx, ry, rx + c->rw, ry + c->rh};
    uint8_t *a;
    int mode, bad = 0;
    int integer = c->vw % c->rw == 0 && c->vh % c->rh == 0;

    fill_blocks(rx, ry, c->rw, c->rh, 1 + (ci % 3));
    upload16(t16);
    upload32(t32);
    for (mode = 1; mode <= 2; mode++) {
        /* 1: opaque program */
        clear_sentinel();
        draw(qf, t16, c->vw, c->vh, uv0, uv1, NULL, mode);
        a = readback(c->vw, c->vh);
        bad += compare_ref("opaque program", a, &s16, c->vw, c->vh, mode, 0,
                           NULL);
        if (mode == 2 && c->vw / c->rw >= 2) {
            uint8_t *ideal = nearest_ref(&s16, c->vw, c->vh);
            /* a row through the middle of a source row, so only the vertical
             * seams are crossed */
            int y = (int)(((c->rh / 2) + 0.5) * c->vh / c->rh - 0.5 + 0.5), x, run = 0,
                maxrun = 0, flat = 0;
            size_t k, diff = 0;
            double P = floor((double)c->vw / c->rw);

            for (k = 0; k < (size_t)c->vw * c->vh; k++) {
                if (memcmp(a + k * 4, ideal + k * 4, 3) != 0) diff++;
            }
            if (integer) {
                /* 2: at an integer scale every pixel centre is in a plateau */
                if (diff) {
                    bad++;
                    g_fail++;
                    printf("FAIL: sharp at the integer scale %dx differs from "
                           "nearest in %zu pixels\n", c->vw / c->rw, diff);
                } else {
                    printf("OK: sharp at the integer scale %dx is identical to "
                           "nearest\n", c->vw / c->rw);
                }
            } else {
                /* 3: the middle of a source pixel is flat, the seams are short */
                for (x = 0; x < c->vw; x++) {
                    const uint8_t *pa = a + ((size_t)y * c->vw + x) * 4;
                    const uint8_t *pb = ideal + ((size_t)y * c->vw + x) * 4;

                    if (memcmp(pa, pb, 3) == 0) {
                        run = 0;
                        flat++;
                    } else {
                        run++;
                        if (run > maxrun) maxrun = run;
                    }
                }
                if (maxrun > 2 || flat < c->vw * 6 / 10) {
                    bad++;
                    g_fail++;
                    printf("FAIL: sharp seams: longest run of pixels off nearest "
                           "%d, flat %d of %d\n", maxrun, flat, c->vw);
                } else {
                    printf("OK: sharp seams at %.2fx (prescale %.0f): longest run "
                           "off nearest %d px, %d%% of a row equals nearest, "
                           "%.1f%% of the picture differs\n",
                           (double)c->vw / c->rw, P, maxrun, flat * 100 / c->vw,
                           100.0 * (double)diff / ((double)c->vw * c->vh));
                }
            }
            {
                /* the Nearest program against the ideal one: the same texel
                 * everywhere except where the pixel centre is exactly on a
                 * texel edge (4.5x: one pixel in nine), where either neighbour is
                 * the nearest and float rounding picks one. With `mediump`
                 * coordinates (fp16 in Mesa llvmpipe) 2 to 50% of the pixels were
                 * off, whatever the scale. */
                uint8_t *n;
                size_t nd = 0, ties = 0;

                clear_sentinel();
                draw(q, t16, c->vw, c->vh, uv0, uv1, NULL, 0);
                n = readback(c->vw, c->vh);
                for (k = 0; k < (size_t)c->vw * c->vh; k++) {
                    int px = (int)(k % (size_t)c->vw), py = (int)(k / (size_t)c->vw);
                    double ex = s16.x0 + (px + 0.5) / c->vw * (s16.x1 - s16.x0);
                    double ey = s16.y0 + (py + 0.5) / c->vh * (s16.y1 - s16.y0);
                    int tie = fabs(ex - floor(ex + 0.5)) < 1e-3 ||
                              fabs(ey - floor(ey + 0.5)) < 1e-3;

                    if (memcmp(n + k * 4, ideal + k * 4, 3) != 0) {
                        if (tie) ties++; else nd++;
                    }
                }
                if (nd) {
                    g_fail++;
                    printf("FAIL: the Nearest program picks a neighbouring texel "
                           "in %zu of %zu pixels (%.2fx, rectangle at x %d)\n", nd,
                           (size_t)c->vw * c->vh, (double)c->vw / c->rw, rx);
                } else {
                    printf("OK: the Nearest program is the ideal nearest at "
                           "%.2fx (rectangle at x %d; %zu edge-tie pixels differ, "
                           "either side is nearest)\n", (double)c->vw / c->rw, rx,
                           ties);
                }
                free(n);
            }
            free(ideal);
        }
        free(a);

        /* 4: overlay program */
        clear_sentinel();
        draw(uf, t16, c->vw, c->vh, uv0, uv1, keep, mode);
        a = readback(c->vw, c->vh);
        bad += compare_ref("overlay program", a, &s16, c->vw, c->vh, mode, 1,
                           keep);
        free(a);

        /* 5: mesh blit program */
        clear_sentinel();
        draw(mf, t32, c->vw, c->vh, uv0, uv1, NULL, mode);
        a = readback(c->vw, c->vh);
        bad += compare_ref("mesh blit program", a, &s32, c->vw, c->vh, mode, 0,
                           NULL);
        free(a);
    }
    (void)mn;
    (void)bad;
}

static int64_t now_us(void) {
    struct timespec ts;

    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (int64_t)ts.tv_sec * 1000000 + ts.tv_nsec / 1000;
}

static int cmp64(const void *a, const void *b) {
    int64_t x = *(const int64_t *)a, y = *(const int64_t *)b;

    return x < y ? -1 : x > y;
}

static void bench(const Prog *q, const Prog *qf, const Prog *u, const Prog *uf,
                  GLuint t16) {
    enum { ITERS = 41 };
    const int vw = 1440, vh = 1080;
    float uv0[2] = {0.0f, 0.0f};
    float uv1[2] = {320.0f / DW3_NX_VRAM_W, 240.0f / DW3_NX_VRAM_H};
    float keep[4] = {2.0f, 2.0f, -1.0f, -1.0f};
    int64_t t[ITERS];
    int mode, it, which;

    fill_blocks(0, 0, 320, 240, 2);
    upload16(t16);
    for (which = 0; which < 2; which++) {
        for (mode = 0; mode <= 2; mode++) {
            const Prog *p = which ? (mode ? uf : u) : (mode ? qf : q);

            for (it = 0; it < ITERS + 3; it++) {
                int64_t t0 = now_us();

                draw(p, t16, vw, vh, uv0, uv1, which ? keep : NULL, mode);
                glFinish();
                if (it >= 3) t[it - 3] = now_us() - t0;
            }
            qsort(t, ITERS, sizeof(t[0]), cmp64);
            printf("bench %s program, %s: median %lld us (min %lld) for %dx%d "
                   "on llvmpipe\n", which ? "overlay" : "opaque",
                   mode == 0 ? "nearest" : mode == 1 ? "bilinear" : "sharp",
                   (long long)t[ITERS / 2], (long long)t[0], vw, vh);
        }
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
    GLuint fbo, rb, t16, t32;
    Prog q, qf, u, uf, mf, mn;
    size_t i;

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
    printf("GL: %s | %s\n", (const char *)glGetString(GL_VERSION),
           (const char *)glGetString(GL_RENDERER));

    glGenFramebuffers(1, &fbo);
    glGenRenderbuffers(1, &rb);
    glBindRenderbuffer(GL_RENDERBUFFER, rb);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_RGBA8, 1920, 1080);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                              GL_RENDERBUFFER, rb);
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        fprintf(stderr, "SKIP: RGBA8 renderbuffer is not complete\n");
        return SKIP;
    }

    quad_init();
    q = link_prog(DW3_NX_P16_VS, DW3_NX_P16_QUAD_FS);
    qf = link_prog(DW3_NX_P16_VS, DW3_NX_P16_QUAD_FS_F);
    u = link_prog(DW3_NX_P16_VS, DW3_NX_P16_UI_FS);
    uf = link_prog(DW3_NX_P16_VS, DW3_NX_P16_UI_FS_F);
    mf = link_prog(MESH_VS, DW3_NX_MESH_BLIT_FS_F);
    mn = mf;
    t16 = make_tex(1);
    t32 = make_tex(0);
    if (glGetError() != GL_NO_ERROR) {
        fprintf(stderr, "GL error while setting up\n");
        return 1;
    }

    for (i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
        test_case(&cases[i], (int)i, &q, &qf, &uf, &mf, &mn, t16, t32);
    }
    if (glGetError() != GL_NO_ERROR) {
        fprintf(stderr, "GL error during the run\n");
        return 1;
    }
    if (g_fail) {
        printf("FAIL: %d differences\n", g_fail);
        return 1;
    }
    printf("OK: the output filter programs match the CPU reference\n");
    if (argc > 1 && strcmp(argv[1], "--bench") == 0) {
        bench(&q, &qf, &u, &uf, t16);
    }
    return 0;
}
