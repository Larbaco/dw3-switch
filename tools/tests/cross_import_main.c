/* P6.25 host test driver: opens both ports of a memcard folder with the
 * cross-region import wired the way the hosts do it, and prints what the
 * ports hold.
 *
 *   cross_import_main explicit <own_dir> <import_dir> us|eu
 *       set_dir + set_cross_import(import_dir)
 *   cross_import_main sibling <runtime_root> us|eu
 *       memcard dir = <root>/memcard, import from the sibling region folder
 *   cross_import_main env <own_dir> <runtime_root> us|eu <explicit 0|1>
 *       memcard_host_setup_cross_import (reads the DW3_* environment)
 *   cross_import_main sibling-rc <runtime_root> us|eu
 *       only prints the return code of the sibling setup
 * Output: "port N has=0|1 name=<sidecar name>" per port.
 */
#include "memcard_host.h"
#include "save_convert.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int region_arg(const char *s) {
    return strcmp(s, "us") == 0 ? SAVE_REGION_US
                                : (strcmp(s, "eu") == 0 ? SAVE_REGION_EU : 0);
}

int main(int argc, char **argv) {
    char dir[600];
    int slot;
    int region;

    if (argc == 5 && strcmp(argv[1], "explicit") == 0) {
        region = region_arg(argv[4]);
        if (memcard_host_set_dir(argv[2]) != 0 ||
            memcard_host_set_cross_import(argv[3], region) != 0) {
            return 2;
        }
    } else if (argc == 4 && strcmp(argv[1], "sibling") == 0) {
        region = region_arg(argv[3]);
        snprintf(dir, sizeof(dir), "%s/memcard", argv[2]);
        if (memcard_host_set_dir(dir) != 0 ||
            memcard_host_set_cross_import_sibling(argv[2], region) != 0) {
            return 2;
        }
    } else if (argc == 4 && strcmp(argv[1], "sibling-rc") == 0) {
        region = region_arg(argv[3]);
        printf("rc=%d\n", memcard_host_set_cross_import_sibling(argv[2], region));
        return 0;
    } else if (argc == 6 && strcmp(argv[1], "env") == 0) {
        region = region_arg(argv[4]);
        if (memcard_host_set_dir(argv[2]) != 0) {
            return 2;
        }
        printf("setup=%d\n", memcard_host_setup_cross_import(
                                 argv[3], region, atoi(argv[5])));
    } else {
        fprintf(stderr, "usage: see source\n");
        return 1;
    }
    for (slot = 0; slot < MEMCARD_HOST_SLOTS; slot++) {
        int rc = memcard_host_ensure(slot);

        printf("port %d ensure=%d has=%d name=%s\n", slot, rc,
               memcard_host_has_file(slot), memcard_host_file_name(slot));
    }
    memcard_host_shutdown();
    return 0;
}
