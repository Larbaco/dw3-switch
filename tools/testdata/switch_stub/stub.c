/* libnx stand-in implementations for the P4.5 host harness (see switch.h). */
#define _POSIX_C_SOURCE 200809L
#include "switch.h"

#include <time.h>

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

static Result s_mount_result = 0;
static bool s_device_present = true;
static Result s_thread_start_result = 0;
static int s_thread_close_count;
static int s_thread_create_count;
static int s_handle_live;
static int s_event_create_count;

/* psyz_log: nx_os.c logs through it (declared in nx.h). */
void psyz_log(unsigned int level, const char *file, unsigned int line,
              const char *func, const char *fmt, ...) {
    (void)level;
    (void)file;
    (void)line;
    (void)func;
    (void)fmt;
}

Result fsdevMountSdmc(void) { return s_mount_result; }

FsFileSystem *fsdevGetDeviceFileSystem(const char *name) {
    (void)name;
    return s_device_present ? (FsFileSystem *)&s_handle_live : NULL;
}

/* One tick is one nanosecond of the monotonic clock: the bell's deadline maths
 * needs a clock that moves. */
uint64_t armGetSystemTick(void) {
    struct timespec ts;

    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ULL + (uint64_t)ts.tv_nsec;
}
uint64_t armTicksToNs(uint64_t ticks) { return ticks; }

Result threadCreate(Thread *t, ThreadFunc entry, void *arg, void *stack_mem,
                    size_t stack_sz, int prio, int cpuid) {
    (void)entry;
    (void)arg;
    (void)stack_mem;
    (void)stack_sz;
    (void)prio;
    (void)cpuid;
    s_thread_create_count++;
    s_handle_live = 1;
    t->handle = 1;
    return 0;
}

Result threadStart(Thread *t) {
    (void)t;
    return s_thread_start_result;
}

Result threadClose(Thread *t) {
    (void)t;
    s_thread_close_count++;
    s_handle_live = 0;
    return 0;
}

Handle threadGetCurHandle(void) { return 1; }

void threadExit(void) { exit(0); }

/* nx_exit.c is not compiled in this harness; nx_os.c only needs these two. */
void dw3_nx_exit_log(const char *fmt, ...) {
    va_list ap;

    va_start(ap, fmt);
    vprintf(fmt, ap);
    va_end(ap);
    printf("\n");
}

int dw3_nx_exit_join(Thread *t, const char *name) {
    (void)name;
    threadClose(t);
    return 0;
}

void mutexInit(Mutex *m) { pthread_mutex_init(&m->m, NULL); }
void mutexLock(Mutex *m) { pthread_mutex_lock(&m->m); }
void mutexUnlock(Mutex *m) { pthread_mutex_unlock(&m->m); }

void condvarInit(CondVar *c) {
    pthread_condattr_t attr;

    pthread_condattr_init(&attr);
    pthread_condattr_setclock(&attr, CLOCK_MONOTONIC);
    pthread_cond_init(&c->c, &attr);
    pthread_condattr_destroy(&attr);
}
Result condvarWait(CondVar *c, Mutex *m) {
    pthread_cond_wait(&c->c, &m->m);
    return 0;
}
Result condvarWaitTimeout(CondVar *c, Mutex *m, uint64_t timeout_ns) {
    struct timespec ts;
    uint64_t t;

    clock_gettime(CLOCK_MONOTONIC, &ts);
    t = (uint64_t)ts.tv_nsec + timeout_ns % 1000000000ULL;
    ts.tv_sec += (time_t)(timeout_ns / 1000000000ULL + t / 1000000000ULL);
    ts.tv_nsec = (long)(t % 1000000000ULL);
    return pthread_cond_timedwait(&c->c, &m->m, &ts) == 0 ? 0 : 1;
}
Result condvarWakeOne(CondVar *c) {
    pthread_cond_signal(&c->c);
    return 0;
}

Result eventCreate(Event *e, bool autoclear) {
    (void)autoclear;
    s_event_create_count++;
    e->fired = 0;
    return 0;
}
void eventFire(Event *e) { e->fired = 1; }
bool eventWait(Event *e, uint64_t timeout_ns) {
    (void)e;
    (void)timeout_ns;
    return true;
}

void stub_reset(void) {
    s_mount_result = 0;
    s_device_present = true;
    s_thread_start_result = 0;
    s_thread_close_count = 0;
    s_thread_create_count = 0;
    s_handle_live = 0;
    s_event_create_count = 0;
}
void stub_set_mount_result(Result rc) { s_mount_result = rc; }
void stub_set_device_present(bool present) { s_device_present = present; }
void stub_set_thread_start_result(Result rc) { s_thread_start_result = rc; }
int stub_thread_close_count(void) { return s_thread_close_count; }
int stub_thread_create_count(void) { return s_thread_create_count; }
int stub_is_handle_live(void) { return s_handle_live; }
int stub_event_create_count(void) { return s_event_create_count; }
