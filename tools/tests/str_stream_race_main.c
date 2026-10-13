/* Host regression for the STR file bridge (port/movies/str_stream.c).
 *
 * The bridge serves both the video ring (game/main thread, via CD_ready) and
 * the XA decoder (audio thread) from one shared FILE*. The two synthetic
 * files here cover disjoint LBA ranges, so every call from one thread also
 * forces open_for_lba to close and reopen the file, and the other thread's
 * seek+read pair can land in the middle. Each sector encodes its own absolute
 * LBA, so a sector served from the wrong file or from a stale file offset is
 * immediately visible.
 *
 * Prints one `name=value` line per counter and exits non-zero when a read
 * returned a sector other than the one requested. All data is synthesized
 * here; no game data is read.
 */
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cdfs.h"
#include "str_stream.h"

#define SECTOR_BYTES 2352u
#define FILE_SECTORS 4096u
#define BASE_A 100000u
#define BASE_B 200000u
#define ITERS 20000u
#define N_THREADS 2u

static const char *REL_A = "synthetic_a.str";
static const char *REL_B = "synthetic_b.str";

static char g_path_a[1024];
static char g_path_b[1024];
static pthread_mutex_t g_audio_mutex;

/* The host platforms' Psyz_AudioLock is recursive (libnx RMutex, SDL mutex);
 * mirror that, since str_stream.c may re-enter it from the XA path. */
void Psyz_AudioLock(void) { pthread_mutex_lock(&g_audio_mutex); }

void Psyz_AudioUnlock(void) { pthread_mutex_unlock(&g_audio_mutex); }

void Psyz_CdSetReadCB(PsyzCdReadCB cb) { (void)cb; }

/* Map stub: two extents, one per synthetic file. */
int cdfs_lookup_lba(uint32_t lba, uint32_t *id_out, char *relpath_out,
                    size_t relpath_len, uint32_t *base_lba_out,
                    uint32_t *sectors_out, uint32_t *form_out) {
    uint32_t base;
    const char *rel;

    if (lba >= BASE_A && lba < BASE_A + FILE_SECTORS) {
        base = BASE_A;
        rel = REL_A;
    } else if (lba >= BASE_B && lba < BASE_B + FILE_SECTORS) {
        base = BASE_B;
        rel = REL_B;
    } else {
        return -1;
    }
    if (id_out) {
        *id_out = base;
    }
    if (relpath_out && relpath_len) {
        snprintf(relpath_out, relpath_len, "%s", rel);
    }
    if (base_lba_out) {
        *base_lba_out = base;
    }
    if (sectors_out) {
        *sectors_out = FILE_SECTORS;
    }
    if (form_out) {
        *form_out = 2;
    }
    return 0;
}

/* PSY-Z's BCD MSF -> LBA. Unused by this test (it reads by LBA), but
 * dw3_str_stream_open_at_loc() references it. */
static int bcd_to_int(unsigned char v) {
    return (v >> 4) * 10 + (v & 0x0F);
}

int CdPosToInt(CdlLOC *p) {
    return (bcd_to_int(p->minute) * 60 + bcd_to_int(p->second)) * 75 +
           bcd_to_int(p->sector);
}

int cdfs_join_root(const char *rel, char *out, size_t outlen) {
    const char *path;

    if (strcmp(rel, REL_A) == 0) {
        path = g_path_a;
    } else if (strcmp(rel, REL_B) == 0) {
        path = g_path_b;
    } else {
        return -1;
    }
    snprintf(out, outlen, "%s", path);
    return 0;
}

static void encode_sector(uint32_t lba, uint8_t *out) {
    unsigned i;

    out[0] = (uint8_t)(lba & 0xFFu);
    out[1] = (uint8_t)((lba >> 8) & 0xFFu);
    out[2] = (uint8_t)((lba >> 16) & 0xFFu);
    out[3] = (uint8_t)((lba >> 24) & 0xFFu);
    for (i = 4; i < SECTOR_BYTES; i++) {
        out[i] = (uint8_t)(lba + (uint32_t)i * 7u);
    }
}

static int write_synthetic(const char *path, uint32_t base) {
    uint8_t sector[SECTOR_BYTES];
    uint8_t *blob;
    uint32_t i;
    size_t total = (size_t)FILE_SECTORS * SECTOR_BYTES;
    size_t written;
    FILE *fp;

    blob = (uint8_t *)malloc(total);
    if (!blob) {
        return -1;
    }
    for (i = 0; i < FILE_SECTORS; i++) {
        encode_sector(base + i, sector);
        memcpy(blob + (size_t)i * SECTOR_BYTES, sector, SECTOR_BYTES);
    }
    fp = fopen(path, "wb");
    if (!fp) {
        free(blob);
        return -1;
    }
    written = fwrite(blob, 1, total, fp);
    fclose(fp);
    free(blob);
    return written == total ? 0 : -1;
}

typedef struct Reader {
    uint32_t base;
    uint32_t seed;
    unsigned long mismatches;
    unsigned long errors;
    uint32_t first_bad_want;
    uint32_t first_bad_got;
    uint32_t first_bad_iter;
} Reader;

static uint32_t sector_lba(const uint8_t *buf) {
    return (uint32_t)buf[0] | ((uint32_t)buf[1] << 8) |
           ((uint32_t)buf[2] << 16) | ((uint32_t)buf[3] << 24);
}

static void *reader_thread(void *arg) {
    Reader *r = (Reader *)arg;
    uint8_t buf[SECTOR_BYTES];
    unsigned iter;

    for (iter = 0; iter < ITERS; iter++) {
        uint32_t lba = r->base + ((iter * 2654435761u + r->seed) % FILE_SECTORS);
        struct PsyzCdRead read;
        uint32_t got;
        unsigned i;
        int body_bad = 0;

        read.sector = lba;
        read.size = SECTOR_BYTES;
        read.buffer = buf;
        memset(buf, 0, sizeof(buf));

        if (dw3_str_cd_read_cb(&read) != (int)SECTOR_BYTES) {
            r->errors++;
            continue;
        }
        got = sector_lba(buf);
        for (i = 4; i < SECTOR_BYTES; i++) {
            if (buf[i] != (uint8_t)(got + (uint32_t)i * 7u)) {
                body_bad = 1;
                break;
            }
        }
        if (got != lba || body_bad) {
            if (r->mismatches == 0) {
                r->first_bad_want = lba;
                r->first_bad_got = got;
                r->first_bad_iter = iter;
            }
            r->mismatches++;
        }
    }
    return NULL;
}

int main(int argc, char **argv) {
    pthread_mutexattr_t attr;
    pthread_t threads[N_THREADS];
    Reader readers[N_THREADS];
    unsigned long reads = 0;
    unsigned long overlaps = 0;
    unsigned long mismatches = 0;
    unsigned long errors = 0;
    unsigned i;

    if (argc != 2) {
        fprintf(stderr, "usage: %s <scratch-dir>\n", argv[0]);
        return 2;
    }
    snprintf(g_path_a, sizeof(g_path_a), "%s/synthetic_a.str", argv[1]);
    snprintf(g_path_b, sizeof(g_path_b), "%s/synthetic_b.str", argv[1]);

    /* Two different extents, so both readers keep swapping the shared FILE*. */
    if (write_synthetic(g_path_a, BASE_A) != 0 ||
        write_synthetic(g_path_b, BASE_B) != 0) {
        fprintf(stderr, "synthetic write failed\n");
        return 2;
    }

    pthread_mutexattr_init(&attr);
    pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
    pthread_mutex_init(&g_audio_mutex, &attr);
    pthread_mutexattr_destroy(&attr);

    memset(readers, 0, sizeof(readers));
    readers[0].base = BASE_A;
    readers[0].seed = 1u;
    readers[1].base = BASE_B;
    readers[1].seed = 7u;

    for (i = 0; i < N_THREADS; i++) {
        if (pthread_create(&threads[i], NULL, reader_thread, &readers[i]) != 0) {
            fprintf(stderr, "pthread_create failed\n");
            return 2;
        }
    }
    for (i = 0; i < N_THREADS; i++) {
        pthread_join(threads[i], NULL);
        mismatches += readers[i].mismatches;
        errors += readers[i].errors;
    }

    dw3_str_stream_get_debug(&reads, &overlaps);

    printf("reads=%lu\n", reads);
    printf("overlaps=%lu\n", overlaps);
    printf("mismatches=%lu\n", mismatches);
    printf("errors=%lu\n", errors);
    for (i = 0; i < N_THREADS; i++) {
        if (readers[i].mismatches != 0) {
            printf("first_bad_%u=iter=%u want=%u got=%u\n", i,
                   readers[i].first_bad_iter, readers[i].first_bad_want,
                   readers[i].first_bad_got);
        }
    }
    return mismatches == 0 && errors == 0 ? 0 : 1;
}
