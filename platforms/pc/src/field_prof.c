/* P6.22 field CPU profiler for the PC host (CMake -DDW3_PC_FIELD_PROF=ON).
 *
 * The Switch host draws everything outside battle with PSY-Z's CPU rasterizer
 * (sdl3_software.c through platforms/switch/psyz_backend/switch_gpu.c); the PC
 * host draws with SDL_GPU. To measure the Switch's raster cost on the PC, this
 * file wraps the Draw_* contract at link time (-Wl,--wrap) and feeds every
 * call to the PC renderer AND to a shadow copy of the CPU rasterizer, with the
 * same gutter bookkeeping as switch_gpu.c. Nothing here changes what the PC
 * shows or what the game does.
 *
 * Environment (all optional):
 *   DW3_PROF_WINDOW=N     print a `dw3-prof:` line every N game frames (60)
 *   DW3_SOFT_OFF=1        do not run the shadow rasterizer
 *   DW3_PROF_SAMPLE=path  sample the game thread's PC (thread CPU clock,
 *                         every 250 us) with a short frame-pointer stack and
 *                         write them, plus /proc/self/maps, at exit
 *   DW3_SOFT_HASH=path    one line per game frame: hash of the shadow VRAM
 *                         and gutter planes (pixel-identity checks)
 *   DW3_SOFT_TRACE=path   binary trace of every Draw_* call the shadow sees,
 *                         for tools/soft_raster_bench.c
 *   DW3_SOFT_DEFER=1      run the shadow the way the Switch host runs its
 *                         rasterizer at turbo (P6.22 pass 2): frames turbo
 *                         does not present are queued, not drawn, and the
 *                         present syncs the display rectangle
 *   DW3_SOFT_PRESENT_HASH=path
 *                         one line per presented frame: hash of the display
 *                         rectangle and its gutter rows as the Switch present
 *                         would read them. Equal files with and without
 *                         DW3_SOFT_DEFER=1 mean the deferral changed no
 *                         presented pixel
 *
 * The `dw3-prof:` line also reports the SPU mixer (Psyz_SpuPullSamples): on
 * the PC it runs on SDL's audio thread, on the Switch on nx_audio's thread,
 * so it is not part of the game thread's time; mix_us is its CPU per 1 s of
 * audio (44100 frames).
 *
 * Off by default; release and replay builds do not compile this file. */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <psyz.h>
#include <psyz/video.h>

#include "rasterizer.h"
#include "port_turbo.h"

#include <pthread.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/syscall.h>
#include <time.h>
#include <ucontext.h>
#include <unistd.h>

/* ---- clocks -------------------------------------------------------------- */

static uint64_t now_ns(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ull + (uint64_t)ts.tv_nsec;
}

static uint64_t thread_cpu_ns(void) {
    struct timespec ts;
    clock_gettime(CLOCK_THREAD_CPUTIME_ID, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ull + (uint64_t)ts.tv_nsec;
}

/* ---- state --------------------------------------------------------------- */

enum { PH_GAME, PH_SOFT, PH_PCGPU, PH_VSYNC, PH_COUNT };
static volatile int s_phase = PH_GAME;

static int s_init;
static int s_soft_on = 1;
static unsigned s_window = 60;
static unsigned s_frame;
static FILE *s_hash_fp;
static FILE *s_trace_fp;
static FILE *s_present_fp;
static int s_defer_env;

/* per-window accumulators */
enum {
    K_TILE,      /* untextured rect (0x60..0x7f, !TEXTURED) */
    K_SPRT,      /* textured rect */
    K_POLY_F,    /* flat untextured tri/quad */
    K_POLY_G,    /* gouraud untextured */
    K_POLY_FT,   /* flat textured */
    K_POLY_GT,   /* gouraud textured */
    K_LINE,
    K_OTHER,     /* clear/load/move/store */
    K_COUNT
};
static const char *k_name[K_COUNT] = {"tile", "sprt", "pf", "pg", "pft",
                                      "pgt", "line", "img"};
static uint64_t w_cnt[K_COUNT];
static uint64_t w_px[K_COUNT];  /* bbox pixels (rects: w*h; polys: bbox) */
static uint64_t w_ns[K_COUNT];  /* shadow raster time */
static uint64_t w_semi_px;
static uint64_t w_soft_ns, w_pcgpu_ns, w_vsync_ns;
static uint64_t w_cpu0, w_wall0;
static uint64_t w_load_bytes;
/* SPU mixer, written by the audio thread */
static volatile uint64_t s_mix_ns, s_mix_frames;

/* gutter bookkeeping, as switch_gpu.c */
static int s_gutter;
static int s_disp_x, s_disp_y;
static int s_mode_w, s_mode_h;
static int s_rgb24;

/* ---- trace --------------------------------------------------------------- */

enum {
    T_RESET = 1, T_TEXPAGE, T_TWIN, T_AREA0, T_AREA1, T_OFFSET, T_MASK,
    T_CLEAR, T_LOAD, T_STORE, T_MOVE, T_PRIM, T_GUTTER, T_GUTTER_CLEAR,
    T_FRAME, T_GRID, T_DISPLAY
};

static void tr_u32(uint32_t v) { fwrite(&v, 4, 1, s_trace_fp); }
static void tr_op(int op, int n, const int32_t *a) {
    if (!s_trace_fp) {
        return;
    }
    tr_u32((uint32_t)op);
    for (int i = 0; i < n; i++) {
        tr_u32((uint32_t)a[i]);
    }
}

/* ---- sampler ------------------------------------------------------------- */

#define SAMPLE_DEPTH 8
typedef struct {
    uint32_t frame;
    uint32_t phase;
    uint64_t pc[SAMPLE_DEPTH];
} Sample;

static Sample *s_samples;
static size_t s_nsamples, s_cap_samples;
static uintptr_t s_stack_lo, s_stack_hi;
static char s_sample_path[512];

static void on_sigprof(int sig, siginfo_t *si, void *ctx) {
    (void)sig;
    (void)si;
    if (!s_samples || s_nsamples >= s_cap_samples) {
        return;
    }
    ucontext_t *uc = (ucontext_t *)ctx;
    Sample *s = &s_samples[s_nsamples++];
    memset(s->pc, 0, sizeof(s->pc));
    s->frame = s_frame;
    s->phase = (uint32_t)s_phase;
#if defined(__x86_64__)
    s->pc[0] = (uint64_t)uc->uc_mcontext.gregs[REG_RIP];
    uintptr_t fp = (uintptr_t)uc->uc_mcontext.gregs[REG_RBP];
#elif defined(__aarch64__)
    s->pc[0] = (uint64_t)uc->uc_mcontext.pc;
    uintptr_t fp = (uintptr_t)uc->uc_mcontext.regs[29];
#else
    uintptr_t fp = 0;
#endif
    for (int i = 1; i < SAMPLE_DEPTH; i++) {
        if (fp < s_stack_lo || fp + 16 > s_stack_hi || (fp & 7)) {
            break;
        }
        uintptr_t next = ((uintptr_t *)fp)[0];
        s->pc[i] = ((uint64_t *)fp)[1];
        if (next <= fp) {
            break;
        }
        fp = next;
    }
}

static void sampler_start(const char *path) {
    pthread_attr_t attr;
    void *addr;
    size_t size;
    struct sigaction sa;
    struct sigevent sev;
    timer_t timer;
    struct itimerspec its;

    snprintf(s_sample_path, sizeof(s_sample_path), "%s", path);
    s_cap_samples = 4u << 20;
    s_samples = calloc(s_cap_samples, sizeof(Sample));
    if (!s_samples) {
        return;
    }
    if (pthread_getattr_np(pthread_self(), &attr) == 0) {
        pthread_attr_getstack(&attr, &addr, &size);
        s_stack_lo = (uintptr_t)addr;
        s_stack_hi = (uintptr_t)addr + size;
        pthread_attr_destroy(&attr);
    }
    memset(&sa, 0, sizeof(sa));
    sa.sa_sigaction = on_sigprof;
    sa.sa_flags = SA_SIGINFO | SA_RESTART;
    sigaction(SIGPROF, &sa, NULL);
    memset(&sev, 0, sizeof(sev));
    sev.sigev_notify = SIGEV_THREAD_ID;
    sev.sigev_signo = SIGPROF;
    sev._sigev_un._tid = (pid_t)syscall(SYS_gettid);
    if (timer_create(CLOCK_THREAD_CPUTIME_ID, &sev, &timer) != 0) {
        perror("dw3-prof: timer_create");
        return;
    }
    memset(&its, 0, sizeof(its));
    its.it_interval.tv_nsec = 250000;
    its.it_value.tv_nsec = 250000;
    timer_settime(timer, 0, &its, NULL);
    fprintf(stderr, "dw3-prof: sampling the game thread every 250 us -> %s\n",
            path);
}

static void sampler_dump(void) {
    FILE *fp;
    FILE *maps;
    char line[1024];

    if (!s_samples || !s_sample_path[0]) {
        return;
    }
    fp = fopen(s_sample_path, "w");
    if (!fp) {
        return;
    }
    maps = fopen("/proc/self/maps", "r");
    while (maps && fgets(line, sizeof(line), maps)) {
        fprintf(fp, "map %s", line);
    }
    if (maps) {
        fclose(maps);
    }
    for (size_t i = 0; i < s_nsamples; i++) {
        const Sample *s = &s_samples[i];
        fprintf(fp, "s %u %u", s->frame, s->phase);
        for (int d = 0; d < SAMPLE_DEPTH && s->pc[d]; d++) {
            fprintf(fp, " %llx", (unsigned long long)s->pc[d]);
        }
        fputc('\n', fp);
    }
    fclose(fp);
    fprintf(stderr, "dw3-prof: %zu samples written\n", s_nsamples);
}

/* ---- init / window ------------------------------------------------------- */

static void prof_exit(void) {
    sampler_dump();
    if (s_hash_fp) {
        fclose(s_hash_fp);
    }
    if (s_present_fp) {
        fclose(s_present_fp);
    }
    if (s_trace_fp) {
        fclose(s_trace_fp);
    }
}

static void prof_init(void) {
    const char *v;

    if (s_init) {
        return;
    }
    s_init = 1;
    if ((v = getenv("DW3_PROF_WINDOW")) && atoi(v) > 0) {
        s_window = (unsigned)atoi(v);
    }
    if ((v = getenv("DW3_SOFT_OFF")) && v[0] == '1') {
        s_soft_on = 0;
    }
    if ((v = getenv("DW3_SOFT_HASH")) && v[0]) {
        s_hash_fp = fopen(v, "w");
    }
    if ((v = getenv("DW3_SOFT_DEFER")) && v[0] == '1') {
        s_defer_env = 1;
    }
    if ((v = getenv("DW3_SOFT_PRESENT_HASH")) && v[0]) {
        s_present_fp = fopen(v, "w");
    }
    if ((v = getenv("DW3_SOFT_TRACE")) && v[0]) {
        s_trace_fp = fopen(v, "wb");
        if (s_trace_fp) {
            static char buf[1 << 20];
            setvbuf(s_trace_fp, buf, _IOFBF, sizeof(buf));
            fwrite("DW3T", 4, 1, s_trace_fp);
        }
    }
    if ((v = getenv("DW3_PROF_SAMPLE")) && v[0]) {
        sampler_start(v);
    }
    atexit(prof_exit);
    w_cpu0 = thread_cpu_ns();
    w_wall0 = now_ns();
    fprintf(stderr, "dw3-prof: field profiler on (shadow CPU rasterizer %s, "
                    "window %u frames)\n",
            s_soft_on ? "on" : "off", s_window);
}

static uint64_t hash_vram(void) {
    const uint64_t *p = (const uint64_t *)SoftwareRasterizer_GetVram();
    uint64_t h = 1469598103934665603ull;
    for (size_t i = 0; i < 1024u * 512u / 4u; i++) {
        h = (h ^ p[i]) * 1099511628211ull;
    }
    for (int side = 0; side < 2; side++) {
        const uint64_t *g =
            (const uint64_t *)SoftwareRasterizer_GetGutterPlane(side);
        for (size_t i = 0; i < 512u * PSYZ_GUTTER_MAX / 4u; i++) {
            h = (h ^ g[i]) * 1099511628211ull;
        }
    }
    return h;
}

static void window_end(void) {
    uint64_t cpu = thread_cpu_ns() - w_cpu0;
    uint64_t wall = now_ns() - w_wall0;
    uint64_t prims = 0;
    char buf[1024];
    int at = 0;

    for (int k = 0; k < K_COUNT; k++) {
        prims += w_cnt[k];
        at += snprintf(buf + at, sizeof(buf) - (size_t)at,
                       " %s=%llu/%llukpx/%lluus", k_name[k],
                       (unsigned long long)(w_cnt[k] / s_window),
                       (unsigned long long)(w_px[k] / s_window / 1000),
                       (unsigned long long)(w_ns[k] / s_window / 1000));
    }
    /* per frame averages; game = thread CPU minus soft, PC GPU and vsync */
    fprintf(stderr,
            "dw3-prof: frame=%u per-frame wall_us=%llu cpu_us=%llu "
            "soft_us=%llu pcgpu_us=%llu vsync_cpu_us=%llu game_us=%llu "
            "prims=%llu semi_kpx=%llu load_kb=%llu mix_us_per_s=%llu |%s\n",
            s_frame, (unsigned long long)(wall / s_window / 1000),
            (unsigned long long)(cpu / s_window / 1000),
            (unsigned long long)(w_soft_ns / s_window / 1000),
            (unsigned long long)(w_pcgpu_ns / s_window / 1000),
            (unsigned long long)(w_vsync_ns / s_window / 1000),
            (unsigned long long)((cpu > w_soft_ns + w_pcgpu_ns + w_vsync_ns
                                      ? cpu - w_soft_ns - w_pcgpu_ns -
                                            w_vsync_ns
                                      : 0) /
                                 s_window / 1000),
            (unsigned long long)(prims / s_window),
            (unsigned long long)(w_semi_px / s_window / 1000),
            (unsigned long long)(w_load_bytes / s_window / 1024),
            (unsigned long long)(s_mix_frames
                                     ? s_mix_ns * 44100ull / s_mix_frames / 1000
                                     : 0),
            buf);
    memset(w_cnt, 0, sizeof(w_cnt));
    memset(w_px, 0, sizeof(w_px));
    memset(w_ns, 0, sizeof(w_ns));
    w_semi_px = w_soft_ns = w_pcgpu_ns = w_vsync_ns = w_load_bytes = 0;
    w_cpu0 = thread_cpu_ns();
    w_wall0 = now_ns();
}

/* ---- wrapper helpers ----------------------------------------------------- */

#define PCGPU_BEGIN()                                                          \
    prof_init();                                                               \
    uint64_t _t0 = now_ns();                                                   \
    s_phase = PH_PCGPU
#define PCGPU_END()                                                            \
    w_pcgpu_ns += now_ns() - _t0;                                              \
    s_phase = PH_GAME
#define SOFT_BEGIN()                                                           \
    uint64_t _s0 = now_ns();                                                   \
    s_phase = PH_SOFT
#define SOFT_END(k)                                                            \
    do {                                                                       \
        uint64_t _d = now_ns() - _s0;                                          \
        w_soft_ns += _d;                                                       \
        w_ns[k] += _d;                                                         \
        s_phase = PH_GAME;                                                     \
    } while (0)

static void apply_gutter(void) {
    int px = s_rgb24 ? 0 : s_gutter;
    int w = s_mode_w > 0 ? s_mode_w : 320;
    int32_t a[4] = {px, s_disp_x, w, s_mode_h};

    if (!s_soft_on) {
        return;
    }
    SoftwareRasterizer_SetGutter(px, s_disp_x, w, s_mode_h);
    tr_op(T_GUTTER, 4, a);
}

/* The rectangle the Switch present reads (switch_gpu.c's
 * dw3_nx_switch_gpu_display): the display mode's width, 240 lines, or the 24-bit
 * movie's 416 lines from y 36. */
static void display_rect(int *r) {
    r[0] = s_disp_x;
    r[1] = s_rgb24 ? 36 : s_disp_y;
    r[2] = s_mode_w > 0 ? s_mode_w : 320;
    r[3] = s_rgb24 ? 416 : 240;
}

static void trace_display(void) {
    int32_t a[4];

    if (s_trace_fp) {
        display_rect(a);
        tr_op(T_DISPLAY, 4, a);
    }
}

/* ---- wrapped Draw_* contract --------------------------------------------- */

void __real_Draw_Reset(void);
void __wrap_Draw_Reset(void) {
    PCGPU_BEGIN();
    __real_Draw_Reset();
    PCGPU_END();
    if (s_soft_on) {
        software_rasterizer.reset();
        tr_op(T_RESET, 0, NULL);
    }
}

void __real_Draw_SetTexpageMode(ParamDrawTexpageMode *p);
void __wrap_Draw_SetTexpageMode(ParamDrawTexpageMode *p) {
    PCGPU_BEGIN();
    __real_Draw_SetTexpageMode(p);
    PCGPU_END();
    if (s_soft_on && p) {
        int32_t a[1] = {(int32_t)(*(uint32_t *)p)};
        software_rasterizer.texpage_mode(p);
        tr_op(T_TEXPAGE, 1, a);
    }
}

void __real_Draw_SetTextureWindow(unsigned, unsigned, unsigned, unsigned);
void __wrap_Draw_SetTextureWindow(unsigned mx, unsigned my, unsigned ox,
                                  unsigned oy) {
    PCGPU_BEGIN();
    __real_Draw_SetTextureWindow(mx, my, ox, oy);
    PCGPU_END();
    if (s_soft_on) {
        int32_t a[4] = {(int32_t)mx, (int32_t)my, (int32_t)ox, (int32_t)oy};
        software_rasterizer.texture_window(mx, my, ox, oy);
        tr_op(T_TWIN, 4, a);
    }
}

void __real_Draw_SetAreaStart(int x, int y);
void __wrap_Draw_SetAreaStart(int x, int y) {
    PCGPU_BEGIN();
    __real_Draw_SetAreaStart(x, y);
    PCGPU_END();
    if (s_soft_on) {
        int32_t a[2] = {x, y};
        software_rasterizer.area_start(x, y);
        tr_op(T_AREA0, 2, a);
    }
}

void __real_Draw_SetAreaEnd(int x, int y);
void __wrap_Draw_SetAreaEnd(int x, int y) {
    PCGPU_BEGIN();
    __real_Draw_SetAreaEnd(x, y);
    PCGPU_END();
    if (s_soft_on) {
        int32_t a[2] = {x, y};
        software_rasterizer.area_end(x, y);
        tr_op(T_AREA1, 2, a);
    }
}

void __real_Draw_SetOffset(int x, int y);
void __wrap_Draw_SetOffset(int x, int y) {
    PCGPU_BEGIN();
    __real_Draw_SetOffset(x, y);
    PCGPU_END();
    if (s_soft_on) {
        int32_t a[2] = {x, y};
        software_rasterizer.offset(x, y);
        tr_op(T_OFFSET, 2, a);
    }
}

void __real_Draw_SetMask(int b0, int b1);
void __wrap_Draw_SetMask(int b0, int b1) {
    PCGPU_BEGIN();
    __real_Draw_SetMask(b0, b1);
    PCGPU_END();
    if (s_soft_on) {
        int32_t a[2] = {b0, b1};
        software_rasterizer.mask(b0, b1);
        tr_op(T_MASK, 2, a);
    }
}

void __real_Draw_ClearImage(PS1_RECT *rect, u_char r, u_char g, u_char b);
void __wrap_Draw_ClearImage(PS1_RECT *rect, u_char r, u_char g, u_char b) {
    PCGPU_BEGIN();
    __real_Draw_ClearImage(rect, r, g, b);
    PCGPU_END();
    if (s_soft_on && rect) {
        int32_t a[7] = {rect->x, rect->y, rect->w, rect->h, r, g, b};
        SOFT_BEGIN();
        software_rasterizer.clear_image(rect, r, g, b);
        SOFT_END(K_OTHER);
        w_cnt[K_OTHER]++;
        w_px[K_OTHER] += (uint64_t)(rect->w > 0 ? rect->w : 0) *
                         (uint64_t)(rect->h > 0 ? rect->h : 0);
        tr_op(T_CLEAR, 7, a);
    }
}

void __real_Draw_LoadImage(PS1_RECT *rect, u_long *p);
void __wrap_Draw_LoadImage(PS1_RECT *rect, u_long *p) {
    PCGPU_BEGIN();
    __real_Draw_LoadImage(rect, p);
    PCGPU_END();
    if (s_soft_on && rect) {
        int32_t a[4] = {rect->x, rect->y, rect->w, rect->h};
        SOFT_BEGIN();
        software_rasterizer.load_image(rect, p);
        SOFT_END(K_OTHER);
        w_cnt[K_OTHER]++;
        if (rect->w > 0 && rect->h > 0) {
            w_load_bytes += (uint64_t)rect->w * (uint64_t)rect->h * 2u;
        }
        if (s_trace_fp) {
            tr_op(T_LOAD, 4, a);
            if (rect->w > 0 && rect->h > 0) {
                fwrite(p, 2, (size_t)rect->w * (size_t)rect->h, s_trace_fp);
            }
        }
    }
}

void __real_Draw_StoreImage(PS1_RECT *rect, u_long *p);
void __wrap_Draw_StoreImage(PS1_RECT *rect, u_long *p) {
    PCGPU_BEGIN();
    __real_Draw_StoreImage(rect, p);
    PCGPU_END();
    /* the shadow does not write the game's buffer: the PC renderer owns it */
    if (s_soft_on && rect) {
        int32_t a[4] = {rect->x, rect->y, rect->w, rect->h};
        tr_op(T_STORE, 4, a);
    }
}

void __real_Draw_MoveImage(PS1_RECT *rect, unsigned x, unsigned y);
void __wrap_Draw_MoveImage(PS1_RECT *rect, unsigned x, unsigned y) {
    PCGPU_BEGIN();
    __real_Draw_MoveImage(rect, x, y);
    PCGPU_END();
    if (s_soft_on && rect) {
        int32_t a[6] = {rect->x, rect->y, rect->w, rect->h, (int32_t)x,
                        (int32_t)y};
        SOFT_BEGIN();
        software_rasterizer.move_image(rect, x, y);
        SOFT_END(K_OTHER);
        w_cnt[K_OTHER]++;
        tr_op(T_MOVE, 6, a);
    }
}

static int classify(uint32_t code) {
    if ((code & 0xE0) == 0x20) {
        int tex = (code & 0x04) != 0;
        int gour = (code & 0x10) != 0;
        return tex ? (gour ? K_POLY_GT : K_POLY_FT)
                   : (gour ? K_POLY_G : K_POLY_F);
    }
    if ((code & 0xE0) == 0x40) {
        return K_LINE;
    }
    if ((code & 0xE0) == 0x60) {
        return (code & 0x04) ? K_SPRT : K_TILE;
    }
    return K_OTHER;
}

static short s11(uint32_t v) {
    return (short)((((int)v & 0x7FF) ^ 0x400) - 0x400);
}

/* bounding-box pixels of a primitive (what the rasterizer loops over) */
static uint64_t prim_px(const u_long *w, int n, int k, uint32_t code) {
    if (k == K_TILE || k == K_SPRT) {
        int tex = (code & 0x04) != 0;
        int sz = code & 0x18;
        int pw, ph;
        if (sz == 0) {
            if (n < 3 + tex) {
                return 0;
            }
            pw = (short)(w[2 + tex] & 0xFFFF);
            ph = (short)((w[2 + tex] >> 16) & 0xFFFF);
        } else {
            pw = ph = sz == 0x08 ? 1 : sz == 0x10 ? 8 : 16;
        }
        return pw > 0 && ph > 0 ? (uint64_t)pw * (uint64_t)ph : 0;
    }
    if (k >= K_POLY_F && k <= K_POLY_GT) {
        int tex = (code & 0x04) != 0;
        int gour = (code & 0x10) != 0;
        int cnt = (code & 0x08) ? 4 : 3;
        int at = 1, minx = 1 << 30, maxx = -(1 << 30), miny = 1 << 30,
            maxy = -(1 << 30);
        for (int i = 0; i < cnt && at < n; i++) {
            if (i && gour) {
                at++;
            }
            int x = s11((uint32_t)w[at]);
            int y = s11((uint32_t)w[at] >> 16);
            at += 1 + tex;
            minx = x < minx ? x : minx;
            maxx = x > maxx ? x : maxx;
            miny = y < miny ? y : miny;
            maxy = y > maxy ? y : maxy;
        }
        if (maxx < minx || maxy < miny) {
            return 0;
        }
        /* a quad is two triangles, each walking (most of) the bbox */
        return (uint64_t)(maxx - minx + 1) * (uint64_t)(maxy - miny + 1) *
               (cnt == 4 ? 2u : 1u);
    }
    return 0;
}

int __real_Draw_PushPrim(u_long *packets, int max_len);
int __wrap_Draw_PushPrim(u_long *packets, int max_len) {
    int ret;
    PCGPU_BEGIN();
    ret = __real_Draw_PushPrim(packets, max_len);
    PCGPU_END();
    if (s_soft_on && packets && max_len > 0) {
        uint32_t code = (uint32_t)(packets[0] >> 24) & 0xFF;
        int k = classify(code);
        int n;
        SOFT_BEGIN();
        n = software_rasterizer.push_prim(packets, max_len);
        SOFT_END(k);
        w_cnt[k]++;
        {
            uint64_t px = prim_px(packets, n, k, code);
            w_px[k] += px;
            if (code & 0x02) {
                w_semi_px += px;
            }
        }
        if (s_trace_fp) {
            tr_u32(T_PRIM);
            tr_u32((uint32_t)n);
            for (int i = 0; i < n; i++) {
                uint64_t word = (uint64_t)packets[i];
                fwrite(&word, 8, 1, s_trace_fp);
            }
        }
        if (n != ret) {
            static int warned;
            if (!warned++) {
                fprintf(stderr,
                        "dw3-prof: prim %02x length soft=%d pc=%d\n", code, n,
                        ret);
            }
        }
    }
    return ret;
}

void __real_Draw_DisplayArea(unsigned x, unsigned y);
void __wrap_Draw_DisplayArea(unsigned x, unsigned y) {
    int moved = s_disp_x != (int)x || s_disp_y != (int)y;
    int h = s_mode_h > 0 ? s_mode_h : 240;

    PCGPU_BEGIN();
    __real_Draw_DisplayArea(x, y);
    PCGPU_END();
    s_disp_x = (int)x;
    s_disp_y = (int)y;
    trace_display();
    if (s_soft_on && moved && s_gutter > 0) {
        int32_t a[2] = {0, s_disp_y};
        int32_t b[2] = {s_disp_y + h, 512};
        SoftwareRasterizer_ClearGutterRows(0, s_disp_y);
        SoftwareRasterizer_ClearGutterRows(s_disp_y + h, 512);
        tr_op(T_GUTTER_CLEAR, 2, a);
        tr_op(T_GUTTER_CLEAR, 2, b);
        apply_gutter();
    }
}

void __real_Draw_SetDisplayMode(DisplayMode *mode);
void __wrap_Draw_SetDisplayMode(DisplayMode *mode) {
    PCGPU_BEGIN();
    __real_Draw_SetDisplayMode(mode);
    PCGPU_END();
    if (!mode) {
        return;
    }
    if (mode->horizontal_resolution_368) {
        s_mode_w = 368;
    } else {
        static const int widths[4] = {256, 320, 512, 640};
        s_mode_w = widths[mode->horizontal_resolution & 3];
    }
    s_rgb24 = mode->rgb24 ? 1 : 0;
    s_mode_h = mode->vertical_resolution ? 480 : 240;
    trace_display();
    apply_gutter();
}

int __real_Psyz_VideoSetGutter(unsigned px);
int __wrap_Psyz_VideoSetGutter(unsigned px) {
    int rc;
    PCGPU_BEGIN();
    rc = __real_Psyz_VideoSetGutter(px);
    PCGPU_END();
    if (px <= PSYZ_GUTTER_MAX && (int)px != s_gutter) {
        s_gutter = (int)px;
        apply_gutter();
    }
    return rc;
}

void __real_Psyz_SpuPullSamples(short *out, int num_frames);
void __wrap_Psyz_SpuPullSamples(short *out, int num_frames) {
    uint64_t t0 = thread_cpu_ns();
    __real_Psyz_SpuPullSamples(out, num_frames);
    s_mix_ns += thread_cpu_ns() - t0;
    s_mix_frames += (uint64_t)(num_frames > 0 ? num_frames : 0);
}

/* What nx_gpu.c's Psyz_VideoVSync(0) does around the shadow rasterizer: queue
 * the frames turbo does not show, sync the display rectangle for the ones it
 * does (and hash what the present would read). */
static void present_emulation(void) {
    int speed = dw3_port_turbo_speed();
    int r[4];
    uint64_t h = 1469598103934665603ull;

    if (s_defer_env) {
        int want = speed > 1;

        if ((SoftwareRasterizer_GetDefer() != 0) != want) {
            SOFT_BEGIN();
            SoftwareRasterizer_SetDefer(want);
            SOFT_END(K_OTHER);
        }
    }
    if (speed > 1 && !dw3_port_turbo_present_due()) {
        return;
    }
    display_rect(r);
    if (s_defer_env) {
        SOFT_BEGIN();
        SoftwareRasterizer_SyncRect(r[0], r[1], r[2], r[3], s_gutter > 0);
        SOFT_END(K_OTHER);
    }
    if (s_present_fp) {
        const uint16_t *v = SoftwareRasterizer_GetVramRaw();

        for (int y = r[1]; y < r[1] + r[3] && y < 512; y++) {
            for (int x = r[0]; x < r[0] + r[2] && x < 1024; x++) {
                h = (h ^ v[y * 1024 + x]) * 1099511628211ull;
            }
            if (s_gutter > 0 && !s_rgb24) {
                for (int side = 0; side < 2; side++) {
                    const uint16_t *g = SoftwareRasterizer_GetGutterPlaneRaw(side);

                    for (int x = 0; x < s_gutter; x++) {
                        h = (h ^ g[y * PSYZ_GUTTER_MAX + x]) * 1099511628211ull;
                    }
                }
            }
        }
        fprintf(s_present_fp, "%u %016llx\n", s_frame, (unsigned long long)h);
    }
}

/* One game frame ends at each VSync(0) (Psyz_VideoVSync(0): the present). */
int __real_Psyz_VideoVSync(int mode);
int __wrap_Psyz_VideoVSync(int mode) {
    int rc;
    uint64_t t0;

    if (mode != 0) {
        return __real_Psyz_VideoVSync(mode);
    }
    prof_init();
    if (s_soft_on && (s_defer_env || s_present_fp)) {
        present_emulation();
    }
    t0 = thread_cpu_ns();
    s_phase = PH_VSYNC;
    rc = __real_Psyz_VideoVSync(mode);
    w_vsync_ns += thread_cpu_ns() - t0; /* CPU, not the wait */
    s_phase = PH_GAME;
    if (s_hash_fp && s_soft_on) {
        fprintf(s_hash_fp, "%u %016llx\n", s_frame,
                (unsigned long long)hash_vram());
    }
    if (s_trace_fp) {
        int32_t a[1] = {(int32_t)s_frame};
        tr_op(T_FRAME, 1, a);
    }
    s_frame++;
    if (s_frame % s_window == 0) {
        window_end();
    }
    return rc;
}
