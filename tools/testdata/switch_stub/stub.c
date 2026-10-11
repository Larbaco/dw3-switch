/* libnx stand-in implementations for the P4.5 host harness (see switch.h). */
#include "switch.h"

#include <stdarg.h>
#include <stdint.h>

static Result s_mount_result = 0;
static bool s_device_present = true;
static Result s_thread_start_result = 0;
static int s_thread_close_count;
static int s_thread_create_count;
static int s_handle_live;

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

uint64_t armGetSystemTick(void) { return 0; }
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

uintptr_t threadGetCurHandle(void) { return 1; }

void mutexInit(Mutex *m) { m->locked = 0; }
void mutexLock(Mutex *m) { m->locked = 1; }
void mutexUnlock(Mutex *m) { m->locked = 0; }

Result eventCreate(Event *e, bool autoclear) {
    (void)autoclear;
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
}
void stub_set_mount_result(Result rc) { s_mount_result = rc; }
void stub_set_device_present(bool present) { s_device_present = present; }
void stub_set_thread_start_result(Result rc) { s_thread_start_result = rc; }
int stub_thread_close_count(void) { return s_thread_close_count; }
int stub_thread_create_count(void) { return s_thread_create_count; }
int stub_is_handle_live(void) { return s_handle_live; }
