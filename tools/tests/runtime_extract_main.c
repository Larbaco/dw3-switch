/* Host for port/cdfs/runtime_extract.c. Supplies the FILE_SECTORS symbols
 * the game normally gets from file_table.c, and prints progress lines. */
#include "runtime_extract.h"

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

#undef rename
int rename(const char *old_path, const char *new_path);

/* Model Switch FAT/exFAT: rename to an existing destination returns EEXIST. */
int dw3_test_rename(const char *old_path, const char *new_path) {
    struct stat st;
    if (stat(new_path, &st) == 0) {
        errno = EEXIST;
        return -1;
    }
    if (errno != ENOENT) {
        return -1;
    }
    return rename(old_path, new_path);
}

int FILE_SECTORS[32];
unsigned short FILE_SECTOR_COUNTS[32];
int FILE_COUNT;

void dw3_str_restore_emit(const char *msg, unsigned long long done,
                          unsigned long long total) {
    if (msg != NULL) {
        printf("MSG %s done=%llu total=%llu\n", msg, done, total);
    } else {
        printf("TICK done=%llu total=%llu\n", done, total);
    }
    fflush(stdout);
}

static int load_table(const char *path) {
    FILE *fp;
    int id;
    int lba;
    int sectors;
    int max_id = -1;

    fp = fopen(path, "r");
    if (fp == NULL) {
        return -1;
    }
    memset(FILE_SECTORS, 0, sizeof(FILE_SECTORS));
    memset(FILE_SECTOR_COUNTS, 0, sizeof(FILE_SECTOR_COUNTS));
    while (fscanf(fp, "%d %d %d", &id, &lba, &sectors) == 3) {
        if (id < 0 || id >= 32 || sectors < 0 || sectors > 65535) {
            fclose(fp);
            return -1;
        }
        FILE_SECTORS[id] = lba;
        FILE_SECTOR_COUNTS[id] = (unsigned short)sectors;
        if (id > max_id) {
            max_id = id;
        }
    }
    fclose(fp);
    FILE_COUNT = max_id + 1;
    return 0;
}

int main(int argc, char **argv) {
    if (argc != 3) {
        fprintf(stderr, "usage: runtime_extract_main <runtime-root> <table>\n");
        return 2;
    }
    if (load_table(argv[2]) != 0) {
        fprintf(stderr, "bad table\n");
        return 2;
    }
    return dw3_runtime_ensure(argv[1]) == 0 ? 0 : 1;
}
