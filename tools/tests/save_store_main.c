/* P6.25 slice 2 host test driver: the shared store behind memcard_host.
 *
 *   save_store_main <own us|eu> <own_dir> <import_dir|-> <store_dir|-> <cfg_dir|-> <op>...
 *
 * Sets the folders the way the hosts do (memcard_host_set_dir, the cross
 * import when import_dir is given, memcard_host_setup_store), then runs the
 * ops in order; each prints one line. The test sets DW3_SAVE_STORE=0|1.
 *   ensure:N              memcard_host_ensure          -> "ensure N rc=.. has=.. store=.."
 *   load:N:file           write the whole 32 KiB file into port N as the game
 *                         would (creates the file when the port is empty)
 *   put:N:off:file        file_write of the file's bytes at off (creates when empty)
 *   dump:N:file           resident image to file       -> "dump N rc=.."
 *   flush:N               memcard_host_flush           -> "flush N rc=.. failed=.."
 *   format:N              memcard_host_format_slot
 *   failflush:K           the next K flushes fail
 *   fault:STEP:NTH        memcard_host_test_fault_step
 *   shutdown              memcard_host_shutdown (flushes dirty ports)
 *   abort                 _exit(3): no shutdown, nothing flushed
 */
#include "memcard_host.h"
#include "save_convert.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static uint8_t *slurp(const char *path, size_t *len) {
    FILE *fp = fopen(path, "rb");
    uint8_t *buf;

    if (fp == NULL) {
        return NULL;
    }
    fseek(fp, 0, SEEK_END);
    *len = (size_t)ftell(fp);
    fseek(fp, 0, SEEK_SET);
    buf = (uint8_t *)malloc(*len + 1);
    if (buf == NULL || fread(buf, 1, *len, fp) != *len) {
        fclose(fp);
        free(buf);
        return NULL;
    }
    fclose(fp);
    return buf;
}

static int put(int slot, uint32_t off, const uint8_t *buf, size_t len) {
    if (memcard_host_ensure(slot) != 0) {
        return -2;
    }
    if (!memcard_host_has_file(slot) &&
        memcard_host_create_file(slot, "BASLUS-01436DMW3-USA") != 0) {
        return -3;
    }
    return memcard_host_file_write(slot, off, buf, len);
}

int main(int argc, char **argv) {
    int region;
    int i;

    if (argc < 7) {
        fprintf(stderr, "usage: see source\n");
        return 1;
    }
    region = strcmp(argv[1], "us") == 0 ? SAVE_REGION_US : SAVE_REGION_EU;
    if (memcard_host_set_dir(argv[2]) != 0) {
        return 2;
    }
    if (strcmp(argv[3], "-") != 0 && memcard_host_set_cross_import(argv[3], region) != 0) {
        return 2;
    }
    printf("store setup=%d\n",
           memcard_host_setup_store(strcmp(argv[5], "-") != 0 ? argv[5] : NULL,
                                    strcmp(argv[4], "-") != 0 ? argv[4] : NULL, region));
    for (i = 6; i < argc; i++) {
        const char *op = argv[i];
        int n = 0;
        char file[512];
        unsigned off = 0;

        if (strncmp(op, "ensure:", 7) == 0) {
            int rc;

            n = atoi(op + 7);
            rc = memcard_host_ensure(n);
            printf("ensure %d rc=%d has=%d store=%d\n", n, rc,
                   memcard_host_has_file(n), memcard_host_store_active(n));
        } else if (sscanf(op, "load:%d:%511s", &n, file) == 2) {
            size_t len = 0;
            uint8_t *b = slurp(file, &len);

            printf("load %d rc=%d\n", n, b != NULL ? put(n, 0, b, len) : -9);
            free(b);
        } else if (sscanf(op, "put:%d:%u:%511s", &n, &off, file) == 3) {
            size_t len = 0;
            uint8_t *b = slurp(file, &len);

            printf("put %d rc=%d\n", n, b != NULL ? put(n, off, b, len) : -9);
            free(b);
        } else if (sscanf(op, "dump:%d:%511s", &n, file) == 2) {
            uint8_t *b = (uint8_t *)malloc(SAVE_IMAGE_SIZE);
            int rc = memcard_host_file_read(n, 0, b, SAVE_IMAGE_SIZE);
            FILE *fp = rc > 0 ? fopen(file, "wb") : NULL;

            if (fp != NULL) {
                fwrite(b, 1, SAVE_IMAGE_SIZE, fp);
                fclose(fp);
            }
            printf("dump %d rc=%d\n", n, rc);
            free(b);
        } else if (strncmp(op, "flush:", 6) == 0) {
            int rc;

            n = atoi(op + 6);
            rc = memcard_host_flush(n);
            printf("flush %d rc=%d failed=%d\n", n, rc, memcard_host_flush_failed(n));
        } else if (strncmp(op, "format:", 7) == 0) {
            int rc;

            n = atoi(op + 7);
            rc = memcard_host_format_slot(n);
            printf("format %d rc=%d has=%d\n", n, rc, memcard_host_has_file(n));
        } else if (strncmp(op, "failflush:", 10) == 0) {
            memcard_host_test_fail_flush(atoi(op + 10));
            printf("failflush set\n");
        } else if (sscanf(op, "fault:%d:%d", &n, (int *)&off) == 2) {
            memcard_host_test_fault_step(n, (int)off);
            printf("fault set\n");
        } else if (strcmp(op, "shutdown") == 0) {
            memcard_host_shutdown();
            printf("shutdown\n");
        } else if (strcmp(op, "abort") == 0) {
            fflush(stdout);
            _exit(3);
        } else {
            fprintf(stderr, "bad op %s\n", op);
            return 1;
        }
        fflush(stdout);
    }
    return 0;
}
