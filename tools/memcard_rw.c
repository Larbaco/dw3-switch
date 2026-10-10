/* Tiny host helper for port/memcard sector/byte I/O.
 *
 * Usage:
 *   memcard_rw --dir DIR ensure --slot N
 *   memcard_rw --dir DIR read --slot N --offset OFF --len LEN
 *   memcard_rw --dir DIR write --slot N --offset OFF --hex HEXBYTES
 *   memcard_rw --dir DIR read-sector --slot N --sector S
 *   memcard_rw --dir DIR write-sector --slot N --sector S --hex HEXBYTES
 *
 * read / read-sector write raw bytes to stdout.
 */
#include "memcard_host.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void usage(const char *argv0) {
    fprintf(stderr,
            "usage:\n"
            "  %s --dir DIR ensure --slot N\n"
            "  %s --dir DIR read --slot N --offset OFF --len LEN\n"
            "  %s --dir DIR write --slot N --offset OFF --hex HEX\n"
            "  %s --dir DIR read-sector --slot N --sector S\n"
            "  %s --dir DIR write-sector --slot N --sector S --hex HEX\n",
            argv0, argv0, argv0, argv0, argv0);
}

static int parse_u32(const char *s, uint32_t *out) {
    char *end = NULL;
    unsigned long v;

    if (s == NULL || out == NULL) {
        return -1;
    }
    v = strtoul(s, &end, 0);
    if (end == s || *end != '\0') {
        return -1;
    }
    *out = (uint32_t)v;
    return 0;
}

static int parse_hex(const char *hex, uint8_t **out, size_t *outlen) {
    size_t n;
    size_t i;
    uint8_t *buf;

    if (hex == NULL) {
        return -1;
    }
    n = strlen(hex);
    if (n == 0 || (n % 2) != 0) {
        return -1;
    }
    buf = (uint8_t *)malloc(n / 2);
    if (buf == NULL) {
        return -1;
    }
    for (i = 0; i < n; i += 2) {
        unsigned int b;
        if (!isxdigit((unsigned char)hex[i]) ||
            !isxdigit((unsigned char)hex[i + 1])) {
            free(buf);
            return -1;
        }
        if (sscanf(hex + i, "%2x", &b) != 1) {
            free(buf);
            return -1;
        }
        buf[i / 2] = (uint8_t)b;
    }
    *out = buf;
    *outlen = n / 2;
    return 0;
}

int main(int argc, char **argv) {
    const char *dir = NULL;
    const char *cmd = NULL;
    const char *hex = NULL;
    int slot = -1;
    uint32_t offset = 0;
    uint32_t len = 0;
    uint32_t sector = 0;
    int have_offset = 0;
    int have_len = 0;
    int have_sector = 0;
    int i;

    for (i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--dir") == 0 && i + 1 < argc) {
            dir = argv[++i];
        } else if (strcmp(argv[i], "--slot") == 0 && i + 1 < argc) {
            uint32_t v;
            if (parse_u32(argv[++i], &v) != 0 || v > 1) {
                fprintf(stderr, "error: bad --slot\n");
                return 2;
            }
            slot = (int)v;
        } else if (strcmp(argv[i], "--offset") == 0 && i + 1 < argc) {
            if (parse_u32(argv[++i], &offset) != 0) {
                fprintf(stderr, "error: bad --offset\n");
                return 2;
            }
            have_offset = 1;
        } else if (strcmp(argv[i], "--len") == 0 && i + 1 < argc) {
            if (parse_u32(argv[++i], &len) != 0) {
                fprintf(stderr, "error: bad --len\n");
                return 2;
            }
            have_len = 1;
        } else if (strcmp(argv[i], "--sector") == 0 && i + 1 < argc) {
            if (parse_u32(argv[++i], &sector) != 0) {
                fprintf(stderr, "error: bad --sector\n");
                return 2;
            }
            have_sector = 1;
        } else if (strcmp(argv[i], "--hex") == 0 && i + 1 < argc) {
            hex = argv[++i];
        } else if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            usage(argv[0]);
            return 0;
        } else if (cmd == NULL) {
            cmd = argv[i];
        } else {
            fprintf(stderr, "error: unexpected argument: %s\n", argv[i]);
            usage(argv[0]);
            return 2;
        }
    }

    if (dir == NULL || cmd == NULL || slot < 0) {
        usage(argv[0]);
        return 2;
    }
    if (memcard_host_set_dir(dir) != 0) {
        fprintf(stderr, "error: memcard_host_set_dir failed\n");
        return 1;
    }

    if (strcmp(cmd, "ensure") == 0) {
        if (memcard_host_ensure(slot) != 0) {
            fprintf(stderr, "error: ensure failed\n");
            return 1;
        }
        return 0;
    }

    if (strcmp(cmd, "read") == 0) {
        uint8_t *buf;
        int n;

        if (!have_offset || !have_len || len == 0) {
            usage(argv[0]);
            return 2;
        }
        buf = (uint8_t *)malloc(len);
        if (buf == NULL) {
            fprintf(stderr, "error: oom\n");
            return 1;
        }
        n = memcard_host_read(slot, offset, buf, len);
        if (n < 0 || (uint32_t)n != len) {
            fprintf(stderr, "error: read failed\n");
            free(buf);
            return 1;
        }
        if (fwrite(buf, 1, len, stdout) != len) {
            fprintf(stderr, "error: short stdout write\n");
            free(buf);
            return 1;
        }
        free(buf);
        return 0;
    }

    if (strcmp(cmd, "write") == 0) {
        uint8_t *buf = NULL;
        size_t buflen = 0;
        int n;

        if (!have_offset || hex == NULL) {
            usage(argv[0]);
            return 2;
        }
        if (parse_hex(hex, &buf, &buflen) != 0 || buflen == 0) {
            fprintf(stderr, "error: bad --hex\n");
            return 2;
        }
        n = memcard_host_write(slot, offset, buf, buflen);
        free(buf);
        if (n < 0 || (size_t)n != buflen) {
            fprintf(stderr, "error: write failed\n");
            return 1;
        }
        return 0;
    }

    if (strcmp(cmd, "read-sector") == 0) {
        uint8_t buf[MEMCARD_HOST_SECTOR_SIZE];

        if (!have_sector) {
            usage(argv[0]);
            return 2;
        }
        if (memcard_host_read_sector(slot, (int)sector, buf) != 0) {
            fprintf(stderr, "error: read-sector failed\n");
            return 1;
        }
        if (fwrite(buf, 1, sizeof(buf), stdout) != sizeof(buf)) {
            fprintf(stderr, "error: short stdout write\n");
            return 1;
        }
        return 0;
    }

    if (strcmp(cmd, "write-sector") == 0) {
        uint8_t *buf = NULL;
        size_t buflen = 0;
        uint8_t sector_buf[MEMCARD_HOST_SECTOR_SIZE];

        if (!have_sector || hex == NULL) {
            usage(argv[0]);
            return 2;
        }
        if (parse_hex(hex, &buf, &buflen) != 0 || buflen == 0) {
            fprintf(stderr, "error: bad --hex\n");
            return 2;
        }
        if (buflen > MEMCARD_HOST_SECTOR_SIZE) {
            fprintf(stderr, "error: hex longer than one sector\n");
            free(buf);
            return 2;
        }
        memset(sector_buf, 0, sizeof(sector_buf));
        memcpy(sector_buf, buf, buflen);
        free(buf);
        if (memcard_host_write_sector(slot, (int)sector, sector_buf) != 0) {
            fprintf(stderr, "error: write-sector failed\n");
            return 1;
        }
        return 0;
    }

    fprintf(stderr, "error: unknown command: %s\n", cmd);
    usage(argv[0]);
    return 2;
}
