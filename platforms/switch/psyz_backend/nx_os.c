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
    if (!ev) {
        return NULL;
    }
    /* autoclear gives exactly the documented "auto-reset signal: a ring wakes
     * one waiter, or the next one to wait". */
    if (R_FAILED(eventCreate(ev, true))) {
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
 * Do NOT reach for appletMainLoop() here, which is the obvious-looking answer:
 * it wraps appletGetMessage, which blocks until the applet sends something, and
 * the game polls this from inside its frame loop — the first call hangs. The
 * libnx equivalent of the PSP exit callback is an applet hook
 * (appletSetHook with AppletHookType_OnExitRequest) setting a flag; until that
 * is wired, report "no quit requested" so the game keeps running.
 *
 * Consequence, and it is a real one: the home button's exit request is ignored.
 * Tracked with the other exit/sleep work in docs/ROADMAP.md P5.3. */
int Psyz_QuitRequested(void) { return 0; }

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
