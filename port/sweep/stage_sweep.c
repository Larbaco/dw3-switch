/* P5.5 stage sweep: see port/include/port_stage_sweep.h and
 * docs/P5.5_STAGE_SWEEP.md. Compiled only with DW3_CHEATS (port/CMakeLists.txt);
 * tools/tests/stage_sweep_main.c builds it alone (DW3_SWEEP_TEST).
 *
 * The state machine is driven by dw3_sweep_tick(), once per game frame of the
 * mode task. Per stage: request the mode from the stage select, wait for the
 * mode (20 s), let it run `need` frames, take the free game heap, request the
 * stage select again, wait for it (20 s), log one line. The progress file is
 * appended at every step, so a crash or a hang leaves a "begin" with no
 * result, which the next run reports as that stage's failure and moves on.
 * A watchdog thread (not in the host test) turns a hang into the same record
 * and ends the process, so a relaunch continues. */
#include "port_stage_sweep.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <pthread.h>
#include <unistd.h>

#ifdef DW3_SWEEP_TEST
#define SW_INFO(...) (fprintf(stderr, __VA_ARGS__), fputc('\n', stderr))
#define SW_WARN(...) (fprintf(stderr, __VA_ARGS__), fputc('\n', stderr))
#else
#include "port_options.h"
#include <psyz/log.h>
/* psyz_log: on the Switch the lines reach sdmc:/switch/dw3/dw3.log. */
#define SW_INFO(...) INFOF(__VA_ARGS__)
#define SW_WARN(...) WARNF(__VA_ARGS__)
#endif

typedef struct SweepStage {
    int mode;
    int num;
} SweepStage;

static const SweepStage g_stages[] = {
#include "stage_list.inc"
};
#define NSTAGE ((int)(sizeof(g_stages) / sizeof(g_stages[0])))

enum { SWR_NONE = 0, SWR_OK = 1, SWR_FAIL = 2, SWR_SKIP = 3 };
enum { S_INIT, S_GOTO_SELECT, S_SELECT_DWELL, S_ARRIVE, S_DWELL, S_RETURN,
       S_DONE };

#define SW_KEY "stage_sweep"
#define SW_KEY_FRAMES "stage_sweep_frames"
#define SW_FILE "stage_sweep.txt"
#define SW_DEFAULT_FRAMES 90
#define SW_SELECT_FRAMES 30
#define SW_TIMEOUT_MS 20000ul
#define SW_HANG_DEFAULT_MS 45000ul
#define SW_WHY 128

static int g_enabled = -1;
static int g_need = SW_DEFAULT_FRAMES;
static char g_dir[256];
static unsigned char g_res[NSTAGE];
static char g_why[NSTAGE][SW_WHY];
static int g_only[NSTAGE];
static int g_only_n;
static int g_file_done;
static volatile int g_state = S_INIT;
static int g_cur = -1;
static int g_frames;
static int g_level;
static unsigned long g_t_req;
static unsigned long g_t_arrive;
static unsigned long g_t_ret;
static unsigned long g_mem_free;
static volatile unsigned long g_last_tick;
static unsigned long g_test_clock;
static int g_watchdog_started;
static unsigned long g_hang_ms = SW_HANG_DEFAULT_MS;

/* --- the game heap, for "mem free" (the walk of nx_mem_budget.c) ---------- */

#ifndef DW3_SWEEP_TEST
typedef struct SwBlock {
    struct SwBlock *prev;
    struct SwBlock *next;
    int tag;
} SwBlock;
typedef struct SwHeap {
    int size;
    SwBlock *first;
    SwBlock *end;
} SwHeap;
extern SwHeap HEAP;
extern unsigned char DW3_HOST_HEAP[];
extern const unsigned long DW3_HOST_HEAP_SIZE;

static int sw_in_arena(const void *p) {
    const unsigned char *q = (const unsigned char *)p;

    return q >= DW3_HOST_HEAP && q < DW3_HOST_HEAP + DW3_HOST_HEAP_SIZE;
}

/* Free bytes of the game heap (blocks tagged MEM_FREE = 0); 0 if unwalkable. */
static unsigned long sw_heap_free(void) {
    SwBlock *b = HEAP.first;
    unsigned long total = 0;
    unsigned steps = 0;

    if (b == NULL || !sw_in_arena(b)) {
        return 0;
    }
    for (; b != NULL && steps < 100000u; steps++) {
        SwBlock *next;

        if (!sw_in_arena(b) || b->tag == 1) {
            break;
        }
        next = b->next;
        if (next == NULL || !sw_in_arena(next) || next <= b) {
            return 0;
        }
        if (b->tag == 0) {
            total += (unsigned long)((unsigned char *)next - (unsigned char *)b);
        }
        b = next;
    }
    return total;
}
#else
static unsigned long sw_heap_free(void) {
    return 12345ul;
}
#endif

/* --- small helpers -------------------------------------------------------- */

static unsigned long sw_real_ms(void) {
    struct timespec ts;

    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (unsigned long)ts.tv_sec * 1000ul + (unsigned long)ts.tv_nsec / 1000000ul;
}

static unsigned long sw_now(void) {
    return g_test_clock != 0 ? g_test_clock : sw_real_ms();
}

static int sw_index(int mode) {
    int i;

    for (i = 0; i < NSTAGE; i++) {
        if (g_stages[i].mode == mode) {
            return i;
        }
    }
    return -1;
}

/* Modes the sweep leaves out, with the reason it logs. None now: WSTAG460
 * (mode 0x236) was the only one, a weak stub until it was linked (patch 0197),
 * and every stage of the table is visited. The mechanism stays for a stage
 * that cannot be loaded without its progress. */
static const char *sw_skip_reason(int mode) {
    (void)mode;
    return NULL;
}

static int sw_selected(int index) {
    int i;

    if (g_only_n == 0) {
        return 1;
    }
    for (i = 0; i < g_only_n; i++) {
        if (g_only[i] == g_stages[index].mode) {
            return 1;
        }
    }
    return 0;
}

static void sw_path(char *out, size_t n) {
    if (g_dir[0] == '\0') {
        out[0] = '\0';
        return;
    }
    snprintf(out, n, "%s/%s", g_dir, SW_FILE);
}

/* One line of the progress file: "<kind> 0x<mode> <text>". Opened and closed
 * every time, so a crash right after loses nothing. */
static void sw_record(const char *kind, int mode, const char *fmt, ...) {
    char path[300];
    char text[SW_WHY];
    FILE *f;
    va_list ap;

    text[0] = '\0';
    if (fmt != NULL) {
        va_start(ap, fmt);
        vsnprintf(text, sizeof(text), fmt, ap);
        va_end(ap);
    }
    sw_path(path, sizeof(path));
    if (path[0] == '\0') {
        return;
    }
    f = fopen(path, "a");
    if (f == NULL) {
        SW_WARN("dw3-sweep: cannot write %s; a crash will not be resumable",
                path);
        return;
    }
    if (mode >= 0) {
        fprintf(f, "%s 0x%03x %s\n", kind, mode, text);
    } else {
        fprintf(f, "%s\n", kind);
    }
    fclose(f);
}

static void sw_set_result(int index, int res, const char *why) {
    g_res[index] = (unsigned char)res;
    snprintf(g_why[index], SW_WHY, "%s", why != NULL ? why : "");
}

/* A stage failed: log, record, remember. */
static void sw_fail(int index, const char *fmt, ...) {
    char why[SW_WHY];
    va_list ap;

    va_start(ap, fmt);
    vsnprintf(why, sizeof(why), fmt, ap);
    va_end(ap);
    sw_set_result(index, SWR_FAIL, why);
    SW_WARN("dw3-sweep: stage WSTAG%03d FAILED: %s", g_stages[index].num, why);
    sw_record("fail", g_stages[index].mode, "%s", why);
}

/* done X/Y ok, failed: ... (one line, the answer of the whole run) */
static void sw_summary(const char *head) {
    char line[2048];
    size_t n;
    int i, ok = 0, tested = 0, skipped = 0, untested = 0, failed = 0;

    for (i = 0; i < NSTAGE; i++) {
        if (!sw_selected(i)) {
            continue;
        }
        switch (g_res[i]) {
        case SWR_OK:
            ok++;
            tested++;
            break;
        case SWR_FAIL:
            failed++;
            tested++;
            break;
        case SWR_SKIP:
            skipped++;
            break;
        default:
            untested++;
            break;
        }
    }
    n = (size_t)snprintf(line, sizeof(line), "%s %d/%d ok, failed: ", head, ok,
                         tested);
    if (failed == 0 && n < sizeof(line)) {
        n += (size_t)snprintf(line + n, sizeof(line) - n, "none");
    }
    for (i = 0; i < NSTAGE && n + 90 < sizeof(line); i++) {
        if (sw_selected(i) && g_res[i] == SWR_FAIL) {
            n += (size_t)snprintf(line + n, sizeof(line) - n, "%sWSTAG%03d (%s)",
                                  n > 0 && line[n - 1] != ' ' ? ", " : "",
                                  g_stages[i].num, g_why[i]);
        }
    }
    if (skipped > 0 && n + 40 < sizeof(line)) {
        n += (size_t)snprintf(line + n, sizeof(line) - n, "; skipped:");
        for (i = 0; i < NSTAGE && n + 40 < sizeof(line); i++) {
            if (sw_selected(i) && g_res[i] == SWR_SKIP) {
                n += (size_t)snprintf(line + n, sizeof(line) - n, " WSTAG%03d",
                                      g_stages[i].num);
            }
        }
    }
    if (untested > 0 && n + 40 < sizeof(line)) {
        n += (size_t)snprintf(line + n, sizeof(line) - n, "; not run: %d",
                              untested);
    }
    SW_INFO("dw3-sweep: %s", line);
}

/* --- configuration and the progress file ---------------------------------- */

static void sw_parse_only(const char *s) {
    char *end;

    g_only_n = 0;
    while (s != NULL && *s != '\0' && g_only_n < NSTAGE) {
        long v = strtol(s, &end, 0);

        if (end == s) {
            break;
        }
        g_only[g_only_n++] = (int)v;
        s = end;
        while (*s == ',' || *s == ' ' || *s == ';') {
            s++;
        }
    }
}

/* P51 item 7: `stage_sweep=1` in the TOP-LEVEL port_options.cfg (the region
 * picker's file, beside the us/ and eu/ folders) is not read: the sweep reads
 * its key from the region folder's file. When the owner put it in the wrong one
 * the log says so, once per boot, and nothing else changes. */
static void sw_note_stray_key(void) {
    char top[300];
    char path[320];
    char line[160];
    char *slash;
    size_t n;
    FILE *f;
    int found = 0;

    if (g_dir[0] == '\0' || strlen(g_dir) >= sizeof(top)) {
        return;
    }
    strcpy(top, g_dir);
    n = strlen(top);
    while (n > 1 && top[n - 1] == '/') {
        top[--n] = '\0';
    }
    slash = strrchr(top, '/');
    if (slash == NULL || slash == top) {
        return;
    }
    *slash = '\0';
    snprintf(path, sizeof(path), "%s/port_options.cfg", top);
    f = fopen(path, "r");
    if (f == NULL) {
        return;
    }
    while (fgets(line, sizeof(line), f) != NULL) {
        if (strncmp(line, SW_KEY "=", sizeof(SW_KEY)) == 0 &&
            strtol(line + sizeof(SW_KEY), NULL, 0) != 0) {
            found = 1;
        }
    }
    fclose(f);
    if (found) {
        SW_INFO("dw3-sweep: stage_sweep found in %s, ignored; put it in %s/port_options.cfg",
                path, g_dir);
    }
}

/* Reads the key (or the environment). Returns the key value: 0 off, 1 run or
 * resume, 2 start from the beginning. */
static int sw_read_config(void) {
    const char *env = getenv("DW3_STAGE_SWEEP");
    const char *v;
    int level = 0;

    if (env != NULL && env[0] != '\0' && env[0] != '0') {
        level = env[0] == '2' ? 2 : 1;
    }
#ifndef DW3_SWEEP_TEST
    if (level == 0) {
        level = dw3_port_options_file_int(SW_KEY, 0);
        if (level < 0 || level > 2) {
            level = 0;
        }
    }
    g_need = dw3_port_options_file_int(SW_KEY_FRAMES, SW_DEFAULT_FRAMES);
    if (g_dir[0] == '\0') {
        const char *d = dw3_port_options_cfg_dir();

        if (d != NULL && strlen(d) < sizeof(g_dir)) {
            strcpy(g_dir, d);
        }
    }
#endif
    v = getenv("DW3_STAGE_SWEEP_FRAMES");
    if (v != NULL && v[0] != '\0') {
        g_need = (int)strtol(v, NULL, 0);
    }
    if (g_need < 30 || g_need > 3600) {
        g_need = SW_DEFAULT_FRAMES;
    }
    v = getenv("DW3_STAGE_SWEEP_HANG_MS");
    if (v != NULL && v[0] != '\0' && strtol(v, NULL, 0) > 0) {
        g_hang_ms = (unsigned long)strtol(v, NULL, 0);
    }
    v = getenv("DW3_STAGE_SWEEP_DIR");
    if (v != NULL && v[0] != '\0' && strlen(v) < sizeof(g_dir)) {
        strcpy(g_dir, v);
    }
    v = getenv("DW3_STAGE_SWEEP_ONLY");
    if (v != NULL && v[0] != '\0') {
        sw_parse_only(v);
    }
    if (level == 0) {
        sw_note_stray_key();
    }
    return level;
}

/* Loads the progress file. Returns the mode that was "begun" and never got a
 * result (0 if none). */
static int sw_load_file(void) {
    char path[300];
    char line[160];
    FILE *f;
    int pending = 0;

    sw_path(path, sizeof(path));
    g_file_done = 0;
    f = path[0] != '\0' ? fopen(path, "r") : NULL;
    if (f == NULL) {
        return 0;
    }
    while (fgets(line, sizeof(line), f) != NULL) {
        char kind[16];
        unsigned mode = 0;
        char *rest;
        int index;

        if (strncmp(line, "done", 4) == 0) {
            g_file_done = 1;
            continue;
        }
        if (sscanf(line, "%15s 0x%x", kind, &mode) != 2) {
            continue;
        }
        index = sw_index((int)mode);
        rest = strchr(line, 'x');
        rest = rest != NULL ? strchr(rest, ' ') : NULL;
        if (rest != NULL) {
            char *nl = strchr(rest, '\n');

            if (nl != NULL) {
                *nl = '\0';
            }
            rest++;
        }
        if (strcmp(kind, "begin") == 0) {
            pending = (int)mode;
        } else if (index >= 0) {
            if (strcmp(kind, "ok") == 0) {
                sw_set_result(index, SWR_OK, rest);
            } else if (strcmp(kind, "fail") == 0) {
                sw_set_result(index, SWR_FAIL, rest);
            } else if (strcmp(kind, "skip") == 0) {
                sw_set_result(index, SWR_SKIP, rest);
            }
            if ((int)mode == pending) {
                pending = 0;
            }
        }
    }
    fclose(f);
    return pending;
}

static void *sw_watchdog(void *arg) {
    (void)arg;
    for (;;) {
        struct timespec ts;
        int st = g_state;

        ts.tv_sec = 1;
        ts.tv_nsec = 0;
        nanosleep(&ts, NULL);
        if (st == S_INIT || st == S_DONE || g_last_tick == 0) {
            continue;
        }
        if (sw_real_ms() - g_last_tick > g_hang_ms) {
            int idx = g_cur;

            if (idx >= 0 && (st == S_ARRIVE || st == S_DWELL || st == S_RETURN) &&
                g_res[idx] == SWR_NONE) {
                sw_fail(idx, "hang: no game frame for %lu s",
                        g_hang_ms / 1000ul);
            } else {
                SW_WARN("dw3-sweep: HUNG: no game frame for %lu s (state %d)",
                        g_hang_ms / 1000ul, st);
            }
            SW_WARN("dw3-sweep: ending the run; start the game again to "
                    "continue with the next stage");
            fflush(NULL);
            _exit(0);
        }
    }
    return NULL;
}

/* First tick: read the key and the file, report the previous run. */
static void sw_start(int level) {
    int i, pending, done = 0;

    if (level == 2) {
        char path[300];

        sw_path(path, sizeof(path));
        if (path[0] != '\0') {
            remove(path);
        }
    }
    pending = sw_load_file();
    SW_INFO("dw3-sweep: stage sweep on (key %s=%d, %d stages in the table, %d "
            "frames each, progress file %s/%s)", SW_KEY, level, NSTAGE, g_need,
            g_dir[0] != '\0' ? g_dir : "(none: not resumable)", SW_FILE);
    if (g_file_done) {
        SW_INFO("dw3-sweep: the previous sweep is complete; not running "
                "again (set %s=2 or delete %s to run it again)", SW_KEY,
                SW_FILE);
        sw_summary("previous sweep, done");
        g_state = S_DONE;
        if (getenv("DW3_STAGE_SWEEP_EXIT") != NULL) {
            fflush(NULL);
            exit(0);
        }
        return;
    }
    if (pending != 0) {
        int idx = sw_index(pending);

        if (idx >= 0 && g_res[idx] == SWR_NONE) {
            sw_fail(idx, "crash or hang in the previous run (see "
                    "crash_report.txt)");
        }
    }
    for (i = 0; i < NSTAGE; i++) {
        const char *why = sw_skip_reason(g_stages[i].mode);

        if (g_res[i] == SWR_NONE && why != NULL && sw_selected(i)) {
            sw_set_result(i, SWR_SKIP, why);
            sw_record("skip", g_stages[i].mode, "%s", why);
            SW_INFO("dw3-sweep: stage WSTAG%03d skipped: %s", g_stages[i].num,
                    why);
        }
        if (g_res[i] == SWR_OK && sw_selected(i)) {
            done++;
        }
    }
    if (done > 0) {
        SW_INFO("dw3-sweep: resuming: %d stage(s) already ok in an earlier run",
                done);
    }
    if (!g_watchdog_started) {
        pthread_t th;

        g_watchdog_started = 1;
        if (pthread_create(&th, NULL, sw_watchdog, NULL) == 0) {
            pthread_detach(th);
        }
    }
    g_state = S_GOTO_SELECT;
}

static int sw_next_index(void) {
    int i;

    for (i = 0; i < NSTAGE; i++) {
        if (g_res[i] == SWR_NONE && sw_selected(i)) {
            return i;
        }
    }
    return -1;
}

static void sw_finish(void) {
    sw_summary("done");
    sw_record("done", -1, NULL);
    g_state = S_DONE;
    if (getenv("DW3_STAGE_SWEEP_EXIT") != NULL) {
        fflush(NULL);
        exit(0);
    }
}

/* The select cannot be reached: nothing more can be told. */
static void sw_abort(int mode) {
    SW_WARN("dw3-sweep: ABORTED: the stage select (mode 0x%x) was not reached "
            "in %lu s (game is in mode 0x%x)", DW3_SWEEP_SELECT_MODE,
            SW_TIMEOUT_MS / 1000ul, mode);
    sw_summary("aborted");
    g_state = S_DONE;
    if (getenv("DW3_STAGE_SWEEP_EXIT") != NULL) {
        fflush(NULL);
        exit(1);
    }
}

/* --- public --------------------------------------------------------------- */

int dw3_sweep_enabled(void) {
    if (g_enabled < 0) {
        int level = sw_read_config();

        g_enabled = level != 0;
        if (g_enabled) {
            g_level = level; /* used by the first tick */
        }
    }
    return g_enabled;
}

int dw3_sweep_active(void) {
    return dw3_sweep_enabled() && g_state != S_DONE;
}

int dw3_sweep_tick(int mode) {
    unsigned long now;
    int idx;

    if (!dw3_sweep_enabled()) {
        return 0;
    }
    now = sw_now();
    g_last_tick = sw_real_ms();
    switch (g_state) {
    case S_INIT:
        sw_start(g_level);
        if (g_state == S_DONE) {
            return 0;
        }
        g_t_req = now;
        return mode == DW3_SWEEP_SELECT_MODE ? 0 : DW3_SWEEP_SELECT_MODE;
    case S_GOTO_SELECT:
        if (mode == DW3_SWEEP_SELECT_MODE) {
            g_state = S_SELECT_DWELL;
            g_frames = 0;
            return 0;
        }
        if (now - g_t_req > SW_TIMEOUT_MS) {
            sw_abort(mode);
        }
        return 0;
    case S_SELECT_DWELL:
        if (mode != DW3_SWEEP_SELECT_MODE) {
            g_state = S_GOTO_SELECT;
            g_t_req = now;
            return DW3_SWEEP_SELECT_MODE;
        }
        if (++g_frames < SW_SELECT_FRAMES) {
            return 0;
        }
        g_cur = sw_next_index();
        if (g_cur < 0) {
            sw_finish();
            return 0;
        }
        SW_INFO("dw3-sweep: stage WSTAG%03d (mode 0x%x) begin", g_stages[g_cur].num,
                g_stages[g_cur].mode);
        sw_record("begin", g_stages[g_cur].mode, "");
        g_state = S_ARRIVE;
        g_t_req = now;
        return g_stages[g_cur].mode;
    case S_ARRIVE:
        if (mode == g_stages[g_cur].mode) {
            g_state = S_DWELL;
            g_t_arrive = now;
            g_frames = 0;
            return 0;
        }
        if (now - g_t_req > SW_TIMEOUT_MS) {
            sw_fail(g_cur, "timeout: mode 0x%x not reached in %lu s (game is in "
                    "mode 0x%x)", g_stages[g_cur].mode, SW_TIMEOUT_MS / 1000ul,
                    mode);
            g_state = S_GOTO_SELECT;
            g_t_req = now;
            return mode == DW3_SWEEP_SELECT_MODE ? 0 : DW3_SWEEP_SELECT_MODE;
        }
        return 0;
    case S_DWELL:
        if (mode != g_stages[g_cur].mode) {
            sw_fail(g_cur, "left the stage for mode 0x%x after %d frames", mode,
                    g_frames);
            g_state = S_GOTO_SELECT;
            g_t_req = now;
            return mode == DW3_SWEEP_SELECT_MODE ? 0 : DW3_SWEEP_SELECT_MODE;
        }
        if (++g_frames >= g_need) {
            g_mem_free = sw_heap_free();
            g_state = S_RETURN;
            g_t_ret = now;
            return DW3_SWEEP_SELECT_MODE;
        }
        return 0;
    case S_RETURN:
        idx = g_cur;
        if (mode == DW3_SWEEP_SELECT_MODE) {
            unsigned long total = now - g_t_req;

            sw_set_result(idx, SWR_OK, "");
            SW_INFO("dw3-sweep: stage WSTAG%03d ok in %lu ms (frames %d, mem "
                    "free %lu, load %lu ms)", g_stages[idx].num, total,
                    g_frames, g_mem_free, g_t_arrive - g_t_req);
            sw_record("ok", g_stages[idx].mode, "%lu ms, mem free %lu", total,
                      g_mem_free);
            g_state = S_SELECT_DWELL;
            g_frames = 0;
            return 0;
        }
        if (now - g_t_ret > SW_TIMEOUT_MS) {
            sw_fail(idx, "did not return to the stage select (game is in mode "
                    "0x%x)", mode);
            g_state = S_GOTO_SELECT;
            g_t_req = now;
            return DW3_SWEEP_SELECT_MODE;
        }
        return 0;
    default:
        return 0;
    }
}

int dw3_sweep_stage_count(void) {
    return NSTAGE;
}

int dw3_sweep_stage_mode(int index) {
    return index >= 0 && index < NSTAGE ? g_stages[index].mode : 0;
}

int dw3_sweep_stage_number(int index) {
    return index >= 0 && index < NSTAGE ? g_stages[index].num : 0;
}

void dw3_sweep_set_dir(const char *dir) {
    g_dir[0] = '\0';
    if (dir != NULL && strlen(dir) < sizeof(g_dir)) {
        strcpy(g_dir, dir);
    }
}

void dw3_sweep_test_reset(void) {
    memset(g_res, 0, sizeof(g_res));
    memset(g_why, 0, sizeof(g_why));
    g_only_n = 0;
    g_enabled = -1;
    g_state = S_INIT;
    g_cur = -1;
    g_frames = 0;
    g_level = 0;
    g_file_done = 0;
    g_need = SW_DEFAULT_FRAMES;
    g_test_clock = 0;
    g_last_tick = 0;
}

void dw3_sweep_test_set_clock(unsigned long ms) {
    g_test_clock = ms;
}

void dw3_sweep_test_only(const int *modes, int n) {
    int i;

    g_only_n = 0;
    for (i = 0; modes != NULL && i < n && i < NSTAGE; i++) {
        g_only[g_only_n++] = modes[i];
    }
}

void dw3_sweep_test_enable(int on, int frames) {
    g_enabled = on != 0;
    g_level = on;
    if (frames >= 1) {
        g_need = frames;
    }
}
