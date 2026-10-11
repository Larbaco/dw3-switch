/*
 * Flat-save MemCard* checks for port/memcard.
 *
 *   memcard_flat_test <dir>
 *
 * Covers format/create, write+read round-trip, dirty flush-once, and
 * migration from a legacy memcard0.mcr fixture built in-process.
 */
#include "memcard_host.h"

#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

void MemCardInit(long val);
void MemCardStart(void);
long MemCardSync(long mode, long *cmds, long *result);
long MemCardExist(long chan);
long MemCardAccept(long chan);
long MemCardReadFile(long chan, char *file, unsigned long *adrs, long offset,
                     long bytes);
long MemCardWriteFile(long chan, char *file, unsigned long *adrs, long offset,
                      long bytes);
long MemCardCreateFile(long chan, char *filename, long blocks);
long MemCardFormat(long chan);

typedef struct HostDirEntry {
    char name[20];
    int32_t attr;
    int32_t size;
    struct HostDirEntry *next;
    int32_t head;
    char system[4];
} HostDirEntry;

long MemCardGetDirentry(long chan, char *name, HostDirEntry *dir, long *files,
                        long ofs, long max);

#define MC_ERR_NONE 0
#define SAVE_NAME "BASLUS-01436DMW3-USA"
#define CHAN0 0

static int sync_ok(void) {
    long cmds = 0;
    long result = -1;

    if (MemCardSync(0, &cmds, &result) != 1) {
        return 0;
    }
    return result == MC_ERR_NONE;
}

static int make_dir(const char *path) {
    if (mkdir(path, 0755) != 0 && errno != EEXIST) {
        perror(path);
        return -1;
    }
    return 0;
}

static int join_path(char *out, size_t outlen, const char *dir, const char *leaf) {
    int n = snprintf(out, outlen, "%s/%s", dir, leaf);
    if (n < 0 || (size_t)n >= outlen) {
        return -1;
    }
    return 0;
}

static uint8_t frame_xor(const uint8_t *frame) {
    uint8_t x = 0;
    int i;

    for (i = 0; i < 127; i++) {
        x ^= frame[i];
    }
    return x;
}

static int test_create_write_read(const char *dir) {
    char name[] = SAVE_NAME;
    unsigned char pat[128];
    unsigned char got[128];
    HostDirEntry entry;
    long files = -1;
    int i;

    if (memcard_host_set_dir(dir) != 0 || memcard_host_ensure(0) != 0) {
        fprintf(stderr, "ensure failed\n");
        return 1;
    }
    MemCardInit(0);
    MemCardStart();
    if (MemCardFormat(CHAN0) != MC_ERR_NONE) {
        fprintf(stderr, "format failed\n");
        return 1;
    }
    if (MemCardCreateFile(CHAN0, name, 4) != MC_ERR_NONE) {
        fprintf(stderr, "create failed\n");
        return 1;
    }
    if (MemCardGetDirentry(CHAN0, "*", &entry, &files, 0, 1) != MC_ERR_NONE ||
        files != 1 || entry.size != (int32_t)MEMCARD_FLAT_SIZE) {
        fprintf(stderr, "direntry size=%d files=%ld\n", entry.size, files);
        return 1;
    }

    memcard_host_reset_flush_count(0);
    for (i = 0; i < 4; i++) {
        memset(pat, (uint8_t)(0xA0 + i), sizeof(pat));
        pat[127] = 0x5A; /* must survive; not a frame XOR */
        if (MemCardWriteFile(CHAN0, name, (unsigned long *)pat, i * 128, 128) !=
                1 ||
            !sync_ok()) {
            fprintf(stderr, "write sector %d failed\n", i);
            return 1;
        }
    }
    if (memcard_host_flush_count(0) != 0) {
        fprintf(stderr, "flushed during sector writes (%u)\n",
                memcard_host_flush_count(0));
        return 1;
    }
    /* Section end: non-write MemCard* commits once. */
    if (MemCardExist(CHAN0) != 1 || !sync_ok()) {
        fprintf(stderr, "exist after write failed\n");
        return 1;
    }
    if (memcard_host_flush_count(0) != 1) {
        fprintf(stderr, "expected one flush, got %u\n",
                memcard_host_flush_count(0));
        return 1;
    }

    memset(got, 0, sizeof(got));
    if (MemCardReadFile(CHAN0, name, (unsigned long *)got, 128, 128) != 1 ||
        !sync_ok()) {
        fprintf(stderr, "read back failed\n");
        return 1;
    }
    if (got[0] != 0xA1 || got[127] != 0x5A) {
        fprintf(stderr, "round-trip bytes %02x/%02x\n", got[0], got[127]);
        return 1;
    }
    return 0;
}

static int test_shutdown_flush(const char *dir) {
    char name[] = SAVE_NAME;
    unsigned char pat[128];
    char path[768];
    FILE *fp;
    uint8_t buf[128];

    if (memcard_host_set_dir(dir) != 0 || memcard_host_ensure(0) != 0) {
        return 1;
    }
    MemCardInit(0);
    MemCardStart();
    (void)MemCardFormat(CHAN0);
    if (MemCardCreateFile(CHAN0, name, 4) != MC_ERR_NONE) {
        return 1;
    }
    memset(pat, 0x3C, sizeof(pat));
    if (MemCardWriteFile(CHAN0, name, (unsigned long *)pat, 0, 128) != 1 ||
        !sync_ok()) {
        return 1;
    }
    memcard_host_reset_flush_count(0);
    memcard_host_shutdown();
    if (memcard_host_flush_count(0) != 0) {
        /* shutdown clears counts after flush; check disk instead */
    }
    if (join_path(path, sizeof(path), dir, "save0.bin") != 0) {
        return 1;
    }
    fp = fopen(path, "rb");
    if (fp == NULL) {
        fprintf(stderr, "save0.bin missing after shutdown\n");
        return 1;
    }
    if (fread(buf, 1, sizeof(buf), fp) != sizeof(buf)) {
        fclose(fp);
        return 1;
    }
    fclose(fp);
    if (buf[0] != 0x3C) {
        fprintf(stderr, "shutdown flush byte %02x\n", buf[0]);
        return 1;
    }
    return 0;
}

/* Build a 4-block linked file inside a legacy .mcr and migrate it. */
static int test_migrate_from_mcr(const char *dir) {
    uint8_t *img;
    uint8_t frame[MEMCARD_HOST_SECTOR_SIZE];
    char mcr_path[768];
    char bak_path[768];
    char flat_path[768];
    FILE *fp;
    int b;
    int s;
    char name[] = SAVE_NAME;
    unsigned char got[128];

    img = (uint8_t *)malloc(MEMCARD_HOST_SIZE);
    if (img == NULL) {
        return 1;
    }
    memcard_host_format_image(img);

    /* Directory: blocks 1-4, links stored as next-block-minus-1. */
    for (b = 1; b <= 4; b++) {
        memset(frame, 0, sizeof(frame));
        if (b == 1) {
            frame[0] = 0x51;
            frame[4] = 0x00;
            frame[5] = 0x80; /* 0x8000 */
            memcpy(frame + 0x0A, SAVE_NAME, 20);
            frame[8] = 1; /* next block 2 */
            frame[9] = 0;
        } else if (b < 4) {
            frame[0] = 0x52;
            frame[8] = (uint8_t)b; /* next = b+1 → store b */
            frame[9] = 0;
        } else {
            frame[0] = 0x53;
            frame[8] = 0xFF;
            frame[9] = 0xFF;
        }
        frame[0x7F] = frame_xor(frame);
        memcpy(img + (size_t)b * MEMCARD_HOST_SECTOR_SIZE, frame, sizeof(frame));
        for (s = 0; s < 64; s++) {
            memset(img + (size_t)b * 0x2000u + (size_t)s * 128u, (uint8_t)(0x10 + b),
                   128);
        }
        /* Put a recognizable pattern at file offset 0. */
        if (b == 1) {
            img[0x2000] = 0xD3;
            img[0x2000 + 127] = 0x77;
        }
    }

    if (join_path(mcr_path, sizeof(mcr_path), dir, "memcard0.mcr") != 0) {
        free(img);
        return 1;
    }
    fp = fopen(mcr_path, "wb");
    if (fp == NULL || fwrite(img, 1, MEMCARD_HOST_SIZE, fp) != MEMCARD_HOST_SIZE) {
        if (fp) {
            fclose(fp);
        }
        free(img);
        fprintf(stderr, "write mcr fixture failed\n");
        return 1;
    }
    fclose(fp);
    free(img);

    memcard_host_shutdown();
    if (memcard_host_set_dir(dir) != 0 || memcard_host_ensure(0) != 0) {
        fprintf(stderr, "migrate ensure failed\n");
        return 1;
    }
    if (!memcard_host_has_file(0)) {
        fprintf(stderr, "migrate did not load flat save\n");
        return 1;
    }
    if (join_path(flat_path, sizeof(flat_path), dir, "save0.bin") != 0) {
        return 1;
    }
    fp = fopen(flat_path, "rb");
    if (fp == NULL) {
        fprintf(stderr, "save0.bin not created\n");
        return 1;
    }
    fclose(fp);
    if (join_path(bak_path, sizeof(bak_path), dir, "memcard0.mcr.bak") != 0) {
        return 1;
    }
    fp = fopen(bak_path, "rb");
    if (fp == NULL) {
        fprintf(stderr, "memcard0.mcr.bak missing\n");
        return 1;
    }
    fclose(fp);
    {
        FILE *still = fopen(mcr_path, "rb");
        if (still != NULL) {
            fclose(still);
            fprintf(stderr, "memcard0.mcr still present (expected rename)\n");
            return 1;
        }
    }

    MemCardInit(0);
    MemCardStart();
    memset(got, 0, sizeof(got));
    if (MemCardReadFile(CHAN0, name, (unsigned long *)got, 0, 128) != 1 ||
        !sync_ok()) {
        fprintf(stderr, "migrated read failed\n");
        return 1;
    }
    if (got[0] != 0xD3 || got[127] != 0x77) {
        fprintf(stderr, "migrated bytes %02x/%02x\n", got[0], got[127]);
        return 1;
    }
    return 0;
}

int main(int argc, char **argv) {
    const char *dir;
    char sub[768];

    if (argc < 2) {
        fprintf(stderr, "usage: %s <dir>\n", argv[0]);
        return 2;
    }
    dir = argv[1];
    if (make_dir(dir) != 0) {
        return 1;
    }

    if (join_path(sub, sizeof(sub), dir, "rw") != 0 || make_dir(sub) != 0 ||
        test_create_write_read(sub) != 0) {
        return 1;
    }
    memcard_host_shutdown();

    if (join_path(sub, sizeof(sub), dir, "shutdown") != 0 || make_dir(sub) != 0 ||
        test_shutdown_flush(sub) != 0) {
        return 1;
    }
    memcard_host_shutdown();

    if (join_path(sub, sizeof(sub), dir, "migrate") != 0 || make_dir(sub) != 0 ||
        test_migrate_from_mcr(sub) != 0) {
        return 1;
    }
    memcard_host_shutdown();

    printf("ok\n");
    return 0;
}
