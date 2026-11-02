#define _POSIX_C_SOURCE 200809L

#include "memcard_host.h"
#include "save_convert.h"
#include "save_store.h"
#include "port_stderr.h"

#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#ifdef _WIN32
#include <io.h>
#else
#include <unistd.h>
#endif

#define MEMCARD_DIR_MAX 512
#define MEMCARD_PATH_MAX 640
#define MC_BLOCK_SIZE 0x2000u
#define MC_DIR_FRAMES 15
#define MC_ALLOC_FIRST 0x51u

typedef struct MemSlot {
    uint8_t *data;
    char name[21];
    int present; /* resident flat save loaded or created */
    int dirty;
    int section_open; /* a game write section is in flight (RAM is partial) */
    int flush_failed;
    unsigned flush_count;
} MemSlot;

/* Product file names Digimon World 3 uses (decomp SAVE_FILE_NAMES). */
static const char *const k_dw3_save_names[] = {
    "BASLUS-01436DMW3-USA",
    "BESLES-03936DMW3-EUR",
    "BISLPS-03446DMW3-JPN",
};

static char g_dir[MEMCARD_DIR_MAX];
/* P6.25: folder of the other region's saves to import from, and this build's
 * region (SAVE_REGION_*). Empty = no cross-region import. */
static char g_import_dir[MEMCARD_DIR_MAX];
static int g_own_region;
static MemSlot g_slots[MEMCARD_HOST_ALL];

static uint64_t mono_us(void) {
    struct timespec ts;

    if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0) {
        return 0;
    }
    return (uint64_t)ts.tv_sec * 1000000ull + (uint64_t)ts.tv_nsec / 1000ull;
}

static uint8_t frame_checksum(const uint8_t *frame) {
    uint8_t x = 0;
    int i;

    for (i = 0; i < 127; i++) {
        x ^= frame[i];
    }
    return x;
}

const char *memcard_host_slot_name(int slot) {
    if (slot == 0) {
        return "save0.bin";
    }
    if (slot == 1) {
        return "save1.bin";
    }
    if (slot == MEMCARD_HOST_AUTO) {
        return "autosave.bin";
    }
    return NULL;
}

const char *memcard_host_legacy_slot_name(int slot) {
    if (slot == 0) {
        return "memcard0.mcr";
    }
    if (slot == 1) {
        return "memcard1.mcr";
    }
    return NULL;
}

void memcard_host_format_image(uint8_t *buf) {
    int s;

    if (buf == NULL) {
        return;
    }

    memset(buf, 0xFF, MEMCARD_HOST_SIZE);

    memset(buf, 0x00, MEMCARD_HOST_SECTOR_SIZE);
    buf[0] = (uint8_t)'M';
    buf[1] = (uint8_t)'C';
    buf[0x7F] = frame_checksum(buf);

    for (s = 1; s <= 15; s++) {
        uint8_t *f = buf + (size_t)s * MEMCARD_HOST_SECTOR_SIZE;
        memset(f, 0x00, MEMCARD_HOST_SECTOR_SIZE);
        f[0] = 0xA0; /* free */
        f[8] = 0xFF;
        f[9] = 0xFF;
        f[0x7F] = frame_checksum(f);
    }

    for (s = 16; s <= 35; s++) {
        uint8_t *f = buf + (size_t)s * MEMCARD_HOST_SECTOR_SIZE;
        memset(f, 0x00, MEMCARD_HOST_SECTOR_SIZE);
        f[0] = 0xFF;
        f[1] = 0xFF;
        f[2] = 0xFF;
        f[3] = 0xFF;
        f[8] = 0xFF;
        f[9] = 0xFF;
        f[0x7F] = frame_checksum(f);
    }

    for (s = 36; s <= 62; s++) {
        memset(buf + (size_t)s * MEMCARD_HOST_SECTOR_SIZE, 0x00,
               MEMCARD_HOST_SECTOR_SIZE);
    }

    memcpy(buf + 63u * MEMCARD_HOST_SECTOR_SIZE, buf, MEMCARD_HOST_SECTOR_SIZE);
}

static int slot_ok(int slot) {
    return slot >= 0 && slot < MEMCARD_HOST_ALL;
}

static int join_path(char *out, size_t outlen, const char *leaf) {
    int n;

    if (g_dir[0] == '\0' || leaf == NULL) {
        return -1;
    }
    n = snprintf(out, outlen, "%s/%s", g_dir, leaf);
    if (n < 0 || (size_t)n >= outlen) {
        return -1;
    }
    return 0;
}

static int flat_path(int slot, char *out, size_t outlen) {
    const char *name = memcard_host_slot_name(slot);
    if (name == NULL) {
        return -1;
    }
    return join_path(out, outlen, name);
}

static int name_path(int slot, char *out, size_t outlen) {
    char leaf[32];
    int n;

    if (slot == MEMCARD_HOST_AUTO) {
        n = snprintf(leaf, sizeof(leaf), "autosave.name");
    } else {
        n = snprintf(leaf, sizeof(leaf), "save%d.name", slot);
    }
    if (n < 0 || (size_t)n >= sizeof(leaf)) {
        return -1;
    }
    return join_path(out, outlen, leaf);
}

static int legacy_path(int slot, char *out, size_t outlen) {
    const char *name = memcard_host_legacy_slot_name(slot);
    if (name == NULL) {
        return -1;
    }
    return join_path(out, outlen, name);
}

static void slot_clear_ram(int slot) {
    MemSlot *s;

    if (!slot_ok(slot)) {
        return;
    }
    s = &g_slots[slot];
    free(s->data);
    s->data = NULL;
    s->name[0] = '\0';
    s->present = 0;
    s->dirty = 0;
    s->section_open = 0;
    s->flush_failed = 0;
}

static int slot_alloc(int slot) {
    MemSlot *s;

    if (!slot_ok(slot)) {
        return -1;
    }
    s = &g_slots[slot];
    if (s->data == NULL) {
        s->data = (uint8_t *)malloc(MEMCARD_FLAT_SIZE);
        if (s->data == NULL) {
            return -1;
        }
    }
    memset(s->data, 0, MEMCARD_FLAT_SIZE);
    s->present = 1;
    s->dirty = 0;
    return 0;
}

static void store_name(int slot, const char *name) {
    size_t n;

    if (!slot_ok(slot) || name == NULL) {
        return;
    }
    n = strlen(name);
    if (n > 20) {
        n = 20;
    }
    memcpy(g_slots[slot].name, name, n);
    g_slots[slot].name[n] = '\0';
}

static int atomic_write_file(const char *path, const uint8_t *data, size_t len);

static int write_name_sidecar(int slot) {
    char path[MEMCARD_PATH_MAX];

    if (!slot_ok(slot) || g_slots[slot].name[0] == '\0') {
        return 0;
    }
    if (name_path(slot, path, sizeof(path)) != 0) {
        return -1;
    }
    return atomic_write_file(path, (const uint8_t *)g_slots[slot].name,
                             strlen(g_slots[slot].name));
}

static int read_name_sidecar(int slot) {
    char path[MEMCARD_PATH_MAX];
    FILE *fp;
    char buf[21];
    size_t n;

    if (name_path(slot, path, sizeof(path)) != 0) {
        return -1;
    }
    fp = fopen(path, "rb");
    if (fp == NULL) {
        return -1;
    }
    n = fread(buf, 1, 20, fp);
    fclose(fp);
    if (n == 0) {
        return -1;
    }
    buf[n] = '\0';
    store_name(slot, buf);
    return 0;
}

static int file_size_is(FILE *fp, long want) {
    long sz;

    if (fseek(fp, 0, SEEK_END) != 0) {
        return 0;
    }
    sz = ftell(fp);
    if (sz != want) {
        return 0;
    }
    if (fseek(fp, 0, SEEK_SET) != 0) {
        return 0;
    }
    return 1;
}

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

static void dir_name(const uint8_t *frame, char *out, size_t outlen) {
    size_t i;

    if (outlen == 0) {
        return;
    }
    for (i = 0; i < 20 && i + 1 < outlen; i++) {
        char c = (char)frame[0x0A + i];
        if (c == '\0') {
            break;
        }
        out[i] = c;
    }
    out[i] = '\0';
}

static int is_dw3_save_name(const char *name) {
    size_t i;

    if (name == NULL || name[0] == '\0') {
        return 0;
    }
    for (i = 0; i < sizeof(k_dw3_save_names) / sizeof(k_dw3_save_names[0]);
         i++) {
        if (strcmp(name, k_dw3_save_names[i]) == 0) {
            return 1;
        }
    }
    return 0;
}

/* Copy one directory-linked file starting at first_frame into dest. */
static int extract_file_chain(const uint8_t *img, int first_frame, uint8_t *dest) {
    unsigned block = (unsigned)first_frame;
    unsigned got = 0;

    memset(dest, 0, MEMCARD_FLAT_SIZE);
    while (got < MEMCARD_FLAT_SIZE) {
        const uint8_t *dir_frame;
        const uint8_t *src;
        unsigned next = 0;
        int link;
        size_t chunk;

        if (block < 1 || block > MC_DIR_FRAMES) {
            return -1;
        }
        src = img + (size_t)block * MC_BLOCK_SIZE;
        chunk = MC_BLOCK_SIZE;
        if (got + chunk > MEMCARD_FLAT_SIZE) {
            chunk = MEMCARD_FLAT_SIZE - got;
        }
        memcpy(dest + got, src, chunk);
        got += (unsigned)chunk;
        if (got >= MEMCARD_FLAT_SIZE) {
            return 0;
        }
        dir_frame = img + (size_t)block * MEMCARD_HOST_SECTOR_SIZE;
        link = dir_next_block(dir_frame, &next);
        if (link != 1) {
            /* Short file: keep what we copied (zero-padded). */
            return 0;
        }
        block = next;
    }
    return 0;
}

/* Extract a Digimon World 3 save by product name, not the first 0x51 entry.
 * Scans all directory frames; leaves the image alone if none match. */
static int extract_flat_from_mcr(const uint8_t *img, uint8_t *dest, char *name_out,
                                 size_t name_outlen) {
    int frame_i;

    for (frame_i = 1; frame_i <= MC_DIR_FRAMES; frame_i++) {
        const uint8_t *dir = img + (size_t)frame_i * MEMCARD_HOST_SECTOR_SIZE;
        char name[21];

        if (dir[0] != MC_ALLOC_FIRST) {
            continue;
        }
        dir_name(dir, name, sizeof(name));
        if (!is_dw3_save_name(name)) {
            continue;
        }
        if (extract_file_chain(img, frame_i, dest) != 0) {
            return -1;
        }
        if (name_out != NULL && name_outlen > 0) {
            size_t n = strlen(name);
            if (n + 1 > name_outlen) {
                n = name_outlen - 1;
            }
            memcpy(name_out, name, n);
            name_out[n] = '\0';
        }
        return 0;
    }
    return -1;
}

static int quarantine_bad_flat(int slot) {
    char path[MEMCARD_PATH_MAX];
    char bad[MEMCARD_PATH_MAX + 8];
    char npath[MEMCARD_PATH_MAX];
    char nbad[MEMCARD_PATH_MAX + 8];
    int n;

    if (flat_path(slot, path, sizeof(path)) != 0) {
        return -1;
    }
    n = snprintf(bad, sizeof(bad), "%s.bad", path);
    if (n < 0 || (size_t)n >= sizeof(bad)) {
        return -1;
    }
    remove(bad);
    if (rename(path, bad) != 0) {
        DW3_STDERRF("dw3: memcard: cannot quarantine %s -> %s (%s)\n", path,
                bad, strerror(errno));
        return -1;
    }
    DW3_STDERRF("dw3: memcard: quarantined corrupt flat as %s\n", bad);
    if (name_path(slot, npath, sizeof(npath)) == 0) {
        n = snprintf(nbad, sizeof(nbad), "%s.bad", npath);
        if (n >= 0 && (size_t)n < sizeof(nbad)) {
            remove(nbad);
            (void)rename(npath, nbad);
        }
    }
    return 0;
}

/* Test hook (P10.2): simulate the process dying at step N of an atomic
 * write, leaving the disk exactly as a kill at that point would. 1 = tmp
 * created and empty, 2 = tmp half written, 3 = tmp complete and closed,
 * 4 = destination removed, 5 = renamed (write complete). The hook fires in
 * the `nth` atomic write after arming (1 = the saveN.bin write, 2 = the
 * saveN.name sidecar that follows it in a flush). 0 = off. The environment
 * variables DW3_MEMCARD_FAULT_STEP / DW3_MEMCARD_FAULT_NTH arm the same hook
 * in any build. */
static int g_test_fault_step;
static int g_test_fault_nth = 1;
static int g_fault_here;
static int g_fault_env_read;

void memcard_host_test_fault_step(int step, int nth) {
    g_test_fault_step = step;
    g_test_fault_nth = nth > 0 ? nth : 1;
    g_fault_env_read = 1;
}

static void fault_begin(void) {
    if (!g_fault_env_read) {
        const char *e = getenv("DW3_MEMCARD_FAULT_STEP");
        const char *n = getenv("DW3_MEMCARD_FAULT_NTH");

        g_fault_env_read = 1;
        if (e != NULL) {
            g_test_fault_step = atoi(e);
            g_test_fault_nth = n != NULL && atoi(n) > 0 ? atoi(n) : 1;
        }
    }
    g_fault_here = 0;
    if (g_test_fault_step != 0 && --g_test_fault_nth <= 0) {
        g_fault_here = 1;
    }
}

static int fault_at(int step) {
    if (g_fault_here && g_test_fault_step == step) {
        g_test_fault_step = 0;
        g_fault_here = 0;
        errno = EINTR;
        return 1;
    }
    return 0;
}

static const char *path_base(const char *path) {
    const char *a = strrchr(path, '/');
    const char *b = strrchr(path, '\\');

    if (b != NULL && (a == NULL || b > a)) {
        a = b;
    }
    return a != NULL ? a + 1 : path;
}

static int atomic_write_file(const char *path, const uint8_t *data, size_t len) {
    char tmp[MEMCARD_PATH_MAX + 8];
    FILE *fp;
    size_t nw;
    int n;
    int err;
    uint64_t t_begin = mono_us();

    fault_begin();
    n = snprintf(tmp, sizeof(tmp), "%s.tmp", path);
    if (n < 0 || (size_t)n >= sizeof(tmp)) {
        return -1;
    }
    fp = fopen(tmp, "wb");
    if (fp == NULL) {
        return -1;
    }
    if (fault_at(1)) {
        fclose(fp);
        return -1;
    }
    nw = fwrite(data, 1, len / 2, fp);
    if (fault_at(2)) {
        fflush(fp);
        fclose(fp);
        return -1;
    }
    nw += fwrite(data + len / 2, 1, len - len / 2, fp);
    /* fsync before the rename: the new name must never point at data that
     * is still only in the SD driver's cache when the power goes. */
    if (nw != len || fflush(fp) != 0 ||
#ifdef _WIN32
        _commit(_fileno(fp)) != 0
#else
        fsync(fileno(fp)) != 0
#endif
        ) {
        err = errno;
        fclose(fp);
        remove(tmp);
        errno = err;
        return -1;
    }
    if (fclose(fp) != 0) {
        err = errno;
        remove(tmp);
        errno = err;
        return -1;
    }
    if (fault_at(3)) {
        return -1;
    }
    /*
     * POSIX rename replaces an existing dest; FAT/exFAT (Switch SD) and
     * Windows return EEXIST ("File exists"). Unlink dest first so the
     * second (and later) flush of saveN.bin succeeds on every host.
     */
    if (remove(path) != 0 && errno != ENOENT) {
        err = errno;
        DW3_STDERRF("dw3: memcard atomic_write: remove(%s) failed: %s\n",
                path, strerror(err));
        remove(tmp);
        errno = err;
        return -1;
    }
    if (fault_at(4)) {
        return -1;
    }
    if (rename(tmp, path) != 0) {
        err = errno;
        DW3_STDERRF(
                "dw3: memcard atomic_write: rename(%s -> %s) failed: %s "
                "(keeping .tmp; dest was unlinked)\n",
                tmp, path, strerror(err));
        /* Do not remove tmp — dest is gone; RAM is still dirty for retry. */
        errno = err;
        return -1;
    }
    (void)fault_at(5);
    DW3_STDERRF("dw3-save: write %s committed (%zu bytes, fsync + rename, "
            "%llu us)\n", path_base(path), len,
            (unsigned long long)(mono_us() - t_begin));
    return 0;
}

/* P10.2: leftovers of an interrupted write, on the next boot. The write is
 * tmp (fsynced) -> remove dest -> rename, so a kill leaves one of: dest only
 * (old file, maybe a stale tmp), a short tmp, or tmp only (dest removed, tmp
 * complete). A tmp the exact size of the file is promoted when dest is
 * missing; every other tmp is dropped. `want` is the size a good file has
 * (0 = any non-empty size, for the name sidecar). */
static void recover_tmp(const char *path, long want) {
    char tmp[MEMCARD_PATH_MAX + 8];
    FILE *fp;
    FILE *tf;
    long tsz = -1;
    int have_dest;

    if (snprintf(tmp, sizeof(tmp), "%s.tmp", path) >= (int)sizeof(tmp)) {
        return;
    }
    tf = fopen(tmp, "rb");
    if (tf == NULL) {
        return;
    }
    if (fseek(tf, 0, SEEK_END) == 0) {
        tsz = ftell(tf);
    }
    fclose(tf);
    fp = fopen(path, "rb");
    have_dest = fp != NULL;
    if (fp != NULL) {
        fclose(fp);
    }
    if (!have_dest && tsz > 0 && (want == 0 || tsz == want) &&
        rename(tmp, path) == 0) {
        DW3_STDERRF("dw3: memcard: recovered %s from an interrupted write\n",
                path);
        DW3_STDERRF("dw3-save: write %s interrupted after the old file "
                "was removed; the complete new file was recovered\n",
                path_base(path));
        return;
    }
    DW3_STDERRF("dw3: memcard: removed stale %s (%ld bytes)\n", tmp, tsz);
    DW3_STDERRF("dw3-save: write %s interrupted, previous file %s\n",
            path_base(path), have_dest ? "kept" : "absent (nothing to keep)");
    remove(tmp);
}

static void recover_slot_tmp(int slot) {
    char path[MEMCARD_PATH_MAX];

    if (flat_path(slot, path, sizeof(path)) == 0) {
        recover_tmp(path, (long)MEMCARD_FLAT_SIZE);
    }
    if (name_path(slot, path, sizeof(path)) == 0) {
        recover_tmp(path, 0);
    }
}

static int load_flat_from_disk(int slot) {
    char path[MEMCARD_PATH_MAX];
    FILE *fp;
    size_t nr;

    if (flat_path(slot, path, sizeof(path)) != 0) {
        return -1;
    }
    fp = fopen(path, "rb");
    if (fp == NULL) {
        return -1;
    }
    if (!file_size_is(fp, (long)MEMCARD_FLAT_SIZE)) {
        fclose(fp);
        return -1;
    }
    if (slot_alloc(slot) != 0) {
        fclose(fp);
        return -1;
    }
    nr = fread(g_slots[slot].data, 1, MEMCARD_FLAT_SIZE, fp);
    fclose(fp);
    if (nr != MEMCARD_FLAT_SIZE) {
        slot_clear_ram(slot);
        return -1;
    }
    g_slots[slot].dirty = 0;
    if (read_name_sidecar(slot) != 0) {
        g_slots[slot].name[0] = '\0';
    }
    return 0;
}

static int migrate_legacy_mcr(int slot) {
    char mcr_path[MEMCARD_PATH_MAX];
    char bak_path[MEMCARD_PATH_MAX + 8];
    char flat[MEMCARD_PATH_MAX];
    FILE *fp;
    uint8_t *img;
    char name[21];
    size_t nr;
    int n;
    uint64_t t0;
    uint64_t dt;

    if (legacy_path(slot, mcr_path, sizeof(mcr_path)) != 0) {
        return -1;
    }
    t0 = mono_us();
    fp = fopen(mcr_path, "rb");
    if (fp == NULL) {
        return 0; /* no legacy file — empty port */
    }
    if (!file_size_is(fp, (long)MEMCARD_HOST_SIZE)) {
        fclose(fp);
        return -1;
    }
    img = (uint8_t *)malloc(MEMCARD_HOST_SIZE);
    if (img == NULL) {
        fclose(fp);
        return -1;
    }
    nr = fread(img, 1, MEMCARD_HOST_SIZE, fp);
    fclose(fp);
    if (nr != MEMCARD_HOST_SIZE) {
        free(img);
        return -1;
    }
    if (slot_alloc(slot) != 0) {
        free(img);
        return -1;
    }
    name[0] = '\0';
    if (extract_flat_from_mcr(img, g_slots[slot].data, name, sizeof(name)) !=
        0) {
        /* No DW3 product save — leave .mcr untouched (do not rename to .bak). */
        free(img);
        slot_clear_ram(slot);
        DW3_STDERRF(
                "dw3: memcard migrate: no DW3 save in %s (left in place)\n",
                mcr_path);
        return 0;
    }
    free(img);
    store_name(slot, name);
    g_slots[slot].dirty = 1;
    if (flat_path(slot, flat, sizeof(flat)) != 0 ||
        memcard_host_flush(slot) != 0) {
        slot_clear_ram(slot);
        return -1;
    }
    (void)write_name_sidecar(slot);

    /* Rename only after a successful DW3 extract + flat write. */
    n = snprintf(bak_path, sizeof(bak_path), "%s.bak", mcr_path);
    if (n >= 0 && (size_t)n < sizeof(bak_path)) {
        if (rename(mcr_path, bak_path) != 0) {
            DW3_STDERRF(
                    "dw3: memcard migrate: kept %s (rename to .bak failed: %s)\n",
                    mcr_path, strerror(errno));
        } else {
            dt = mono_us() - t0;
            DW3_STDERRF(
                    "dw3: memcard migrate slot=%d name=%s -> %s (from %s.bak) "
                    "us=%llu\n",
                    slot, name, flat, memcard_host_legacy_slot_name(slot),
                    (unsigned long long)dt);
        }
    }
    return 0;
}

static const char *region_name(int region) {
    return region == SAVE_REGION_US ? "us" : "eu";
}

int memcard_host_set_cross_import(const char *foreign_dir, int own_region) {
    size_t n;

    g_import_dir[0] = '\0';
    g_own_region = 0;
    if (foreign_dir == NULL || foreign_dir[0] == '\0' ||
        (own_region != SAVE_REGION_US && own_region != SAVE_REGION_EU)) {
        return -1;
    }
    n = strlen(foreign_dir);
    if (n >= MEMCARD_DIR_MAX) {
        return -1;
    }
    memcpy(g_import_dir, foreign_dir, n + 1);
    while (n > 1 && (g_import_dir[n - 1] == '/' || g_import_dir[n - 1] == '\\')) {
        g_import_dir[--n] = '\0';
    }
    g_own_region = own_region;
    return 0;
}

int memcard_host_set_cross_import_sibling(const char *runtime_root,
                                          int own_region) {
    char buf[MEMCARD_DIR_MAX];
    size_t n;
    int other;

    if (runtime_root == NULL ||
        (own_region != SAVE_REGION_US && own_region != SAVE_REGION_EU)) {
        return -1;
    }
    n = strlen(runtime_root);
    if (n < 2 || n + 16 >= sizeof(buf)) {
        return -1;
    }
    memcpy(buf, runtime_root, n + 1);
    while (n > 2 && (buf[n - 1] == '/' || buf[n - 1] == '\\')) {
        buf[--n] = '\0';
    }
    /* The last path component must be this build's own region folder. */
    if (n < 2 || strcmp(buf + n - 2, region_name(own_region)) != 0 ||
        (n > 2 && buf[n - 3] != '/' && buf[n - 3] != '\\' && buf[n - 3] != ':')) {
        return -1;
    }
    other = own_region == SAVE_REGION_US ? SAVE_REGION_EU : SAVE_REGION_US;
    memcpy(buf + n - 2, region_name(other), 2);
    memcpy(buf + n, "/memcard", 9);
    return memcard_host_set_cross_import(buf, own_region);
}

int memcard_host_setup_cross_import(const char *runtime_root, int own_region,
                                    int memdir_is_explicit) {
    const char *off = getenv("DW3_CROSS_IMPORT");
    const char *dir = getenv("DW3_MEMCARD_IMPORT_DIR");
    int rc = -1;

    if (off != NULL && strcmp(off, "0") == 0) {
        DW3_STDERRF("dw3: memcard cross-import disabled (DW3_CROSS_IMPORT=0)\n");
        return -1;
    }
    if (dir != NULL && dir[0] != '\0') {
        rc = memcard_host_set_cross_import(dir, own_region);
    } else if (!memdir_is_explicit) {
        rc = memcard_host_set_cross_import_sibling(runtime_root, own_region);
    }
    if (rc == 0) {
        DW3_STDERRF("dw3: memcard cross-import source=%s own=%s\n",
                g_import_dir, region_name(own_region));
    }
    return rc;
}

static int import_marker_path(int slot, char *out, size_t outlen) {
    char leaf[32];
    int n = snprintf(leaf, sizeof(leaf), "save%d.import", slot);

    if (n < 0 || (size_t)n >= sizeof(leaf)) {
        return -1;
    }
    return join_path(out, outlen, leaf);
}

static int file_exists(const char *path) {
    FILE *fp = fopen(path, "rb");

    if (fp == NULL) {
        return 0;
    }
    fclose(fp);
    return 1;
}

/* P6.25 slice 1: the port has no save of its own and the other region's
 * folder has one. Convert it in memory and write it as this port's save. The
 * source file is only read. Fail closed: anything unexpected leaves the port
 * empty with nothing written. Runs for a port that has no save yet; the
 * `saveN.import` marker keeps a deleted or formatted save from coming back. */
static void cross_import(int slot) {
    char marker[MEMCARD_PATH_MAX];
    char src_path[MEMCARD_PATH_MAX];
    SaveConvertReport rep;
    uint8_t *src;
    FILE *fp;
    FILE *mk;
    size_t nr;
    int s;
    int from_region;

    if (g_import_dir[0] == '\0' || g_own_region == SAVE_REGION_NONE) {
        return;
    }
    from_region = g_own_region == SAVE_REGION_US ? SAVE_REGION_EU : SAVE_REGION_US;
    if (import_marker_path(slot, marker, sizeof(marker)) != 0 ||
        file_exists(marker)) {
        return;
    }
    if (snprintf(src_path, sizeof(src_path), "%s/%s", g_import_dir,
                 memcard_host_slot_name(slot)) >= (int)sizeof(src_path)) {
        return;
    }
    fp = fopen(src_path, "rb");
    if (fp == NULL) {
        return; /* nothing to import */
    }
    if (!file_size_is(fp, (long)SAVE_IMAGE_SIZE)) {
        fclose(fp);
        DW3_STDERRF(
                "dw3: memcard cross-import slot=%d skipped: %s is not a "
                "32 KiB save\n", slot, src_path);
        return;
    }
    src = (uint8_t *)malloc(SAVE_IMAGE_SIZE);
    if (src == NULL) {
        fclose(fp);
        return;
    }
    nr = fread(src, 1, SAVE_IMAGE_SIZE, fp);
    fclose(fp);
    if (nr != SAVE_IMAGE_SIZE || slot_alloc(slot) != 0) {
        free(src);
        slot_clear_ram(slot);
        return;
    }
    if (save_convert_image(src, g_slots[slot].data, g_own_region, &rep) != 0 ||
        rep.usable == 0) {
        DW3_STDERRF(
                "dw3: memcard cross-import slot=%d skipped: nothing usable in "
                "%s (not a DW3 card, or only saves this region cannot load)\n",
                slot, src_path);
        free(src);
        slot_clear_ram(slot);
        return;
    }
    free(src);
    g_slots[slot].name[0] = '\0'; /* naked save: any DW3 product name matches */
    g_slots[slot].dirty = 1;
    if (memcard_host_flush(slot) != 0) {
        DW3_STDERRF("dw3: memcard cross-import slot=%d FAILED to write %s\n",
                slot, memcard_host_slot_name(slot));
        slot_clear_ram(slot);
        return;
    }
    for (s = 0; s < SAVE_SLOT_COUNT; s++) {
        if (rep.warn[s] != 0) {
            DW3_STDERRF(
                    "dw3: memcard cross-import slot=%d save=%d warning: set "
                    "flag bytes with no place in the %s layout were dropped "
                    "from the copy (the original is untouched)\n",
                    slot, s, region_name(g_own_region));
        }
    }
    DW3_STDERRF(
            "dw3: memcard cross-import slot=%d from=%s to=%s converter=%d "
            "usable=%d converted=%d results=%d,%d,%d src=%s\n",
            slot, region_name(from_region), region_name(g_own_region),
            SAVE_CONVERT_VERSION, rep.usable, rep.converted, rep.result[0],
            rep.result[1], rep.result[2], src_path);
    mk = fopen(marker, "wb");
    if (mk != NULL) {
        fprintf(mk,
                "dw3-cross-region-import 1\nconverter=%d\nfrom=%s\nto=%s\n"
                "source=%s\nresults=%d,%d,%d\n",
                SAVE_CONVERT_VERSION, region_name(from_region),
                region_name(g_own_region), src_path, rep.result[0],
                rep.result[1], rep.result[2]);
        fclose(mk);
    }
}

/* ---- P6.25 slice 2: the shared per-profile store (opt-in, save_store=1) ----
 *
 * `cardN.dw3s` in the profile folder holds card N in the canonical layout
 * (save_store.h). While it is on, ports 0 and 1 are served from it: the game
 * sees the card in this build's own layout and the flush folds the slots it
 * wrote back. saveN.bin, saveN.name, saveN.import and the .mcr files are only
 * ever read (migration), never written, renamed or deleted. autosave.bin
 * (port 2) is not part of the store. Anything doubtful turns the store off for
 * the session (the flat files then work exactly as in slice 1) and leaves the
 * files alone. Every decision logs one `dw3-store:` line. */

#define STORE_READ_MAX 0x100000u

typedef struct StoreCardState {
    int exists;     /* cardN.dw3s is on disk (read, or written this session) */
    int content;    /* the card holds a canonical image */
    int formatted;  /* the game formatted the port: the file stays until the next save */
    int deferred_said; /* "not settled yet" was logged for this wait */
    SaveStoreCard card;
    uint8_t *base;  /* the native image shown to the game (SAVE_IMAGE_SIZE) */
} StoreCardState;

static struct {
    int requested;   /* memcard_host_setup_store accepted it */
    char dir[MEMCARD_DIR_MAX];
    int own_region;
    int opened;
    int usable;
    int backed_up[SAVE_STORE_CARDS];
    StoreCardState c[SAVE_STORE_CARDS];
} g_store;

static int store_slot(int slot) {
    return g_store.requested && g_store.usable && slot >= 0 &&
           slot < SAVE_STORE_CARDS;
}

int memcard_host_store_active(int slot) {
    return store_slot(slot);
}

static int store_path(const char *leaf, char *out, size_t outlen) {
    int n = snprintf(out, outlen, "%s/%s", g_store.dir, leaf);

    return (n < 0 || (size_t)n >= outlen) ? -1 : 0;
}

static int store_card_path(int n, char *out, size_t outlen) {
    char leaf[24];

    snprintf(leaf, sizeof(leaf), "card%d.dw3s", n);
    return store_path(leaf, out, outlen);
}

/* 1 = read (len set), 0 = absent, -1 = unreadable or over `cap`. */
static int read_whole(const char *path, uint8_t *buf, size_t cap, size_t *len) {
    FILE *fp = fopen(path, "rb");
    size_t n;

    if (fp == NULL) {
        return errno == ENOENT ? 0 : -1;
    }
    n = fread(buf, 1, cap, fp);
    if (ferror(fp) || n >= cap) {
        fclose(fp);
        return -1;
    }
    fclose(fp);
    *len = n;
    return 1;
}

/* DW3_SAVE_STORE=0|1 wins; else `save_store=0|1` in <cfgdir>/port_options.cfg
 * (the last such line wins; a value that is not 0 or 1 reads as off and is
 * logged). */
int memcard_host_store_requested(const char *cfgdir) {
    const char *env = getenv("DW3_SAVE_STORE");
    char path[MEMCARD_PATH_MAX];
    char text[4200];
    size_t len = 0;
    size_t i = 0;
    int val = 0;

    if (env != NULL && env[0] != '\0') {
        return env[0] == '1' && env[1] == '\0';
    }
    if (cfgdir == NULL || cfgdir[0] == '\0' ||
        snprintf(path, sizeof(path), "%s/port_options.cfg", cfgdir) >=
            (int)sizeof(path) ||
        read_whole(path, (uint8_t *)text, sizeof(text) - 1, &len) != 1) {
        return 0;
    }
    text[len] = '\0';
    while (i < len) {
        char *line = text + i;
        char *nl = memchr(line, '\n', len - i);
        size_t ll = nl != NULL ? (size_t)(nl - line) : len - i;

        i += ll + 1;
        line[ll] = '\0';
        while (ll > 0 && (line[ll - 1] == '\r' || line[ll - 1] == ' ' ||
                          line[ll - 1] == '\t')) {
            line[--ll] = '\0';
        }
        if (strncmp(line, "save_store=", 11) == 0) {
            if ((line[11] == '0' || line[11] == '1') && line[12] == '\0') {
                val = line[11] == '1';
            } else {
                val = 0;
                DW3_STDERRF(
                        "dw3-store: save_store=%s is not valid (0 or 1): the store "
                        "stays off\n", line + 11);
            }
        }
    }
    return val;
}

int memcard_host_setup_store(const char *cfgdir, const char *profile_dir,
                             int own_region) {
    const char *env_dir = getenv("DW3_SAVE_STORE_DIR");
    const char *dir = (env_dir != NULL && env_dir[0] != '\0') ? env_dir : profile_dir;
    size_t n;

    memset(g_store.dir, 0, sizeof(g_store.dir));
    g_store.requested = 0;
    g_store.opened = 0;
    g_store.usable = 0;
    if (!memcard_host_store_requested(cfgdir)) {
        return -1;
    }
    if (dir == NULL || dir[0] == '\0' ||
        (own_region != SAVE_REGION_US && own_region != SAVE_REGION_EU)) {
        DW3_STDERRF(
                "dw3-store: save_store=1 but there is no profile folder (legacy "
                "or explicit memory card folder): the store stays off\n");
        return -1;
    }
    n = strlen(dir);
    if (n >= MEMCARD_DIR_MAX) {
        return -1;
    }
    memcpy(g_store.dir, dir, n + 1);
    while (n > 1 && (g_store.dir[n - 1] == '/' || g_store.dir[n - 1] == '\\')) {
        g_store.dir[--n] = '\0';
    }
    g_store.own_region = own_region;
    g_store.requested = 1;
    DW3_STDERRF("dw3-store: on dir=%s own=%s\n", g_store.dir,
            region_name(own_region));
    return 0;
}

static void store_hex_name(const uint8_t *image, int s, char *out, size_t cap) {
    /* The summary's name is Shift-JIS: print it as hex, never as text. */
    const uint8_t *nm = image + SAVE_INFO_OFFSET + 8 + s * SAVE_SUMMARY_SIZE;
    size_t i;

    out[0] = '\0';
    for (i = 0; i < 4 && i * 2 + 3 < cap; i++) {
        snprintf(out + i * 2, cap - i * 2, "%02x", nm[i]);
    }
}

static int store_write_card(int n, const SaveStoreCard *card) {
    char path[MEMCARD_PATH_MAX];
    uint8_t *buf;
    int rc;

    if (store_card_path(n, path, sizeof(path)) != 0) {
        return -1;
    }
    buf = (uint8_t *)malloc(SAVE_STORE_FILE_SIZE);
    if (buf == NULL) {
        return -1;
    }
    save_store_serialize(card, buf);
    rc = atomic_write_file(path, buf, SAVE_STORE_FILE_SIZE);
    free(buf);
    return rc;
}

/* A copy of the card file as it is on disk, next to it, before the first
 * overwrite of the session. Never rotated or deleted. */
static int store_backup(int n) {
    char path[MEMCARD_PATH_MAX];
    char bak[MEMCARD_PATH_MAX + 40];
    char stamp[32];
    uint8_t *buf;
    uint8_t *chk;
    size_t len = 0;
    size_t clen = 0;
    time_t t;
    struct tm tmv;
    int rc = -1;
    int k;

    if (g_store.backed_up[n]) {
        return 0;
    }
    if (store_card_path(n, path, sizeof(path)) != 0) {
        return -1;
    }
    buf = (uint8_t *)malloc(STORE_READ_MAX);
    chk = (uint8_t *)malloc(STORE_READ_MAX);
    if (buf == NULL || chk == NULL) {
        goto done;
    }
    if (read_whole(path, buf, STORE_READ_MAX, &len) != 1) {
        goto done;
    }
    t = time(NULL);
    if (gmtime_r(&t, &tmv) == NULL ||
        strftime(stamp, sizeof(stamp), "%Y%m%d-%H%M%S", &tmv) == 0) {
        snprintf(stamp, sizeof(stamp), "t%lld", (long long)t);
    }
    for (k = 0; k < 100; k++) {
        int w = k == 0 ? snprintf(bak, sizeof(bak), "%s.%s.bak", path, stamp)
                       : snprintf(bak, sizeof(bak), "%s.%s-%d.bak", path, stamp, k);

        if (w < 0 || (size_t)w >= sizeof(bak)) {
            goto done;
        }
        if (!file_exists(bak)) {
            break;
        }
    }
    if (k == 100 || atomic_write_file(bak, buf, len) != 0) {
        goto done;
    }
    if (read_whole(bak, chk, STORE_READ_MAX, &clen) != 1 || clen != len ||
        memcmp(buf, chk, len) != 0) {
        DW3_STDERRF("dw3-store: backup %s did not read back the same\n",
                path_base(bak));
        goto done;
    }
    DW3_STDERRF("dw3-store: backup card%d.dw3s -> %s (%zu bytes)\n", n,
            path_base(bak), len);
    g_store.backed_up[n] = 1;
    rc = 0;
done:
    free(buf);
    free(chk);
    return rc;
}

/* Source folder of a legacy region's saveN.bin (NULL: none known). */
static const char *store_source_dir(int region) {
    if (region == g_store.own_region) {
        return g_dir[0] != '\0' ? g_dir : NULL;
    }
    return g_import_dir[0] != '\0' ? g_import_dir : NULL;
}

static const char *import_outcome_name(int o) {
    switch (o) {
    case SAVE_IMPORT_IMPORTED: return "imported";
    case SAVE_IMPORT_DUPLICATE: return "duplicate of one already imported";
    case SAVE_IMPORT_INVALID: return "refused: wrong version or checksum";
    case SAVE_IMPORT_LEFTOVER: return "left in the legacy folder: all 6 slots taken";
    case SAVE_IMPORT_SOURCE_BAD: return "refused: not a DW3 card";
    default: return "empty";
    }
}

/* First start with the store on and no imported.txt: copy the legacy flat
 * saves in (the originals are only read) and write imported.txt last. */
static void store_migrate(void) {
    static const int order[SAVE_IMPORT_MAX_SOURCES][2] = {
        {SAVE_REGION_US, 0}, {SAVE_REGION_EU, 0},
        {SAVE_REGION_US, 1}, {SAVE_REGION_EU, 1}};
    char marker[MEMCARD_PATH_MAX];
    char spath[SAVE_IMPORT_MAX_SOURCES][MEMCARD_PATH_MAX];
    uint8_t *img[SAVE_IMPORT_MAX_SOURCES] = {NULL, NULL, NULL, NULL};
    SaveImportSource src[SAVE_IMPORT_MAX_SOURCES];
    SaveImportReport rep;
    SaveStoreCard *cards;
    char text[2048];
    size_t tl = 0;
    int i;
    int s;
    int n;
    int failed = 0;

    if (store_path("imported.txt", marker, sizeof(marker)) != 0) {
        g_store.usable = 0;
        return;
    }
    cards = (SaveStoreCard *)malloc(sizeof(SaveStoreCard) * SAVE_STORE_CARDS);
    if (cards == NULL) {
        g_store.usable = 0;
        return;
    }
    for (i = 0; i < SAVE_IMPORT_MAX_SOURCES; i++) {
        const char *d = store_source_dir(order[i][0]);
        size_t len = 0;

        src[i].image = NULL;
        spath[i][0] = '\0';
        if (d == NULL ||
            snprintf(spath[i], sizeof(spath[i]), "%s/%s", d,
                     memcard_host_slot_name(order[i][1])) >= (int)sizeof(spath[i])) {
            spath[i][0] = '\0';
            continue;
        }
        img[i] = (uint8_t *)malloc(MEMCARD_FLAT_SIZE + 1);
        if (img[i] == NULL) {
            failed = 1;
            continue;
        }
        if (read_whole(spath[i], img[i], MEMCARD_FLAT_SIZE + 1, &len) != 1 ||
            len != MEMCARD_FLAT_SIZE) {
            if (file_exists(spath[i])) {
                DW3_STDERRF(
                        "dw3-store: migrate skipped %s: not a 32 KiB save (%zu bytes)\n",
                        spath[i], len);
            }
            free(img[i]);
            img[i] = NULL;
            continue;
        }
        src[i].image = img[i];
    }
    if (failed) {
        g_store.usable = 0;
        goto out;
    }
    (void)save_store_import(cards, src, SAVE_IMPORT_MAX_SOURCES, &rep);
    for (i = 0; i < SAVE_IMPORT_MAX_SOURCES; i++) {
        for (s = 0; s < SAVE_SLOT_COUNT; s++) {
            int o = rep.outcome[i][s];
            char nm[16];

            if (o == SAVE_IMPORT_EMPTY || src[i].image == NULL) {
                continue;
            }
            store_hex_name(src[i].image, s, nm, sizeof(nm));
            DW3_STDERRF(
                    "dw3-store: migrate %s slot=%d (%s, name %s...): %s",
                    spath[i], s,
                    rep.src_region[i][s] != 0 ? region_name(rep.src_region[i][s]) : "?",
                    nm, import_outcome_name(o));
            if (rep.target[i][s] >= 0) {
                DW3_STDERRF(" -> card%d slot %d", rep.target[i][s] / SAVE_SLOT_COUNT,
                        rep.target[i][s] % SAVE_SLOT_COUNT);
            }
            DW3_STDERRF("\n");
            tl += (size_t)snprintf(text + tl, sizeof(text) - tl,
                                   "source=%s slot=%d result=%s\n", spath[i], s,
                                   import_outcome_name(o));
            if (tl >= sizeof(text)) {
                tl = sizeof(text) - 1;
            }
        }
    }
    for (n = 0; n < SAVE_STORE_CARDS; n++) {
        if (!rep.card_used[n]) {
            continue;
        }
        if (g_store.c[n].exists) {
            DW3_STDERRF(
                    "dw3-store: migrate: card%d.dw3s already exists, kept as it is\n", n);
            continue;
        }
        if (store_write_card(n, &cards[n]) != 0) {
            DW3_STDERRF(
                    "dw3-store: migrate FAILED to write card%d.dw3s: the store is "
                    "off this session, imported.txt not written\n", n);
            g_store.usable = 0;
            goto out;
        }
        g_store.c[n].card = cards[n];
        g_store.c[n].exists = 1;
        g_store.c[n].content = 1;
    }
    DW3_STDERRF(
            "dw3-store: migration done imported=%d duplicate=%d refused=%d "
            "left_in_legacy=%d (originals untouched)\n",
            rep.imported, rep.duplicates, rep.invalid, rep.leftover);
    if (rep.leftover > 0) {
        DW3_STDERRF(
                "dw3-store: migration: %d save(s) did not fit in the 6 slots and "
                "stay in the legacy folder (see imported.txt)\n", rep.leftover);
    }
    {
        char head[256];
        int hl = snprintf(head, sizeof(head),
                          "dw3-store-import 1\nconverter=%d\nimported=%d\nduplicate=%d\n"
                          "refused=%d\nleft_in_legacy=%d\n", SAVE_CONVERT_VERSION,
                          rep.imported, rep.duplicates, rep.invalid, rep.leftover);
        char full[sizeof(head) + sizeof(text)];

        memcpy(full, head, (size_t)hl);
        memcpy(full + hl, text, tl);
        if (atomic_write_file(marker, (const uint8_t *)full, (size_t)hl + tl) != 0) {
            DW3_STDERRF(
                    "dw3-store: migrate: could not write imported.txt (the import "
                    "runs again next start; existing cards are kept)\n");
        }
    }
out:
    for (i = 0; i < SAVE_IMPORT_MAX_SOURCES; i++) {
        free(img[i]);
    }
    free(cards);
}

/* Once per process: read both cards, migrate on the very first start. 0 = the
 * store serves ports 0 and 1; -1 = it is off for this session. */
static int store_open_once(void) {
    char marker[MEMCARD_PATH_MAX];
    uint8_t *buf;
    int n;

    if (g_store.opened) {
        return g_store.usable ? 0 : -1;
    }
    g_store.opened = 1;
    g_store.usable = 1;
    (void)mkdir(g_store.dir, 0777);
    buf = (uint8_t *)malloc(STORE_READ_MAX);
    if (buf == NULL) {
        g_store.usable = 0;
        return -1;
    }
    for (n = 0; n < SAVE_STORE_CARDS && g_store.usable; n++) {
        char path[MEMCARD_PATH_MAX];
        size_t len = 0;
        int rc;
        int st;

        if (store_card_path(n, path, sizeof(path)) != 0) {
            g_store.usable = 0;
            break;
        }
        recover_tmp(path, (long)SAVE_STORE_FILE_SIZE);
        rc = read_whole(path, buf, STORE_READ_MAX, &len);
        if (rc == 0) {
            continue;
        }
        st = rc < 0 ? SAVE_STORE_SHORT : save_store_parse(buf, len, &g_store.c[n].card);
        if (st != SAVE_STORE_OK) {
            DW3_STDERRF(
                    "dw3-store: card%d.dw3s refused: %s (file left untouched); the "
                    "store is off this session and the flat saves are used\n", n,
                    rc < 0 ? "unreadable" : save_store_status_name(st));
            g_store.usable = 0;
            break;
        }
        g_store.c[n].exists = 1;
        g_store.c[n].content = 1;
        DW3_STDERRF("dw3-store: read card%d.dw3s crc ok last_writer=%s\n", n,
                g_store.c[n].card.last_region == SAVE_REGION_US
                    ? "us"
                    : (g_store.c[n].card.last_region == SAVE_REGION_EU ? "eu" : "?"));
    }
    free(buf);
    if (g_store.usable && store_path("imported.txt", marker, sizeof(marker)) == 0 &&
        !file_exists(marker)) {
        store_migrate();
    }
    if (!g_store.usable) {
        return -1;
    }
    return 0;
}

/* Serve port `slot` from the store. 0 = served (the port may be empty);
 * 1 = the store is off, take the flat path. */
static int store_ensure(int slot) {
    StoreCardState *sc;
    MemSlot *ms = &g_slots[slot];
    SaveConvertReport rep;

    if (store_open_once() != 0) {
        return 1;
    }
    sc = &g_store.c[slot];
    if (sc->formatted || !sc->content) {
        return 0; /* empty port: the game creates its save */
    }
    if (slot_alloc(slot) != 0) {
        return 1;
    }
    if (save_store_to_native(&sc->card, g_store.own_region, ms->data, &rep) != 0) {
        DW3_STDERRF("dw3-store: card%d.dw3s has no usable info section: the "
                "store is off this session\n", slot);
        slot_clear_ram(slot);
        g_store.usable = 0;
        return 1;
    }
    if (sc->base == NULL) {
        sc->base = (uint8_t *)malloc(SAVE_IMAGE_SIZE);
    }
    if (sc->base == NULL) {
        slot_clear_ram(slot);
        g_store.usable = 0;
        return 1;
    }
    memcpy(sc->base, ms->data, SAVE_IMAGE_SIZE);
    ms->name[0] = '\0'; /* any DW3 product name matches (design: name binding) */
    ms->dirty = 0;
    DW3_STDERRF(
            "dw3-store: open card%d as %s results=%d,%d,%d (0 empty 1 native "
            "2 converted 3 refused: the game shows error 8) usable=%d\n",
            slot, region_name(g_store.own_region), rep.result[0], rep.result[1],
            rep.result[2], rep.usable);
    for (int k = 0; k < SAVE_SLOT_COUNT; k++) {
        if (rep.result[k] == SAVE_SLOT_REFUSED) {
            DW3_STDERRF(
                    "dw3-store: card%d slot %d needs the EU extra chapter: the %s "
                    "build cannot load it (error 8), the bytes stay untouched in "
                    "the store\n", slot, k, region_name(g_store.own_region));
        }
        if (rep.warn[k] != 0) {
            DW3_STDERRF(
                    "dw3-store: card%d slot %d warning: set flag bytes with no "
                    "place in the %s layout are not shown (kept in the store)\n",
                    slot, k, region_name(g_store.own_region));
        }
    }
    return 0;
}

static int g_store_final; /* the process is ending: the last flush decides */

static int store_flush(int slot) {
    StoreCardState *sc = &g_store.c[slot];
    MemSlot *ms = &g_slots[slot];
    SaveStoreWriteReport rep;
    SaveStoreCard *work;
    char path[MEMCARD_PATH_MAX];
    int rc = -1;
    int frc;

    work = (SaveStoreCard *)malloc(sizeof(SaveStoreCard));
    if (work == NULL) {
        ms->flush_failed = 1;
        return -1;
    }
    *work = sc->card;
    frc = save_store_from_native(work, g_store.own_region, ms->data,
                                 sc->formatted ? NULL : sc->base, SAVE_STORE_LANG_NONE,
                                 g_store_final, &rep);
    if (frc > 0 && !g_store_final) {
        /* Between two bursts of one game save (header, info, slot): wait for
         * the rest, nothing is written, the port stays dirty. Said once. */
        if (!sc->deferred_said) {
            DW3_STDERRF("dw3-store: card%d not settled yet (%s): waiting\n", slot,
                    rep.reason);
            sc->deferred_said = 1;
        }
        ms->flush_failed = 0;
        rc = 0;
        goto out;
    }
    if (frc != 0) {
        DW3_STDERRF(
                "dw3-store: REFUSED to write card%d: %s (nothing written)\n", slot,
                rep.reason);
        ms->flush_failed = 1;
        goto out;
    }
    sc->deferred_said = 0;
    if (rep.changed == 0 && !rep.header_changed) {
        ms->dirty = 0;
        ms->flush_failed = 0;
        rc = 0;
        goto out;
    }
    if (!sc->exists && !sc->formatted &&
        work->image[SAVE_INFO_OFFSET + 8] == 0 &&
        work->image[SAVE_INFO_OFFSET + 8 + SAVE_SUMMARY_SIZE] == 0 &&
        work->image[SAVE_INFO_OFFSET + 8 + 2 * SAVE_SUMMARY_SIZE] == 0) {
        /* A new card with no save in it (the game formats the file before the
         * first save): nothing worth a file yet. The RAM image stays. */
        ms->dirty = 0;
        ms->flush_failed = 0;
        rc = 0;
        goto out;
    }
    if (sc->formatted) {
        /* The formatted port starts a fresh card: slots the game did not write
         * are empty, so nothing of the old card is carried into the new one
         * (the old file is backed up before it is replaced). */
        memset(work->meta, 0, sizeof(work->meta));
    }
    if (sc->exists && store_backup(slot) != 0) {
        DW3_STDERRF(
                "dw3-store: backup of card%d.dw3s FAILED: not writing (the old file "
                "is untouched)\n", slot);
        ms->flush_failed = 1;
        goto out;
    }
    if (store_write_card(slot, work) != 0) {
        if (store_card_path(slot, path, sizeof(path)) != 0) {
            path[0] = '\0';
        }
        DW3_STDERRF("dw3-store: write card%d.dw3s FAILED (%s)\n", slot,
                strerror(errno));
        ms->flush_failed = 1;
        goto out;
    }
    sc->card = *work;
    sc->exists = 1;
    sc->content = 1;
    sc->formatted = 0;
    if (sc->base == NULL) {
        sc->base = (uint8_t *)malloc(SAVE_IMAGE_SIZE);
    }
    if (sc->base != NULL) {
        memcpy(sc->base, ms->data, SAVE_IMAGE_SIZE);
    }
    ms->dirty = 0;
    ms->flush_failed = 0;
    ms->flush_count++;
    DW3_STDERRF(
            "dw3-store: write card%d by %s slots_changed=%u%u%u header=%d "
            "(canonical EU layout, crc32 %08x)\n", slot,
            region_name(g_store.own_region), (rep.changed >> 0) & 1u,
            (rep.changed >> 1) & 1u, (rep.changed >> 2) & 1u, rep.header_changed,
            (unsigned)save_crc32(sc->card.image, SAVE_IMAGE_SIZE));
    rc = 0;
out:
    free(work);
    return rc;
}

static void store_reset(void) {
    int n;

    for (n = 0; n < SAVE_STORE_CARDS; n++) {
        free(g_store.c[n].base);
    }
    memset(&g_store, 0, sizeof(g_store));
}

int memcard_host_set_dir(const char *dir) {
    size_t n;

    if (dir == NULL || dir[0] == '\0') {
        return -1;
    }
    n = strlen(dir);
    if (n >= MEMCARD_DIR_MAX) {
        return -1;
    }
    memcpy(g_dir, dir, n + 1);
    while (n > 1 && (g_dir[n - 1] == '/' || g_dir[n - 1] == '\\')) {
        g_dir[--n] = '\0';
    }
    return 0;
}

int memcard_host_has_file(int slot) {
    return slot_ok(slot) && g_slots[slot].present && g_slots[slot].data != NULL;
}

const char *memcard_host_file_name(int slot) {
    if (!slot_ok(slot)) {
        return "";
    }
    return g_slots[slot].name;
}

int memcard_host_create_file(int slot, const char *name) {
    if (!slot_ok(slot) || name == NULL || name[0] == '\0') {
        return -1;
    }
    if (memcard_host_has_file(slot)) {
        return -1;
    }
    if (slot_alloc(slot) != 0) {
        return -1;
    }
    if (!store_slot(slot)) {
        store_name(slot, name); /* the store accepts any DW3 name: stays "naked" */
    }
    g_slots[slot].dirty = 1;
    return 0;
}

int memcard_host_format_slot(int slot) {
    char path[MEMCARD_PATH_MAX];
    char npath[MEMCARD_PATH_MAX];

    if (!slot_ok(slot)) {
        return -1;
    }
    slot_clear_ram(slot);
    if (store_slot(slot)) {
        /* Never delete: the card file stays; the next save replaces it (after
         * a backup). Until then this port reads as an empty card. */
        g_store.c[slot].formatted = 1;
        DW3_STDERRF(
                "dw3-store: card%d formatted by the game: the port is empty, "
                "card%d.dw3s stays on disk until the next save (then a backup "
                "is kept)\n", slot, slot);
        return 0;
    }
    if (flat_path(slot, path, sizeof(path)) == 0) {
        remove(path);
    }
    if (name_path(slot, npath, sizeof(npath)) == 0) {
        remove(npath);
    }
    return 0;
}

int memcard_host_file_read(int slot, uint32_t offset, void *buf, size_t len) {
    if (!memcard_host_has_file(slot) || buf == NULL || len == 0) {
        return -1;
    }
    if ((uint64_t)offset + (uint64_t)len > (uint64_t)MEMCARD_FLAT_SIZE) {
        return -1;
    }
    memcpy(buf, g_slots[slot].data + offset, len);
    return (int)len;
}

int memcard_host_file_write(int slot, uint32_t offset, const void *buf,
                            size_t len) {
    if (!memcard_host_has_file(slot) || buf == NULL || len == 0) {
        return -1;
    }
    if ((uint64_t)offset + (uint64_t)len > (uint64_t)MEMCARD_FLAT_SIZE) {
        return -1;
    }
    memcpy(g_slots[slot].data + offset, buf, len);
    g_slots[slot].dirty = 1;
    return (int)len;
}

static int g_test_fail_flush;

void memcard_host_test_fail_flush(int count) {
    g_test_fail_flush = count;
}

int memcard_host_snapshot(int slot, const char *suffix) {
    char path[MEMCARD_PATH_MAX];
    char dest[MEMCARD_PATH_MAX + 16];
    int n;

    if (!slot_ok(slot) || suffix == NULL || suffix[0] == '\0' ||
        !memcard_host_has_file(slot) ||
        flat_path(slot, path, sizeof(path)) != 0) {
        return -1;
    }
    n = snprintf(dest, sizeof(dest), "%s%s", path, suffix);
    if (n < 0 || (size_t)n >= sizeof(dest)) {
        return -1;
    }
    return atomic_write_file(dest, g_slots[slot].data, MEMCARD_FLAT_SIZE);
}

int memcard_host_flush(int slot) {
    char path[MEMCARD_PATH_MAX];

    if (!slot_ok(slot)) {
        return -1;
    }
    g_slots[slot].section_open = 0; /* a flush call ends the caller's section */
    if (!g_slots[slot].dirty || !memcard_host_has_file(slot)) {
        return 0;
    }
    if (g_test_fail_flush > 0) {
        g_test_fail_flush--;
        g_slots[slot].flush_failed = 1;
        DW3_STDERRF("dw3: memcard flush FAILED slot=%d (test)\n", slot);
        return -1;
    }
    if (store_slot(slot)) {
        return store_flush(slot);
    }
    if (flat_path(slot, path, sizeof(path)) != 0) {
        g_slots[slot].flush_failed = 1;
        DW3_STDERRF("dw3: memcard flush FAILED slot=%d (bad path)\n", slot);
        return -1;
    }
    if (atomic_write_file(path, g_slots[slot].data, MEMCARD_FLAT_SIZE) != 0) {
        g_slots[slot].flush_failed = 1;
        DW3_STDERRF("dw3: memcard flush FAILED slot=%d path=%s (%s)\n", slot,
                path, strerror(errno));
        return -1;
    }
    if (write_name_sidecar(slot) != 0) {
        g_slots[slot].flush_failed = 1;
        DW3_STDERRF(
                "dw3: memcard flush FAILED slot=%d (name sidecar) path=%s\n",
                slot, path);
        return -1;
    }
    g_slots[slot].dirty = 0;
    g_slots[slot].flush_failed = 0;
    g_slots[slot].flush_count++;
    return 0;
}

int memcard_host_flush_all(void) {
    int slot;
    int rc = 0;

    for (slot = 0; slot < MEMCARD_HOST_ALL; slot++) {
        if (memcard_host_flush(slot) != 0) {
            rc = -1;
        }
    }
    return rc;
}

unsigned memcard_host_flush_count(int slot) {
    if (!slot_ok(slot)) {
        return 0;
    }
    return g_slots[slot].flush_count;
}

void memcard_host_reset_flush_count(int slot) {
    if (slot_ok(slot)) {
        g_slots[slot].flush_count = 0;
    }
}

int memcard_host_flush_failed(int slot) {
    if (!slot_ok(slot)) {
        return 0;
    }
    return g_slots[slot].flush_failed;
}

int memcard_host_ensure(int slot) {
    char path[MEMCARD_PATH_MAX];
    FILE *fp;
    uint64_t t0;
    uint64_t dt;

    if (!slot_ok(slot) || g_dir[0] == '\0') {
        return -1;
    }
    if (memcard_host_has_file(slot)) {
        return 0;
    }
    if (flat_path(slot, path, sizeof(path)) != 0) {
        return -1;
    }
    if (g_store.requested && slot != MEMCARD_HOST_AUTO && store_ensure(slot) == 0) {
        return 0;
    }
    recover_slot_tmp(slot);
    /* One open via load_flat_from_disk (no probe fopen). Missing file falls
     * through to migrate; present-but-bad is quarantined then migrated. */
    t0 = mono_us();
    if (load_flat_from_disk(slot) == 0) {
        dt = mono_us() - t0;
        DW3_STDERRF("dw3: memcard ensure slot=%d flat us=%llu\n", slot,
                (unsigned long long)dt);
        return 0;
    }
    fp = fopen(path, "rb");
    if (fp != NULL) {
        fclose(fp);
        /* Wrong size / unreadable: quarantine and fall through to .mcr. */
        DW3_STDERRF(
                "dw3: memcard: bad flat %s (wrong size or read error)\n", path);
        (void)quarantine_bad_flat(slot);
        slot_clear_ram(slot);
    }
    if (slot == MEMCARD_HOST_AUTO) {
        return 0; /* no legacy image, no cross-region import: absent */
    }
    /* One-time migration from this port's older 128 KiB image. */
    if (migrate_legacy_mcr(slot) != 0) {
        return -1;
    }
    /* P6.25: no save of its own yet; take the other region's, converted. */
    if (!memcard_host_has_file(slot)) {
        cross_import(slot);
    }
    return 0;
}

void memcard_host_section_begin(int slot) {
    if (slot_ok(slot)) {
        g_slots[slot].section_open = 1;
    }
}

int memcard_host_section_open_any(void) {
    int slot;

    for (slot = 0; slot < MEMCARD_HOST_ALL; slot++) {
        if (g_slots[slot].section_open) {
            return 1;
        }
    }
    return 0;
}

/* Flush every slot that is not mid-section. A slot whose game write is half
 * done holds a RAM image the game never meant to leave on the card (data
 * written, summary not yet), so it stays out. */
int memcard_host_flush_settled(void) {
    int slot;
    int rc = 0;

    for (slot = 0; slot < MEMCARD_HOST_ALL; slot++) {
        if (g_slots[slot].section_open) {
            continue;
        }
        if (memcard_host_flush(slot) != 0) {
            rc = -1;
        }
    }
    return rc;
}

/* Give up the unfinished sections: the card file keeps its last complete
 * content. Only for a process that is about to end. */
void memcard_host_abandon_sections(void) {
    int slot;

    for (slot = 0; slot < MEMCARD_HOST_ALL; slot++) {
        if (g_slots[slot].section_open) {
            DW3_STDERRF("dw3: memcard: slot=%d write section unfinished; "
                    "the card file keeps its previous content\n", slot);
            DW3_STDERRF("dw3-save: write slot=%d interrupted, previous "
                    "file kept (exit with a save section open)\n", slot);
            g_slots[slot].section_open = 0;
            g_slots[slot].dirty = 0;
        }
    }
}

/* HOME / Quit exit gate (P10.2). 1 = the host may end the session now (the
 * settled slots are flushed); 0 = a game save is mid-write, call again next
 * frame. `waited_us` is how long the exit has been held; past
 * MEMCARD_EXIT_WAIT_US the section is abandoned (HOME force-kills at 15 s). */
int memcard_host_exit_ready(uint64_t waited_us) {
    if (memcard_host_section_open_any()) {
        if (waited_us < MEMCARD_EXIT_WAIT_US) {
            return 0;
        }
        memcard_host_abandon_sections();
    }
    (void)memcard_host_flush_settled();
    return 1;
}

void memcard_host_shutdown(void) {
    int slot;

    /* A section still open here ended its burst without a later MemCard call
     * (the game loop returned): the RAM image is the finished save, flush it.
     * A section the exit gate gave up on was already dropped (dirty = 0). */
    g_store_final = 1;
    (void)memcard_host_flush_all();
    g_store_final = 0;
    for (slot = 0; slot < MEMCARD_HOST_ALL; slot++) {
        slot_clear_ram(slot);
        g_slots[slot].flush_count = 0;
        g_slots[slot].flush_failed = 0;
    }
    g_dir[0] = '\0';
    g_import_dir[0] = '\0';
    g_own_region = 0;
    store_reset();
}

/* ---- Legacy 128 KiB helpers (fixtures / wrap) ---- */

static int legacy_slot_path(int slot, char *out, size_t outlen) {
    return legacy_path(slot, out, outlen);
}

int memcard_host_legacy_ensure(int slot) {
    char path[MEMCARD_PATH_MAX];
    FILE *fp;
    uint8_t *img;
    size_t nw;

    if (legacy_slot_path(slot, path, sizeof(path)) != 0) {
        return -1;
    }

    fp = fopen(path, "rb");
    if (fp != NULL) {
        int ok = file_size_is(fp, (long)MEMCARD_HOST_SIZE);
        fclose(fp);
        return ok ? 0 : -1;
    }

    img = (uint8_t *)malloc(MEMCARD_HOST_SIZE);
    if (img == NULL) {
        return -1;
    }
    memcard_host_format_image(img);

    fp = fopen(path, "wb");
    if (fp == NULL) {
        free(img);
        return -1;
    }
    nw = fwrite(img, 1, MEMCARD_HOST_SIZE, fp);
    fclose(fp);
    free(img);
    return (nw == MEMCARD_HOST_SIZE) ? 0 : -1;
}

int memcard_host_read(int slot, uint32_t offset, void *buf, size_t len) {
    char path[MEMCARD_PATH_MAX];
    FILE *fp;
    size_t nr;

    if (buf == NULL || len == 0) {
        return -1;
    }
    if ((uint64_t)offset + (uint64_t)len > (uint64_t)MEMCARD_HOST_SIZE) {
        return -1;
    }
    if (legacy_slot_path(slot, path, sizeof(path)) != 0) {
        return -1;
    }

    fp = fopen(path, "rb");
    if (fp == NULL) {
        return -1;
    }
    if (!file_size_is(fp, (long)MEMCARD_HOST_SIZE)) {
        fclose(fp);
        return -1;
    }
    if (fseek(fp, (long)offset, SEEK_SET) != 0) {
        fclose(fp);
        return -1;
    }
    nr = fread(buf, 1, len, fp);
    fclose(fp);
    return (nr == len) ? (int)nr : -1;
}

int memcard_host_write(int slot, uint32_t offset, const void *buf, size_t len) {
    char path[MEMCARD_PATH_MAX];
    FILE *fp;
    size_t nw;

    if (buf == NULL || len == 0) {
        return -1;
    }
    if ((uint64_t)offset + (uint64_t)len > (uint64_t)MEMCARD_HOST_SIZE) {
        return -1;
    }
    if (legacy_slot_path(slot, path, sizeof(path)) != 0) {
        return -1;
    }

    if (memcard_host_legacy_ensure(slot) != 0) {
        return -1;
    }

    fp = fopen(path, "r+b");
    if (fp == NULL) {
        return -1;
    }
    if (!file_size_is(fp, (long)MEMCARD_HOST_SIZE)) {
        fclose(fp);
        return -1;
    }
    if (fseek(fp, (long)offset, SEEK_SET) != 0) {
        fclose(fp);
        return -1;
    }
    nw = fwrite(buf, 1, len, fp);
    if (fflush(fp) != 0) {
        fclose(fp);
        return -1;
    }
    fclose(fp);
    return (nw == len) ? (int)nw : -1;
}

int memcard_host_read_sector(int slot, int sector, void *buf) {
    if (sector < 0 || (uint32_t)sector >= MEMCARD_HOST_SECTORS) {
        return -1;
    }
    if (memcard_host_read(slot, (uint32_t)sector * MEMCARD_HOST_SECTOR_SIZE, buf,
                          MEMCARD_HOST_SECTOR_SIZE) !=
        (int)MEMCARD_HOST_SECTOR_SIZE) {
        return -1;
    }
    return 0;
}

int memcard_host_write_sector(int slot, int sector, const void *buf) {
    if (sector < 0 || (uint32_t)sector >= MEMCARD_HOST_SECTORS) {
        return -1;
    }
    if (memcard_host_write(slot, (uint32_t)sector * MEMCARD_HOST_SECTOR_SIZE, buf,
                           MEMCARD_HOST_SECTOR_SIZE) !=
        (int)MEMCARD_HOST_SECTOR_SIZE) {
        return -1;
    }
    return 0;
}

#ifdef DW3_CHEATS
/* P6.20: the save marks (port/options/cheat_saves.c) sit beside the saves. */
const char *memcard_host_dir(void) {
    return g_dir[0] != '\0' ? g_dir : NULL;
}
#endif
