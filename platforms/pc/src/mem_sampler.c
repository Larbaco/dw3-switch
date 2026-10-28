/*
 * P5.1 PC memory sampler and soak driver (docs/P5.1_MEMORY.md).
 *
 * The PC equivalent of the Switch `mem dbg:` line (nx_mem_budget.c), plus what
 * only a Linux process can tell: RSS, the glibc malloc counters, threads and
 * open files. Everything is off unless the environment asks for it, so a
 * normal run, a replay run and the release build behave as before.
 *
 *   DW3_MEM_LOG=<file.csv>     write one CSV row per sample ("-" = only the
 *                              stderr line). Also turns the sampler on.
 *   DW3_MEM_LOG_EVERY=<n>      sample every n game frames (default 600, ~10 s
 *                              at 60 Hz). Frames are the PadChkVsync count of
 *                              a replay/record run; without one, vsyncs.
 *   DW3_SOAK_ARM_EVERY=<n>     every n game frames re-arm the next entry of
 *                              DW3_SOAK_ARM_LIST with the Cheats one-shots
 *                              (needs cheat_enabled=1 in port_options.cfg).
 *   DW3_SOAK_ARM_LIST=a;b;c    DW3_CHEATS_ARM specs, used in turn, e.g.
 *                              "battle_now;warp=0x233;money_10k;item=47:7".
 *
 * The game heap walk mirrors platforms/switch/psyz_backend/nx_mem_budget.c
 * (HEAP / MemBlock ABI on LP64) so both hosts report the same numbers.
 *
 * An LD_PRELOAD allocation audit (tools/memaudit/malloc_audit.c) hooks in
 * through `dw3_malloc_audit_frame`, found with dlsym when the library is
 * loaded; without it that costs one failed dlsym at start-up.
 */
#define _GNU_SOURCE
#include <dirent.h>
#include <dlfcn.h>
#include <malloc.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#ifdef DW3_CHEATS
#include "port_cheats.h"
#endif
#include "dw3_game_api.h" /* last: in a `both` image it redirects the game calls */

unsigned dw3_pad_host_frame(void);

enum {
    MEM_FREE = 0,
    MEM_END = 1,
    MEM_MODE = 2,
    MEM_FILE_CACHE = 3,
    MEM_LOCKED = 4
};

typedef struct MemBlockView {
    struct MemBlockView *prev;
    struct MemBlockView *next;
    int32_t tag;
} MemBlockView;

typedef struct HeapView {
    int32_t size;
    MemBlockView *first;
    MemBlockView *end;
} HeapView;

#if defined(DW3_BOTH) && DW3_BOTH
/* the running region's game copy (its arrays are local to the copy) */
#define HEAP (*(const HeapView *)DW3_API(heap))
#define DW3_HOST_HEAP (DW3_API(host_heap))
#define DW3_HOST_HEAP_SIZE (*DW3_API(host_heap_size))
#else
extern HeapView HEAP;
extern unsigned char DW3_HOST_HEAP[];
extern const unsigned long DW3_HOST_HEAP_SIZE;
#endif

static int g_init;
static int g_on;
static FILE *g_csv;
static unsigned g_every = 600;
static unsigned g_arm_every;
static char *g_arm_list;
static int g_arm_n;
static char **g_arm_item;
static int g_arm_next;
static unsigned g_vsyncs;
static unsigned g_next_sample;
static unsigned g_next_arm;
static struct timespec g_t0;
static void (*g_audit_frame)(unsigned);
static void (*g_audit_pause)(int);
static uint64_t g_hw_used, g_hw_fcache, g_hw_libc;
static long g_rss_peak_kb;

static int in_arena(const void *p) {
    const unsigned char *q = (const unsigned char *)p;

    return q >= DW3_HOST_HEAP && q < DW3_HOST_HEAP + DW3_HOST_HEAP_SIZE;
}

/* used / free / file cache / mode / locked bytes; 0 before initHeap. */
static int walk_heap(uint64_t out[5]) {
    MemBlockView *b = HEAP.first;
    unsigned steps = 0;

    memset(out, 0, 5 * sizeof(out[0]));
    if (b == NULL || !in_arena(b)) {
        return 0;
    }
    for (; b != NULL && steps < 100000u; steps++) {
        MemBlockView *next;
        uint64_t span;

        if (!in_arena(b) || b->tag == MEM_END) {
            break;
        }
        next = b->next;
        if (next == NULL || !in_arena(next) || next <= b) {
            return 0;
        }
        span = (uint64_t)((unsigned char *)next - (unsigned char *)b);
        if (b->tag == MEM_FREE) {
            out[1] += span;
        } else {
            out[0] += span;
            if (b->tag == MEM_FILE_CACHE) {
                out[2] += span;
            } else if (b->tag == MEM_MODE) {
                out[3] += span;
            } else if (b->tag == MEM_LOCKED) {
                out[4] += span;
            }
        }
        b = next;
    }
    return steps < 100000u;
}

static long status_kb(const char *key) {
    FILE *f = fopen("/proc/self/status", "r");
    char line[256];
    size_t n = strlen(key);
    long v = -1;

    if (f == NULL) {
        return -1;
    }
    while (fgets(line, sizeof(line), f) != NULL) {
        if (strncmp(line, key, n) == 0 && line[n] == ':') {
            v = strtol(line + n + 1, NULL, 10);
            break;
        }
    }
    fclose(f);
    return v;
}

static int count_dir(const char *path) {
    DIR *d = opendir(path);
    int n = 0;

    if (d == NULL) {
        return -1;
    }
    while (readdir(d) != NULL) {
        n++;
    }
    closedir(d);
    return n - 2;
}

static void sample(unsigned frame) {
    uint64_t h[5];
    int heap_ok = walk_heap(h);
    struct mallinfo2 mi = mallinfo2();
    long rss = status_kb("VmRSS");
    long hwm = status_kb("VmHWM");
    long data = status_kb("VmData");
    long thr = status_kb("Threads");
    int fds = count_dir("/proc/self/fd");
    struct timespec t;
    double wall;
    uint64_t libc = (uint64_t)mi.uordblks + (uint64_t)mi.hblkhd;

    clock_gettime(CLOCK_MONOTONIC, &t);
    wall = (double)(t.tv_sec - g_t0.tv_sec) +
           (double)(t.tv_nsec - g_t0.tv_nsec) / 1e9;
    if (heap_ok) {
        if (h[0] > g_hw_used) {
            g_hw_used = h[0];
        }
        if (h[2] > g_hw_fcache) {
            g_hw_fcache = h[2];
        }
    }
    if (libc > g_hw_libc) {
        g_hw_libc = libc;
    }
    if (rss > g_rss_peak_kb) {
        g_rss_peak_kb = rss;
    }
    if (g_csv != NULL) {
        fprintf(g_csv,
                "%.1f,%u,%d,%llu,%llu,%llu,%llu,%llu,%llu,%llu,%llu,%llu,%llu,"
                "%llu,%ld,%ld,%ld,%ld,%d\n",
                wall, frame, heap_ok, (unsigned long long)h[0],
                (unsigned long long)h[1], (unsigned long long)g_hw_used,
                (unsigned long long)h[2], (unsigned long long)g_hw_fcache,
                (unsigned long long)h[3], (unsigned long long)h[4],
                (unsigned long long)mi.uordblks, (unsigned long long)mi.hblkhd,
                (unsigned long long)mi.arena, (unsigned long long)g_hw_libc,
                rss, hwm, data, thr, fds);
        fflush(g_csv);
    }
    fprintf(stderr,
            "dw3-mem: t=%.0fs frame=%u heap=%lluK hw=%lluK free=%lluK "
            "fcache=%lluK/hw=%lluK mode=%lluK locked=%lluK libc=%lluK/hw=%lluK "
            "rss=%ldK hwm=%ldK thr=%ld fds=%d\n",
            wall, frame, (unsigned long long)(h[0] / 1024),
            (unsigned long long)(g_hw_used / 1024),
            (unsigned long long)(h[1] / 1024),
            (unsigned long long)(h[2] / 1024),
            (unsigned long long)(g_hw_fcache / 1024),
            (unsigned long long)(h[3] / 1024),
            (unsigned long long)(h[4] / 1024), (unsigned long long)(libc / 1024),
            (unsigned long long)(g_hw_libc / 1024), rss, hwm, thr, fds);
}

static void mem_sampler_init(void) {
    const char *log = getenv("DW3_MEM_LOG");
    const char *every = getenv("DW3_MEM_LOG_EVERY");
    const char *arm_every = getenv("DW3_SOAK_ARM_EVERY");
    const char *list = getenv("DW3_SOAK_ARM_LIST");

    g_init = 1;
    g_audit_frame = (void (*)(unsigned))dlsym(RTLD_DEFAULT,
                                              "dw3_malloc_audit_frame");
    g_audit_pause = (void (*)(int))dlsym(RTLD_DEFAULT,
                                         "dw3_malloc_audit_pause");
    if (log == NULL || log[0] == '\0') {
        return;
    }
    g_on = 1;
    clock_gettime(CLOCK_MONOTONIC, &g_t0);
    if (strcmp(log, "-") != 0) {
        g_csv = fopen(log, "w");
        if (g_csv == NULL) {
            fprintf(stderr, "dw3-mem: cannot write %s\n", log);
        } else {
            fputs("wall_s,frame,heap_ok,heap_used,heap_free,heap_hw,fcache,"
                  "fcache_hw,mode_bytes,locked_bytes,libc_inuse,libc_mmap,"
                  "libc_arena,libc_hw,rss_kb,hwm_kb,vmdata_kb,threads,fds\n",
                  g_csv);
        }
    }
    if (every != NULL && atoi(every) > 0) {
        g_every = (unsigned)atoi(every);
    }
    if (arm_every != NULL && atoi(arm_every) > 0 && list != NULL &&
        list[0] != '\0') {
        char *p;

        g_arm_every = (unsigned)atoi(arm_every);
        g_arm_list = strdup(list);
        g_arm_item = calloc(strlen(list) + 2, sizeof(char *));
        for (p = strtok(g_arm_list, ";"); p != NULL; p = strtok(NULL, ";")) {
            g_arm_item[g_arm_n++] = p;
        }
        g_next_arm = g_arm_every;
    }
    g_next_sample = 0;
    fprintf(stderr, "dw3-mem: sampler on, every %u frames, csv=%s, arm every "
            "%u (%d spec(s)), audit hook %s\n", g_every, log, g_arm_every,
            g_arm_n, g_audit_frame != NULL ? "yes" : "no");
}

/* Called once per vsync from the PC host. */
void dw3_pc_mem_sampler_tick(void) {
    unsigned frame;

    if (!g_init) {
        mem_sampler_init();
    }
    if (!g_on) {
        return;
    }
    g_vsyncs++;
    frame = dw3_pad_host_frame();
    if (frame == 0) {
        frame = g_vsyncs;
    }
    if (g_audit_frame != NULL) {
        g_audit_frame(frame);
    }
    if (g_arm_every != 0 && frame >= g_next_arm && g_arm_n > 0) {
        const char *spec = g_arm_item[g_arm_next++ % g_arm_n];

        g_next_arm = frame + g_arm_every;
        fprintf(stderr, "dw3-mem: soak arm frame=%u spec=%s\n", frame, spec);
#ifdef DW3_CHEATS
        dw3_cheats_soak_arm(spec);
#endif
    }
    if (frame >= g_next_sample) {
        g_next_sample = frame + g_every;
        if (g_audit_pause != NULL) {
            g_audit_pause(1); /* the sampler's own fopen/fclose is not game */
        }
        sample(frame);
        if (g_audit_pause != NULL) {
            g_audit_pause(0);
        }
    }
}
