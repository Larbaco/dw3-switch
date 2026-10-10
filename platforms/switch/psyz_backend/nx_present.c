/* PSY-Z present hooks that the Switch link options wrap (P4.7).
 *
 * platforms/pc wraps LoadImage/PutDispEnv to route the P3.7 movie path through
 * its own RGB24 conversion (psyz_dma_present.c). The Switch build asks for the
 * same wrapping so both hosts link the same way, but the renderer that will use
 * it does not exist yet (P4.7 Stage 3) — so for now these forward to PSY-Z's
 * real implementations, which is exactly what not wrapping would do.
 *
 * Keeping the wrapper in place means Stage 3 fills in bodies here instead of
 * changing the link options, and the movie path cannot silently diverge later.
 *
 * This TU deliberately includes only the PSY-Z headers: those redirect libc
 * names (`open` -> `psyz_open`) under __psyz, and mixing that with the libnx and
 * EGL headers is asking for trouble.
 */
#include <psyz.h>
#include <libgpu.h> /* RECT / DISPENV; psyz.h alone does not pull this in */

int __real_LoadImage(RECT *rect, u_long *p);
DISPENV *__real_PutDispEnv(DISPENV *env);

int __wrap_LoadImage(RECT *rect, u_long *p) { return __real_LoadImage(rect, p); }

DISPENV *__wrap_PutDispEnv(DISPENV *env) { return __real_PutDispEnv(env); }
