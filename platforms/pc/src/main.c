/*
 * PC host bootstrap (P2.8): init cdfs + memcard + overlays, load boot TIM,
 * then enter the decomp game loop (dw3_game_main).
 */
#include <psyz.h>

#include "boot_tim_host.h"
#include "cdfs.h"
#include "memcard_host.h"
#include "overlay_host.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int dw3_game_main(void);

static const char *env_or(const char *key, const char *fallback) {
    const char *v = getenv(key);
    return (v != NULL && v[0] != '\0') ? v : fallback;
}

static int init_paths(int argc, char **argv) {
    const char *root = env_or("DW3_RUNTIME", "runtime/us");
    const char *memdir = env_or("DW3_MEMCARD_DIR", NULL);
    char memcard_buf[512];
    char boot_path[512];
    int i;

    for (i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--runtime") == 0 && i + 1 < argc) {
            root = argv[++i];
        } else if (strcmp(argv[i], "--memcard-dir") == 0 && i + 1 < argc) {
            memdir = argv[++i];
        }
    }

    if (cdfs_set_root(root) != 0) {
        fprintf(stderr, "dw3-pc: cdfs_set_root(%s) failed\n", root);
        return -1;
    }
    if (cdfs_load_map(NULL) != 0) {
        fprintf(stderr, "dw3-pc: cdfs_load_map under %s failed\n", root);
        return -1;
    }
    fprintf(stderr, "dw3-pc: cdfs root=%s\n", root);

    if (memdir == NULL) {
        snprintf(memcard_buf, sizeof(memcard_buf), "%s/memcard", root);
        memdir = memcard_buf;
    }
    if (memcard_host_set_dir(memdir) != 0) {
        fprintf(stderr, "dw3-pc: memcard_host_set_dir(%s) failed\n", memdir);
        return -1;
    }
    if (memcard_host_ensure(0) != 0 || memcard_host_ensure(1) != 0) {
        fprintf(stderr, "dw3-pc: memcard ensure failed under %s\n", memdir);
        return -1;
    }
    fprintf(stderr, "dw3-pc: memcard dir=%s\n", memdir);

    snprintf(boot_path, sizeof(boot_path), "%s/boot_tim.bin", root);
    if (dw3_host_load_boot_tim(boot_path) != 0) {
        fprintf(stderr, "dw3-pc: boot TIM load failed (%s)\n", boot_path);
        return -1;
    }
    fprintf(stderr, "dw3-pc: boot TIM loaded from %s\n", boot_path);

    dw3_overlay_register_us_title();
    return 0;
}

int main(int argc, char **argv) {
    if (init_paths(argc, argv) != 0) {
        return 1;
    }
    fprintf(stderr, "dw3-pc: entering dw3_game_main()\n");
    return dw3_game_main();
}
