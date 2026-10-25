/*
 * P5.1 host allocation audit: an LD_PRELOAD malloc counter for dw3-pc.
 *
 *   cc -O1 -g -shared -fPIC -o libdw3_malloc_audit.so malloc_audit.c -ldl -lpthread
 *   LD_PRELOAD=$PWD/libdw3_malloc_audit.so DW3_MEM_LOG=- \
 *     DW3_MALLOC_AUDIT_OUT=/path/prefix DW3_MALLOC_AUDIT_FROM=3000 dw3-pc ...
 *
 * It hooks malloc, calloc, realloc, free, posix_memalign, aligned_alloc,
 * memalign and reallocarray (the glibc __libc_* entry points are the real
 * allocator, so nothing here allocates through the hooks) and, once armed,
 * keys every allocation by its call stack. The game frame comes from
 * dw3_malloc_audit_frame(), which platforms/pc/src/mem_sampler.c calls every
 * vsync when it finds this library (it needs DW3_MEM_LOG set, "-" is enough).
 *
 * Environment:
 *   DW3_MALLOC_AUDIT_OUT=<prefix>   writes <prefix>.windows.tsv,
 *                                   <prefix>.sites.tsv, <prefix>.deltas.tsv,
 *                                   <prefix>.summary.txt  (required)
 *   DW3_MALLOC_AUDIT_FROM=<frame>   arm at this game frame (default 3000):
 *                                   allocations before it are "boot".
 *   DW3_MALLOC_AUDIT_WINDOW=<n>     frames per window row (default 600).
 *   DW3_MALLOC_AUDIT_DEPTH=<n>      stack frames kept per site (default 14).
 *
 * "main" is the thread that called dw3_malloc_audit_frame (the game thread:
 * the frame loop). Everything else (SDL audio, the GPU driver's workers) is
 * "other". tools/memaudit/report.py symbolizes and tabulates the output.
 */
#define _GNU_SOURCE
#include <dlfcn.h>
#include <errno.h>
#include <execinfo.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/syscall.h>
#include <unistd.h>

extern void *__libc_malloc(size_t);
extern void *__libc_calloc(size_t, size_t);
extern void *__libc_realloc(void *, size_t);
extern void __libc_free(void *);
extern void *__libc_memalign(size_t, size_t);

#define MAX_DEPTH 24
#define SITE_CAP (1u << 16)
#define PTR_CAP (1u << 21) /* live tracked allocations */

typedef struct Site {
    uint64_t hash;
    uint32_t used;
    uint32_t main; /* 1 = game thread */
    uint32_t depth;
    uint32_t pad;
    uint64_t count, bytes, frees;
    int64_t live_n, live_bytes;
    uint64_t win_count, win_bytes;
    uint32_t first_frame, last_frame, last_hit_frame;
    uint32_t active_frames; /* frames in which this site allocated */
    void *pc[MAX_DEPTH];
} Site;

typedef struct PtrEnt {
    uintptr_t ptr; /* 0 empty, 1 tombstone */
    uint32_t site;
    uint32_t size;
} PtrEnt;

static Site *g_sites;
static PtrEnt *g_ptrs;
static atomic_flag g_lock = ATOMIC_FLAG_INIT;
static int g_ready;
static int g_armed;
static unsigned g_from = 3000;
static unsigned g_window = 600;
static unsigned g_depth = 14;
static unsigned g_frame;
static unsigned g_win_start;
static pid_t g_main_tid;
static char g_prefix[400];
static FILE *g_win_fp, *g_delta_fp;
static __thread int t_in_hook;
static __thread int t_paused;

/* main-thread / other-thread counters, whole run since arming and per window */
static uint64_t g_tot[2][3]; /* [main][alloc, free, bytes] */
static uint64_t g_win[2][3];
static uint64_t g_ptr_full, g_site_full;
/* per-frame allocation count of the main thread */
static uint64_t g_frame_main_allocs, g_frames_seen, g_frames_alloc, g_frame_max;
static uint64_t g_frame_hist[8]; /* 0, 1, 2-3, 4-7, 8-15, 16-31, 32-63, 64+ */
static unsigned g_win_new_sites;

static void *tab_alloc(size_t n) {
    void *p = mmap(NULL, n, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS,
                   -1, 0);
    return p == MAP_FAILED ? NULL : p;
}

static void lock(void) {
    while (atomic_flag_test_and_set_explicit(&g_lock, memory_order_acquire)) {
    }
}
static void unlock(void) {
    atomic_flag_clear_explicit(&g_lock, memory_order_release);
}

static void init_once(void) {
    const char *e;

    if (g_ready) {
        return;
    }
    g_sites = tab_alloc(sizeof(Site) * SITE_CAP);
    g_ptrs = tab_alloc(sizeof(PtrEnt) * PTR_CAP);
    if ((e = getenv("DW3_MALLOC_AUDIT_FROM")) != NULL) {
        g_from = (unsigned)strtoul(e, NULL, 10);
    }
    if ((e = getenv("DW3_MALLOC_AUDIT_WINDOW")) != NULL && atoi(e) > 0) {
        g_window = (unsigned)atoi(e);
    }
    if ((e = getenv("DW3_MALLOC_AUDIT_DEPTH")) != NULL && atoi(e) > 0) {
        g_depth = (unsigned)atoi(e) > MAX_DEPTH ? MAX_DEPTH : (unsigned)atoi(e);
    }
    if ((e = getenv("DW3_MALLOC_AUDIT_OUT")) != NULL) {
        snprintf(g_prefix, sizeof(g_prefix), "%s", e);
    }
    g_ready = g_sites != NULL && g_ptrs != NULL;
}

static uint64_t hash_pcs(void *const *pc, unsigned n, unsigned main) {
    uint64_t h = 1469598103934665603ull ^ main;
    unsigned i;

    for (i = 0; i < n; i++) {
        h ^= (uint64_t)(uintptr_t)pc[i];
        h *= 1099511628211ull;
    }
    return h | 1;
}

/* caller holds the lock */
static Site *find_site(uint64_t h, void *const *pc, unsigned n, unsigned main,
                       uint32_t *idx_out) {
    uint32_t i = (uint32_t)(h & (SITE_CAP - 1));
    uint32_t probes;

    for (probes = 0; probes < SITE_CAP; probes++, i = (i + 1) & (SITE_CAP - 1)) {
        Site *s = &g_sites[i];

        if (!s->used) {
            s->used = 1;
            s->hash = h;
            s->main = main;
            s->depth = n;
            memcpy(s->pc, pc, n * sizeof(void *));
            s->first_frame = g_frame;
            g_win_new_sites++;
            *idx_out = i;
            return s;
        }
        if (s->hash == h && s->main == main) {
            *idx_out = i;
            return s;
        }
    }
    g_site_full++;
    return NULL;
}

static PtrEnt *ptr_find(uintptr_t p) {
    uint32_t i = (uint32_t)((p >> 4) * 0x9E3779B1u) & (PTR_CAP - 1);
    uint32_t probes;

    for (probes = 0; probes < PTR_CAP; probes++, i = (i + 1) & (PTR_CAP - 1)) {
        if (g_ptrs[i].ptr == p) {
            return &g_ptrs[i];
        }
        if (g_ptrs[i].ptr == 0) {
            return NULL;
        }
    }
    return NULL;
}

static void ptr_insert(uintptr_t p, uint32_t site, uint32_t size) {
    uint32_t i = (uint32_t)((p >> 4) * 0x9E3779B1u) & (PTR_CAP - 1);
    uint32_t probes;

    for (probes = 0; probes < PTR_CAP; probes++, i = (i + 1) & (PTR_CAP - 1)) {
        if (g_ptrs[i].ptr == 0 || g_ptrs[i].ptr == 1 || g_ptrs[i].ptr == p) {
            g_ptrs[i].ptr = p;
            g_ptrs[i].site = site;
            g_ptrs[i].size = size;
            return;
        }
    }
    g_ptr_full++;
}

static int is_main(void) {
    return (pid_t)syscall(SYS_gettid) == g_main_tid;
}

static void record_alloc(void *p, size_t size) {
    void *pc[MAX_DEPTH + 3];
    int n;
    unsigned m;
    uint64_t h;
    uint32_t idx;
    Site *s;

    if (p == NULL || !g_armed || t_in_hook || t_paused) {
        return;
    }
    t_in_hook = 1;
    n = backtrace(pc, (int)g_depth + 2);
    if (n > 2) {
        m = is_main();
        h = hash_pcs(pc + 2, (unsigned)n - 2, m);
        lock();
        s = find_site(h, pc + 2, (unsigned)n - 2, m, &idx);
        if (s != NULL) {
            s->count++;
            s->bytes += size;
            s->live_n++;
            s->live_bytes += (int64_t)size;
            s->win_count++;
            s->win_bytes += size;
            s->last_frame = g_frame;
            if (s->last_hit_frame != g_frame || s->active_frames == 0) {
                s->active_frames++;
                s->last_hit_frame = g_frame;
            }
            ptr_insert((uintptr_t)p, idx, (uint32_t)(size > 0xFFFFFFFFu ? 0xFFFFFFFFu : size));
        }
        g_tot[m][0]++;
        g_tot[m][2] += size;
        g_win[m][0]++;
        g_win[m][2] += size;
        if (m) {
            g_frame_main_allocs++;
        }
        unlock();
    }
    t_in_hook = 0;
}

static void record_free(void *p) {
    PtrEnt *e;

    if (p == NULL || !g_ready || t_in_hook) {
        return;
    }
    t_in_hook = 1;
    lock();
    e = ptr_find((uintptr_t)p);
    if (e != NULL) {
        Site *s = &g_sites[e->site];

        s->frees++;
        s->live_n--;
        s->live_bytes -= (int64_t)e->size;
        g_tot[s->main][1]++;
        g_win[s->main][1]++;
        e->ptr = 1; /* tombstone */
    }
    unlock();
    t_in_hook = 0;
}

void *malloc(size_t n) {
    void *p = __libc_malloc(n);

    if (g_armed) {
        record_alloc(p, n);
    }
    return p;
}

void *calloc(size_t a, size_t b) {
    void *p = __libc_calloc(a, b);

    if (g_armed) {
        record_alloc(p, a * b);
    }
    return p;
}

void *realloc(void *old, size_t n) {
    void *p;

    if (g_armed && old != NULL) {
        record_free(old);
    }
    p = __libc_realloc(old, n);
    if (g_armed && p != NULL) {
        record_alloc(p, n);
    }
    return p;
}

void free(void *p) {
    if (g_armed && p != NULL) {
        record_free(p);
    }
    __libc_free(p);
}

void *reallocarray(void *old, size_t a, size_t b) {
    if (b != 0 && a > (size_t)-1 / b) {
        errno = ENOMEM;
        return NULL;
    }
    return realloc(old, a * b);
}

void *memalign(size_t al, size_t n) {
    void *p = __libc_memalign(al, n);

    if (g_armed) {
        record_alloc(p, n);
    }
    return p;
}

void *aligned_alloc(size_t al, size_t n) {
    return memalign(al, n);
}

int posix_memalign(void **out, size_t al, size_t n) {
    void *p = __libc_memalign(al, n);

    if (p == NULL) {
        return ENOMEM;
    }
    *out = p;
    if (g_armed) {
        record_alloc(p, n);
    }
    return 0;
}

static void dump_sites(void) {
    char path[460];
    FILE *f;
    uint32_t i;
    unsigned d;

    if (g_prefix[0] == '\0') {
        return;
    }
    snprintf(path, sizeof(path), "%s.sites.tsv", g_prefix);
    f = fopen(path, "w");
    if (f != NULL) {
        fputs("site\tmain\tcount\tbytes\tfrees\tlive_n\tlive_bytes\tactive_frames"
              "\tfirst_frame\tlast_frame\tstack\n", f);
        for (i = 0; i < SITE_CAP; i++) {
            Site *s = &g_sites[i];

            if (!s->used) {
                continue;
            }
            fprintf(f, "%u\t%u\t%llu\t%llu\t%llu\t%lld\t%lld\t%u\t%u\t%u\t", i,
                    s->main, (unsigned long long)s->count,
                    (unsigned long long)s->bytes, (unsigned long long)s->frees,
                    (long long)s->live_n, (long long)s->live_bytes,
                    s->active_frames, s->first_frame, s->last_frame);
            for (d = 0; d < s->depth; d++) {
                Dl_info di;

                if (dladdr(s->pc[d], &di) != 0 && di.dli_fname != NULL) {
                    fprintf(f, "%s%s+0x%lx", d ? "," : "", di.dli_fname,
                            (unsigned long)((char *)s->pc[d] - (char *)di.dli_fbase));
                } else {
                    fprintf(f, "%s?+%p", d ? "," : "", s->pc[d]);
                }
            }
            fputc('\n', f);
        }
        fclose(f);
    }
    snprintf(path, sizeof(path), "%s.summary.txt", g_prefix);
    f = fopen(path, "w");
    if (f != NULL) {
        fprintf(f, "armed_from_frame=%u last_frame=%u\n", g_from, g_frame);
        fprintf(f, "main_allocs=%llu main_frees=%llu main_bytes=%llu\n",
                (unsigned long long)g_tot[1][0], (unsigned long long)g_tot[1][1],
                (unsigned long long)g_tot[1][2]);
        fprintf(f, "other_allocs=%llu other_frees=%llu other_bytes=%llu\n",
                (unsigned long long)g_tot[0][0], (unsigned long long)g_tot[0][1],
                (unsigned long long)g_tot[0][2]);
        fprintf(f, "frames_seen=%llu frames_with_main_alloc=%llu "
                "max_main_allocs_in_one_frame=%llu\n",
                (unsigned long long)g_frames_seen,
                (unsigned long long)g_frames_alloc,
                (unsigned long long)g_frame_max);
        fprintf(f, "main_allocs_per_frame_hist(0,1,2-3,4-7,8-15,16-31,32-63,64+)=");
        for (d = 0; d < 8; d++) {
            fprintf(f, "%s%llu", d ? "," : "", (unsigned long long)g_frame_hist[d]);
        }
        fprintf(f, "\nptr_table_full=%llu site_table_full=%llu\n",
                (unsigned long long)g_ptr_full, (unsigned long long)g_site_full);
        fclose(f);
    }
}

static void flush_window(unsigned frame) {
    uint32_t i;

    if (g_win_fp != NULL) {
        int64_t live_n = 0, live_b = 0;

        for (i = 0; i < SITE_CAP; i++) {
            if (g_sites[i].used) {
                live_n += g_sites[i].live_n;
                live_b += g_sites[i].live_bytes;
            }
        }
        fprintf(g_win_fp,
                "%u\t%u\t%llu\t%llu\t%llu\t%llu\t%llu\t%llu\t%lld\t%lld\t%u\n",
                g_win_start, frame, (unsigned long long)g_win[1][0],
                (unsigned long long)g_win[1][1], (unsigned long long)g_win[1][2],
                (unsigned long long)g_win[0][0], (unsigned long long)g_win[0][1],
                (unsigned long long)g_win[0][2], (long long)live_n,
                (long long)live_b, g_win_new_sites);
        fflush(g_win_fp);
    }
    if (g_delta_fp != NULL) {
        for (i = 0; i < SITE_CAP; i++) {
            Site *s = &g_sites[i];

            if (s->used && s->win_count != 0) {
                fprintf(g_delta_fp, "%u\t%u\t%u\t%llu\t%llu\n", frame, i, s->main,
                        (unsigned long long)s->win_count,
                        (unsigned long long)s->win_bytes);
                s->win_count = 0;
                s->win_bytes = 0;
            }
        }
        fflush(g_delta_fp);
    }
    memset(g_win, 0, sizeof(g_win));
    g_win_new_sites = 0;
    g_win_start = frame;
}

static void audit_exit(void) {
    if (g_armed) {
        g_armed = 0;
        t_in_hook = 1;
        dump_sites();
    }
}

void dw3_malloc_audit_pause(int on) {
    t_paused = on;
}

void dw3_malloc_audit_frame(unsigned frame) {
    unsigned b;
    uint64_t n;

    if (!g_ready) {
        init_once();
        if (!g_ready) {
            return;
        }
        g_main_tid = (pid_t)syscall(SYS_gettid);
    }
    if (!g_armed) {
        if (frame >= g_from) {
            char path[460];

            t_in_hook = 1;
            if (g_prefix[0] != '\0') {
                snprintf(path, sizeof(path), "%s.windows.tsv", g_prefix);
                g_win_fp = fopen(path, "w");
                if (g_win_fp != NULL) {
                    fputs("frame_from\tframe_to\tmain_allocs\tmain_frees\tmain_bytes"
                          "\tother_allocs\tother_frees\tother_bytes\tlive_n"
                          "\tlive_bytes\tnew_sites\n", g_win_fp);
                }
                snprintf(path, sizeof(path), "%s.deltas.tsv", g_prefix);
                g_delta_fp = fopen(path, "w");
                if (g_delta_fp != NULL) {
                    fputs("frame\tsite\tmain\tcount\tbytes\n", g_delta_fp);
                }
            }
            atexit(audit_exit);
            g_win_start = frame;
            g_frame = frame;
            t_in_hook = 0;
            g_armed = 1;
        }
        return;
    }
    /* close the previous frame's main-thread allocation count */
    lock();
    n = g_frame_main_allocs;
    g_frame_main_allocs = 0;
    unlock();
    g_frames_seen++;
    if (n != 0) {
        g_frames_alloc++;
    }
    if (n > g_frame_max) {
        g_frame_max = n;
    }
    b = n == 0 ? 0 : n == 1 ? 1 : n < 4 ? 2 : n < 8 ? 3 : n < 16 ? 4
        : n < 32 ? 5 : n < 64 ? 6 : 7;
    g_frame_hist[b]++;
    g_frame = frame;
    if (frame - g_win_start >= g_window) {
        t_in_hook = 1;
        lock();
        flush_window(frame);
        unlock();
        t_in_hook = 0;
        if ((frame / g_window) % 10 == 0) {
            t_in_hook = 1;
            dump_sites(); /* a crash or kill -9 still leaves a recent file */
            t_in_hook = 0;
        }
    }
}
