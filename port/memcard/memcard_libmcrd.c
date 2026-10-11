/*
 * Host libmcrd (MemCard*) backed by port/memcard flat saves in RAM.
 *
 * Digimon World 3 talks MemCard* only. This shim keeps that API and maps
 * each port to saveN.bin (32 KiB resident image). No directory frame walk
 * on the hot path; flush happens when a write burst ends (non-write MemCard
 * call or memcard_host_shutdown), not on every 128-byte sector.
 */
#include "memcard_host.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

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

/* After a write burst, any other MemCard* call commits dirty RAM once.
 * Returns -1 if flush failed (caller should surface McErrCardInvalid). */
static int flush_if_dirty(int slot) {
    if (slot < 0) {
        return -1;
    }
    if (memcard_host_flush(slot) != 0 || memcard_host_flush_failed(slot)) {
        return -1;
    }
    return 0;
}

static int name_matches(int slot, const char *file) {
    const char *have;

    if (!memcard_host_has_file(slot) || file == NULL) {
        return 0;
    }
    have = memcard_host_file_name(slot);
    if (have[0] == '\0') {
        /* Naked saveN.bin without sidecar: one file per port. */
        return 1;
    }
    return strcmp(have, file) == 0;
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
    if (flush_if_dirty(slot) != 0) {
        pending_set(McFuncExist, McErrCardInvalid);
        return 1;
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
    if (flush_if_dirty(slot) != 0) {
        pending_set(McFuncAccept, McErrCardInvalid);
        return 1;
    }
    if (memcard_host_ensure(slot) != 0) {
        pending_set(McFuncAccept, McErrCardNotExist);
        return 1;
    }
    /* Flat ports are always "formatted"; MemCardFormat clears the save. */
    pending_set(McFuncAccept, McErrNone);
    return 1;
}

long MemCardReadFile(long chan, char *file, unsigned long *adrs, long offset,
                     long bytes) {
    int slot = chan_to_slot(chan);

    if (slot < 0 || file == NULL || adrs == NULL || bytes <= 0 ||
        (bytes & (long)(MEMCARD_HOST_SECTOR_SIZE - 1)) != 0) {
        return 0;
    }
    if (flush_if_dirty(slot) != 0) {
        pending_set(McFuncReadFile, McErrCardInvalid);
        return 1;
    }
    if (memcard_host_ensure(slot) != 0) {
        pending_set(McFuncReadFile, McErrCardNotExist);
        return 1;
    }
    if (!name_matches(slot, file)) {
        pending_set(McFuncReadFile, McErrFileNotExist);
        return 1;
    }
    if (memcard_host_file_read(slot, (uint32_t)offset, adrs, (size_t)bytes) !=
        (int)bytes) {
        pending_set(McFuncReadFile, McErrCardInvalid);
        return 1;
    }
    pending_set(McFuncReadFile, McErrNone);
    return 1;
}

long MemCardWriteFile(long chan, char *file, unsigned long *adrs, long offset,
                      long bytes) {
    int slot = chan_to_slot(chan);

    if (slot < 0 || file == NULL || adrs == NULL || bytes <= 0 ||
        (bytes & (long)(MEMCARD_HOST_SECTOR_SIZE - 1)) != 0) {
        return 0;
    }
    if (memcard_host_ensure(slot) != 0) {
        pending_set(McFuncWriteFile, McErrCardNotExist);
        return 1;
    }
    if (!memcard_host_has_file(slot)) {
        if (memcard_host_create_file(slot, file) != 0) {
            pending_set(McFuncWriteFile, McErrBlockFull);
            return 1;
        }
    } else if (!name_matches(slot, file)) {
        pending_set(McFuncWriteFile, McErrFileNotExist);
        return 1;
    }
    if (memcard_host_file_write(slot, (uint32_t)offset, adrs, (size_t)bytes) !=
        (int)bytes) {
        pending_set(McFuncWriteFile, McErrCardInvalid);
        return 1;
    }
    /* Stay dirty until the write section ends (next non-write MemCard*). */
    pending_set(McFuncWriteFile, McErrNone);
    return 1;
}

long MemCardGetDirentry(long chan, char *name, HostDirEntry *dir, long *files,
                        long ofs, long max) {
    int slot = chan_to_slot(chan);
    HostDirEntry *out = dir;
    const char *fname;
    long count = 0;

    (void)name;
    (void)ofs;
    if (slot < 0) {
        return McErrCardNotExist;
    }
    if (flush_if_dirty(slot) != 0) {
        if (files) {
            *files = 0;
        }
        return McErrCardInvalid;
    }
    if (memcard_host_ensure(slot) != 0) {
        if (files) {
            *files = 0;
        }
        return McErrCardNotExist;
    }
    if (memcard_host_has_file(slot) && (max <= 0 || count < max)) {
        fname = memcard_host_file_name(slot);
        if (out != NULL) {
            memset(&out[count], 0, sizeof(out[count]));
            if (fname[0] != '\0') {
                strncpy(out[count].name, fname, sizeof(out[count].name) - 1);
            } else {
                strncpy(out[count].name, "BASLUS-01436DMW3-USA",
                        sizeof(out[count].name) - 1);
            }
            /* STGMCARD_askError sums size / 0x2000; 4 blocks => 0x8000. */
            out[count].size = (int32_t)MEMCARD_FLAT_SIZE;
            out[count].head = 1;
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

    (void)blocks;
    if (slot < 0 || filename == NULL) {
        return McErrCardInvalid;
    }
    if (flush_if_dirty(slot) != 0) {
        return McErrCardInvalid;
    }
    if (memcard_host_ensure(slot) != 0) {
        return McErrCardNotExist;
    }
    if (memcard_host_has_file(slot)) {
        return McErrAlreadyExist;
    }
    if (memcard_host_create_file(slot, filename) != 0) {
        return McErrBlockFull;
    }
    fprintf(stderr, "dw3: MemCardCreate file=%s flat=%s\n", filename,
            memcard_host_slot_name(slot));
    return McErrNone;
}

long MemCardFormat(long chan) {
    int slot = chan_to_slot(chan);

    if (slot < 0) {
        return McErrCardInvalid;
    }
    if (memcard_host_format_slot(slot) != 0) {
        return McErrCardInvalid;
    }
    fprintf(stderr, "dw3: MemCardFormat slot=%d (flat)\n", slot);
    return McErrNone;
}

long MemCardUnformat(long chan) {
    int slot = chan_to_slot(chan);

    if (slot < 0) {
        return 0;
    }
    if (memcard_host_format_slot(slot) != 0) {
        return 0;
    }
    return 1;
}
