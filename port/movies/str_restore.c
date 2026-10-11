/* Rewrite packed form-2 STR files from the owner's Mode2/2352 disc image.
 *
 * The 2048-byte extract drops the last two XA ADPCM blocks. Playback then
 * fills that gap with silence and the soundtrack runs long. A file whose
 * size is sectors*2352 is already raw Mode2 and is left alone, so this runs
 * once.
 */
#include "str_stream.h"

#include "cdfs.h"
#include "str_disc.h"

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#define STR_RAW_BYTES 2352
#define COPY_CHUNK (256 * 1024)
#define PROGRESS_EVERY (8 * 1024 * 1024)

static void (*g_progress)(const char *msg, unsigned long long done,
                          unsigned long long total);
static unsigned long long g_bytes_done;
static unsigned long long g_bytes_total;

void dw3_str_restore_set_progress(
    void (*fn)(const char *msg, unsigned long long done, unsigned long long total)) {
    g_progress = fn;
}

static void report(const char *msg) {
    if (g_progress != NULL) {
        g_progress(msg, g_bytes_done, g_bytes_total);
    }
}

static void note(const char *msg) {
    fprintf(stderr, "dw3-str: %s\n", msg);
    fflush(stderr);
    report(msg);
}

static void note_fmt(const char *fmt, const char *a, unsigned long b) {
    char buf[512];
    snprintf(buf, sizeof(buf), fmt, a, b);
    note(buf);
}

static int ends_with_str(const char *path) {
    size_t n;
    char a, b, c;

    if (path == NULL) {
        return 0;
    }
    n = strlen(path);
    if (n < 4 || path[n - 4] != '.') {
        return 0;
    }
    a = (char)tolower((unsigned char)path[n - 3]);
    b = (char)tolower((unsigned char)path[n - 2]);
    c = (char)tolower((unsigned char)path[n - 1]);
    return a == 's' && b == 't' && c == 'r';
}

static int file_size(const char *path, unsigned long long *out) {
    struct stat st;

    if (stat(path, &st) != 0 || !S_ISREG(st.st_mode)) {
        return -1;
    }
    *out = (unsigned long long)st.st_size;
    return 0;
}

static int is_mode2_sync(const unsigned char *s) {
    int i;

    if (s[0] != 0x00 || s[11] != 0x00 || s[15] != 0x02) {
        return 0;
    }
    for (i = 1; i < 11; i++) {
        if (s[i] != 0xFF) {
            return 0;
        }
    }
    return 1;
}

struct Need {
    int count;
    unsigned long long min_image;
    /* Sum of sectors*2352 for form-2 STR files that are not raw yet. */
    unsigned long long bytes;
};

static int count_need(const CdfsMapInfo *info, void *user) {
    struct Need *need = (struct Need *)user;
    char abs[1024];
    unsigned long long sz = 0;
    unsigned long long expect;

    if (info->form != 2 || info->sectors == 0 || !ends_with_str(info->path)) {
        return 0;
    }
    expect = (unsigned long long)info->sectors * (unsigned long long)STR_RAW_BYTES;
    if (cdfs_join_root(info->path, abs, sizeof(abs)) != 0) {
        need->count++;
    } else if (file_size(abs, &sz) != 0 || sz != expect) {
        need->count++;
    } else {
        return 0;
    }
    need->bytes += expect;
    {
        unsigned long long end =
            ((unsigned long long)info->lba + (unsigned long long)info->sectors) *
            (unsigned long long)STR_RAW_BYTES;
        if (end > need->min_image) {
            need->min_image = end;
        }
    }
    return 0;
}

static int exists_file(const char *path) {
    unsigned long long sz;
    return path != NULL && path[0] != '\0' && file_size(path, &sz) == 0;
}

struct CopyCtx {
    FILE *bin;
    int failed;
    int wrote;
};

static int restore_one(const CdfsMapInfo *info, void *user) {
    struct CopyCtx *ctx = (struct CopyCtx *)user;
    char abs[1024];
    char tmp[1100];
    char bak[1100];
    unsigned long long expect;
    unsigned long long sz = 0;
    unsigned long long done = 0;
    unsigned long long next_note = PROGRESS_EVERY;
    unsigned char *chunk = NULL;
    FILE *out = NULL;
    int ok = 0;

    if (info->form != 2 || info->sectors == 0 || !ends_with_str(info->path)) {
        return 0;
    }
    if (cdfs_join_root(info->path, abs, sizeof(abs)) != 0) {
        ctx->failed++;
        return 0;
    }
    expect = (unsigned long long)info->sectors * (unsigned long long)STR_RAW_BYTES;
    if (file_size(abs, &sz) == 0 && sz == expect) {
        return 0;
    }
    if (snprintf(tmp, sizeof(tmp), "%s.tmp", abs) >= (int)sizeof(tmp) ||
        snprintf(bak, sizeof(bak), "%s.bak", abs) >= (int)sizeof(bak)) {
        ctx->failed++;
        return 0;
    }

    note_fmt("restore %s (%lu sectors)", info->path, (unsigned long)info->sectors);
    if (fseeko(ctx->bin, (off_t)info->lba * (off_t)STR_RAW_BYTES, SEEK_SET) != 0) {
        note_fmt("seek failed %s lba %lu", info->path, (unsigned long)info->lba);
        ctx->failed++;
        return 0;
    }

    chunk = (unsigned char *)malloc(COPY_CHUNK);
    out = fopen(tmp, "wb");
    if (chunk == NULL || out == NULL) {
        note("restore: out of memory or cannot create temp file");
        ctx->failed++;
        goto done;
    }

    while (done < expect) {
        size_t want = COPY_CHUNK;
        size_t nread;

        if ((unsigned long long)want > expect - done) {
            want = (size_t)(expect - done);
        }
        nread = fread(chunk, 1, want, ctx->bin);
        if (nread != want) {
            note_fmt("short read %s at %lu", info->path, (unsigned long)done);
            ctx->failed++;
            goto done;
        }
        if (done == 0 && !is_mode2_sync(chunk)) {
            note_fmt("not Mode2/2352 at %s lba %lu", info->path, (unsigned long)info->lba);
            ctx->failed++;
            goto done;
        }
        if (fwrite(chunk, 1, nread, out) != nread) {
            note_fmt("write failed %s", info->path, 0);
            ctx->failed++;
            goto done;
        }
        done += nread;
        g_bytes_done += (unsigned long long)nread;
        if (done >= next_note || done == expect) {
            char buf[128];
            snprintf(buf, sizeof(buf), "restore %s %llu/%llu", info->path, done, expect);
            note(buf);
            next_note += PROGRESS_EVERY;
        } else {
            /* Every chunk, so the host can repaint on its own clock.
             * PROGRESS_EVERY stays the log interval. Swapping here would
             * wait on vsync and throttle the copy. */
            report(NULL);
        }
    }
    if (fclose(out) != 0) {
        out = NULL;
        ctx->failed++;
        goto done;
    }
    out = NULL;

    if (exists_file(abs)) {
        remove(bak);
        if (rename(abs, bak) != 0) {
            note_fmt("cannot move aside %s errno %lu", abs, (unsigned long)errno);
            ctx->failed++;
            goto done;
        }
    }
    if (rename(tmp, abs) != 0) {
        rename(bak, abs);
        note_fmt("cannot install %s errno %lu", abs, (unsigned long)errno);
        ctx->failed++;
        goto done;
    }
    remove(bak);
    ok = 1;
    ctx->wrote++;

done:
    if (out != NULL) {
        fclose(out);
    }
    if (!ok) {
        remove(tmp);
    }
    free(chunk);
    return 0;
}

int dw3_str_restore_from_disc(void) {
    struct Need need;
    struct CopyCtx ctx;
    char bin_path[1024];
    char msg[1100];

    g_bytes_done = 0;
    g_bytes_total = 0;
    memset(&need, 0, sizeof(need));
    if (cdfs_visit(count_need, &need) != 0) {
        return -1;
    }
    if (need.count == 0) {
        return 0;
    }
    g_bytes_total = need.bytes;
    if (dw3_str_find_disc(cdfs_root(), bin_path, sizeof(bin_path), need.min_image) != 0) {
        note("STR still 2048-byte; put the Mode2 BIN/CUE next to the runtime "
             "(or set DW3_DISC_BIN) to restore XA");
        return 0;
    }

    /* boot_main.c holds the opening sweep on the "restoring " prefix. */
    snprintf(msg, sizeof(msg), "restoring %d STR from %s", need.count, bin_path);
    note(msg);

    memset(&ctx, 0, sizeof(ctx));
    ctx.bin = fopen(bin_path, "rb");
    if (ctx.bin == NULL) {
        note("cannot open disc image");
        return -1;
    }
    cdfs_visit(restore_one, &ctx);
    fclose(ctx.bin);

    snprintf(msg, sizeof(msg), "STR restore wrote=%d failed=%d", ctx.wrote, ctx.failed);
    note(msg);
    return ctx.failed != 0 ? -1 : 0;
}
