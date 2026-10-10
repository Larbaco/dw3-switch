/* P4.1 sketch — native present (PSP: psp_gpu.c).
 * Target: EGL + OpenGL ES (mesa/nouveau) or deko3d; see docs/SWITCH_BACKEND.md.
 * Not linked yet (P4.2). */
#include "nx.h"

int dw3_nx_backend_sketch_version(void) {
    return 1;
}

#if 0 /* future: InitPlatform / ResetPlatform / Draw_* / Psyz_Video* */
bool InitPlatform(void);
void ResetPlatform(void);
#endif
