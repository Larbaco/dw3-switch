/* PSY-Z present/MDEC hooks that the Switch link options wrap (P4.7).
 *
 * platforms/pc wraps LoadImage / PutDispEnv / DMACallback / DecDCTReset /
 * DecDCTout to route the P3.7 movie path through its own RGB24 conversion and
 * its own DMA table (psyz_dma_present.c). The Switch build asks for the same
 * wrapping so both hosts link the same way, but the renderer that will use it
 * does not exist yet (P4.7 Stage 3) — so for now these forward to PSY-Z's real
 * implementations, which is exactly what not wrapping would do.
 *
 * Keeping the wrappers in place means Stage 3 fills in bodies here instead of
 * changing the link options, and the movie path cannot silently diverge later.
 *
 * This TU deliberately includes only the PSY-Z headers: those redirect libc
 * names (`open` -> `psyz_open`) under __psyz, and mixing that with the libnx and
 * EGL headers is asking for trouble.
 */
#include <psyz.h>
#include <libcd.h> /* CdControl / CdInit / CdSetDebug */
#include <libgpu.h> /* RECT / DISPENV; psyz.h alone does not pull this in */

int __real_LoadImage(RECT *rect, u_long *p);
DISPENV *__real_PutDispEnv(DISPENV *env);
void *__real_DMACallback(int dma, void (*func)());
void __real_DecDCTReset(int mode);
void __real_DecDCTout(u_long *buf, int size);

/* --- TEMPORARY: boot-stop trace -------------------------------------------
 *
 * The boot reaches its fourth VSync() and stops. That VSync is the one after
 * LoadImage(), and the next thing the game does is a short straight line —
 * SetDispMask(1), CdInit(), CdSetDebug(), SetGraphDebug(), then a
 * `while (CdControl(CdlSetmode, ...) == 0)` — before the VSync(3) that would
 * print pump #5. Reading that line says every call in it returns immediately:
 * D_800B5718[CdlSetmode] is 0, so cd_cw skips Setloc, and CD_command's
 * CdlSetmode arm only fails on a NULL param. So one of them does not, and
 * guessing which from the source has already produced one wrong answer.
 *
 * These forwarders just say which is entered and what it returned; the game
 * ignores every one of these return values, so they cannot change behaviour.
 * Remove with the other boot-hang instrumentation.
 */
int __real_CdInit(void);
int __real_CdControl(u_char com, u_char* param, u_char* result);
int __real_CdSetDebug(int level);
int __real_SetGraphDebug(int level);
void __real_SetDispMask(int mask);

int __wrap_LoadImage(RECT *rect, u_long *p) {
    static int logged;
    int r = __real_LoadImage(rect, p);

    if (!logged) {
        logged = 1;
        INFOF("trace: LoadImage(%d,%d %dx%d) -> %d", rect ? rect->x : -1,
              rect ? rect->y : -1, rect ? rect->w : -1, rect ? rect->h : -1, r);
    }
    return r;
}

DISPENV *__wrap_PutDispEnv(DISPENV *env) {
    DISPENV *r = __real_PutDispEnv(env);

    INFOF("trace: PutDispEnv -> disp(%d,%d %dx%d)", r ? r->disp.x : -1,
          r ? r->disp.y : -1, r ? r->disp.w : -1, r ? r->disp.h : -1);
    return r;
}

void __wrap_SetDispMask(int mask) {
    INFOF("trace: SetDispMask(%d) ->", mask);
    __real_SetDispMask(mask);
    INFOF("trace: SetDispMask <-");
}

int __wrap_CdInit(void) {
    int r;

    INFOF("trace: CdInit ->");
    r = __real_CdInit();
    INFOF("trace: CdInit <- %d", r);
    return r;
}

int __wrap_CdSetDebug(int level) {
    int r = __real_CdSetDebug(level);

    INFOF("trace: CdSetDebug(%d) -> %d", level, r);
    return r;
}

int __wrap_SetGraphDebug(int level) {
    int r = __real_SetGraphDebug(level);

    INFOF("trace: SetGraphDebug(%d) -> %d", level, r);
    return r;
}

/* The busy-wait is the reason for the power-of-two ratchet: if this is the
 * call that never satisfies the loop, it is called millions of times, and
 * every log line is a syscall and a file write. */
int __wrap_CdControl(u_char com, u_char* param, u_char* result) {
    static unsigned calls;
    int r;

    calls++;
    if (calls <= 8 || (calls & (calls - 1)) == 0) {
        INFOF("trace: CdControl(com=%02X) call #%u ->", com, calls);
    }
    r = __real_CdControl(com, param, result);
    if (calls <= 8 || (calls & (calls - 1)) == 0) {
        INFOF("trace: CdControl(com=%02X) call #%u <- %d", com, calls, r);
    }
    return r;
}

void *__wrap_DMACallback(int dma, void (*func)()) {
    return __real_DMACallback(dma, func);
}

void __wrap_DecDCTReset(int mode) { __real_DecDCTReset(mode); }

void __wrap_DecDCTout(u_long *buf, int size) { __real_DecDCTout(buf, size); }
