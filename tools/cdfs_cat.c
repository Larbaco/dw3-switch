/* Tiny host helper: print file-id bytes from a packed runtime root.
 *
 * Usage:
 *   cdfs_cat --root DIR [--map PATH] [--size] ID
 *
 * Default map: <root>/files_us.json (then files_eu.json).
 * --size prints decimal size only; otherwise file bytes go to stdout.
 */
#include "cdfs.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void usage(const char *argv0) {
    fprintf(stderr,
            "usage: %s --root DIR [--map PATH] [--size] ID\n",
            argv0);
}

int main(int argc, char **argv) {
    const char *root = NULL;
    const char *map = NULL;
    int size_only = 0;
    uint32_t id = 0;
    int have_id = 0;
    int i;
    uint32_t sz;
    unsigned char *buf;
    int n;

    for (i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--root") == 0 && i + 1 < argc) {
            root = argv[++i];
        } else if (strcmp(argv[i], "--map") == 0 && i + 1 < argc) {
            map = argv[++i];
        } else if (strcmp(argv[i], "--size") == 0) {
            size_only = 1;
        } else if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            usage(argv[0]);
            return 0;
        } else {
            char *end = NULL;
            unsigned long v = strtoul(argv[i], &end, 0);
            if (end == argv[i] || *end != '\0') {
                fprintf(stderr, "error: bad argument: %s\n", argv[i]);
                usage(argv[0]);
                return 2;
            }
            id = (uint32_t)v;
            have_id = 1;
        }
    }

    if (root == NULL || !have_id) {
        usage(argv[0]);
        return 2;
    }
    if (cdfs_set_root(root) != 0) {
        fprintf(stderr, "error: cdfs_set_root failed\n");
        return 1;
    }
    if (cdfs_load_map(map) != 0) {
        fprintf(stderr, "error: cdfs_load_map failed\n");
        return 1;
    }

    sz = cdfs_file_size(id);
    if (sz == (uint32_t)-1) {
        fprintf(stderr, "error: unknown or unreadable id %u\n", id);
        return 1;
    }
    if (size_only) {
        printf("%u\n", sz);
        cdfs_shutdown();
        return 0;
    }

    buf = (unsigned char *)malloc(sz ? sz : 1);
    if (buf == NULL) {
        fprintf(stderr, "error: oom\n");
        return 1;
    }
    n = cdfs_read(id, buf, sz);
    if (n < 0 || (uint32_t)n != sz) {
        fprintf(stderr, "error: cdfs_read failed for id %u\n", id);
        free(buf);
        return 1;
    }
    if (fwrite(buf, 1, (size_t)n, stdout) != (size_t)n) {
        fprintf(stderr, "error: short write\n");
        free(buf);
        return 1;
    }
    free(buf);
    cdfs_shutdown();
    return 0;
}
