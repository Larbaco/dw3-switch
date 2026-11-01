/* P6.11 phase 0: OLED measurement aid for internal resolution scaling.
 * See nx_res_probe.h for the environment and docs/P6.11_INTERNAL_RES.md for
 * the rows to run. Off unless DW3_NX_RES_PROBE is set.
 *
 * The `dw3-res-probe: v=1 ...` line, one per 60 game frames. Every `_us` field
 * is a per-frame average in microseconds over the frames it applies to, and a
 * `_max_us` field is the worst single frame of the window:
 *
 *   mode        handheld | docked        out  the surface WxH
 *   res         the battle mesh scale (1, 2 or 3)
 *   src         where the GPU figures come from: query | finish | none
 *   speed       the turbo factor (1 = normal)
 *   skip        1 when frames turbo does not show skip the mesh flush
 *   scene       battle (the mesh drew in most frames) | field (it did not)
 *   game_fps    game frames per second over the window (60.0 = full speed)
 *   frames      game frames in the window; shown the ones that were presented
 *   mesh_n      frames with battle-mesh draws; mesh_tris the triangles drawn
 *   mesh_cpu_us CPU time to submit the mesh draws (glDraw*, no upload)
 *   mesh_up_us  CPU time to upload the VRAM sample texture before the draws
 *   mesh_gpu_us GPU time of the mesh draws of a frame (all its flushes,
 *               the gutter pass included); mesh_gpu_n frames it was read for
 *   pres_cpu_us CPU time of the present of a shown frame (pack, upload, draws)
 *   pres_gpu_us GPU time of the present (upload, mesh blit, UI draw)
 *   raster_us   CPU time of the software rasterizer work done at a present
 *               (the `sync_us` of `pace dbg:`): the field's CPU raster cost
 *   swap_us     time in eglSwapBuffers (the wait for the display)
 *   skip_n / skip_us  frames turbo did not show, and what their skip path cost
 *   busy_pct    100 * (dt - swap) / dt, as in `pace dbg:`
 *
 * The first line after a scale change or a dock change is partial; read the
 * second. */
#include "nx_res_probe.h"
#include "nx.h"
#include "nx_mesh.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <EGL/egl.h>
#include <GLES3/gl3.h>

/* GL_EXT_disjoint_timer_query, declared here so the build does not depend on
 * which gl2ext.h the toolchain ships. */
#define PROBE_GL_TIME_ELAPSED_EXT 0x88BF
#define PROBE_GL_QUERY_RESULT_EXT 0x8866
#define PROBE_GL_QUERY_RESULT_AVAILABLE_EXT 0x8867
#define PROBE_GL_GPU_DISJOINT_EXT 0x8FBB

typedef void (*ProbeGenQueries)(GLsizei, GLuint *);
typedef void (*ProbeDeleteQueries)(GLsizei, const GLuint *);
typedef void (*ProbeBeginQuery)(GLenum, GLuint);
typedef void (*ProbeEndQuery)(GLenum);
typedef void (*ProbeGetQuery64)(GLuint, GLenum, uint64_t *);
typedef void (*ProbeGetQuery32)(GLuint, GLenum, GLuint *);

enum {
    KIND_MESH = 0,
    KIND_PRESENT = 1,
    RING = 128,   /* queries in flight */
    FRAMES = 64,  /* per-frame mesh accumulators, indexed by frame number */
    WINDOW_MAX = 256
};

static int s_on = -1;
static int s_src; /* 0 none, 1 query, 2 finish */

static ProbeGenQueries p_gen;
static ProbeDeleteQueries p_del;
static ProbeBeginQuery p_begin;
static ProbeEndQuery p_end;
static ProbeGetQuery64 p_get64;
static ProbeGetQuery32 p_get32;

typedef struct Slot {
    GLuint id;
    uint32_t frame;
    int kind;
} Slot;
static Slot s_ring[RING];
static unsigned s_head;
static unsigned s_tail;
static int s_open_kind = -1; /* the query between begin and end, if any */
static int s_open_slot = -1;

typedef struct Frame {
    uint64_t gpu_ns;
    uint32_t outstanding; /* queries not read back yet */
    uint32_t flushes;
    int ended;
} Frame;
static Frame s_fr[FRAMES];
static uint32_t s_frame; /* the game frame being built */

/* Window accumulators. Reset by emit. */
typedef struct Stat {
    uint64_t sum;
    uint64_t max;
    uint32_t n;
} Stat;
static void stat_add(Stat *s, uint64_t v) {
    s->sum += v;
    if (v > s->max) {
        s->max = v;
    }
    s->n++;
}
static uint64_t stat_avg(const Stat *s) { return s->n ? s->sum / s->n : 0; }

static struct {
    Stat mesh_gpu;   /* per frame, ns -> us at stat_add */
    Stat mesh_cpu;   /* per frame */
    Stat mesh_up;    /* per frame */
    Stat pres_gpu;   /* per shown frame */
    Stat pres_cpu;   /* per shown frame */
    Stat raster;     /* per shown frame */
    Stat swap;       /* per shown frame */
    Stat skip;       /* per skipped frame */
    uint64_t tris;
    uint32_t q_drop;
    uint32_t disjoint;
} s_w;
/* Figures of the frame being built (flushes may be several). */
static uint64_t s_cur_cpu_us;
static uint64_t s_cur_up_us;
static uint32_t s_cur_flushes;
static uint64_t s_cur_tris;
static uint64_t s_t0; /* CPU start of the open section */
static uint64_t s_t0_present;

uint64_t dw3_nx_res_probe_clock_us(void) {
    return armTicksToNs(armGetSystemTick()) / 1000ULL;
}
#define now_us() dw3_nx_res_probe_clock_us()

static void plog(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
static void plog(const char *fmt, ...) {
    char buf[1024];
    va_list ap;

    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    psyz_log(1, "nx_res_probe.c", __LINE__, __func__, "%s", buf);
}

int dw3_nx_res_probe_on(void) {
    if (s_on < 0) {
        const char *v = getenv("DW3_NX_RES_PROBE");

        s_on = v && v[0] ? (v[0] == '2' ? 2 : (v[0] == '0' ? 0 : 1)) : 0;
        if (s_on) {
            plog("dw3-res-probe: on, mode=%d (1 = timer query, 2 = glFinish)",
                 s_on);
        }
    }
    return s_on;
}

static int s_skip = -1;

int dw3_nx_res_probe_skip_unshown(void) {
    if (s_skip < 0) {
        const char *v = getenv("DW3_NX_MESH_SKIP");

        s_skip = v && v[0] && v[0] != '0';
        if (s_skip) {
            plog("dw3-res-probe: DW3_NX_MESH_SKIP=1, unshown turbo frames do "
                 "not flush the mesh (measurement only)");
        }
    }
    return s_skip;
}

void dw3_nx_res_probe_gl_init(void) {
    const char *ext;
    int want = dw3_nx_res_probe_on();

    memset(s_ring, 0, sizeof(s_ring));
    memset(s_fr, 0, sizeof(s_fr));
    s_head = s_tail = 0;
    s_open_kind = -1;
    s_open_slot = -1;
    p_gen = NULL;
    s_src = 0;
    if (!want) {
        return;
    }
    s_src = 2;
    ext = (const char *)glGetString(GL_EXTENSIONS);
    if (want == 1 && ext && strstr(ext, "GL_EXT_disjoint_timer_query")) {
        p_gen = (ProbeGenQueries)eglGetProcAddress("glGenQueriesEXT");
        p_del = (ProbeDeleteQueries)eglGetProcAddress("glDeleteQueriesEXT");
        p_begin = (ProbeBeginQuery)eglGetProcAddress("glBeginQueryEXT");
        p_end = (ProbeEndQuery)eglGetProcAddress("glEndQueryEXT");
        p_get64 = (ProbeGetQuery64)eglGetProcAddress("glGetQueryObjectui64vEXT");
        p_get32 = (ProbeGetQuery32)eglGetProcAddress("glGetQueryObjectuivEXT");
        if (p_gen && p_del && p_begin && p_end && p_get64 && p_get32) {
            unsigned i;

            for (i = 0; i < RING; i++) {
                p_gen(1, &s_ring[i].id);
            }
            s_src = 1;
            (void)glGetError();
        } else {
            p_gen = NULL;
        }
    }
    plog("dw3-res-probe: gpu timing source=%s (extension %s) renderer=\"%s\"",
         s_src == 1 ? "query" : "finish",
         ext && strstr(ext, "GL_EXT_disjoint_timer_query") ? "present"
                                                           : "absent",
         (const char *)glGetString(GL_RENDERER));
}

void dw3_nx_res_probe_gl_shutdown(void) {
    /* The context is going away with every query in it. */
    p_gen = NULL;
    s_src = 0;
    s_head = s_tail = 0;
    s_open_kind = -1;
    s_open_slot = -1;
    memset(s_ring, 0, sizeof(s_ring));
}

static void frame_close(Frame *f) {
    if (f->flushes) {
        stat_add(&s_w.mesh_gpu, f->gpu_ns / 1000ULL);
    }
}

/* Reads back every finished query, oldest first. Results come in order, so
 * the first one still pending ends the pass. */
static void drain(void) {
    GLint disjoint = 0;
    int any = 0;

    if (s_src != 1) {
        return;
    }
    while (s_tail != s_head) {
        Slot *s = &s_ring[s_tail % RING];
        GLuint avail = 0;
        uint64_t ns = 0;

        p_get32(s->id, PROBE_GL_QUERY_RESULT_AVAILABLE_EXT, &avail);
        if (!avail) {
            break;
        }
        p_get64(s->id, PROBE_GL_QUERY_RESULT_EXT, &ns);
        any = 1;
        if (s->kind == KIND_PRESENT) {
            stat_add(&s_w.pres_gpu, ns / 1000ULL);
        } else {
            Frame *f = &s_fr[s->frame % FRAMES];

            f->gpu_ns += ns;
            if (f->outstanding) {
                f->outstanding--;
            }
            if (f->ended && f->outstanding == 0) {
                frame_close(f);
            }
        }
        s_tail++;
    }
    if (any) {
        glGetIntegerv(PROBE_GL_GPU_DISJOINT_EXT, &disjoint);
        if (disjoint) {
            s_w.disjoint++;
        }
    }
}

static void section_begin(int kind) {
    if (s_src == 2) {
        glFinish(); /* what came before is not this section's */
    } else if (s_src == 1) {
        Slot *s;

        if (s_open_kind >= 0) {
            return; /* never nested: a flush inside a present is not timed twice */
        }
        drain();
        if (s_head - s_tail >= RING) {
            s_w.q_drop++;
            return;
        }
        s = &s_ring[s_head % RING];
        s->frame = s_frame;
        s->kind = kind;
        p_begin(PROBE_GL_TIME_ELAPSED_EXT, s->id);
        s_open_kind = kind;
        s_open_slot = (int)(s_head % RING);
        s_head++;
        if (kind == KIND_MESH) {
            s_fr[s_frame % FRAMES].outstanding++;
        }
    }
}

/* Returns the finish-mode GPU time in ns (0 for the query source). */
static uint64_t section_end(int kind, uint64_t t_begin_us) {
    if (s_src == 2) {
        glFinish();
        return (now_us() - t_begin_us) * 1000ULL;
    }
    if (s_src == 1 && s_open_kind == kind) {
        p_end(PROBE_GL_TIME_ELAPSED_EXT);
        s_open_kind = -1;
        s_open_slot = -1;
    }
    return 0;
}

void dw3_nx_res_probe_mesh_begin(void) {
    if (!dw3_nx_res_probe_on()) {
        return;
    }
    section_begin(KIND_MESH);
    s_t0 = now_us();
}

void dw3_nx_res_probe_mesh_end(int tris) {
    uint64_t t_sub;
    uint64_t ns;

    if (!dw3_nx_res_probe_on()) {
        return;
    }
    t_sub = now_us();
    s_cur_cpu_us += t_sub - s_t0;
    s_cur_flushes++;
    s_cur_tris += (uint64_t)(tris > 0 ? tris : 0);
    s_fr[s_frame % FRAMES].flushes++;
    ns = section_end(KIND_MESH, s_t0);
    if (s_src == 2) {
        s_fr[s_frame % FRAMES].gpu_ns += ns;
    }
}

void dw3_nx_res_probe_note_upload(uint32_t us) {
    if (dw3_nx_res_probe_on()) {
        s_cur_up_us += us;
    }
}

void dw3_nx_res_probe_frame_end(void) {
    Frame *f;

    if (!dw3_nx_res_probe_on()) {
        return;
    }
    if (s_cur_flushes) {
        stat_add(&s_w.mesh_cpu, s_cur_cpu_us);
        stat_add(&s_w.mesh_up, s_cur_up_us);
        s_w.tris += s_cur_tris;
    }
    s_cur_cpu_us = s_cur_up_us = s_cur_tris = 0;
    s_cur_flushes = 0;
    f = &s_fr[s_frame % FRAMES];
    f->ended = 1;
    if (f->outstanding == 0) {
        frame_close(f);
    }
    s_frame++;
    memset(&s_fr[s_frame % FRAMES], 0, sizeof(Frame));
}

void dw3_nx_res_probe_present_begin(void) {
    if (!dw3_nx_res_probe_on()) {
        return;
    }
    section_begin(KIND_PRESENT);
    s_t0_present = now_us();
}

void dw3_nx_res_probe_present_end(void) {
    uint64_t ns;

    if (!dw3_nx_res_probe_on()) {
        return;
    }
    ns = section_end(KIND_PRESENT, s_t0_present);
    if (s_src == 2) {
        stat_add(&s_w.pres_gpu, ns / 1000ULL);
    }
}

void dw3_nx_res_probe_note_shown(uint64_t sync_us, uint64_t present_us,
                                 uint64_t swap_us) {
    if (!dw3_nx_res_probe_on()) {
        return;
    }
    stat_add(&s_w.raster, sync_us);
    stat_add(&s_w.pres_cpu, present_us);
    stat_add(&s_w.swap, swap_us);
}

void dw3_nx_res_probe_note_skipped(uint64_t us) {
    if (dw3_nx_res_probe_on()) {
        stat_add(&s_w.skip, us);
    }
}

void dw3_nx_res_probe_note_targets(int scale, int w, int h, int gutter_target,
                                   uint64_t bytes, uint32_t alloc_us,
                                   int ok) {
    if (!dw3_nx_res_probe_on()) {
        return;
    }
    /* proposed_bytes: the display-sized wide pair of the doc's design
     * ((320 + 2 * 53) x 240 per scale step, RGBA8, mesh + gutter target),
     * for comparison with what this VRAM-sized probe layout really holds. */
    plog("dw3-res-probe: targets res=%d size=%dx%d gutter_target=%d bytes=%llu "
         "(%.1f MiB) proposed_bytes=%llu alloc_ms=%u.%03u ok=%d",
         scale, w, h, gutter_target, (unsigned long long)bytes,
         (double)bytes / (1024.0 * 1024.0),
         (unsigned long long)(426ull * (unsigned)scale * 240ull *
                              (unsigned)scale * 4ull * 2ull),
         alloc_us / 1000u, alloc_us % 1000u, ok);
}

void dw3_nx_res_probe_emit(unsigned long long dt_us, int speed, bool docked,
                           unsigned out_w, unsigned out_h, int res) {
    uint32_t frames_win;
    uint32_t shown;
    const char *scene;
    unsigned long long fps10;

    if (!dw3_nx_res_probe_on()) {
        return;
    }
    drain();
    shown = s_w.raster.n;
    frames_win = shown + s_w.skip.n;
    if (frames_win == 0 || dt_us == 0) {
        memset(&s_w, 0, sizeof(s_w));
        return;
    }
    scene = s_w.mesh_cpu.n * 2u >= frames_win ? "battle" : "field";
    fps10 = frames_win * 10000000ULL / dt_us;
    plog("dw3-res-probe: v=1 mode=%s out=%ux%u res=%d src=%s speed=%dx skip=%d "
         "scene=%s game_fps=%llu.%llu frames=%u shown=%u "
         "mesh_n=%u mesh_tris=%llu mesh_cpu_us=%llu mesh_cpu_max_us=%llu "
         "mesh_up_us=%llu mesh_gpu_us=%llu mesh_gpu_max_us=%llu "
         "mesh_gpu_n=%u pres_cpu_us=%llu pres_cpu_max_us=%llu "
         "pres_gpu_us=%llu pres_gpu_max_us=%llu raster_us=%llu "
         "raster_max_us=%llu swap_us=%llu skip_n=%u skip_us=%llu "
         "busy_pct=%llu q_drop=%u disjoint=%u",
         docked ? "docked" : "handheld", out_w, out_h, res,
         s_src == 1 ? "query" : (s_src == 2 ? "finish" : "none"), speed,
         dw3_nx_res_probe_skip_unshown(), scene,
         fps10 / 10, fps10 % 10, frames_win, shown, s_w.mesh_cpu.n,
         (unsigned long long)s_w.tris,
         (unsigned long long)stat_avg(&s_w.mesh_cpu),
         (unsigned long long)s_w.mesh_cpu.max,
         (unsigned long long)stat_avg(&s_w.mesh_up),
         (unsigned long long)stat_avg(&s_w.mesh_gpu),
         (unsigned long long)s_w.mesh_gpu.max, s_w.mesh_gpu.n,
         (unsigned long long)stat_avg(&s_w.pres_cpu),
         (unsigned long long)s_w.pres_cpu.max,
         (unsigned long long)stat_avg(&s_w.pres_gpu),
         (unsigned long long)s_w.pres_gpu.max,
         (unsigned long long)stat_avg(&s_w.raster),
         (unsigned long long)s_w.raster.max,
         (unsigned long long)stat_avg(&s_w.swap), s_w.skip.n,
         (unsigned long long)stat_avg(&s_w.skip),
         100ULL * (dt_us > s_w.swap.sum ? dt_us - s_w.swap.sum : 0) / dt_us,
         s_w.q_drop, s_w.disjoint);
    memset(&s_w, 0, sizeof(s_w));
}

void dw3_nx_res_probe_poll(uint64_t held) {
    enum {
        CHORD_SCALE = HidNpadButton_Minus | HidNpadButton_ZL | HidNpadButton_R,
        CHORD_SKIP = HidNpadButton_Minus | HidNpadButton_ZL | HidNpadButton_L
    };
    static int was_scale;
    static int was_skip;
    int down;

    if (!dw3_nx_res_probe_on()) {
        return;
    }
    down = (held & CHORD_SCALE) == CHORD_SCALE;
    if (down && !was_scale) {
        dw3_nx_mesh_cycle_scale();
    }
    was_scale = down;
    down = (held & CHORD_SKIP) == CHORD_SKIP;
    if (down && !was_skip) {
        s_skip = !dw3_nx_res_probe_skip_unshown();
        plog("dw3-res-probe: skip unshown mesh frames in turbo -> %d", s_skip);
    }
    was_skip = down;
}
