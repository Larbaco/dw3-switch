/* Switch filesystem bring-up (P4.5): mount the SD and make sure the runtime
 * directory exists.
 *
 * The port's file reading is plain stdio — port/cdfs, boot_tim_host and
 * memcard_host all fopen() their way through the runtime tree — so mounting the
 * SD device under `sdmc:` is the whole job. Nothing here reimplements PSY-Z's
 * file group (psyz_open / my_firstfile / ...): the aarch64 link resolves
 * without it, which means nothing in the linked image calls those. If that
 * changes, the linker will say so rather than us guessing.
 */
#include "nx.h"

#include <errno.h>
#include <sys/stat.h>

#include <switch.h>

/* Parent of the version root that source/boot_main.c resolves as
 * DW3_DEFAULT_RUNTIME (sdmc:/switch/dw3/<ver>/); see docs/RUNTIME_LAYOUT.md. */
#define DW3_NX_SD_DIR "sdmc:/switch/dw3"

bool dw3_nx_fs_mount_sd(void) {
    Result rc = fsdevMountSdmc();

    /* fsdevMountSdmc() "initializes and mounts the sdmc device if accessible",
     * and mounting a device that is already mounted fails. That is not a reason
     * to report the runtime root unreachable: hbmenu, another homebrew or a
     * re-entry can have mounted sdmc: first, and then the paths resolve fine.
     * The devoptab is the source of truth, not the Result. */
    if (R_FAILED(rc) && fsdevGetDeviceFileSystem("sdmc") == NULL) {
        return false;
    }
    /* EEXIST is the normal case on a second run. */
    if (mkdir(DW3_NX_SD_DIR, 0777) != 0 && errno != EEXIST) {
        return false;
    }
    return true;
}
