#include "str_stream.h"

#include "cdfs.h"

#include <libcd.h>
#include <stdio.h>
#include <string.h>

#define STR_USER_BYTES 2048
#define STR_RAW_BYTES 2352

typedef struct {
    int open;
    uint32_t id;
    uint32_t base_lba;
    uint32_t sectors;
    uint32_t form;
    char relpath[512];
    char path[1024];
    FILE *fp;
} StrStream;

static StrStream g_str;

static void encode_bcd_byte(int v, u_char *out) {
    if (v < 0) {
        v = 0;
    }
    if (v > 99) {
        v = 99;
    }
    *out = (u_char)(((v / 10) << 4) | (v % 10));
}

static void fill_mode2_headers(int abs_lba, uint8_t out[STR_RAW_BYTES]) {
    int total = abs_lba + 150;
    int mm = total / (75 * 60);
    int ss = (total / 75) % 60;
    int ff = total % 75;

    memset(out, 0, STR_RAW_BYTES);
    /* Sync */
    out[0] = 0x00;
    memset(out + 1, 0xFF, 10);
    out[11] = 0x00;
    /* Header: MSF + mode 2 */
    encode_bcd_byte(mm, &out[12]);
    encode_bcd_byte(ss, &out[13]);
    encode_bcd_byte(ff, &out[14]);
    out[15] = 0x02;
    /* Subheader (Form2 | RealTime | Data): file=1 chan=1 submode=0x64 */
    out[16] = 0x01;
    out[17] = 0x01;
    out[18] = 0x64;
    out[19] = 0x00;
    out[20] = 0x01;
    out[21] = 0x01;
    out[22] = 0x64;
    out[23] = 0x00;
}

static int open_path(uint32_t id, const char *rel, uint32_t base_lba,
                     uint32_t sectors, uint32_t form) {
    char abs[1024];

    if (cdfs_join_root(rel, abs, sizeof(abs)) != 0) {
        return -1;
    }
    if (g_str.fp) {
        fclose(g_str.fp);
        g_str.fp = NULL;
    }
    g_str.fp = fopen(abs, "rb");
    if (!g_str.fp) {
        fprintf(stderr, "dw3-str: open failed %s\n", abs);
        g_str.open = 0;
        return -1;
    }
    g_str.open = 1;
    g_str.id = id;
    g_str.base_lba = base_lba;
    g_str.sectors = sectors;
    g_str.form = form;
    snprintf(g_str.relpath, sizeof(g_str.relpath), "%s", rel);
    snprintf(g_str.path, sizeof(g_str.path), "%s", abs);
    fprintf(stderr,
            "dw3-str: open id=%u lba=%u sectors=%u form=%u path=%s\n",
            (unsigned)id, (unsigned)base_lba, (unsigned)sectors,
            (unsigned)form, abs);
    return 0;
}

static int open_for_lba(int lba) {
    uint32_t id = 0;
    uint32_t base = 0;
    uint32_t sectors = 0;
    uint32_t form = 0;
    char rel[512];

    if (lba < 0) {
        return -1;
    }
    if (g_str.open && (uint32_t)lba >= g_str.base_lba &&
        (uint32_t)lba < g_str.base_lba + g_str.sectors) {
        return 0;
    }
    if (cdfs_lookup_lba((uint32_t)lba, &id, rel, sizeof(rel), &base, &sectors,
                        &form) != 0) {
        return -1;
    }
    return open_path(id, rel, base, sectors, form);
}

int dw3_str_stream_open_at_lba(int lba) { return open_for_lba(lba); }

int dw3_str_stream_open_at_loc(const CdlLOC *loc) {
    if (!loc) {
        return -1;
    }
    return open_for_lba(CdPosToInt((CdlLOC *)loc));
}

int dw3_str_stream_active(void) { return g_str.open != 0; }

void dw3_str_stream_close(void) {
    if (g_str.fp) {
        fclose(g_str.fp);
        g_str.fp = NULL;
    }
    memset(&g_str, 0, sizeof(g_str));
}

int dw3_str_stream_read_sector(int lba, uint8_t out[STR_RAW_BYTES]) {
    long off;
    size_t n;
    uint8_t user[STR_USER_BYTES];

    if (!out || open_for_lba(lba) != 0) {
        return -1;
    }
    off = (long)((uint32_t)lba - g_str.base_lba) * (long)STR_USER_BYTES;
    if (fseek(g_str.fp, off, SEEK_SET) != 0) {
        return -1;
    }
    memset(user, 0, sizeof(user));
    n = fread(user, 1, STR_USER_BYTES, g_str.fp);
    if (n == 0) {
        return -1;
    }
    fill_mode2_headers(lba, out);
    memcpy(out + 24, user, STR_USER_BYTES);
    /* Remaining Form2 user (2324-2048) + EDC stay zero from fill. */
    return 0;
}

int dw3_str_cd_read_cb(struct PsyzCdRead *read) {
    if (!read || !read->buffer || read->size < STR_RAW_BYTES) {
        return -1;
    }
    if (dw3_str_stream_read_sector((int)read->sector, (uint8_t *)read->buffer) !=
        0) {
        return -1;
    }
    return (int)STR_RAW_BYTES;
}

void dw3_str_stream_install_cd_bridge(void) {
    Psyz_CdSetReadCB(dw3_str_cd_read_cb);
    fprintf(stderr, "dw3-str: installed Psyz_CdSetReadCB file bridge\n");
}
