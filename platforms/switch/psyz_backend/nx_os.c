/* PSY-Z host primitives for the Switch (P4.5): clock, threads, mutex, bell.
 *
 * Contract: vendor/psyz/psyz/src/psyz/os.h. Template is the PSP backend
 * (psyz/src/psp/psp_os.c), which libnx maps onto cleanly:
 *
 *   clock   -> armGetSystemTick() / armTicksToNs
 *   threads -> threadCreate / threadStart
 *   lock    -> Mutex
 *   bell    -> auto-reset Event (eventCreate(.., autoclear = true))
 */
#include "nx.h"

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
    Event *ev = malloc(sizeof(*ev));
    Result rc;

    if (!ev) {
        DW3_NX_ERROR("OsBellCreate: malloc failed (bell_events/bell_irq)");
        return NULL;
    }
    /* autoclear gives exactly the documented "auto-reset signal: a ring wakes
     * one waiter, or the next one to wait". */
    rc = eventCreate(ev, true);
    if (R_FAILED(rc)) {
        /* KernelInit's aggregate warning hides the failing object and the rc;
         * svcCreateEvent reserves from the process event resource limit, so a
         * limit/OUT-OF-RESOURCE code here is the probable cause of
         * "cannot create the kernel synchronisation objects". See
         * docs/NX_KERNEL_SYNC_OBJECTS.md. */
        DW3_NX_ERROR("OsBellCreate: eventCreate failed rc=0x%x", rc);
        free(ev);
        return NULL;
    }
    return ev;
}

void Psyz_OsBellRing(void *bell) {
    s_bell_rings++;
    if (bell) {
        eventFire((Event *)bell);
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
    uint64_t ns;

    if (n <= 8 || (n & (n - 1)) == 0) {
        DW3_NX_INFO("bell wait #%llu timeout_us=%u rings=%llu",
              (unsigned long long)n, timeout_us,
              (unsigned long long)s_bell_rings);
    }

    if (!bell) {
        return;
    }
    /* UINT32_MAX microseconds is both the documented "forever" and the point
     * where the 32-bit microsecond clock wraps. */
    ns = (timeout_us == DW3_NX_OS_FOREVER) ? UINT64_MAX
                                          : (uint64_t)timeout_us * 1000ULL;
    eventWait((Event *)bell, ns);
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
    return 0;
}
