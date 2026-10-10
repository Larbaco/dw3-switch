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

/* Kept in sync with DW3_DEFAULT_RUNTIME in source/main.c. */
#define DW3_NX_SD_DIR "sdmc:/switch/dw3"

bool dw3_nx_fs_mount_sd(void) {
    Result rc = fsdevMountSdmc();

    if (R_FAILED(rc)) {
        return false;
    }
    /* EEXIST is the normal case on a second run. */
    if (mkdir(DW3_NX_SD_DIR, 0777) != 0 && errno != EEXIST) {
        return false;
    }
    return true;
}
