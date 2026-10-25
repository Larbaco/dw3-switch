/* P10.2 interrupted-write driver (no disc, no GPU): simulates the process
 * dying at each step of a card file write, and the boot that follows.
 *
 *   save_interrupt write <dir> <step> <nth>   old file = 0xAA fill, then try
 *                                             to flush a 0xBB fill with the
 *                                             write dying at <step> of the
 *                                             <nth> atomic write; _exit()s
 *                                             without any cleanup.
 *   save_interrupt boot <dir>                 a new process: ensure(0), print
 *                                             "fill=<byte|mixed> name=<..>".
 *   save_interrupt section <dir> <mode>       a game write section (0xBB
 *                                             written, not yet closed) meets
 *                                             an exit: mode "wait" (gate says
 *                                             not ready, disk still 0xAA),
 *                                             "timeout" (gate gives up, disk
 *                                             still 0xAA), "settle" (section
 *                                             closes, gate ready, disk 0xBB),
 *                                             "shutdown" (atexit path after
 *                                             the burst: disk 0xBB),
 *                                             "timeout-shutdown" (gate gave
 *                                             up, then atexit: disk 0xAA).
 */
#include "memcard_host.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define NAME "BASLUS-01436DMW3-USA"

static uint8_t g_img[MEMCARD_FLAT_SIZE];

static int slot_fill(void) {
    uint8_t first;
    size_t i;

    if (memcard_host_file_read(0, 0, g_img, sizeof(g_img)) != (int)sizeof(g_img)) {
        return -1;
    }
    first = g_img[0];
    for (i = 1; i < sizeof(g_img); i++) {
        if (g_img[i] != first) {
            return 256; /* mixed */
        }
    }
    return first;
}

static void fill_ram(uint8_t v) {
    memset(g_img, v, sizeof(g_img));
    if (memcard_host_file_write(0, 0, g_img, sizeof(g_img)) !=
        (int)sizeof(g_img)) {
        fprintf(stderr, "fill_ram failed\n");
        exit(2);
    }
}

static int setup_old(const char *dir) {
    if (memcard_host_set_dir(dir) != 0 || memcard_host_ensure(0) != 0 ||
        memcard_host_create_file(0, NAME) != 0) {
        return -1;
    }
    fill_ram(0xAA);
    return memcard_host_flush(0);
}

/* What is on the disk now, read the way the next boot would. */
static int disk_fill(const char *dir) {
    char path[600];
    FILE *fp;
    uint8_t buf[MEMCARD_FLAT_SIZE];
    size_t n, i;

    snprintf(path, sizeof(path), "%s/save0.bin", dir);
    fp = fopen(path, "rb");
    if (fp == NULL) {
        return -2;
    }
    n = fread(buf, 1, sizeof(buf), fp);
    fclose(fp);
    if (n != sizeof(buf)) {
        return -3;
    }
    for (i = 1; i < n; i++) {
        if (buf[i] != buf[0]) {
            return 256;
        }
    }
    return buf[0];
}

int main(int argc, char **argv) {
    if (argc >= 5 && strcmp(argv[1], "write") == 0) {
        if (setup_old(argv[2]) != 0) {
            fprintf(stderr, "setup failed\n");
            return 2;
        }
        fill_ram(0xBB);
        if (strcmp(argv[3], "env") != 0) { /* "env": DW3_MEMCARD_FAULT_* arms it */
            memcard_host_test_fault_step(atoi(argv[3]), atoi(argv[4]));
        }
        (void)memcard_host_flush(0);
        _exit(0); /* the kill: no atexit, no shutdown */
    }
    if (argc >= 3 && strcmp(argv[1], "boot") == 0) {
        int f;

        if (memcard_host_set_dir(argv[2]) != 0) {
            return 2;
        }
        (void)memcard_host_ensure(0);
        if (!memcard_host_has_file(0)) {
            printf("fill=none\n");
            return 0;
        }
        f = slot_fill();
        printf("fill=%d\n", f);
        return 0;
    }
    if (argc >= 4 && strcmp(argv[1], "section") == 0) {
        const char *mode = argv[3];

        if (setup_old(argv[2]) != 0) {
            return 2;
        }
        fill_ram(0xBB);
        memcard_host_section_begin(0);
        if (strcmp(mode, "wait") == 0) {
            int ready = memcard_host_exit_ready(0);

            (void)memcard_host_flush_settled();
            printf("ready=%d disk=%d\n", ready, disk_fill(argv[2]));
        } else if (strcmp(mode, "timeout") == 0) {
            int ready = memcard_host_exit_ready(MEMCARD_EXIT_WAIT_US);

            printf("ready=%d disk=%d\n", ready, disk_fill(argv[2]));
        } else if (strcmp(mode, "settle") == 0) {
            int w = memcard_host_exit_ready(1000);

            (void)memcard_host_flush(0); /* the game's next MemCard call */
            printf("waited=%d ready=%d disk=%d\n", !w,
                   memcard_host_exit_ready(2000), disk_fill(argv[2]));
        } else if (strcmp(mode, "timeout-shutdown") == 0) {
            (void)memcard_host_exit_ready(MEMCARD_EXIT_WAIT_US);
            memcard_host_shutdown();
            printf("disk=%d\n", disk_fill(argv[2]));
        } else if (strcmp(mode, "shutdown") == 0) {
            memcard_host_shutdown();
            printf("disk=%d\n", disk_fill(argv[2]));
        } else {
            return 2;
        }
        return 0;
    }
    fprintf(stderr, "usage\n");
    return 2;
}
