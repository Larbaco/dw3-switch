/* PSY-Z host primitives for the Switch (P4.5): clock, threads, mutex, bell.
 *
 * Contract: vendor/psyz/psyz/src/psyz/os.h. Template is the PSP backend
 * (psyz/src/psp/psp_os.c), which libnx maps onto cleanly:
 *
 *   clock   -> armGetSystemTick() / armTicksToNs
 *   threads -> threadCreate / threadStart
 *   lock    -> Mutex
 *   bell    -> auto-reset signal on a Mutex + CondVar (no kernel event: the
 *              process event limit is already used up when PSY-Z starts,
 *              OBS-7, so svcCreateEvent failed with rc 0x10801 on the OLED)
 */
#include "nx.h"
#include "nx_exit.h"

#include <stdlib.h>

#include <switch.h>

/* psyz_log is declared in nx.h. Do not include <psyz/log.h>: the Makefile
 * host_hello sample compiles this file and has no PSY-Z include path.
 * Levels match psyz/log.h: I = 1, E = 3. */
#define DW3_NX_INFO(...) psyz_log(1, __FILE__, __LINE__, __func__, __VA_ARGS__)
#define DW3_NX_ERROR(...) psyz_log(3, __FILE__, __LINE__, __func__, __VA_ARGS__)

/* Mirrors PSYZ_OS_FOREVER from vendor/psyz/psyz/src/psyz/os.h, which is a
 * PSY-Z-private header and deliberately not on the include path. */
#define DW3_NX_OS_FOREVER UINT32_MAX

/* PSY-Z runs its scheduler helpers above the game's own thread, as the PSP
 * backend does (psp_os.c keeps 0x18 against the game's 0x20). libnx priorities
 * run the other way — lower wins — and nx_audio.c puts the audio thread at
 * 0x20, so 0x1C is "above the game, below audio". */
#define DW3_NX_OS_THREAD_PRIO 0x1C
#define DW3_NX_OS_THREAD_STACK 0x10000 /* matches nx_audio.c */

/* See Psyz_OsBellWait: boot-hang instrumentation, remove once understood. */
static uint64_t s_bell_waits;
static uint64_t s_bell_rings;

/* A PSY-Z bell: an auto-reset signal ("a ring wakes one waiter, or the next
 * one to wait"). It is a flag under a mutex plus a condition variable, which
 * libnx implements with svcWaitProcessWideKeyAtomic and which needs no kernel
 * event object: svcCreateEvent reserves from the process event limit, and on
 * the OLED (hbloader, p56 logs run2 to run13) that limit was already used up
 * when KernelInit ran, so both eventCreate calls failed with 0x10801
 * (kernel LimitReached) and PSY-Z ran with NULL bells (OBS-7). */
typedef struct Bell {
    Mutex lock;
    CondVar cv;
    int signaled;
} Bell;

/* BUG-1: the threads PSY-Z starts (its IRQ loop) never return by themselves.
 * They are recorded so the exit path can ask them to leave, join them and
 * threadClose() them: a finished-but-unclosed libnx thread keeps its stack
 * pages borrowed (Perm_None) in the heap the loader reads the next NRO over.
 * The bells are recorded to wake a thread parked in an infinite wait. */
#define DW3_NX_OS_MAX_THREADS 4
#define DW3_NX_OS_MAX_BELLS 8
static Thread *s_os_threads[DW3_NX_OS_MAX_THREADS];
static const char *s_os_names[DW3_NX_OS_MAX_THREADS];
static int s_os_thread_count;
static struct Bell *s_os_bells[DW3_NX_OS_MAX_BELLS];
static int s_os_bell_count;
static volatile int s_os_stop;

/* An OS thread that finds the stop flag set leaves instead of waiting again. */
static void os_thread_leave_if_stopping(void) {
    int i;

    if (!s_os_stop) {
        return;
    }
    for (i = 0; i < s_os_thread_count; i++) {
        if (s_os_threads[i] != NULL &&
            s_os_threads[i]->handle == threadGetCurHandle()) {
            threadExit();
        }
    }
}

uint32_t Psyz_OsNowUs(void) {
    /* One-shot: is the emulated clock being used at all before the hang? */
    static int logged;

    if (!logged) {
        logged = 1;
        DW3_NX_INFO("OsNowUs first call");
    }
    /* 32-bit microseconds, wrapping like the PSP's sceKernelGetSystemTimeLow;
     * callers compare two readings through their signed difference. */
    return (uint32_t)(armTicksToNs(armGetSystemTick()) / 1000ULL);
}

uintptr_t Psyz_OsThreadSelf(void) {
    uintptr_t id = (uintptr_t)threadGetCurHandle();
    /* The contract says "never 0": PSY-Z uses this as a lookup key. */
    return id ? id : (uintptr_t)1;
}

void *Psyz_OsLockCreate(void) {
    Mutex *m = malloc(sizeof(*m));
    if (!m) {
        /* The KernelInit aggregate warning does not say which object failed;
         * name it here so the next hardware log distinguishes OOM from the
         * event path. See docs/NX_KERNEL_SYNC_OBJECTS.md. */
        DW3_NX_ERROR("OsLockCreate: malloc failed (post_lock/kernel_lock)");
        return NULL;
    }
    mutexInit(m);
    return m;
}

void Psyz_OsLock(void *lock) {
    if (lock) {
        mutexLock((Mutex *)lock);
    }
}

void Psyz_OsUnlock(void *lock) {
    if (lock) {
        mutexUnlock((Mutex *)lock);
    }
}

void *Psyz_OsBellCreate(void) {
    static int logged;
    Bell *b = malloc(sizeof(*b));

    if (!b) {
        DW3_NX_ERROR("OsBellCreate: malloc failed (bell_events/bell_irq)");
        return NULL;
    }
    mutexInit(&b->lock);
    condvarInit(&b->cv);
    b->signaled = 0;
    if (!logged) {
        logged = 1;
        DW3_NX_INFO("OsBellCreate: bells are a mutex + condvar signal (no "
                    "kernel event, so the process event limit does not matter)");
    }
    if (s_os_bell_count < DW3_NX_OS_MAX_BELLS) {
        s_os_bells[s_os_bell_count++] = b;
    }
    return b;
}

/* Sets the signal and wakes one waiter. */
static void bell_ring(Bell *b) {
    mutexLock(&b->lock);
    b->signaled = 1;
    condvarWakeOne(&b->cv);
    mutexUnlock(&b->lock);
}

void Psyz_OsBellRing(void *bell) {
    s_bell_rings++;
    if (bell) {
        bell_ring((Bell *)bell);
    }
}

void Psyz_OsBellWait(void *bell, uint32_t timeout_us) {
    /* Instrumentation while the boot hangs before its first frame: PSY-Z's
     * kernel event loop is the prime suspect, and its shape (how often, with
     * which timeout, and whether anything ever rings) distinguishes "the poll
     * loop is spinning" from "the IRQ thread never signals".
     *
     * Logged at powers of two so a runaway loop shows up logarithmically
     * instead of flooding — every line here is a file write. Remove once the
     * boot is understood. */
    uint64_t n = ++s_bell_waits;
    Bell *b = (Bell *)bell;
    uint64_t deadline = 0;
    int forever = (timeout_us == DW3_NX_OS_FOREVER);

    if (n <= 8 || (n & (n - 1)) == 0) {
        DW3_NX_INFO("bell wait #%llu timeout_us=%u rings=%llu",
              (unsigned long long)n, timeout_us,
              (unsigned long long)s_bell_rings);
    }

    os_thread_leave_if_stopping();
    if (!b) {
        return;
    }
    /* UINT32_MAX microseconds is both the documented "forever" and the point
     * where the 32-bit microsecond clock wraps. */
    if (!forever) {
        deadline = armTicksToNs(armGetSystemTick()) +
                   (uint64_t)timeout_us * 1000ULL;
    }
    mutexLock(&b->lock);
    while (!b->signaled) {
        if (forever) {
            condvarWait(&b->cv, &b->lock);
        } else {
            uint64_t now = armTicksToNs(armGetSystemTick());

            if (now >= deadline) {
                break;
            }
            condvarWaitTimeout(&b->cv, &b->lock, deadline - now);
        }
        /* a stop request rings every bell: do not go back to sleep on it */
        if (s_os_stop) {
            break;
        }
    }
    b->signaled = 0; /* auto-reset: the ring (or the timeout) is consumed */
    mutexUnlock(&b->lock);
    os_thread_leave_if_stopping();
}

static void thread_trampoline(void *arg) {
    (*(void (**)(void))arg)();
}

/* The game's loop polls this to notice that it should stop. The PSP backend
 * reads a flag its exit callback sets.
 *
 * On Switch, boot_main's vsync pump drains applet messages with
 * appletMainLoop() (non-blocking in current libnx: appletGetMessage uses
 * eventWait timeout 0) and calls dw3_nx_set_quit_requested() on HOME /
 * ExitRequested. Do not call appletMainLoop from here — the host owns the
 * drain so it can flush flat saves in the same path (P5.3). */
static volatile int g_quit_requested;

void dw3_nx_set_quit_requested(void) { g_quit_requested = 1; }

int Psyz_QuitRequested(void) { return g_quit_requested; }

int Psyz_OsThreadStart(void (*fn)(void), const char *name) {
    /* PSY-Z hands over a bare function pointer, so the Thread object and the
     * argument slot have to outlive this call. Both are leaked on purpose:
     * these threads live as long as the process. */
    Thread *t = malloc(sizeof(*t));
    void **slot = malloc(sizeof(*slot));

    if (!t || !slot) {
        free(t);
        free(slot);
        DW3_NX_ERROR("thread %s: out of memory", name);
        return -1;
    }
    *slot = (void *)fn;

    if (R_FAILED(threadCreate(t, thread_trampoline, slot, NULL,
                              DW3_NX_OS_THREAD_STACK, DW3_NX_OS_THREAD_PRIO,
                              -2))) {
        free(t);
        free(slot);
        DW3_NX_ERROR("threadCreate(%s) failed", name);
        return -1;
    }
    if (R_FAILED(threadStart(t))) {
        /* threadCreate() succeeded, so *t is a live libnx thread handle. Freeing
         * it here (as the threadCreate failure path does) would leave the kernel
         * a pointer into freed memory; the handle has to be closed first. The
         * thread never ran, so the trampoline argument slot can go with it. */
        threadClose(t);
        free(t);
        free(slot);
        DW3_NX_ERROR("threadStart(%s) failed", name);
        return -1;
    }
    /* PSY-Z picks its kernel poll cadence from whether this succeeded
     * (kernel.c: irq_thread_state < 0 -> 1 ms waits, else 100 ms), so knowing
     * it here separates "the poll loop spins" from "the IRQ thread never
     * signals". */
    DW3_NX_INFO("thread started: %s", name);
    if (s_os_thread_count < DW3_NX_OS_MAX_THREADS) {
        s_os_names[s_os_thread_count] = name;
        s_os_threads[s_os_thread_count++] = t;
    }
    return 0;
}

/* BUG-1: ask every thread PSY-Z started to leave, wake it, join and close it.
 * Returns how many were still running after the exit budget. */
int dw3_nx_os_threads_shutdown(void) {
    int i, still = 0;

    if (s_os_thread_count == 0) {
        dw3_nx_exit_log("thread psyz os: none started (nothing to close)");
        return 0;
    }
    s_os_stop = 1;
    for (i = 0; i < s_os_bell_count; i++) {
        bell_ring(s_os_bells[i]);
    }
    for (i = 0; i < s_os_thread_count; i++) {
        still += dw3_nx_exit_join(s_os_threads[i], s_os_names[i]);
    }
    s_os_thread_count = 0;
    return still;
}
