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
    const u16 *(*vram_raw)(void);
    const u16 *(*plane_raw)(int);
    uint64_t ns;
    uint64_t kind_ns[8], kind_total[8]; /* by primitive kind, timed frames */
} Raster;

/* tile, sprt, flat/gouraud poly, flat/gouraud textured poly, line, image */
static const char *k_name[8] = {"tile", "sprt", "pf", "pg", "pft", "pgt",
                                "line", "img"};

static Raster g_new = {&software_rasterizer,         SoftwareRasterizer_Reset,
                       SoftwareRasterizer_GetVram,   SoftwareRasterizer_SetGutter,
                       SoftwareRasterizer_GetGutterPlane,
                       SoftwareRasterizer_ClearGutterRows,
                       SoftwareRasterizer_GetVramRaw,
                       SoftwareRasterizer_GetGutterPlaneRaw, 0, {0}, {0}};

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
                       ref_SoftwareRasterizer_ClearGutterRows,
                       ref_SoftwareRasterizer_GetVram,
                       ref_SoftwareRasterizer_GetGutterPlane, 0, {0}, {0}};
#endif

static uint64_t now_ns(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ull + (uint64_t)ts.tv_nsec;
}

enum {
    T_RESET = 1, T_TEXPAGE, T_TWIN, T_AREA0, T_AREA1, T_OFFSET, T_MASK,
    T_CLEAR, T_LOAD, T_STORE, T_MOVE, T_PRIM, T_GUTTER, T_GUTTER_CLEAR,
    T_FRAME, T_GRID, T_DISPLAY
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


/* ---- deferred drawing (SoftwareRasterizer_SetDefer) ----------------------- */

static int g_defer;          /* 0: draw at once; N: a present every N frames */
static int g_defer_alt;      /* present the previous buffer instead */
static int g_disp[4];        /* x, y, w, h of the display, from T_DISPLAY */
static int g_disp_seen;
static int g_area_cur[4], g_area_prev[4];

#ifdef WITH_REF
/* VRAM in the rectangle and the gutter plane rows y .. y + h - 1 */
static long diff_rect(int x, int y, int w, int h) {
    const u16 *a = g_new.vram_raw(), *b = g_ref.vram_raw();
    long n = 0;
    int x1 = x + w, y1 = y + h;

    if (x < 0) x = 0;
    if (y < 0) y = 0;
    if (x1 > 1024) x1 = 1024;
    if (y1 > 512) y1 = 512;
    for (int yy = y; yy < y1; yy++) {
        for (int xx = x; xx < x1; xx++) {
            n += a[yy * 1024 + xx] != b[yy * 1024 + xx];
        }
    }
    for (int s = 0; s < 2; s++) {
        a = g_new.plane_raw(s);
        b = g_ref.plane_raw(s);
        for (int yy = y; yy < y1; yy++) {
            for (int xx = 0; xx < PSYZ_GUTTER_MAX; xx++) {
                n += a[yy * PSYZ_GUTTER_MAX + xx] !=
                     b[yy * PSYZ_GUTTER_MAX + xx];
            }
        }
    }
    return n;
}
#endif

/* the rectangle a present at this frame shows */
static void present_rect(int *r) {
    if (g_disp_seen) {
        memcpy(r, g_disp, sizeof(g_disp));
    } else {
        memcpy(r, g_defer_alt ? g_area_prev : g_area_cur, sizeof(g_disp));
    }
}

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
    if (g_defer) {
        SoftwareRasterizer_SetDefer(1);
    }
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
        case T_DISPLAY: nargs = 4; break;
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
        } else if (op == T_DISPLAY) {
            memcpy(g_disp, o.a, sizeof(g_disp));
            g_disp_seen = 1;
            continue;
        } else if (op == T_FRAME && g_defer) {
            int timed = frame >= from && frame < to;

            if (((frame + 1) % (unsigned)g_defer) == 0) {
                int r[4];
                uint64_t t0 = now_ns();
                uint64_t dt;

                present_rect(r);
                SoftwareRasterizer_SyncRect(r[0], r[1], r[2], r[3], 1);
                dt = now_ns() - t0;
                g_new.ns += dt;
                g_new.kind_ns[7] += dt;
#ifdef WITH_REF
                {
                    long d = diff_rect(r[0], r[1], r[2], r[3]);
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
                g_defer_alt ^= 1;
            }
            if (timed) {
                frames_timed++;
                new_ns += g_new.ns;
            }
            g_new.ns = 0;
            kinds_roll(&g_new, timed);
            frame++;
            continue;
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
        if (op == T_AREA0) {
            if (o.a[1] != g_area_cur[1] || o.a[0] != g_area_cur[0]) {
                memcpy(g_area_prev, g_area_cur, sizeof(g_area_cur));
            }
            g_area_cur[0] = o.a[0];
            g_area_cur[1] = o.a[1];
        } else if (op == T_AREA1) {
            g_area_cur[2] = o.a[0] - g_area_cur[0] + 1;
            g_area_cur[3] = o.a[1] - g_area_cur[1] + 1;
        }
        apply(&g_new, &o);
#ifdef WITH_REF
        apply(&g_ref, &o);
#endif
    }
    if (g_defer) {
        unsigned long q, dr, run, sl, ol;

        SoftwareRasterizer_SyncAll();
        SoftwareRasterizer_GetDeferStats(&q, &dr, &run, &sl, &ol, 0);
        printf("  deferred: %lu prims queued, %lu dropped, %lu drawn "
               "(layers drawn: %lu at presents, %lu for other reasons)\n",
               q, dr, run, sl, ol);
#ifdef WITH_REF
        {
            long d = diff_px();
            if (d) {
                printf("  final VRAM differs: %ld px\n", d);
                mismatched_frames++;
            }
        }
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

/* ---- deferred drawing fuzz ------------------------------------------------ */

/* Structured like a game: two display buffers drawn in turn, each frame
 * starting with an opaque clear, textures that sometimes lie in a buffer,
 * image ops between and into the buffers, gutter row clears at the flips and
 * "presents" (SoftwareRasterizer_SyncRect) that are compared with the
 * immediate rasterizer on the rectangle they show. */
static uint32_t dxy(int ax, int ay) {
    if (rnd() % 8 == 0) {
        return rxy();
    }
    int x = ax + rr(-60, 380);
    int y = ay + rr(-40, 280);
    return ((uint32_t)(y & 0xFFFF) << 16) | (uint32_t)(x & 0xFFFF);
}

static int gen_prim_defer(u_long *words, int ax, int ay, int force_tile) {
    int kind = force_tile ? 4 : (int)(rnd() % 10);
    int code, n = 0;
    uint32_t color = rnd() & 0xFFFFFF;

    if (rnd() % 3 == 0) {
        color = 0x808080;
    }
    if (kind < 3) { /* polygon */
        code = 0x20 | (rnd() & 0x1F);
        int tex = (code & 4) != 0, gour = (code & 0x10) != 0;
        int cnt = (code & 8) ? 4 : 3;
        uint32_t base = dxy(ax, ay);
        words[n++] = ((u_long)code << 24) | color;
        for (int v = 0; v < cnt; v++) {
            if (v && gour) {
                words[n++] = rnd() & 0xFFFFFF;
            }
            if (rnd() % 3) {
                int bx = (int)(base & 0xFFFF), by = (int)(base >> 16);
                words[n++] = (((uint32_t)((by + rr(-40, 60)) & 0xFFFF)) << 16) |
                             (uint32_t)((bx + rr(-40, 60)) & 0xFFFF);
            } else {
                words[n++] = dxy(ax, ay);
            }
            if (tex) {
                uint32_t hi = rnd() & 0xFFFF;
                if (v == 1) {
                    hi = (hi & 0xFE00) | (rnd() & 0x1FF); /* texture page */
                }
                words[n++] = (hi << 16) | (rnd() & 0xFFFF);
            }
        }
    } else if (kind == 3) { /* line */
        code = 0x40 | (rnd() & 0x1B);
        code &= ~0x0C;
        code |= 0x04;
        int gour = (code & 0x10) != 0;
        words[n++] = ((u_long)code << 24) | color;
        for (int v = 0; v < 2; v++) {
            if (v && gour) {
                words[n++] = rnd() & 0xFFFFFF;
            }
            words[n++] = dxy(ax, ay);
        }
        words[n++] = 0x55555555;
    } else { /* rectangle */
        code = 0x60 | (rnd() & 0x1F);
        if (force_tile) {
            code &= ~0x18; /* free size */
            if (rnd() % 4) {
                code &= ~0x06; /* opaque, untextured */
            }
        } else if (rnd() % 3 == 0) {
            code |= 0x04; /* more textured sprites */
        }
        int tex = (code & 4) != 0;
        words[n++] = ((u_long)code << 24) | color;
        words[n++] = force_tile ? (((uint32_t)(ay & 0xFFFF) << 16) |
                                   (uint32_t)(ax & 0xFFFF))
                                : dxy(ax, ay);
        if (tex) {
            words[n++] = rnd();
        }
        if ((code & 0x18) == 0) {
            int w, h;
            if (force_tile) {
                w = 320;
                h = 240;
            } else {
                w = rnd() % 4 ? rr(1, 140) : rr(0, 400);
                h = rnd() % 4 ? rr(1, 140) : rr(0, 300);
            }
            words[n++] = ((uint32_t)(h & 0xFFFF) << 16) | (uint32_t)(w & 0xFFFF);
        }
    }
    return n;
}

static int run_fuzz_defer(long iters, uint64_t seed) {
    static u16 img[1024 * 512], st_new[1024 * 512], st_ref[1024 * 512];
    Op o;
    u_long words[16];
    long bad = 0, observed = 0;
    int gutter_on = (int)(seed & 1);
    int buf = 0, cur_y0 = 0;
    int defer_on = 1;
    long frames = 0;

    g_rng = seed ? seed * 2654435761u + 88172645463325252ull
                 : 88172645463325252ull;
    g_new.rs_reset();
    g_ref.rs_reset();
    for (int i = 0; i < 1024 * 512; i++) {
        img[i] = (u16)rnd();
        if (rnd() % 5 == 0) {
            img[i] = 0;
        }
    }
    memset(&o, 0, sizeof(o));
    o.op = T_LOAD;
    o.a[2] = 1024;
    o.a[3] = 512;
    o.data = img;
    both(&o);
    if (gutter_on) {
        memset(&o, 0, sizeof(o));
        o.op = T_GUTTER;
        o.a[0] = 53;
        o.a[1] = 0;
        o.a[2] = 320;
        o.a[3] = 240;
        both(&o);
    }
    SoftwareRasterizer_SetDefer(1);

#define DOP(code_)                                                             \
    do {                                                                       \
        memset(&o, 0, sizeof(o));                                              \
        code_;                                                                 \
        both(&o);                                                              \
    } while (0)

    for (long it = 0; it < iters; it++) {
        int pick = rr(0, 99);
        int ay = cur_y0;

        if (pick < 5) {
            /* a new frame: flip to the other buffer, area, offset, clear */
            buf ^= (rnd() % 8 != 0);
            cur_y0 = buf ? 256 : 0;
            if (rnd() % 20 == 0) {
                cur_y0 = rr(0, 270);
            }
            ay = cur_y0;
            DOP(o.op = T_AREA0; o.a[0] = 0; o.a[1] = cur_y0);
            DOP(o.op = T_AREA1; o.a[0] = 319;
                o.a[1] = cur_y0 + (rnd() % 10 ? 239 : rr(0, 239)));
            DOP(o.op = T_OFFSET; o.a[0] = 0; o.a[1] = 0);
            if (rnd() % 6 == 0) {
                DOP(o.op = T_MASK; o.a[0] = rnd() % 3 == 0;
                    o.a[1] = rnd() % 6 == 0);
            }
            if (rnd() % 10 < 8) {
                int n = gen_prim_defer(words, 0, cur_y0, 1);
                DOP(o.op = T_PRIM; o.words = words; o.n = n);
            }
            if (gutter_on && rnd() % 2) {
                /* the flip: rows outside the shown buffer lose their gutters */
                int disp_y = buf ? 0 : 256;
                DOP(o.op = T_GUTTER_CLEAR; o.a[0] = 0; o.a[1] = disp_y);
                DOP(o.op = T_GUTTER_CLEAR; o.a[0] = disp_y + 240;
                    o.a[1] = 512);
            }
            frames++;
        } else if (pick < 9) {
            DOP(o.op = T_TEXPAGE; o.a[0] = (int32_t)(rnd() & 0x3FF));
        } else if (pick < 10) {
            DOP(o.op = T_TWIN; for (int i = 0; i < 4; i++) {
                o.a[i] = rnd() % 3 ? 0 : (int32_t)(rnd() & 31);
            });
        } else if (pick < 11) {
            DOP(o.op = T_MASK; o.a[0] = rnd() % 5 == 0; o.a[1] = rnd() % 5 == 0);
        } else if (pick < 12) {
            DOP(o.op = T_OFFSET; o.a[0] = rnd() % 2 ? 0 : rr(0, 0x7FF);
                o.a[1] = rnd() % 2 ? 0 : rr(0, 0x7FF));
        } else if (pick < 14) {
            /* a smaller drawing area inside or beside the buffer */
            int x0 = rr(0, 300), y0 = cur_y0 + rr(0, 200);
            DOP(o.op = T_AREA0; o.a[0] = x0; o.a[1] = y0);
            DOP(o.op = T_AREA1; o.a[0] = x0 + rr(0, 319 - x0);
                o.a[1] = y0 + rr(0, 239));
        } else if (pick < 17) {
            DOP(o.op = T_LOAD; o.a[0] = rr(-8, 1000); o.a[1] = rr(-8, 500);
                o.a[2] = rnd() % 4 ? rr(1, 64) : rr(1, 400);
                o.a[3] = rnd() % 4 ? rr(1, 64) : rr(1, 260); o.data = img);
        } else if (pick < 19) {
            int src_buf = rnd() % 2 ? 256 : 0;
            DOP(o.op = T_MOVE; o.a[0] = rnd() % 2 ? 0 : rr(0, 700);
                o.a[1] = rnd() % 2 ? src_buf : rr(0, 400);
                o.a[2] = rnd() % 2 ? 320 : rr(1, 400);
                o.a[3] = rnd() % 2 ? 240 : rr(1, 300);
                o.a[4] = rnd() % 2 ? 0 : rr(0, 700);
                o.a[5] = rnd() % 2 ? 256 - src_buf : rr(0, 400));
        } else if (pick < 21) {
            if (rnd() % 3 == 0) {
                DOP(o.op = T_MASK; o.a[0] = rnd() % 3 == 0; o.a[1] = 1);
            }
            int exact = rnd() % 2;
            DOP(o.op = T_CLEAR; o.a[0] = exact || rnd() % 2 ? 0 : rr(-10, 800);
                o.a[1] = exact ? cur_y0 : rnd() % 2 ? (rnd() % 2 ? 0 : 256) : rr(-10, 450);
                o.a[2] = exact || rnd() % 2 ? 320 : rr(0, 400);
                o.a[3] = exact || rnd() % 2 ? 240 : rr(0, 300);
                o.a[4] = rnd() & 255; o.a[5] = rnd() & 255; o.a[6] = rnd() & 255);
        } else if (pick < 23) {
            PS1_RECT rect;
            rect.x = (short)(rnd() % 2 ? 0 : rr(0, 900));
            rect.y = (short)(rnd() % 2 ? (rnd() % 2 ? 0 : 256) : rr(0, 450));
            rect.w = (short)(rnd() % 2 ? 320 : rr(1, 120));
            rect.h = (short)(rnd() % 2 ? 240 : rr(1, 60));
            if ((long)rect.w * rect.h <= 1024 * 512) {
                g_new.be->store_image(&rect, (u_long *)st_new);
                g_ref.be->store_image(&rect, (u_long *)st_ref);
                if (memcmp(st_new, st_ref, (size_t)rect.w * rect.h * 2)) {
                    if (bad++ < 5) {
                        fprintf(stderr, "fuzz-defer: StoreImage differs at it %ld\n", it);
                    }
                }
            }
        } else if (pick < 24 && gutter_on) {
            DOP(o.op = T_GUTTER_CLEAR; o.a[0] = rr(0, 511); o.a[1] = rr(0, 512));
        } else if (pick < 25 && rnd() % 10 == 0) {
            defer_on = !defer_on;
            SoftwareRasterizer_SetDefer(defer_on);
        } else if (pick < 33) {
            /* a present: draw what a rectangle shows, compare it */
            int r[4];
            int m = rnd() % 10;
            long d = 0;
            int gut = rnd() % 4 != 0;
            if (m < 5) {
                r[0] = 0; r[1] = rnd() % 2 ? 0 : 256; r[2] = 320; r[3] = 240;
            } else if (m < 8) {
                r[0] = rr(0, 900); r[1] = rr(0, 450); r[2] = rr(1, 200); r[3] = rr(1, 100);
            } else {
                r[0] = rr(0, 1000); r[1] = rr(0, 500); r[2] = rr(1, 30); r[3] = rr(1, 30);
            }
            SoftwareRasterizer_SyncRect(r[0], r[1], r[2], r[3], gut);
            if (gut) {
                d = diff_rect(r[0], r[1], r[2], r[3]);
            } else {
                /* gutter rows were not asked for: only VRAM is comparable */
                const u16 *a = g_new.vram_raw(), *b = g_ref.vram_raw();
                for (int yy = r[1]; yy < r[1] + r[3] && yy < 512; yy++)
                    for (int xx = r[0]; xx < r[0] + r[2] && xx < 1024; xx++)
                        d += a[yy * 1024 + xx] != b[yy * 1024 + xx];
            }
            observed++;
            if (d) {
                if (bad < 5) {
                    fprintf(stderr, "fuzz-defer: it %ld present %d,%d %dx%d gutter %d: %ld px differ\n",
                            it, r[0], r[1], r[2], r[3], gut, d);
                }
                bad++;
                SoftwareRasterizer_SyncAll();
                memcpy(img, g_ref.vram_raw(), sizeof(img));
                DOP(o.op = T_LOAD; o.a[2] = 1024; o.a[3] = 512; o.data = img);
            }
        } else {
            int n = gen_prim_defer(words, 0, ay, 0);
            DOP(o.op = T_PRIM; o.words = words; o.n = n);
        }
    }
    SoftwareRasterizer_SyncAll();
    {
        long d = diff_px();
        if (d) {
            fprintf(stderr, "fuzz-defer: final state differs: %ld px\n", d);
            bad++;
        }
    }
    {
        unsigned long q, dr, run, sl, ol;
        SoftwareRasterizer_GetDeferStats(&q, &dr, &run, &sl, &ol, 0);
        printf("fuzz-defer: %ld iterations, seed %llu, gutters %d: %ld differences "
               "(%ld frames, %ld presents; %lu prims queued, %lu dropped, %lu drawn)\n",
               iters, (unsigned long long)seed, gutter_on, bad, frames, observed,
               q, dr, run);
    }
    return bad ? 1 : 0;
}
#endif

int main(int argc, char **argv) {
    while (argc >= 3 && !strcmp(argv[1], "--defer")) {
        g_defer = atoi(argv[2]);
        argv[2] = argv[0];
        argc -= 2;
        argv += 2;
    }
    if (argc >= 3 && !strcmp(argv[1], "--fuzz-defer")) {
#ifdef WITH_REF
        return run_fuzz_defer(atol(argv[2]), argc > 3 ? strtoull(argv[3], NULL, 0) : 1);
#else
        fprintf(stderr, "--fuzz-defer needs a reference build (WITH_REF)\n");
        return 2;
#endif
    }
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
