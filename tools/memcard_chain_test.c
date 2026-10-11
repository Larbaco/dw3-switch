/*
 * BIOS directory-link check for port/memcard.
 *
 * The halfword at directory offset 8 is the next block number minus 1
 * (0..14), or 0xFFFF on the last block. A raw DuckStation .mcd is the
 * same 128 KiB image as memcard0.mcr.
 *
 *   memcard_chain_test <dir>
 *   memcard_chain_test <dir> --card <raw-128KiB-image>
 */
#include "memcard_host.h"

#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

long _card_read(long chan, long block, unsigned char *buf);
long _card_write(long chan, long block, unsigned char *buf);

void MemCardInit(long val);
void MemCardStart(void);
long MemCardSync(long mode, long *cmds, long *result);
long MemCardReadFile(long chan, char *file, unsigned long *adrs, long offset,
                     long bytes);
long MemCardWriteFile(long chan, char *file, unsigned long *adrs, long offset,
                      long bytes);

#define MC_ERR_NONE 0
#define SAVE_NAME "BASLUS-01436DMW3-USA"
#define GAME_SAVE_SIZE 0x26BC
#define INFO_SIZE 0x100

long _card_read(long chan, long block, unsigned char *buf) {
    int slot = (int)((unsigned long)chan >> 4);

    if (memcard_host_read_sector(slot, (int)block, buf) != 0) {
        return 0;
    }
    return 1;
}

long _card_write(long chan, long block, unsigned char *buf) {
    int slot = (int)((unsigned long)chan >> 4);

    if (memcard_host_write_sector(slot, (int)block, buf) != 0) {
        return 0;
    }
    return 1;
}

static uint8_t frame_xor(const uint8_t *frame) {
    uint8_t x = 0;
    int i;

    for (i = 0; i < 127; i++) {
        x ^= frame[i];
    }
    return x;
}

static int sync_ok(void) {
    long cmds = 0;
    long result = -1;

    if (MemCardSync(0, &cmds, &result) != 1) {
        return 0;
    }
    return result == MC_ERR_NONE;
}

static int fill_block(int slot, int block, uint8_t byte) {
    uint8_t frame[MEMCARD_HOST_SECTOR_SIZE];
    int s;

    memset(frame, byte, sizeof(frame));
    for (s = 0; s < 64; s++) {
        if (memcard_host_write_sector(slot, block * 64 + s, frame) != 0) {
            return -1;
        }
    }
    return 0;
}

/* Two-block file whose link halfword is 1 (next block 2), not 2. */
static int test_bios_link_read(const char *dir) {
    uint8_t frame[MEMCARD_HOST_SECTOR_SIZE];
    unsigned char buf[MEMCARD_HOST_SECTOR_SIZE];
    char name[] = SAVE_NAME;

    if (memcard_host_set_dir(dir) != 0 || memcard_host_ensure(0) != 0) {
        fprintf(stderr, "ensure failed\n");
        return 1;
    }
    if (fill_block(0, 1, 0xA1) != 0 || fill_block(0, 2, 0xB2) != 0) {
        fprintf(stderr, "fill block failed\n");
        return 1;
    }

    memset(frame, 0, sizeof(frame));
    frame[0] = 0x51;
    frame[4] = 0x00;
    frame[5] = 0x40; /* 16384 bytes */
    frame[8] = 1;    /* next block number 2, stored minus 1 */
    frame[9] = 0;
    memcpy(frame + 0x0A, SAVE_NAME, 20);
    frame[0x7F] = frame_xor(frame);
    if (memcard_host_write_sector(0, 1, frame) != 0) {
        return 1;
    }

    memset(frame, 0, sizeof(frame));
    frame[0] = 0x53;
    frame[8] = 0xFF;
    frame[9] = 0xFF;
    frame[0x7F] = frame_xor(frame);
    if (memcard_host_write_sector(0, 2, frame) != 0) {
        return 1;
    }

    MemCardInit(0);
    MemCardStart();
    memset(buf, 0, sizeof(buf));
    if (MemCardReadFile(0, name, (unsigned long *)buf, 8192, 128) != 1 ||
        !sync_ok()) {
        fprintf(stderr, "read across block failed\n");
        return 1;
    }
    if (buf[0] != 0xB2 || buf[127] != 0xB2) {
        fprintf(stderr, "tail byte 0x%02x (wanted block 2)\n", buf[0]);
        return 1;
    }
    return 0;
}

/* Host create/write must store the same minus-1 link and read it back. */
static int test_host_write_link(const char *dir) {
    uint8_t frame[MEMCARD_HOST_SECTOR_SIZE];
    unsigned char head[MEMCARD_HOST_SECTOR_SIZE];
    unsigned char tail[MEMCARD_HOST_SECTOR_SIZE];
    char name[] = SAVE_NAME;
    int i;

    if (memcard_host_set_dir(dir) != 0 || memcard_host_ensure(0) != 0) {
        return 1;
    }
    memset(head, 0x11, sizeof(head));
    memset(tail, 0x22, sizeof(tail));
    MemCardInit(0);
    MemCardStart();
    if (MemCardWriteFile(0, name, (unsigned long *)head, 0, 128) != 1 ||
        !sync_ok()) {
        fprintf(stderr, "write head failed\n");
        return 1;
    }
    if (MemCardWriteFile(0, name, (unsigned long *)tail, 8192, 128) != 1 ||
        !sync_ok()) {
        fprintf(stderr, "write tail failed\n");
        return 1;
    }
    if (memcard_host_read_sector(0, 1, frame) != 0) {
        return 1;
    }
    if (frame[0] != 0x51 || frame[8] != 1 || frame[9] != 0) {
        fprintf(stderr, "link stored as %02x%02x (wanted 01 00)\n", frame[8],
                frame[9]);
        return 1;
    }
    memset(tail, 0, sizeof(tail));
    if (MemCardReadFile(0, name, (unsigned long *)tail, 8192, 128) != 1 ||
        !sync_ok()) {
        fprintf(stderr, "read back tail failed\n");
        return 1;
    }
    for (i = 0; i < 128; i++) {
        if (tail[i] != 0x22) {
            fprintf(stderr, "tail[%d]=0x%02x\n", i, tail[i]);
            return 1;
        }
    }
    /* 0x22 repeated an odd number of times XORs back to 0x22, so the loop
     * above does not catch a data-sector checksum overwrite. */
    memset(head, 0x00, sizeof(head));
    head[0] = 0x01;
    head[127] = 0x5A;
    if (MemCardWriteFile(0, name, (unsigned long *)head, 128, 128) != 1 ||
        !sync_ok()) {
        fprintf(stderr, "write payload tail failed\n");
        return 1;
    }
    memset(head, 0, sizeof(head));
    if (MemCardReadFile(0, name, (unsigned long *)head, 128, 128) != 1 ||
        !sync_ok()) {
        fprintf(stderr, "read payload tail failed\n");
        return 1;
    }
    if (head[0] != 0x01 || head[127] != 0x5A) {
        fprintf(stderr, "data byte 127 = 0x%02x (wanted 0x5A)\n", head[127]);
        return 1;
    }
    if (memcard_host_read_sector(0, 1, frame) != 0 ||
        frame[0x7F] != frame_xor(frame)) {
        fprintf(stderr, "directory checksum missing\n");
        return 1;
    }
    return 0;
}

static int join_path(char *out, size_t outlen, const char *dir, const char *leaf) {
    size_t dl = strlen(dir);
    size_t ll = strlen(leaf);

    if (dl + 1 + ll + 1 > outlen) {
        return -1;
    }
    memcpy(out, dir, dl);
    out[dl] = '/';
    memcpy(out + dl + 1, leaf, ll + 1);
    return 0;
}

static int copy_card(const char *src, const char *dir) {
    FILE *in;
    FILE *out;
    char path[768];
    uint8_t *img;
    size_t n;
    int rc = 1;

    if (memcard_host_set_dir(dir) != 0) {
        return 1;
    }
    if (join_path(path, sizeof(path), dir, "memcard0.mcr") != 0) {
        return 1;
    }
    in = fopen(src, "rb");
    if (in == NULL) {
        perror(src);
        return 1;
    }
    img = (uint8_t *)malloc(MEMCARD_HOST_SIZE);
    if (img == NULL) {
        fclose(in);
        return 1;
    }
    n = fread(img, 1, MEMCARD_HOST_SIZE, in);
    fclose(in);
    if (n != MEMCARD_HOST_SIZE) {
        fprintf(stderr, "card size %zu (wanted %u)\n", n, MEMCARD_HOST_SIZE);
        free(img);
        return 1;
    }
    out = fopen(path, "wb");
    if (out == NULL) {
        perror(path);
        free(img);
        return 1;
    }
    if (fwrite(img, 1, MEMCARD_HOST_SIZE, out) == MEMCARD_HOST_SIZE) {
        rc = 0;
    }
    fclose(out);
    free(img);
    return rc;
}

/* Read the US save the way STGMCARD does and compare the XOR. */
static int test_real_card(const char *src, const char *dir) {
    unsigned char hdr[MEMCARD_HOST_SECTOR_SIZE];
    unsigned char *save;
    char name[] = SAVE_NAME;
    unsigned icons;
    long data_off;
    long got_n;
    uint8_t sum;
    uint8_t got;
    int i;
    const long chunk = 0x2700;

    if (copy_card(src, dir) != 0) {
        return 1;
    }
    MemCardInit(0);
    MemCardStart();
    if (MemCardReadFile(0, name, (unsigned long *)hdr, 0, 128) != 1 ||
        !sync_ok()) {
        fprintf(stderr, "header read failed\n");
        return 1;
    }
    if (hdr[0] != 'S' || hdr[1] != 'C') {
        fprintf(stderr, "header magic %02x %02x\n", hdr[0], hdr[1]);
        return 1;
    }
    icons = (unsigned)hdr[2] & 0x0Fu;
    if (icons < 1 || icons > 3) {
        fprintf(stderr, "icon count %u\n", icons);
        return 1;
    }
    data_off = (long)icons * 128 + 128 + INFO_SIZE;
    save = (unsigned char *)malloc((size_t)chunk);
    if (save == NULL) {
        return 1;
    }
    got_n = 0;
    while (got_n < chunk) {
        if (MemCardReadFile(0, name, (unsigned long *)(save + got_n),
                            data_off + got_n, 128) != 1 ||
            !sync_ok()) {
            fprintf(stderr, "save read at %ld failed\n", data_off + got_n);
            free(save);
            return 1;
        }
        got_n += 128;
    }
    got = save[0];
    sum = 0;
    for (i = 4; i < GAME_SAVE_SIZE; i++) {
        sum ^= save[i];
    }
    printf("slot0 version=%u got=%u calc=%u money=%d\n", save[2], got, sum,
           (int)((uint32_t)save[0x6C] | ((uint32_t)save[0x6D] << 8) |
                 ((uint32_t)save[0x6E] << 16) | ((uint32_t)save[0x6F] << 24)));
    free(save);
    if (got != sum) {
        fprintf(stderr, "checksum mismatch\n");
        return 1;
    }
    return 0;
}

static int make_dir(const char *path) {
    if (mkdir(path, 0755) != 0 && errno != EEXIST) {
        perror(path);
        return -1;
    }
    return 0;
}

int main(int argc, char **argv) {
    const char *dir;
    const char *card = NULL;
    char sub[768];

    if (argc < 2) {
        fprintf(stderr, "usage: %s <dir> [--card <image>]\n", argv[0]);
        return 2;
    }
    dir = argv[1];
    if (argc >= 4 && strcmp(argv[2], "--card") == 0) {
        card = argv[3];
    }
    if (make_dir(dir) != 0) {
        return 1;
    }
    if (test_bios_link_read(dir) != 0) {
        return 1;
    }
    if (join_path(sub, sizeof(sub), dir, "write") != 0 || make_dir(sub) != 0 ||
        test_host_write_link(sub) != 0) {
        return 1;
    }
    if (card != NULL) {
        if (join_path(sub, sizeof(sub), dir, "real") != 0) {
            return 1;
        }
        if (make_dir(sub) != 0 || test_real_card(card, sub) != 0) {
            return 1;
        }
    }
    printf("ok\n");
    return 0;
}
