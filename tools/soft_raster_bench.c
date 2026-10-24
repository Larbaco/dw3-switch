/* P6.22: replay a Draw_* trace (DW3_SOFT_TRACE, platforms/pc/src/field_prof.c)
 * through PSY-Z's CPU rasterizer (sdl3_software.c, what the Switch draws the
 * field with), time it, and optionally compare it pixel for pixel with a
 * reference copy of the rasterizer built into the same binary.
 *
 * Build and run through tools/soft_raster_bench.sh, which compiles the
 * reference copy (a git revision of sdl3_software.c) with its symbols
 * renamed to ref_*.
 *
 *   soft_raster_bench <trace> [from_frame [to_frame]]   time frames [from, to)
 *   soft_raster_bench --fuzz <iterations> [seed]        random primitives,
 *                                                       compared after each one
 *
 * With a reference copy (-DWITH_REF) every frame of the trace (and, in fuzz
 * mode, every primitive) is compared: VRAM and both gutter planes. Exit status
 * 1 on any difference. */
#include <psyz.h>
#include "../internal.h"
#include "rasterizer.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#ifndef AREA_Y_MAX
#define AREA_Y_MAX 1023
#endif

PsyzDitherMode Psyz_VideoGetDitheringMode(void) { return PSYZ_DITHER_AUTO; }
void psyz_log(unsigned int level, const char *file, unsigned int line,
              const char *func, const char *fmt, ...) {
    (void)level;
    (void)file;
    (void)line;
    (void)func;
    (void)fmt;
}

typedef struct {
    const DrawBackend *be;
    void (*rs_reset)(void);
    const u16 *(*vram)(void);
    void (*set_gutter)(int, int, int, int);
    const u16 *(*plane)(int);
    void (*clear_rows)(int, int);
    uint64_t ns;
    uint64_t kind_ns[8], kind_total[8]; /* by primitive kind, timed frames */
} Raster;

/* tile, sprt, flat/gouraud poly, flat/gouraud textured poly, line, image */
static const char *k_name[8] = {"tile", "sprt", "pf", "pg", "pft", "pgt",
                                "line", "img"};

static Raster g_new = {&software_rasterizer,         SoftwareRasterizer_Reset,
                       SoftwareRasterizer_GetVram,   SoftwareRasterizer_SetGutter,
                       SoftwareRasterizer_GetGutterPlane,
                       SoftwareRasterizer_ClearGutterRows, 0, {0}, {0}};

#ifdef WITH_REF
extern const DrawBackend ref_software_rasterizer;
void ref_SoftwareRasterizer_Reset(void);
const u16 *ref_SoftwareRasterizer_GetVram(void);
void ref_SoftwareRasterizer_SetGutter(int px, int x0, int w, int h);
const u16 *ref_SoftwareRasterizer_GetGutterPlane(int side);
void ref_SoftwareRasterizer_ClearGutterRows(int y0, int y1);
static Raster g_ref = {&ref_software_rasterizer,       ref_SoftwareRasterizer_Reset,
                       ref_SoftwareRasterizer_GetVram, ref_SoftwareRasterizer_SetGutter,
                       ref_SoftwareRasterizer_GetGutterPlane,
                       ref_SoftwareRasterizer_ClearGutterRows, 0, {0}, {0}};
#endif

static uint64_t now_ns(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ull + (uint64_t)ts.tv_nsec;
}

enum {
    T_RESET = 1, T_TEXPAGE, T_TWIN, T_AREA0, T_AREA1, T_OFFSET, T_MASK,
    T_CLEAR, T_LOAD, T_STORE, T_MOVE, T_PRIM, T_GUTTER, T_GUTTER_CLEAR,
    T_FRAME, T_GRID
};

/* one decoded op, applied to a rasterizer */
typedef struct {
    int op;
    int32_t a[8];
    const u16 *data;    /* T_LOAD */
    u_long *words;      /* T_PRIM */
    int n;
} Op;

static u16 g_store[1024 * 512];
static int kind_of(const Op *o);

static void apply(Raster *r, const Op *o) {
    const DrawBackend *b = r->be;
    PS1_RECT rect;
    uint64_t t0 = now_ns();

    switch (o->op) {
    case T_RESET:
        b->reset();
        break;
    case T_TEXPAGE: {
        u_long word = (u_long)(uint32_t)o->a[0];
        b->texpage_mode((ParamDrawTexpageMode *)&word);
        break;
    }
    case T_TWIN:
        b->texture_window((unsigned)o->a[0], (unsigned)o->a[1],
                          (unsigned)o->a[2], (unsigned)o->a[3]);
        break;
    case T_AREA0:
        b->area_start(o->a[0], o->a[1]);
        break;
    case T_AREA1:
        b->area_end(o->a[0], o->a[1]);
        break;
    case T_OFFSET:
        b->offset(o->a[0], o->a[1]);
        break;
    case T_MASK:
        b->mask(o->a[0], o->a[1]);
        break;
    case T_CLEAR:
    case T_LOAD:
    case T_STORE:
    case T_MOVE:
        rect.x = (short)o->a[0];
        rect.y = (short)o->a[1];
        rect.w = (short)o->a[2];
        rect.h = (short)o->a[3];
        if (o->op == T_CLEAR) {
            b->clear_image(&rect, (u_char)o->a[4], (u_char)o->a[5],
                           (u_char)o->a[6]);
        } else if (o->op == T_LOAD) {
            b->load_image(&rect, (u_long *)(uintptr_t)o->data);
        } else if (o->op == T_STORE) {
            if ((long)rect.w * rect.h <= (long)(sizeof(g_store) / 2)) {
                b->store_image(&rect, (u_long *)g_store);
            }
        } else {
            b->move_image(&rect, (unsigned)o->a[4], (unsigned)o->a[5]);
        }
        break;
    case T_PRIM:
        b->push_prim(o->words, o->n);
        break;
    case T_GUTTER:
        r->set_gutter(o->a[0], o->a[1], o->a[2], o->a[3]);
        break;
    case T_GUTTER_CLEAR:
        r->clear_rows(o->a[0], o->a[1]);
        break;
    case T_GRID:
        b->horizontal_grid((unsigned)o->a[0], (unsigned)o->a[1]);
        break;
    }
    {
        uint64_t d = now_ns() - t0;
        r->ns += d;
        r->kind_ns[kind_of(o)] += d;
    }
}

static int kind_of(const Op *o) {
    if (o->op != T_PRIM) {
        return 7;
    }
    unsigned code = (unsigned)(o->words[0] >> 24) & 0xFF;
    if ((code & 0xE0) == 0x20) {
        return (code & 4) ? ((code & 0x10) ? 5 : 4) : ((code & 0x10) ? 3 : 2);
    }
    if ((code & 0xE0) == 0x40) {
        return 6;
    }
    return (code & 4) ? 1 : 0;
}

static void kinds_roll(Raster *r, int timed) {
    for (int k = 0; k < 8; k++) {
        if (timed) {
            r->kind_total[k] += r->kind_ns[k];
        }
        r->kind_ns[k] = 0;
    }
}

/* same hash as field_prof.c's DW3_SOFT_HASH, to check the trace replays the
 * game's frames exactly */
static uint64_t hash_new(void) {
    const uint64_t *p = (const uint64_t *)g_new.vram();
    uint64_t h = 1469598103934665603ull;
    for (size_t i = 0; i < 1024u * 512u / 4u; i++) {
        h = (h ^ p[i]) * 1099511628211ull;
    }
    for (int side = 0; side < 2; side++) {
        const uint64_t *g = (const uint64_t *)g_new.plane(side);
        for (size_t i = 0; i < 512u * PSYZ_GUTTER_MAX / 4u; i++) {
            h = (h ^ g[i]) * 1099511628211ull;
        }
    }
    return h;
}

#ifdef WITH_REF
static long diff_px(void) {
    const u16 *a = g_new.vram(), *b = g_ref.vram();
    long n = 0;
    for (int i = 0; i < 1024 * 512; i++) {
        n += a[i] != b[i];
    }
    for (int s = 0; s < 2; s++) {
        a = g_new.plane(s);
        b = g_ref.plane(s);
        for (int i = 0; i < 512 * PSYZ_GUTTER_MAX; i++) {
            n += a[i] != b[i];
        }
    }
    return n;
}
#endif

/* ---- trace mode ---------------------------------------------------------- */

static int run_trace(const char *path, unsigned from, unsigned to) {
    FILE *fp = fopen(path, "rb");
    long size;
    unsigned char *buf, *p, *end;
    unsigned frame = 0, frames_timed = 0;
    uint64_t new_ns = 0, ref_ns = 0;
    long mismatched_frames = 0, first_bad = -1, worst = 0;
    u_long words[64];
    Op o;
    const char *hash_path = getenv("BENCH_HASH");
    FILE *hash_fp = hash_path ? fopen(hash_path, "w") : NULL;

    if (!fp) {
        perror(path);
        if (hash_fp) {
            fclose(hash_fp);
        }
        return 2;
    }
    fseek(fp, 0, SEEK_END);
    size = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    buf = malloc((size_t)size);
    if (!buf || fread(buf, 1, (size_t)size, fp) != (size_t)size ||
        memcmp(buf, "DW3T", 4)) {
        fprintf(stderr, "bad trace %s\n", path);
        free(buf);
        fclose(fp);
        if (hash_fp) {
            fclose(hash_fp);
        }
        return 2;
    }
    fclose(fp);
    p = buf + 4;
    end = buf + size;
    g_new.rs_reset();
#ifdef WITH_REF
    g_ref.rs_reset();
#endif
    while (p + 4 <= end) {
        uint32_t op;
        int nargs = 0;
        memcpy(&op, p, 4);
        p += 4;
        memset(&o, 0, sizeof(o));
        o.op = (int)op;
        switch (op) {
        case T_RESET: nargs = 0; break;
        case T_TEXPAGE: nargs = 1; break;
        case T_TWIN: nargs = 4; break;
        case T_AREA0: case T_AREA1: case T_OFFSET: case T_MASK:
        case T_GUTTER_CLEAR: nargs = 2; break;
        case T_CLEAR: nargs = 7; break;
        case T_LOAD: case T_STORE: case T_GUTTER: nargs = 4; break;
        case T_MOVE: nargs = 6; break;
        case T_FRAME: nargs = 1; break;
        case T_PRIM: nargs = 0; break;
        default:
            fprintf(stderr, "bad op %u at %ld\n", op, (long)(p - buf));
            free(buf);
            if (hash_fp) {
                fclose(hash_fp);
            }
            return 2;
        }
        memcpy(o.a, p, (size_t)nargs * 4);
        p += nargs * 4;
        if (op == T_LOAD) {
            o.data = (const u16 *)p;
            if (o.a[2] > 0 && o.a[3] > 0) {
                p += (size_t)o.a[2] * (size_t)o.a[3] * 2u;
            }
        } else if (op == T_PRIM) {
            uint32_t n;
            memcpy(&n, p, 4);
            p += 4;
            for (uint32_t i = 0; i < n && i < 64; i++) {
                uint64_t w;
                memcpy(&w, p + i * 8, 8);
                words[i] = (u_long)w;
            }
            p += (size_t)n * 8u;
            o.words = words;
            o.n = (int)(n < 64 ? n : 64);
        } else if (op == T_FRAME) {
            int timed = frame >= from && frame < to;
            if (timed) {
                frames_timed++;
                new_ns += g_new.ns;
#ifdef WITH_REF
                ref_ns += g_ref.ns;
#endif
            }
            g_new.ns = 0;
            kinds_roll(&g_new, timed);
#ifdef WITH_REF
            kinds_roll(&g_ref, timed);
#endif
            if (hash_fp) {
                fprintf(hash_fp, "%d %016llx\n", o.a[0],
                        (unsigned long long)hash_new());
            }
#ifdef WITH_REF
            g_ref.ns = 0;
            {
                long d = diff_px();
                if (d) {
                    mismatched_frames++;
                    if (first_bad < 0) {
                        first_bad = frame;
                    }
                    if (d > worst) {
                        worst = d;
                    }
                }
            }
#endif
            frame++;
            continue;
        }
        apply(&g_new, &o);
#ifdef WITH_REF
        apply(&g_ref, &o);
#endif
    }
    free(buf);
    if (hash_fp) {
        fclose(hash_fp);
    }
    printf("trace %s: %u frames, timed [%u, %u): %u frames\n", path, frame,
           from, to, frames_timed);
    if (frames_timed) {
        printf("  new rasterizer: %8.1f us per frame\n",
               new_ns / 1000.0 / frames_timed);
#ifdef WITH_REF
        printf("  ref rasterizer: %8.1f us per frame (speedup %.2fx)\n",
               ref_ns / 1000.0 / frames_timed,
               new_ns ? (double)ref_ns / (double)new_ns : 0.0);
#endif
        printf("  per kind, us per frame (new");
#ifdef WITH_REF
        printf(" / ref");
#endif
        printf("):");
        for (int k = 0; k < 8; k++) {
            printf(" %s=%.0f", k_name[k],
                   g_new.kind_total[k] / 1000.0 / frames_timed);
#ifdef WITH_REF
            printf("/%.0f", g_ref.kind_total[k] / 1000.0 / frames_timed);
#endif
        }
        printf("\n");
    }
#ifdef WITH_REF
    printf("  compare: %ld of %u frames differ (first %ld, worst %ld px)\n",
           mismatched_frames, frame, first_bad, worst);
    return mismatched_frames ? 1 : 0;
#else
    (void)ref_ns;
    (void)mismatched_frames;
    (void)first_bad;
    (void)worst;
    return 0;
#endif
}

/* ---- fuzz mode ----------------------------------------------------------- */

#ifdef WITH_REF
static uint64_t g_rng;
static uint32_t rnd(void) {
    g_rng ^= g_rng << 13;
    g_rng ^= g_rng >> 7;
    g_rng ^= g_rng << 17;
    return (uint32_t)g_rng;
}
static int rr(int lo, int hi) { return lo + (int)(rnd() % (uint32_t)(hi - lo + 1)); }

static const int k_edge[] = {-1024, -1023, -1, 0, 1, 319, 320, 511, 512, 639, 1022, 1023};
static int redge(void) { return k_edge[rnd() % (sizeof(k_edge) / sizeof(k_edge[0]))] + rr(-2, 2); }
static uint32_t rxy(void) {
    int m = rnd() % 6;
    if (m == 0) return rnd(); /* garbage high bits, any 11-bit value */
    int x = m == 1 ? redge() : m == 2 ? rr(-1024, 1023) : rr(-80, 1100);
    int y = m == 1 ? redge() : m == 2 ? rr(-1024, 1023) : rr(-40, 560);
    return ((uint32_t)(y & 0xFFFF) << 16) | (uint32_t)(x & 0xFFFF);
}
static uint32_t g_last_xy;
static uint32_t rxy2(void) {
    /* sometimes reuse / nudge the previous vertex: degenerate and sliver */
    uint32_t w;
    int m = rnd() % 5;
    if (m == 0) w = g_last_xy;
    else if (m == 1) w = ((g_last_xy + (rnd() % 3) - 1) & 0xFFFF) | (g_last_xy & 0xFFFF0000u);
    else if (m == 2) w = (g_last_xy & 0xFFFF) | ((g_last_xy + ((rnd() % 3) << 16)) & 0xFFFF0000u);
    else w = rxy();
    g_last_xy = w;
    return w;
}

static void both(const Op *o) {
    apply(&g_new, o);
    apply(&g_ref, o);
}

static int run_fuzz(long iters, uint64_t seed) {
    static u16 img[1024 * 512];
    Op o;
    u_long words[16];
    long bad = 0;
    int gutter_x0 = 0, gutter_w = 320;

    g_rng = seed ? seed : 88172645463325252ull;
    g_new.rs_reset();
    g_ref.rs_reset();
    for (int i = 0; i < 1024 * 512; i++) {
        img[i] = (u16)rnd();
        if (rnd() % 5 == 0) {
            img[i] = 0; /* transparent texels */
        }
    }
    memset(&o, 0, sizeof(o));
    o.op = T_LOAD;
    o.a[0] = 0; o.a[1] = 0; o.a[2] = 1024; o.a[3] = 512;
    o.data = img;
    both(&o);
    for (long it = 0; it < iters; it++) {
        memset(&o, 0, sizeof(o));
        int pick = rr(0, 99);
        if (pick < 4) {
            o.op = T_TEXPAGE;
            o.a[0] = (int32_t)(rnd() & 0xFFFF);
        } else if (pick < 6) {
            o.op = T_TWIN;
            for (int i = 0; i < 4; i++) {
                o.a[i] = rnd() % 3 ? 0 : (int32_t)(rnd() & 63);
            }
        } else if (pick < 9) {
            /* a display-wide area at the gutter's display (takes the
             * gutters: whole screen or a band of rows), or any area */
            int disp = rnd() % 3 != 0;
            int y0 = rnd() % 2 ? 0 : rr(0, 300);
            o.op = T_AREA0;
            if (rnd() % 3 == 0) y0 = rr(0, AREA_Y_MAX);
            o.a[0] = disp ? gutter_x0 : rr(0, 1023);
            o.a[1] = y0;
            both(&o);
            o.op = T_AREA1;
            o.a[0] = disp ? gutter_x0 + gutter_w - 1 : rr(0, 1023);
            o.a[1] = disp && rnd() % 2 ? y0 + 239 : rr(0, AREA_Y_MAX);
            if (o.a[0] > 1023) o.a[0] = 1023;
            if (o.a[1] > AREA_Y_MAX) o.a[1] = AREA_Y_MAX;
        } else if (pick < 11) {
            o.op = T_OFFSET;
            o.a[0] = rnd() % 3 == 0 ? 0 : rr(0, 0x7FF);
            o.a[1] = rnd() % 3 == 0 ? 0 : rr(0, 0x7FF);
        } else if (pick < 13) {
            o.op = T_MASK;
            o.a[0] = rnd() % 4 == 0;
            o.a[1] = rnd() % 4 == 0;
        } else if (pick < 15) {
            o.op = T_GUTTER;
            o.a[0] = rnd() % 3 ? 53 : rr(0, 100);
            o.a[1] = gutter_x0 = rnd() % 2 ? 0 : rr(0, 1023);
            o.a[2] = gutter_w = rnd() % 2 ? 320 : rr(1, 1100);
            o.a[3] = 240;
        } else if (pick < 16 && rnd() % 4 == 0) {
            o.op = T_GRID;
            o.a[0] = rnd() % 2 ? 1 : rr(1, 4);
            o.a[1] = rnd() % 2 ? o.a[0] : rr(1, 4);
        } else if (pick < 16) {
            o.op = T_GUTTER_CLEAR;
            o.a[0] = rr(0, 511);
            o.a[1] = rr(0, 512);
        } else if (pick < 17) {
            o.op = T_CLEAR;
            o.a[0] = rr(-20, 1000); o.a[1] = rr(-20, 500);
            o.a[2] = rr(0, 400); o.a[3] = rr(0, 300);
            o.a[4] = rr(0, 255); o.a[5] = rr(0, 255); o.a[6] = rr(0, 255);
        } else if (pick < 18) {
            o.op = T_MOVE;
            o.a[0] = rr(0, 1000); o.a[1] = rr(0, 500);
            o.a[2] = rr(0, 400); o.a[3] = rr(0, 300);
            o.a[4] = rr(0, 1000); o.a[5] = rr(0, 500);
        } else {
            /* a primitive: polygon, line or rectangle, any flag mix */
            int kind = rr(0, 2);
            int code;
            int n = 0;
            uint32_t color = rnd() & 0xFFFFFF;
            if (rnd() % 4 == 0) {
                color = 0x808080; /* neutral modulation */
            }
            if (kind == 0) {
                code = 0x20 | (rnd() & 0x1F);
                int tex = (code & 4) != 0, gour = (code & 0x10) != 0;
                int cnt = (code & 8) ? 4 : 3;
                words[n++] = ((u_long)code << 24) | color;
                for (int v = 0; v < cnt; v++) {
                    if (v && gour) {
                        words[n++] = rnd() & 0xFFFFFF;
                    }
                    words[n++] = rxy2();
                    if (tex) {
                        uint32_t hi = v == 0 ? (rnd() & 0xFFFF)
                                      : v == 1 ? (rnd() & 0xFFFF) : rnd() & 0xFFFF;
                        words[n++] = (hi << 16) | (rnd() & 0xFFFF);
                    }
                }
            } else if (kind == 1) {
                code = 0x40 | (rnd() & 0x1F);
                int gour = (code & 0x10) != 0;
                int pts = ((code >> 2) & 3) + 1;
                if (pts == 1) {
                    pts = 2;
                } else {
                    code &= ~0x0C; /* keep polylines short: two points */
                    code |= 0x04;
                    pts = 2;
                }
                words[n++] = ((u_long)code << 24) | color;
                for (int v = 0; v < pts; v++) {
                    if (v && gour) {
                        words[n++] = rnd() & 0xFFFFFF;
                    }
                    words[n++] = rxy();
                }
                if ((((code >> 2) & 3) + 1) != 1) {
                    words[n++] = 0x55555555;
                }
            } else {
                code = 0x60 | (rnd() & 0x1F);
                int tex = (code & 4) != 0;
                words[n++] = ((u_long)code << 24) | color;
                words[n++] = rxy();
                if (tex) {
                    words[n++] = rnd();
                }
                if ((code & 0x18) == 0) {
                    int w = rnd() % 8 ? rr(0, 320) : rnd() % 2 ? rr(-50, 1300) : (int)(rnd() & 0xFFFF);
                    int h = rnd() % 8 ? rr(0, 240) : rnd() % 2 ? rr(-50, 1100) : (int)(rnd() & 0x7FF);
                    words[n++] = ((uint32_t)(h & 0xFFFF) << 16) | (uint32_t)(w & 0xFFFF);
                }
            }
            o.op = T_PRIM;
            o.words = words;
            o.n = n;
        }
        both(&o);
        if (o.op == T_PRIM || it % 64 == 0) {
            long d = diff_px();
            if (d) {
                if (bad < 5) {
                    fprintf(stderr, "fuzz: iteration %ld op %d code %02x: %ld px differ\n",
                            it, o.op, o.op == T_PRIM ? (int)(words[0] >> 24) : 0, d);
                }
                bad++;
                /* resync so later iterations are still meaningful */
                memset(&o, 0, sizeof(o));
                o.op = T_LOAD;
                o.a[0] = 0; o.a[1] = 0; o.a[2] = 1024; o.a[3] = 512;
                memcpy(img, g_ref.vram(), sizeof(img));
                o.data = img;
                both(&o);
            }
        }
    }
    printf("fuzz: %ld iterations, seed %llu: %ld differences\n", iters,
           (unsigned long long)seed, bad);
    printf("  new %.1f ms, ref %.1f ms\n", g_new.ns / 1e6, g_ref.ns / 1e6);
    return bad ? 1 : 0;
}
#endif

int main(int argc, char **argv) {
    if (argc >= 3 && !strcmp(argv[1], "--fuzz")) {
#ifdef WITH_REF
        return run_fuzz(atol(argv[2]), argc > 3 ? strtoull(argv[3], NULL, 0) : 1);
#else
        fprintf(stderr, "--fuzz needs a reference build (WITH_REF)\n");
        return 2;
#endif
    }
    if (argc < 2) {
        fprintf(stderr,
                "usage: %s <trace> [from [to]] | --fuzz <iterations> [seed]\n",
                argv[0]);
        return 2;
    }
    return run_trace(argv[1], argc > 2 ? (unsigned)atoi(argv[2]) : 0,
                     argc > 3 ? (unsigned)atoi(argv[3]) : 0xFFFFFFFFu);
}
