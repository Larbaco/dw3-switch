/*
 * Flat-save MemCard* checks for port/memcard.
 *
 *   memcard_flat_test <dir>
 *
 * Covers format/create, write+read round-trip, dirty flush-once,
 * overwrite flush (existing saveN.bin), and migration from a legacy
 * memcard0.mcr fixture built in-process.
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

/* Second flush must overwrite an existing saveN.bin (FAT EEXIST path). */
static int test_overwrite_flush(const char *dir) {
    char name[] = SAVE_NAME;
    unsigned char pat[128];
    char path[768];
    FILE *fp;
    uint8_t buf[128];

    if (memcard_host_set_dir(dir) != 0 || memcard_host_ensure(0) != 0) {
        fprintf(stderr, "overwrite: ensure failed\n");
        return 1;
    }
    MemCardInit(0);
    MemCardStart();
    if (MemCardFormat(CHAN0) != MC_ERR_NONE) {
        fprintf(stderr, "overwrite: format failed\n");
        return 1;
    }
    if (MemCardCreateFile(CHAN0, name, 4) != MC_ERR_NONE) {
        fprintf(stderr, "overwrite: create failed\n");
        return 1;
    }

    memcard_host_reset_flush_count(0);
    memset(pat, 0x11, sizeof(pat));
    if (MemCardWriteFile(CHAN0, name, (unsigned long *)pat, 0, 128) != 1 ||
        !sync_ok()) {
        fprintf(stderr, "overwrite: first write failed\n");
        return 1;
    }
    if (MemCardExist(CHAN0) != 1 || !sync_ok()) {
        fprintf(stderr, "overwrite: first exist/flush failed\n");
        return 1;
    }
    if (memcard_host_flush_count(0) != 1 || memcard_host_flush_failed(0)) {
        fprintf(stderr, "overwrite: first flush count=%u failed=%d\n",
                memcard_host_flush_count(0), memcard_host_flush_failed(0));
        return 1;
    }

    memset(pat, 0x22, sizeof(pat));
    if (MemCardWriteFile(CHAN0, name, (unsigned long *)pat, 0, 128) != 1 ||
        !sync_ok()) {
        fprintf(stderr, "overwrite: second write failed\n");
        return 1;
    }
    if (MemCardExist(CHAN0) != 1 || !sync_ok()) {
        fprintf(stderr, "overwrite: second exist/flush failed\n");
        return 1;
    }
    if (memcard_host_flush_count(0) != 2 || memcard_host_flush_failed(0)) {
        fprintf(stderr, "overwrite: second flush count=%u failed=%d\n",
                memcard_host_flush_count(0), memcard_host_flush_failed(0));
        return 1;
    }

    if (join_path(path, sizeof(path), dir, "save0.bin") != 0) {
        return 1;
    }
    fp = fopen(path, "rb");
    if (fp == NULL) {
        fprintf(stderr, "overwrite: save0.bin missing\n");
        return 1;
    }
    if (fread(buf, 1, sizeof(buf), fp) != sizeof(buf)) {
        fclose(fp);
        return 1;
    }
    fclose(fp);
    if (buf[0] != 0x22) {
        fprintf(stderr, "overwrite: disk byte %02x (want 22)\n", buf[0]);
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

/* Write a contiguous block chain: first..first+blocks-1 with product name. */
static void plant_file(uint8_t *img, int first, int blocks, const char *prod,
                       uint8_t fill, uint8_t mark0, uint8_t mark127) {
    uint8_t frame[MEMCARD_HOST_SECTOR_SIZE];
    int b;
    int s;
    uint32_t size = (uint32_t)blocks * 0x2000u;

    for (b = 0; b < blocks; b++) {
        int idx = first + b;

        memset(frame, 0, sizeof(frame));
        if (b == 0) {
            frame[0] = 0x51;
            frame[4] = (uint8_t)(size & 0xFF);
            frame[5] = (uint8_t)((size >> 8) & 0xFF);
            frame[6] = (uint8_t)((size >> 16) & 0xFF);
            frame[7] = (uint8_t)((size >> 24) & 0xFF);
            {
                size_t n = strlen(prod);
                if (n > 20) {
                    n = 20;
                }
                memcpy(frame + 0x0A, prod, n);
            }
        } else if (b + 1 < blocks) {
            frame[0] = 0x52;
        } else {
            frame[0] = 0x53;
        }
        if (b + 1 < blocks) {
            unsigned link = (unsigned)(first + b + 1) - 1u;
            frame[8] = (uint8_t)(link & 0xFF);
            frame[9] = (uint8_t)((link >> 8) & 0xFF);
        } else {
            frame[8] = 0xFF;
            frame[9] = 0xFF;
        }
        frame[0x7F] = frame_xor(frame);
        memcpy(img + (size_t)idx * MEMCARD_HOST_SECTOR_SIZE, frame,
               sizeof(frame));
        for (s = 0; s < 64; s++) {
            memset(img + (size_t)idx * 0x2000u + (size_t)s * 128u, fill, 128);
        }
    }
    img[(size_t)first * 0x2000u] = mark0;
    img[(size_t)first * 0x2000u + 127u] = mark127;
}

static int write_mcr(const char *dir, const uint8_t *img) {
    char mcr_path[768];
    FILE *fp;

    if (join_path(mcr_path, sizeof(mcr_path), dir, "memcard0.mcr") != 0) {
        return 1;
    }
    fp = fopen(mcr_path, "wb");
    if (fp == NULL || fwrite(img, 1, MEMCARD_HOST_SIZE, fp) != MEMCARD_HOST_SIZE) {
        if (fp) {
            fclose(fp);
        }
        fprintf(stderr, "write mcr fixture failed\n");
        return 1;
    }
    fclose(fp);
    return 0;
}

static int file_exists(const char *path) {
    FILE *fp = fopen(path, "rb");
    if (fp == NULL) {
        return 0;
    }
    fclose(fp);
    return 1;
}

/* OTHERGAME at block 1, DW3 at blocks 5-8 — must pick DW3 by name. */
static int test_migrate_prefers_dw3_name(const char *dir) {
    uint8_t *img;
    char mcr_path[768];
    char bak_path[768];
    char flat_path[768];
    char name_path[768];
    char name_buf[32];
    FILE *fp;
    char name[] = SAVE_NAME;
    unsigned char got[128];
    size_t nr;

    img = (uint8_t *)malloc(MEMCARD_HOST_SIZE);
    if (img == NULL) {
        return 1;
    }
    memcard_host_format_image(img);
    plant_file(img, 1, 2, "OTHERGAME-SAVE-FILE", 0x11, 0xAA, 0xBB);
    plant_file(img, 5, 4, SAVE_NAME, 0x22, 0xD3, 0x77);
    if (write_mcr(dir, img) != 0) {
        free(img);
        return 1;
    }
    free(img);

    memcard_host_shutdown();
    if (memcard_host_set_dir(dir) != 0 || memcard_host_ensure(0) != 0) {
        fprintf(stderr, "name-pick ensure failed\n");
        return 1;
    }
    if (!memcard_host_has_file(0)) {
        fprintf(stderr, "name-pick: no flat save\n");
        return 1;
    }
    if (strcmp(memcard_host_file_name(0), SAVE_NAME) != 0) {
        fprintf(stderr, "name-pick: got name '%s'\n", memcard_host_file_name(0));
        return 1;
    }
    if (join_path(flat_path, sizeof(flat_path), dir, "save0.bin") != 0 ||
        join_path(bak_path, sizeof(bak_path), dir, "memcard0.mcr.bak") != 0 ||
        join_path(mcr_path, sizeof(mcr_path), dir, "memcard0.mcr") != 0 ||
        join_path(name_path, sizeof(name_path), dir, "save0.name") != 0) {
        return 1;
    }
    if (!file_exists(flat_path) || !file_exists(bak_path) ||
        file_exists(mcr_path)) {
        fprintf(stderr, "name-pick: unexpected flat/bak/mcr presence\n");
        return 1;
    }
    fp = fopen(name_path, "rb");
    if (fp == NULL) {
        fprintf(stderr, "name-pick: save0.name missing\n");
        return 1;
    }
    nr = fread(name_buf, 1, sizeof(name_buf) - 1, fp);
    fclose(fp);
    name_buf[nr] = '\0';
    if (strcmp(name_buf, SAVE_NAME) != 0) {
        fprintf(stderr, "name-pick: sidecar '%s'\n", name_buf);
        return 1;
    }

    MemCardInit(0);
    MemCardStart();
    memset(got, 0, sizeof(got));
    if (MemCardReadFile(CHAN0, name, (unsigned long *)got, 0, 128) != 1 ||
        !sync_ok()) {
        fprintf(stderr, "name-pick read failed\n");
        return 1;
    }
    if (got[0] != 0xD3 || got[127] != 0x77) {
        fprintf(stderr, "name-pick bytes %02x/%02x (wanted DW3, not OTHER)\n",
                got[0], got[127]);
        return 1;
    }
    return 0;
}

/* Corrupt short flat must not block migration from a valid .mcr. */
static int test_bad_flat_then_migrate(const char *dir) {
    uint8_t *img;
    char flat_path[768];
    char bad_path[768];
    char bak_path[768];
    FILE *fp;
    char name[] = SAVE_NAME;
    unsigned char got[128];
    uint8_t junk[100];

    if (join_path(flat_path, sizeof(flat_path), dir, "save0.bin") != 0) {
        return 1;
    }
    memset(junk, 0x5E, sizeof(junk));
    fp = fopen(flat_path, "wb");
    if (fp == NULL || fwrite(junk, 1, sizeof(junk), fp) != sizeof(junk)) {
        if (fp) {
            fclose(fp);
        }
        fprintf(stderr, "write bad flat failed\n");
        return 1;
    }
    fclose(fp);

    img = (uint8_t *)malloc(MEMCARD_HOST_SIZE);
    if (img == NULL) {
        return 1;
    }
    memcard_host_format_image(img);
    plant_file(img, 1, 4, SAVE_NAME, 0x33, 0xC1, 0xC2);
    if (write_mcr(dir, img) != 0) {
        free(img);
        return 1;
    }
    free(img);

    memcard_host_shutdown();
    if (memcard_host_set_dir(dir) != 0 || memcard_host_ensure(0) != 0) {
        fprintf(stderr, "bad-flat ensure failed\n");
        return 1;
    }
    if (!memcard_host_has_file(0)) {
        fprintf(stderr, "bad-flat: migrate did not produce save\n");
        return 1;
    }
    if (join_path(bad_path, sizeof(bad_path), dir, "save0.bin.bad") != 0 ||
        join_path(bak_path, sizeof(bak_path), dir, "memcard0.mcr.bak") != 0) {
        return 1;
    }
    if (!file_exists(bad_path) || !file_exists(bak_path)) {
        fprintf(stderr, "bad-flat: missing .bad or .bak\n");
        return 1;
    }

    MemCardInit(0);
    MemCardStart();
    memset(got, 0, sizeof(got));
    if (MemCardReadFile(CHAN0, name, (unsigned long *)got, 0, 128) != 1 ||
        !sync_ok()) {
        fprintf(stderr, "bad-flat read failed\n");
        return 1;
    }
    if (got[0] != 0xC1 || got[127] != 0xC2) {
        fprintf(stderr, "bad-flat bytes %02x/%02x\n", got[0], got[127]);
        return 1;
    }
    return 0;
}

/* Only OTHERGAME on the card: leave .mcr alone, no flat save. */
static int test_migrate_skips_non_dw3(const char *dir) {
    uint8_t *img;
    char mcr_path[768];
    char bak_path[768];
    char flat_path[768];

    img = (uint8_t *)malloc(MEMCARD_HOST_SIZE);
    if (img == NULL) {
        return 1;
    }
    memcard_host_format_image(img);
    plant_file(img, 1, 2, "OTHERGAME-SAVE-FILE", 0x44, 0x11, 0x22);
    if (write_mcr(dir, img) != 0) {
        free(img);
        return 1;
    }
    free(img);

    memcard_host_shutdown();
    if (memcard_host_set_dir(dir) != 0 || memcard_host_ensure(0) != 0) {
        fprintf(stderr, "skip-non-dw3 ensure failed\n");
        return 1;
    }
    if (memcard_host_has_file(0)) {
        fprintf(stderr, "skip-non-dw3: unexpected flat save\n");
        return 1;
    }
    if (join_path(mcr_path, sizeof(mcr_path), dir, "memcard0.mcr") != 0 ||
        join_path(bak_path, sizeof(bak_path), dir, "memcard0.mcr.bak") != 0 ||
        join_path(flat_path, sizeof(flat_path), dir, "save0.bin") != 0) {
        return 1;
    }
    if (!file_exists(mcr_path) || file_exists(bak_path) ||
        file_exists(flat_path)) {
        fprintf(stderr, "skip-non-dw3: .mcr should stay; no .bak/flat\n");
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

    if (join_path(sub, sizeof(sub), dir, "overwrite") != 0 ||
        make_dir(sub) != 0 || test_overwrite_flush(sub) != 0) {
        return 1;
    }
    memcard_host_shutdown();

    if (join_path(sub, sizeof(sub), dir, "shutdown") != 0 || make_dir(sub) != 0 ||
        test_shutdown_flush(sub) != 0) {
        return 1;
    }
    memcard_host_shutdown();

    if (join_path(sub, sizeof(sub), dir, "migrate_name") != 0 ||
        make_dir(sub) != 0 || test_migrate_prefers_dw3_name(sub) != 0) {
        return 1;
    }
    memcard_host_shutdown();

    if (join_path(sub, sizeof(sub), dir, "bad_flat") != 0 || make_dir(sub) != 0 ||
        test_bad_flat_then_migrate(sub) != 0) {
        return 1;
    }
    memcard_host_shutdown();

    if (join_path(sub, sizeof(sub), dir, "skip_other") != 0 ||
        make_dir(sub) != 0 || test_migrate_skips_non_dw3(sub) != 0) {
        return 1;
    }
    memcard_host_shutdown();

    printf("ok\n");
    return 0;
}
