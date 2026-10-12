#include "memcard_host.h"

#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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
static MemSlot g_slots[MEMCARD_HOST_SLOTS];

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
    return slot >= 0 && slot < MEMCARD_HOST_SLOTS;
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

    n = snprintf(leaf, sizeof(leaf), "save%d.name", slot);
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

static int write_name_sidecar(int slot) {
    char path[MEMCARD_PATH_MAX];
    FILE *fp;
    size_t n;

    if (!slot_ok(slot) || g_slots[slot].name[0] == '\0') {
        return 0;
    }
    if (name_path(slot, path, sizeof(path)) != 0) {
        return -1;
    }
    fp = fopen(path, "wb");
    if (fp == NULL) {
        return -1;
    }
    n = strlen(g_slots[slot].name);
    if (fwrite(g_slots[slot].name, 1, n, fp) != n || fflush(fp) != 0) {
        fclose(fp);
        return -1;
    }
    fclose(fp);
    return 0;
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
        fprintf(stderr, "dw3: memcard: cannot quarantine %s -> %s (%s)\n", path,
                bad, strerror(errno));
        return -1;
    }
    fprintf(stderr, "dw3: memcard: quarantined corrupt flat as %s\n", bad);
    if (name_path(slot, npath, sizeof(npath)) == 0) {
        n = snprintf(nbad, sizeof(nbad), "%s.bad", npath);
        if (n >= 0 && (size_t)n < sizeof(nbad)) {
            remove(nbad);
            (void)rename(npath, nbad);
        }
    }
    return 0;
}

static int atomic_write_file(const char *path, const uint8_t *data, size_t len) {
    char tmp[MEMCARD_PATH_MAX + 8];
    FILE *fp;
    size_t nw;
    int n;

    n = snprintf(tmp, sizeof(tmp), "%s.tmp", path);
    if (n < 0 || (size_t)n >= sizeof(tmp)) {
        return -1;
    }
    fp = fopen(tmp, "wb");
    if (fp == NULL) {
        return -1;
    }
    nw = fwrite(data, 1, len, fp);
    if (nw != len || fflush(fp) != 0) {
        fclose(fp);
        remove(tmp);
        return -1;
    }
    if (fclose(fp) != 0) {
        remove(tmp);
        return -1;
    }
    if (rename(tmp, path) != 0) {
        remove(tmp);
        return -1;
    }
    return 0;
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

    if (legacy_path(slot, mcr_path, sizeof(mcr_path)) != 0) {
        return -1;
    }
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
        fprintf(stderr,
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
            fprintf(stderr,
                    "dw3: memcard migrate: kept %s (rename to .bak failed: %s)\n",
                    mcr_path, strerror(errno));
        } else {
            fprintf(stderr,
                    "dw3: memcard migrate slot=%d name=%s -> %s (from %s.bak)\n",
                    slot, name, flat, memcard_host_legacy_slot_name(slot));
        }
    }
    return 0;
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
    store_name(slot, name);
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

int memcard_host_flush(int slot) {
    char path[MEMCARD_PATH_MAX];

    if (!slot_ok(slot)) {
        return -1;
    }
    if (!g_slots[slot].dirty || !memcard_host_has_file(slot)) {
        return 0;
    }
    if (flat_path(slot, path, sizeof(path)) != 0) {
        g_slots[slot].flush_failed = 1;
        fprintf(stderr, "dw3: memcard flush FAILED slot=%d (bad path)\n", slot);
        return -1;
    }
    if (atomic_write_file(path, g_slots[slot].data, MEMCARD_FLAT_SIZE) != 0) {
        g_slots[slot].flush_failed = 1;
        fprintf(stderr, "dw3: memcard flush FAILED slot=%d path=%s (%s)\n", slot,
                path, strerror(errno));
        return -1;
    }
    if (write_name_sidecar(slot) != 0) {
        g_slots[slot].flush_failed = 1;
        fprintf(stderr,
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

    for (slot = 0; slot < MEMCARD_HOST_SLOTS; slot++) {
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

    if (!slot_ok(slot) || g_dir[0] == '\0') {
        return -1;
    }
    if (memcard_host_has_file(slot)) {
        return 0;
    }
    if (flat_path(slot, path, sizeof(path)) != 0) {
        return -1;
    }
    fp = fopen(path, "rb");
    if (fp != NULL) {
        fclose(fp);
        if (load_flat_from_disk(slot) == 0) {
            return 0;
        }
        /* Wrong size / unreadable: quarantine and fall through to .mcr. */
        fprintf(stderr,
                "dw3: memcard: bad flat %s (wrong size or read error)\n", path);
        (void)quarantine_bad_flat(slot);
        slot_clear_ram(slot);
    }
    /* One-time migration from this port's older 128 KiB image. */
    if (migrate_legacy_mcr(slot) != 0) {
        return -1;
    }
    return 0;
}

void memcard_host_shutdown(void) {
    int slot;

    (void)memcard_host_flush_all();
    for (slot = 0; slot < MEMCARD_HOST_SLOTS; slot++) {
        slot_clear_ram(slot);
        g_slots[slot].flush_count = 0;
        g_slots[slot].flush_failed = 0;
    }
    g_dir[0] = '\0';
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
