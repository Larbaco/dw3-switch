/* Host pace: one field-tile read per frame.
 *
 * Two tile-sized reads in one frame leave the reader busy after the first.
 * cdfs_poll() is the frame boundary (vsyncCallback calls it once per VSync).
 */
#define _POSIX_C_SOURCE 200809L

#include "cdfs.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define SECTOR 2048
#define TILE_SECTORS 2
#define TILE_BYTES (SECTOR * TILE_SECTORS)
/* Header sector plus two tiles. */
#define FILE_BYTES (SECTOR + TILE_BYTES * 2)

static void fail(const char *msg) {
    fprintf(stderr, "FAIL: %s\n", msg);
    exit(1);
}

static void expect(int cond, const char *msg) {
    if (!cond) {
        fail(msg);
    }
}

static void write_bytes(const char *path, const unsigned char *data, size_t n) {
    FILE *fp = fopen(path, "wb");
    if (fp == NULL || fwrite(data, 1, n, fp) != n || fclose(fp) != 0) {
        fail("write fixture");
    }
}

int main(void) {
    char dir[] = "/tmp/dw3-tile-pace-XXXXXX";
    char map_path[256];
    char bin_path[256];
    unsigned char file[FILE_BYTES];
    unsigned char a[TILE_BYTES];
    unsigned char b[TILE_BYTES];
    unsigned char saved[TILE_BYTES];
    size_t i;
    int n;
    FILE *map;

    if (mkdtemp(dir) == NULL) {
        fail("mkdtemp");
    }
    snprintf(bin_path, sizeof(bin_path), "%s/tile.bin", dir);
    snprintf(map_path, sizeof(map_path), "%s/files_us.json", dir);

    for (i = 0; i < FILE_BYTES; i++) {
        file[i] = (unsigned char)(i * 3u + 1u);
    }
    write_bytes(bin_path, file, FILE_BYTES);

    map = fopen(map_path, "w");
    if (map == NULL) {
        fail("map");
    }
    fprintf(map,
            "{\n"
            "  \"version\": \"us\",\n"
            "  \"executable\": \"SLUS_014.36\",\n"
            "  \"files\": {\n"
            "    \"7\": {\n"
            "      \"path\": \"tile.bin\",\n"
            "      \"lba\": 50,\n"
            "      \"sectors\": %d,\n"
            "      \"bytes\": %d,\n"
            "      \"form\": 1\n"
            "    }\n"
            "  }\n"
            "}\n",
            FILE_BYTES / SECTOR, FILE_BYTES);
    if (fclose(map) != 0) {
        fail("map close");
    }

    expect(cdfs_set_root(dir) == 0, "set_root");
    expect(cdfs_load_map(map_path) == 0, "load_map");
    expect(cdfs_field_tile_busy() == 0, "idle before first tile");

    {
        unsigned char *whole = malloc(FILE_BYTES);
        expect(whole != NULL, "alloc");
        n = cdfs_read(7, whole, FILE_BYTES);
        expect(n == FILE_BYTES, "full-file read");
        expect(cdfs_field_tile_busy() == 0, "full-file read does not hold");
        free(whole);
    }

    memset(a, 0xA5, sizeof(a));
    n = cdfs_read_field_tile(7, a, SECTOR, TILE_BYTES);
    expect(n == TILE_BYTES, "first tile read length");
    expect(memcmp(a, file + SECTOR, TILE_BYTES) == 0, "first tile bytes");
    expect(cdfs_field_tile_busy() != 0, "busy after first tile");

    memset(b, 0x5A, sizeof(b));
    memcpy(saved, b, sizeof(b));
    n = cdfs_read_field_tile(7, b, SECTOR + TILE_BYTES, TILE_BYTES);
    expect(n < 0, "second tile in the same frame is refused");
    expect(memcmp(b, saved, sizeof(b)) == 0, "refused read leaves the buffer");
    expect(cdfs_field_tile_busy() != 0, "still busy after the second attempt");

    /* Same tick vsyncCallback runs once per VSync. */
    cdfs_poll();
    expect(cdfs_field_tile_busy() == 0, "next frame clears the hold");

    n = cdfs_read_field_tile(7, b, SECTOR + TILE_BYTES, TILE_BYTES);
    expect(n == TILE_BYTES, "second tile read on the next frame");
    expect(memcmp(b, file + SECTOR + TILE_BYTES, TILE_BYTES) == 0,
           "second tile bytes");
    expect(cdfs_field_tile_busy() != 0, "busy again after the next tile");

    cdfs_shutdown();
    printf("OK: one field tile per frame\n");
    return 0;
}
