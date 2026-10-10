#include "memcard_host.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MEMCARD_DIR_MAX 512
#define MEMCARD_PATH_MAX 640

static char g_dir[MEMCARD_DIR_MAX];

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

    /* DuckStation MemoryCardImage::Format layout (raw 128 KiB). */
    memset(buf, 0xFF, MEMCARD_HOST_SIZE);

    /* Frame 0: header */
    memset(buf, 0x00, MEMCARD_HOST_SECTOR_SIZE);
    buf[0] = (uint8_t)'M';
    buf[1] = (uint8_t)'C';
    buf[0x7F] = frame_checksum(buf);

    /* Frames 1-15: free directory entries */
    for (s = 1; s <= 15; s++) {
        uint8_t *f = buf + (size_t)s * MEMCARD_HOST_SECTOR_SIZE;
        memset(f, 0x00, MEMCARD_HOST_SECTOR_SIZE);
        f[0] = 0xA0; /* free */
        f[8] = 0xFF;
        f[9] = 0xFF;
        f[0x7F] = frame_checksum(f);
    }

    /* Frames 16-35: broken-sector list (none) */
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

    /* Frames 36-62: broken-sector replacement / unused zeros */
    for (s = 36; s <= 62; s++) {
        memset(buf + (size_t)s * MEMCARD_HOST_SECTOR_SIZE, 0x00,
               MEMCARD_HOST_SECTOR_SIZE);
    }

    /* Frame 63: write-test frame (copy of header) */
    memcpy(buf + 63u * MEMCARD_HOST_SECTOR_SIZE, buf, MEMCARD_HOST_SECTOR_SIZE);
}

void memcard_host_shutdown(void) {
    g_dir[0] = '\0';
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

static int slot_path(int slot, char *out, size_t outlen) {
    const char *name;
    int n;

    if (g_dir[0] == '\0') {
        return -1;
    }
    name = memcard_host_slot_name(slot);
    if (name == NULL) {
        return -1;
    }
    n = snprintf(out, outlen, "%s/%s", g_dir, name);
    if (n < 0 || (size_t)n >= outlen) {
        return -1;
    }
    return 0;
}

static int file_size_ok(FILE *fp) {
    long sz;

    if (fseek(fp, 0, SEEK_END) != 0) {
        return 0;
    }
    sz = ftell(fp);
    if (sz != (long)MEMCARD_HOST_SIZE) {
        return 0;
    }
    if (fseek(fp, 0, SEEK_SET) != 0) {
        return 0;
    }
    return 1;
}

int memcard_host_ensure(int slot) {
    char path[MEMCARD_PATH_MAX];
    FILE *fp;
    uint8_t *img;
    size_t nw;

    if (slot_path(slot, path, sizeof(path)) != 0) {
        return -1;
    }

    fp = fopen(path, "rb");
    if (fp != NULL) {
        int ok = file_size_ok(fp);
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
    if (slot_path(slot, path, sizeof(path)) != 0) {
        return -1;
    }

    fp = fopen(path, "rb");
    if (fp == NULL) {
        return -1;
    }
    if (!file_size_ok(fp)) {
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
    if (slot_path(slot, path, sizeof(path)) != 0) {
        return -1;
    }

    /* Create formatted image if the file is missing. */
    if (memcard_host_ensure(slot) != 0) {
        return -1;
    }

    fp = fopen(path, "r+b");
    if (fp == NULL) {
        return -1;
    }
    if (!file_size_ok(fp)) {
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
