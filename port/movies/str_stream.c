#include "str_stream.h"

#include "cdfs.h"

#include <libcd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define STR_USER_BYTES 2048
#define STR_FORM2_USER 2324
#define STR_RAW_BYTES 2352
#define STR_VIDEO_MAGIC 0x0160

typedef struct {
    int open;
    uint32_t id;
    uint32_t base_lba;
    uint32_t sectors;
    uint32_t form;
    uint32_t stride; /* 2048 user payload, or 2352 raw Mode2 */
    char relpath[512];
    char path[1024];
    FILE *fp;
} StrStream;

static StrStream g_str;
static FILE *g_disc_bin;
static int g_disc_bin_tried;
static int g_xa_hdr_logged;
static int g_disc_bin_logged;

static void encode_bcd_byte(int v, u_char *out) {
    if (v < 0) {
        v = 0;
    }
    if (v > 99) {
        v = 99;
    }
    *out = (u_char)(((v / 10) << 4) | (v % 10));
}

static int is_video_user(const uint8_t user[STR_USER_BYTES]) {
    uint16_t magic = (uint16_t)user[0] | ((uint16_t)user[1] << 8);
    return magic == STR_VIDEO_MAGIC;
}

/* Subheader matches raw US MOVIEOPN.STR Mode2/Form2 sectors:
 * video: file=1 chan=1 submode=Data|Form2|RT (0x68) ci=0
 * XA:    file=1 chan=1 submode=Audio|Form2|RT (0x64) ci=stereo 4-bit 37800 (0x01)
 */
static void fill_mode2_headers(int abs_lba, int xa_audio,
                               uint8_t out[STR_RAW_BYTES]) {
    int total = abs_lba + 150;
    int mm = total / (75 * 60);
    int ss = (total / 75) % 60;
    int ff = total % 75;
    uint8_t submode = xa_audio ? 0x64 : 0x68;
    uint8_t ci = xa_audio ? 0x01 : 0x00;

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
    /* Subheader (duplicated) */
    out[16] = 0x01;
    out[17] = 0x01;
    out[18] = submode;
    out[19] = ci;
    out[20] = 0x01;
    out[21] = 0x01;
    out[22] = submode;
    out[23] = ci;
}

static FILE *disc_bin_fp(void) {
    const char *path;

    if (g_disc_bin) {
        return g_disc_bin;
    }
    if (g_disc_bin_tried) {
        return NULL;
    }
    g_disc_bin_tried = 1;
    path = getenv("DW3_DISC_BIN");
    if (!path || !path[0]) {
        return NULL;
    }
    g_disc_bin = fopen(path, "rb");
    if (!g_disc_bin) {
        fprintf(stderr, "dw3-str: DW3_DISC_BIN open failed: %s\n", path);
        return NULL;
    }
    fprintf(stderr, "dw3-str: raw disc bin for Form2 XA: %s\n", path);
    return g_disc_bin;
}

static int read_raw_disc_sector(int lba, uint8_t out[STR_RAW_BYTES]) {
    FILE *fp = disc_bin_fp();
    long off;

    if (!fp || lba < 0) {
        return -1;
    }
    off = (long)lba * (long)STR_RAW_BYTES;
    if (fseek(fp, off, SEEK_SET) != 0) {
        return -1;
    }
    if (fread(out, 1, STR_RAW_BYTES, fp) != STR_RAW_BYTES) {
        return -1;
    }
    if (g_disc_bin_logged < 1) {
        fprintf(stderr,
                "dw3-str: raw bin sector %d submode=0x%02x ci=0x%02x\n", lba,
                out[18], out[19]);
        g_disc_bin_logged++;
    }
    return 0;
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
    g_str.stride = STR_USER_BYTES;
    if (sectors > 0 && fseek(g_str.fp, 0, SEEK_END) == 0) {
        long sz = ftell(g_str.fp);
        if (sz == (long)sectors * (long)STR_RAW_BYTES) {
            g_str.stride = STR_RAW_BYTES;
        }
        fseek(g_str.fp, 0, SEEK_SET);
    }
    snprintf(g_str.relpath, sizeof(g_str.relpath), "%s", rel);
    snprintf(g_str.path, sizeof(g_str.path), "%s", abs);
    fprintf(stderr,
            "dw3-str: open id=%u lba=%u sectors=%u form=%u stride=%u path=%s\n",
            (unsigned)id, (unsigned)base_lba, (unsigned)sectors,
            (unsigned)form, (unsigned)g_str.stride, abs);
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
    if (g_disc_bin) {
        fclose(g_disc_bin);
        g_disc_bin = NULL;
    }
    g_disc_bin_tried = 0;
}

int dw3_str_stream_read_sector(int lba, uint8_t out[STR_RAW_BYTES]) {
    long off;
    size_t n;
    uint8_t user[STR_USER_BYTES];
    int xa_audio;

    if (!out || open_for_lba(lba) != 0) {
        return -1;
    }

    /* A restored STR is already one raw Mode2 sector per map sector, so the
     * last two XA ADPCM blocks are in the file. The 2048-byte extract is
     * not: those blocks are gone, and zero-filling them stretches XA. */
    if (g_str.stride == STR_RAW_BYTES) {
        off = (long)((uint32_t)lba - g_str.base_lba) * (long)STR_RAW_BYTES;
        if (fseek(g_str.fp, off, SEEK_SET) != 0) {
            return -1;
        }
        if (fread(out, 1, STR_RAW_BYTES, g_str.fp) != STR_RAW_BYTES) {
            return -1;
        }
        return 0;
    }

    /* Prefer owner raw Mode2/2352 bin when set: packed STR truncates Form2
     * user data at 2048 (drops last 2 XA ADPCM blocks). */
    if (g_str.form == 2 && read_raw_disc_sector(lba, out) == 0) {
        return 0;
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
    xa_audio = !is_video_user(user);
    fill_mode2_headers(lba, xa_audio, out);
    memcpy(out + 24, user, STR_USER_BYTES);
    /* Remaining Form2 user (2324-2048) + EDC stay zero from fill when the
     * raw disc bin is unavailable — XA decode still gets 16/18 blocks. */
    if (xa_audio && g_xa_hdr_logged < 1) {
        fprintf(stderr,
                "dw3-str: XA sector %d via packed 2048 (submode=0x%02x "
                "ci=0x%02x; last 2 ADPCM blocks missing, soundtrack "
                "stretched — restore_form2_str.py or DW3_DISC_BIN)\n",
                lba, out[18], out[19]);
        g_xa_hdr_logged++;
    }
    (void)STR_FORM2_USER;
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
    /* Probe optional disc bin early so logs are near bridge install. */
    (void)disc_bin_fp();
}
