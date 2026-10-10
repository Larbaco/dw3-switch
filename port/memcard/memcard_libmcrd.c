/*
 * Host libmcrd (MemCard*) backed by port/memcard 128 KiB .mcr images.
 *
 * PSY-Z does not ship a portable libmcrd; game code talks MemCard* while
 * raw sectors go through _card_read/_card_write (wrapped → memcard_host_*).
 */
#include "memcard_host.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* From libapi / libmcrd — avoid including both (DIRENTRY / O_RDONLY clash). */
long _card_read(long chan, long block, unsigned char *buf);
long _card_write(long chan, long block, unsigned char *buf);

#define McErrNone 0x00
#define McErrCardNotExist 0x01
#define McErrCardInvalid 0x02
#define McErrNotFormat 0x04
#define McErrFileNotExist 0x05
#define McErrAlreadyExist 0x06
#define McErrBlockFull 0x07
#define McFuncExist 0x01
#define McFuncAccept 0x02
#define McFuncReadFile 0x03
#define McFuncWriteFile 0x04

#define MC_BLOCK_SIZE 0x2000u
#define MC_DIR_FRAMES 15
#define MC_ALLOC_FREE 0xA0u
#define MC_ALLOC_FIRST 0x51u
#define MC_ALLOC_MID 0x52u
#define MC_ALLOC_LAST 0x53u

/* Matches vendor/dw3_decomp CardDirEntry (name-first kernel DIRENTRY). */
typedef struct HostDirEntry {
    char name[20];
    int32_t attr;
    int32_t size;
    struct HostDirEntry *next;
    int32_t head;
    char system[4];
} HostDirEntry;

typedef struct McPending {
    int active;
    long cmds;
    long result;
} McPending;

static McPending g_pending;
static int g_started;

static void pending_set(long cmds, long result) {
    g_pending.active = 1;
    g_pending.cmds = cmds;
    g_pending.result = result;
}

static int chan_to_slot(long chan) {
    unsigned int port = (unsigned int)(chan >> 4);

    if (port >= MEMCARD_HOST_SLOTS) {
        return -1;
    }
    if ((chan & 15) != 0) {
        return -1;
    }
    return (int)port;
}

static uint8_t frame_checksum(const uint8_t *frame) {
    uint8_t x = 0;
    int i;

    for (i = 0; i < 127; i++) {
        x ^= frame[i];
    }
    return x;
}

static int read_frame(int slot, int frame, uint8_t *buf) {
    if (_card_read((long)slot << 4, frame, buf) != 1) {
        return -1;
    }
    return 0;
}

static int write_frame(int slot, int frame, uint8_t *buf) {
    buf[0x7F] = frame_checksum(buf);
    if (_card_write((long)slot << 4, frame, buf) != 1) {
        return -1;
    }
    return 0;
}

static int card_formatted(int slot) {
    uint8_t hdr[MEMCARD_HOST_SECTOR_SIZE];

    if (memcard_host_ensure(slot) != 0) {
        return 0;
    }
    if (read_frame(slot, 0, hdr) != 0) {
        return 0;
    }
    return hdr[0] == (uint8_t)'M' && hdr[1] == (uint8_t)'C';
}

static void dir_name(const uint8_t *frame, char *out, size_t outlen) {
    size_t i;

    if (outlen == 0) {
        return;
    }
    /* PS1 directory names are a 20-byte field (may omit trailing NUL). */
    for (i = 0; i < 20 && i + 1 < outlen; i++) {
        char c = (char)frame[0x0A + i];
        if (c == '\0') {
            break;
        }
        out[i] = c;
    }
    out[i] = '\0';
}

static int name_eq(const uint8_t *frame, const char *file) {
    char stored[21];
    char want[21];
    size_t n;

    dir_name(frame, stored, sizeof(stored));
    n = strlen(file);
    if (n > 20) {
        n = 20;
    }
    memcpy(want, file, n);
    want[n] = '\0';
    return strcmp(stored, want) == 0;
}

/* Directory frame index 1..15 whose name matches (first block only). */
static int find_file_frame(int slot, const char *file) {
    int frame_i;
    uint8_t frame[MEMCARD_HOST_SECTOR_SIZE];

    for (frame_i = 1; frame_i <= MC_DIR_FRAMES; frame_i++) {
        if (read_frame(slot, frame_i, frame) != 0) {
            return -1;
        }
        if (frame[0] != MC_ALLOC_FIRST) {
            continue;
        }
        if (name_eq(frame, file)) {
            return frame_i;
        }
    }
    return -1;
}

/* Directory bytes 8-9: next block number minus 1 (0..14 → blocks 1..15),
 * or 0xFFFF on the last block. psx-spx "Directory Frames". */
static int dir_next_block(const uint8_t *frame, unsigned *out_block) {
    unsigned link = (unsigned)frame[8] | ((unsigned)frame[9] << 8);

    if (link == 0xFFFFu) {
        return 0;
    }
    if (link >= MC_DIR_FRAMES) {
        return -1;
    }
    *out_block = link + 1u;
    return 1;
}

static void dir_store_next(uint8_t *frame, unsigned next_block) {
    unsigned link;

    if (next_block == 0) {
        frame[8] = 0xFF;
        frame[9] = 0xFF;
        return;
    }
    link = next_block - 1u;
    frame[8] = (uint8_t)(link & 0xFF);
    frame[9] = (uint8_t)((link >> 8) & 0xFF);
}

static int alloc_blocks(int slot, int blocks, int *out_first) {
    int chain[16];
    int n = 0;
    int b;

    for (b = 1; b <= MC_DIR_FRAMES && n < blocks; b++) {
        uint8_t frame[MEMCARD_HOST_SECTOR_SIZE];

        if (read_frame(slot, b, frame) != 0) {
            return -1;
        }
        if (frame[0] == MC_ALLOC_FREE) {
            chain[n++] = b;
        }
    }
    if (n < blocks) {
        return -1;
    }
    *out_first = chain[0];
    for (b = 0; b < n; b++) {
        uint8_t frame[MEMCARD_HOST_SECTOR_SIZE];
        int idx = chain[b];

        memset(frame, 0, sizeof(frame));
        if (b == 0) {
            frame[0] = MC_ALLOC_FIRST;
        } else if (b + 1 == n) {
            frame[0] = MC_ALLOC_LAST;
        } else {
            frame[0] = MC_ALLOC_MID;
        }
        if (b + 1 < n) {
            dir_store_next(frame, (unsigned)chain[b + 1]);
        } else {
            dir_store_next(frame, 0);
        }
        if (write_frame(slot, idx, frame) != 0) {
            return -1;
        }
    }
    return 0;
}

static int file_byte_offset(int slot, int first_frame, long file_ofs,
                            uint32_t *out_abs) {
    uint8_t frame[MEMCARD_HOST_SECTOR_SIZE];
    unsigned block = (unsigned)first_frame;
    long left = file_ofs;

    if (read_frame(slot, (int)block, frame) != 0) {
        return -1;
    }
    while (left >= (long)MC_BLOCK_SIZE) {
        unsigned next = 0;
        int link = dir_next_block(frame, &next);

        if (link != 1 || next == block) {
            return -1;
        }
        block = next;
        left -= (long)MC_BLOCK_SIZE;
        if (read_frame(slot, (int)block, frame) != 0) {
            return -1;
        }
    }
    *out_abs = (uint32_t)block * MC_BLOCK_SIZE + (uint32_t)left;
    return 0;
}

static int ensure_file(int slot, const char *file, long blocks) {
    int first;
    uint8_t frame[MEMCARD_HOST_SECTOR_SIZE];
    uint32_t size;

    first = find_file_frame(slot, file);
    if (first >= 0) {
        return first;
    }
    if (blocks < 1) {
        blocks = 1;
    }
    if (blocks > MC_DIR_FRAMES) {
        return -1;
    }
    if (alloc_blocks(slot, (int)blocks, &first) != 0) {
        return -1;
    }
    if (read_frame(slot, first, frame) != 0) {
        return -1;
    }
    size = (uint32_t)blocks * MC_BLOCK_SIZE;
    frame[4] = (uint8_t)(size & 0xFF);
    frame[5] = (uint8_t)((size >> 8) & 0xFF);
    frame[6] = (uint8_t)((size >> 16) & 0xFF);
    frame[7] = (uint8_t)((size >> 24) & 0xFF);
    memset(frame + 0x0A, 0, 20);
    {
        size_t n = strlen(file);
        if (n > 20) {
            n = 20;
        }
        memcpy(frame + 0x0A, file, n);
    }
    if (write_frame(slot, first, frame) != 0) {
        return -1;
    }
    fprintf(stderr, "dw3: MemCardCreate auto file=%s blocks=%ld frame=%d\n", file,
            blocks, first);
    return first;
}

static int rw_file_bytes(int slot, int first_frame, long offset,
                         unsigned char *buf, long bytes, int do_write) {
    long done = 0;

    while (done < bytes) {
        uint32_t abs_off;
        uint8_t frame[MEMCARD_HOST_SECTOR_SIZE];
        uint32_t frame_off;
        uint32_t sector;
        size_t chunk;

        if (file_byte_offset(slot, first_frame, offset + done, &abs_off) != 0) {
            return -1;
        }
        sector = abs_off / MEMCARD_HOST_SECTOR_SIZE;
        frame_off = abs_off % MEMCARD_HOST_SECTOR_SIZE;
        if (read_frame(slot, (int)sector, frame) != 0) {
            return -1;
        }
        chunk = MEMCARD_HOST_SECTOR_SIZE - frame_off;
        if (chunk > (size_t)(bytes - done)) {
            chunk = (size_t)(bytes - done);
        }
        if (do_write) {
            memcpy(frame + frame_off, buf + done, chunk);
            if (write_frame(slot, (int)sector, frame) != 0) {
                return -1;
            }
        } else {
            memcpy(buf + done, frame + frame_off, chunk);
        }
        done += (long)chunk;
    }
    return 0;
}

void MemCardInit(long val) {
    (void)val;
    g_started = 0;
    g_pending.active = 0;
    g_pending.cmds = 0;
    g_pending.result = McErrNone;
}

void MemCardStart(void) { g_started = 1; }

long MemCardSync(long mode, long *cmds, long *result) {
    (void)mode;
    if (!g_started) {
        return -1;
    }
    if (g_pending.active) {
        g_pending.active = 0;
        if (cmds) {
            *cmds = g_pending.cmds;
        }
        if (result) {
            *result = g_pending.result;
        }
        return 1;
    }
    if (cmds) {
        *cmds = g_pending.cmds;
    }
    if (result) {
        *result = g_pending.result;
    }
    return 1;
}

long MemCardExist(long chan) {
    int slot = chan_to_slot(chan);

    if (slot < 0) {
        return 0;
    }
    if (memcard_host_ensure(slot) != 0) {
        pending_set(McFuncExist, McErrCardNotExist);
        return 1;
    }
    pending_set(McFuncExist, McErrNone);
    return 1;
}

long MemCardAccept(long chan) {
    int slot = chan_to_slot(chan);

    if (slot < 0) {
        return 0;
    }
    if (memcard_host_ensure(slot) != 0) {
        pending_set(McFuncAccept, McErrCardNotExist);
        return 1;
    }
    if (!card_formatted(slot)) {
        pending_set(McFuncAccept, McErrNotFormat);
        return 1;
    }
    pending_set(McFuncAccept, McErrNone);
    return 1;
}

long MemCardReadFile(long chan, char *file, unsigned long *adrs, long offset,
                     long bytes) {
    int slot = chan_to_slot(chan);
    int first;

    if (slot < 0 || file == NULL || adrs == NULL || bytes <= 0 ||
        (bytes & (long)(MEMCARD_HOST_SECTOR_SIZE - 1)) != 0) {
        return 0;
    }
    first = find_file_frame(slot, file);
    if (first < 0) {
        pending_set(McFuncReadFile, McErrFileNotExist);
        return 1;
    }
    if (rw_file_bytes(slot, first, offset, (unsigned char *)adrs, bytes, 0) !=
        0) {
        pending_set(McFuncReadFile, McErrCardInvalid);
        return 1;
    }
    pending_set(McFuncReadFile, McErrNone);
    return 1;
}

long MemCardWriteFile(long chan, char *file, unsigned long *adrs, long offset,
                      long bytes) {
    int slot = chan_to_slot(chan);
    int first;

    if (slot < 0 || file == NULL || adrs == NULL || bytes <= 0 ||
        (bytes & (long)(MEMCARD_HOST_SECTOR_SIZE - 1)) != 0) {
        return 0;
    }
    /* Auto-create a 4-block DW3 save when missing (force-save bring-up). */
    first = ensure_file(slot, file, 4);
    if (first < 0) {
        pending_set(McFuncWriteFile, McErrBlockFull);
        return 1;
    }
    if (rw_file_bytes(slot, first, offset, (unsigned char *)adrs, bytes, 1) !=
        0) {
        pending_set(McFuncWriteFile, McErrCardInvalid);
        return 1;
    }
    pending_set(McFuncWriteFile, McErrNone);
    return 1;
}

long MemCardGetDirentry(long chan, char *name, HostDirEntry *dir, long *files,
                        long ofs, long max) {
    int slot = chan_to_slot(chan);
    long count = 0;
    int frame_i;
    char fname[24];
    uint8_t frame[MEMCARD_HOST_SECTOR_SIZE];
    HostDirEntry *out = dir;

    (void)name;
    (void)ofs;
    if (slot < 0) {
        return McErrCardNotExist;
    }
    if (!card_formatted(slot)) {
        if (files) {
            *files = 0;
        }
        return McErrNotFormat;
    }
    for (frame_i = 1; frame_i <= MC_DIR_FRAMES && (max <= 0 || count < max);
         frame_i++) {
        uint32_t size;

        if (read_frame(slot, frame_i, frame) != 0) {
            break;
        }
        if (frame[0] != MC_ALLOC_FIRST) {
            continue;
        }
        dir_name(frame, fname, sizeof(fname));
        size = (uint32_t)frame[4] | ((uint32_t)frame[5] << 8) |
               ((uint32_t)frame[6] << 16) | ((uint32_t)frame[7] << 24);
        if (out != NULL) {
            memset(&out[count], 0, sizeof(out[count]));
            strncpy(out[count].name, fname, sizeof(out[count].name) - 1);
            out[count].size = (int32_t)size;
            out[count].head = frame_i;
        }
        count++;
    }
    if (files) {
        *files = count;
    }
    return McErrNone;
}

long MemCardCreateFile(long chan, char *filename, long blocks) {
    int slot = chan_to_slot(chan);
    int first;

    if (slot < 0 || filename == NULL) {
        return McErrCardInvalid;
    }
    if (!card_formatted(slot)) {
        return McErrNotFormat;
    }
    if (find_file_frame(slot, filename) >= 0) {
        return McErrAlreadyExist;
    }
    first = ensure_file(slot, filename, blocks);
    if (first < 0) {
        return McErrBlockFull;
    }
    return McErrNone;
}

long MemCardFormat(long chan) {
    int slot = chan_to_slot(chan);
    uint8_t *img;
    uint32_t off;

    if (slot < 0) {
        return McErrCardInvalid;
    }
    img = (uint8_t *)malloc(MEMCARD_HOST_SIZE);
    if (img == NULL) {
        return McErrCardInvalid;
    }
    memcard_host_format_image(img);
    for (off = 0; off < MEMCARD_HOST_SIZE; off += MEMCARD_HOST_SECTOR_SIZE) {
        if (memcard_host_write_sector(
                slot, (int)(off / MEMCARD_HOST_SECTOR_SIZE), img + off) != 0) {
            free(img);
            return McErrCardInvalid;
        }
    }
    free(img);
    fprintf(stderr, "dw3: MemCardFormat slot=%d (.mcr)\n", slot);
    return McErrNone;
}

long MemCardUnformat(long chan) {
    int slot = chan_to_slot(chan);
    uint8_t ff[MEMCARD_HOST_SECTOR_SIZE];
    uint32_t sec;

    if (slot < 0) {
        return 0;
    }
    memset(ff, 0xFF, sizeof(ff));
    for (sec = 0; sec < MEMCARD_HOST_SECTORS; sec++) {
        if (memcard_host_write_sector(slot, (int)sec, ff) != 0) {
            return 0;
        }
    }
    return 1;
}
