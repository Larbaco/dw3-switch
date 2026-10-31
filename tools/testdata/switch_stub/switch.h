/* Minimal libnx stand-in so the Switch backend units (nx_file.c, nx_os.c) can
 * be compiled and exercised on the host (P4.5 tests).
 *
 * This is test scaffolding only: it mirrors just the libnx surface those two
 * units touch, plus a few `stub_*` controls the harness uses to drive results.
 * It is never part of a console build. */
#ifndef DW3_TEST_SWITCH_STUB_H
#define DW3_TEST_SWITCH_STUB_H

#include <pthread.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef uint32_t Result;
#define R_FAILED(res) ((res) != 0)

/* --- filesystem (nx_file.c) ------------------------------------------- */

typedef struct FsFileSystem FsFileSystem;

Result fsdevMountSdmc(void);
FsFileSystem *fsdevGetDeviceFileSystem(const char *name);

/* --- clock (nx_os.c) --------------------------------------------------- */

uint64_t armGetSystemTick(void);
uint64_t armTicksToNs(uint64_t ticks);

/* --- threads (nx_os.c) ------------------------------------------------- */

typedef void (*ThreadFunc)(void *arg);
typedef uint32_t Handle; /* libnx: u32 */
typedef struct Thread { Handle handle; } Thread;
/* a real mutex and condition variable: the bell test (OBS-7) waits across threads */
typedef struct Mutex { pthread_mutex_t m; } Mutex;
typedef struct CondVar { pthread_cond_t c; } CondVar;
typedef struct Event { int fired; } Event;

Result threadCreate(Thread *t, ThreadFunc entry, void *arg, void *stack_mem,
                    size_t stack_sz, int prio, int cpuid);
Result threadStart(Thread *t);
Result threadClose(Thread *t);
Handle threadGetCurHandle(void);
void threadExit(void) __attribute__((noreturn));

void mutexInit(Mutex *m);
void mutexLock(Mutex *m);
void mutexUnlock(Mutex *m);

void condvarInit(CondVar *c);
Result condvarWait(CondVar *c, Mutex *m);
Result condvarWaitTimeout(CondVar *c, Mutex *m, uint64_t timeout_ns);
Result condvarWakeOne(CondVar *c);

Result eventCreate(Event *e, bool autoclear);
void eventFire(Event *e);
bool eventWait(Event *e, uint64_t timeout_ns);

/* --- stub controls (harness only) -------------------------------------- */

void stub_reset(void);
void stub_set_mount_result(Result rc);      /* fsdevMountSdmc() return */
void stub_set_device_present(bool present); /* fsdevGetDeviceFileSystem() */
void stub_set_thread_start_result(Result rc);
int stub_thread_close_count(void);
int stub_thread_create_count(void);
int stub_is_handle_live(void); /* a created handle that was never closed */
int stub_event_create_count(void); /* kernel events created (the bells must not need any) */

#endif /* DW3_TEST_SWITCH_STUB_H */
