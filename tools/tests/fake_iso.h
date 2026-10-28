/* A synthetic Mode2/2352 image for the disc-detection tests: ISO9660 PVD at
 * LBA 16 whose root directory (LBA 20) lists `exe`. Header only, no game data. */
#ifndef FAKE_ISO_H
#define FAKE_ISO_H

#include <stdio.h>
#include <string.h>

static void fake_iso_put(FILE *fp, unsigned lba, const unsigned char *data) {
    fseek(fp, (long)(lba * 2352u + 24u), SEEK_SET);
    fwrite(data, 1, 2048, fp);
}

/* exe NULL = a valid ISO with no executable in the root directory.
 * Returns 0, or -1 when the file cannot be created. */
static int make_iso(const char *path, const char *exe) {
    unsigned char sec[2048];
    FILE *fp = fopen(path, "wb");
    unsigned n;

    if (fp == NULL) {
        return -1;
    }
    memset(sec, 0, sizeof(sec));
    sec[0] = 1;
    memcpy(sec + 1, "CD001", 5);
    sec[156] = 34;
    sec[156 + 2] = 20;    /* root extent LBA, little endian */
    sec[156 + 11] = 0x08; /* size 2048 */
    fake_iso_put(fp, 16, sec);
    memset(sec, 0, sizeof(sec));
    sec[0] = 34; /* the "." entry */
    sec[32] = 1;
    n = 34;
    if (exe != NULL) {
        char name[32];
        unsigned nl;

        snprintf(name, sizeof(name), "%s;1", exe);
        nl = (unsigned)strlen(name);
        sec[n] = (unsigned char)(33 + nl + ((nl & 1u) ? 0 : 1));
        sec[n + 32] = (unsigned char)nl;
        memcpy(sec + n + 33, name, nl);
    }
    fake_iso_put(fp, 20, sec);
    fclose(fp);
    return 0;
}

#endif
