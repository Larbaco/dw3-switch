/* One fread of a field map, then copies.
 *
 * The first tile of a file id reads that whole file. A later tile in the
 * same frame is a copy from that buffer and does not set a busy flag.
 * A different id replaces the buffer. cdfs_read_at stays on the disk.
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
#define FILE2_BYTES (SECTOR * 3)

extern size_t __real_fread(void *ptr, size_t size, size_t nmemb, FILE *stream);

static int g_fread_calls;
static size_t g_fread_last_req;

size_t __wrap_fread(void *ptr, size_t size, size_t nmemb, FILE *stream) {
    size_t n = __real_fread(ptr, size, nmemb, stream);
    g_fread_calls++;
    g_fread_last_req = size * nmemb;
    return n;
}

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
    char bin2_path[256];
    unsigned char file[FILE_BYTES];
    unsigned char file2[FILE2_BYTES];
    unsigned char poison[FILE_BYTES];
    unsigned char a[TILE_BYTES];
    unsigned char b[TILE_BYTES];
    unsigned char disk[TILE_BYTES];
    size_t i;
    int n;
    int calls_after_full;
    FILE *map;

    if (mkdtemp(dir) == NULL) {
        fail("mkdtemp");
    }
    snprintf(bin_path, sizeof(bin_path), "%s/tile.bin", dir);
    snprintf(bin2_path, sizeof(bin2_path), "%s/other.bin", dir);
    snprintf(map_path, sizeof(map_path), "%s/files_us.json", dir);

    for (i = 0; i < FILE_BYTES; i++) {
        file[i] = (unsigned char)(i * 3u + 1u);
        poison[i] = 0x5C;
    }
    for (i = 0; i < FILE2_BYTES; i++) {
        file2[i] = (unsigned char)(0xA0u + (i & 0x0Fu));
    }
    write_bytes(bin_path, file, FILE_BYTES);
    write_bytes(bin2_path, file2, FILE2_BYTES);

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
            "    },\n"
            "    \"8\": {\n"
            "      \"path\": \"other.bin\",\n"
            "      \"lba\": 80,\n"
            "      \"sectors\": %d,\n"
            "      \"bytes\": %d,\n"
            "      \"form\": 1\n"
            "    }\n"
            "  }\n"
            "}\n",
            FILE_BYTES / SECTOR, FILE_BYTES, FILE2_BYTES / SECTOR, FILE2_BYTES);
    if (fclose(map) != 0) {
        fail("map close");
    }

    expect(cdfs_set_root(dir) == 0, "set_root");
    expect(cdfs_load_map(map_path) == 0, "load_map");

    {
        unsigned char *whole = malloc(FILE_BYTES);
        expect(whole != NULL, "alloc");
        n = cdfs_read(7, whole, FILE_BYTES);
        expect(n == FILE_BYTES, "full-file read");
        expect(memcmp(whole, file, FILE_BYTES) == 0, "full-file bytes");
        free(whole);
    }
    calls_after_full = g_fread_calls;

    memset(a, 0xA5, sizeof(a));
    n = cdfs_read_field_tile(7, a, SECTOR, TILE_BYTES);
    expect(n == TILE_BYTES, "first tile read length");
    expect(memcmp(a, file + SECTOR, TILE_BYTES) == 0, "first tile bytes");
    expect(g_fread_calls == calls_after_full + 1, "one fread of the map file");
    expect(g_fread_last_req == FILE_BYTES, "that fread is the whole map");

    /* Disk changes must not affect a tile that is already resident.
     * No busy flag: the second tile does not wait for cdfs_poll. */
    write_bytes(bin_path, poison, FILE_BYTES);

    memset(b, 0x5A, sizeof(b));
    n = cdfs_read_field_tile(7, b, SECTOR + TILE_BYTES, TILE_BYTES);
    expect(n == TILE_BYTES, "second tile in the same frame");
    expect(memcmp(b, file + SECTOR + TILE_BYTES, TILE_BYTES) == 0,
           "second tile is a copy");
    expect(g_fread_calls == calls_after_full + 1, "second tile does not fread");

    cdfs_poll();
    n = cdfs_read_field_tile(7, a, SECTOR, TILE_BYTES);
    expect(n == TILE_BYTES, "poll does not drop the resident map");
    expect(memcmp(a, file + SECTOR, TILE_BYTES) == 0, "poll leaves the copy");
    expect(g_fread_calls == calls_after_full + 1, "poll does not fread");

    memset(disk, 0, sizeof(disk));
    n = cdfs_read_at(7, disk, SECTOR + TILE_BYTES, TILE_BYTES);
    expect(n == TILE_BYTES, "header path still reads the disk");
    expect(memcmp(disk, poison + SECTOR + TILE_BYTES, TILE_BYTES) == 0,
           "cdfs_read_at is not the resident map");

    n = cdfs_read_field_tile(99, a, 0, TILE_BYTES);
    expect(n < 0, "unknown id fails");
    expect(g_fread_calls == calls_after_full + 2, "unknown id does not load");
    n = cdfs_read_field_tile(7, a, SECTOR, TILE_BYTES);
    expect(n == TILE_BYTES, "resident map survives a failed id");
    expect(memcmp(a, file + SECTOR, TILE_BYTES) == 0, "still the first map");
    expect(g_fread_calls == calls_after_full + 2, "failed id does not drop the map");

    n = cdfs_read_field_tile(8, a, 0, TILE_BYTES);
    expect(n == TILE_BYTES, "other map tile");
    expect(memcmp(a, file2, TILE_BYTES) == 0, "other map bytes");
    expect(g_fread_calls == calls_after_full + 3, "different id fread once");
    expect(g_fread_last_req == FILE2_BYTES, "that fread is the other whole file");

    n = cdfs_read_field_tile(7, b, SECTOR, TILE_BYTES);
    expect(n == TILE_BYTES, "returning to the first id reloads it");
    expect(memcmp(b, poison + SECTOR, TILE_BYTES) == 0, "replaced buffer sees the disk");
    expect(g_fread_calls == calls_after_full + 4, "reload is one fread");
    expect(g_fread_last_req == FILE_BYTES, "reload fread is the whole map");

    cdfs_shutdown();
    expect(cdfs_set_root(dir) == 0, "set_root after shutdown");
    expect(cdfs_load_map(map_path) == 0, "load_map after shutdown");
    n = cdfs_read_field_tile(7, a, SECTOR, TILE_BYTES);
    expect(n == TILE_BYTES, "tile after shutdown reloads");
    expect(memcmp(a, poison + SECTOR, TILE_BYTES) == 0, "shutdown dropped the buffer");
    cdfs_shutdown();

    printf("OK: one map fread, later tiles are copies\n");
    return 0;
}
