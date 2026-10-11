/*
 * Switch module host (P4.6) — intentionally no module backend.
 *
 * PSY-Z's module API (`Psyz_ModuleOpen` / `Psyz_ModuleClose`,
 * vendor/psyz/psyz/include/psyz/module.h) exists to load PS1 overlays as
 * dynamic objects (.so / .dll / .prx) at runtime. The Switch port does not
 * need it, and libnx has no `dlopen` to implement it with:
 *
 *  - P2.6 links every overlay into the one game image. "Loading" an overlay
 *    is `dw3_overlay_restore()` (port/overlays): restore the overlay's
 *    initial `.data` and zero its `.bss`, but only when the tracked id
 *    changes — the PS1 `memcpy`-skip semantics.
 *  - The decomp's `loadModeOverlay` / `loadSubOverlay`
 *    (src/engine/system/overlay.c) reach that path through
 *    `port/patches/0008-PORT_HOST-static-overlay-restore.patch`. Nothing in
 *    the decomp, `port/`, `tools/` or the linked game image ever calls
 *    `Psyz_Module*`.
 *  - PSY-Z's own `CMAKE_SYSTEM_NAME STREQUAL "NintendoSwitch"` branch
 *    (vendor/psyz/psyz/CMakeLists.txt) compiles no module source, with the
 *    same "no SDL and no dlopen" reason.
 *
 * Evidence and open hardware criteria: docs/P4.6_MODULES_STATUS.md.
 *
 * Deliberately no `#include <psyz/module.h>` here: the devkitPro host_hello
 * sample (source/main.c) also compiles the psyz_backend units and has no
 * PSY-Z include path.
 */

/* Keep the translation unit non-empty without exporting any symbol. */
typedef int dw3_nx_module_unused_tu;
