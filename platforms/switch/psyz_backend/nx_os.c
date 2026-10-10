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

/* Mirrors PSYZ_OS_FOREVER from vendor/psyz/psyz/src/psyz/os.h, which is a
 * PSY-Z-private header and deliberately not on the include path. */
#define DW3_NX_OS_FOREVER UINT32_MAX

/* PSY-Z runs its scheduler helpers above the game's own thread, as the PSP
 * backend does (psp_os.c keeps 0x18 against the game's 0x20). libnx priorities
 * run the other way — lower wins — and nx_audio.c puts the audio thread at
 * 0x20, so 0x1C is "above the game, below audio". */
#define DW3_NX_OS_THREAD_PRIO 0x1C
#define DW3_NX_OS_THREAD_STACK 0x10000 /* matches nx_audio.c */

uint32_t Psyz_OsNowUs(void) {
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
    if (bell) {
        eventFire((Event *)bell);
    }
}

void Psyz_OsBellWait(void *bell, uint32_t timeout_us) {
    uint64_t ns;

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

int Psyz_OsThreadStart(void (*fn)(void), const char *name) {
    /* PSY-Z hands over a bare function pointer, so the Thread object and the
     * argument slot have to outlive this call. Both are leaked on purpose:
     * these threads live as long as the process. */
    Thread *t = malloc(sizeof(*t));
    void **slot = malloc(sizeof(*slot));
    (void)name;

    if (!t || !slot) {
        free(t);
        free(slot);
        return -1;
    }
    *slot = (void *)fn;

    if (R_FAILED(threadCreate(t, thread_trampoline, slot, NULL,
                              DW3_NX_OS_THREAD_STACK, DW3_NX_OS_THREAD_PRIO,
                              -2))) {
        free(t);
        free(slot);
        return -1;
    }
    if (R_FAILED(threadStart(t))) {
        return -1;
    }
    return 0;
}
