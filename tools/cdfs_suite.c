/* Formal P2.5 / P4.5 CDFS acceptance suite (host).
 *
 * Same code path the Switch uses under sdmc:/switch/dw3/<ver>/ — only the
 * root prefix differs. Point --root at PC runtime/<ver> or a mounted /
 * copied SD tree (…/switch/dw3/us).
 *
 * Usage:
 *   cdfs_suite --self-test WORKDIR
 *   cdfs_suite --root DIR [--map PATH] [--samples N] [--max-full BYTES]
 *
 * Exit 0 only when every check prints OK.
 */
#include "cdfs.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

static int g_fails;
static int g_oks;

static void ok(const char *name) {
    printf("OK  %s\n", name);
    g_oks++;
}

static void fail(const char *name, const char *detail) {
    if (detail && detail[0]) {
        printf("FAIL %s: %s\n", name, detail);
    } else {
        printf("FAIL %s\n", name);
    }
    g_fails++;
}

static int ensure_dir(const char *path) {
    struct stat st;
    if (stat(path, &st) == 0) {
        return S_ISDIR(st.st_mode) ? 0 : -1;
    }
    if (mkdir(path, 0755) != 0 && errno != EEXIST) {
        return -1;
    }
    return 0;
}

static int write_file(const char *path, const void *data, size_t n) {
    FILE *fp = fopen(path, "wb");
    if (fp == NULL) {
        return -1;
    }
    if (n > 0 && fwrite(data, 1, n, fp) != n) {
        fclose(fp);
        return -1;
    }
    fclose(fp);
    return 0;
}

static int read_host(const char *path, unsigned char **out, size_t *out_len) {
    FILE *fp;
    long sz;
    unsigned char *buf;
    size_t n;

    fp = fopen(path, "rb");
    if (fp == NULL) {
        return -1;
    }
    if (fseek(fp, 0, SEEK_END) != 0) {
        fclose(fp);
        return -1;
    }
    sz = ftell(fp);
    if (sz < 0) {
        fclose(fp);
        return -1;
    }
    if (fseek(fp, 0, SEEK_SET) != 0) {
        fclose(fp);
        return -1;
    }
    buf = (unsigned char *)malloc((size_t)sz ? (size_t)sz : 1);
    if (buf == NULL) {
        fclose(fp);
        return -1;
    }
    n = fread(buf, 1, (size_t)sz, fp);
    fclose(fp);
    if (n != (size_t)sz) {
        free(buf);
        return -1;
    }
    *out = buf;
    *out_len = n;
    return 0;
}

static int check_read_id(uint32_t id, const char *host_path) {
    char name[64];
    unsigned char *expect = NULL;
    unsigned char *got = NULL;
    size_t expect_len = 0;
    uint32_t sz;
    int n;
    int rc = -1;

    snprintf(name, sizeof(name), "cdfs_read id=%u", (unsigned)id);
    if (read_host(host_path, &expect, &expect_len) != 0) {
        fail(name, "host open/read failed");
        return -1;
    }
    sz = cdfs_file_size(id);
    if (sz == (uint32_t)-1 || (size_t)sz != expect_len) {
        fail(name, "size mismatch");
        goto done;
    }
    got = (unsigned char *)malloc(sz ? sz : 1);
    if (got == NULL) {
        fail(name, "oom");
        goto done;
    }
    n = cdfs_read(id, got, sz);
    if (n < 0 || (uint32_t)n != sz || memcmp(got, expect, expect_len) != 0) {
        fail(name, "bytes differ");
        goto done;
    }
    ok(name);
    rc = 0;
done:
    free(expect);
    free(got);
    return rc;
}

static int check_read_at(uint32_t id, const unsigned char *expect, size_t expect_len,
                         size_t offset, size_t len) {
    char name[96];
    unsigned char *buf;
    int n;
    size_t from_file;
    size_t i;

    snprintf(name, sizeof(name), "cdfs_read_at id=%u off=%zu len=%zu",
             (unsigned)id, offset, len);
    buf = (unsigned char *)malloc(len ? len : 1);
    if (buf == NULL) {
        fail(name, "oom");
        return -1;
    }
    memset(buf, 0xA5, len ? len : 1);
    n = cdfs_read_at(id, buf, offset, len);
    if (n < 0) {
        fail(name, "cdfs_read_at failed");
        free(buf);
        return -1;
    }
    from_file = 0;
    if (offset < expect_len) {
        from_file = expect_len - offset;
        if (from_file > len) {
            from_file = len;
        }
    }
    if ((size_t)n != from_file) {
        fail(name, "short/long return");
        free(buf);
        return -1;
    }
    if (from_file > 0 && memcmp(buf, expect + offset, from_file) != 0) {
        fail(name, "window bytes differ");
        free(buf);
        return -1;
    }
    for (i = from_file; i < len; i++) {
        if (buf[i] != 0) {
            fail(name, "EOF tail not zero-filled");
            free(buf);
            return -1;
        }
    }
    ok(name);
    free(buf);
    return 0;
}

static int check_lookup_lba(uint32_t lba, uint32_t expect_id, const char *expect_path) {
    char name[64];
    char path[512];
    uint32_t id = 0;
    uint32_t base = 0;
    uint32_t sectors = 0;
    uint32_t form = 0;

    snprintf(name, sizeof(name), "cdfs_lookup_lba lba=%u", (unsigned)lba);
    if (cdfs_lookup_lba(lba, &id, path, sizeof(path), &base, &sectors, &form) != 0) {
        fail(name, "not found");
        return -1;
    }
    if (id != expect_id || strcmp(path, expect_path) != 0 || base == 0 ||
        sectors == 0) {
        fail(name, "id/path/meta mismatch");
        return -1;
    }
    ok(name);
    return 0;
}

typedef struct {
    uint32_t id;
    char path[256];
    uint32_t lba;
    uint32_t sectors;
    uint32_t form;
    uint32_t bytes;
    uint32_t host_size;
} Sample;

typedef struct {
    Sample *items;
    size_t count;
    size_t cap;
    size_t want;
    size_t max_full;
    const char *root;
} CollectCtx;

static int collect_fn(const CdfsMapInfo *info, void *user) {
    CollectCtx *ctx = (CollectCtx *)user;
    char host[1024];
    struct stat st;
    Sample *s;

    if (ctx->count >= ctx->want) {
        return 1;
    }
    if (info->path == NULL || info->path[0] == '\0' || info->lba == 0) {
        return 0;
    }
    if ((size_t)snprintf(host, sizeof(host), "%s/%s", ctx->root, info->path) >=
        sizeof(host)) {
        return 0;
    }
    if (stat(host, &st) != 0 || !S_ISREG(st.st_mode) || st.st_size <= 0) {
        return 0;
    }
    if ((size_t)st.st_size > ctx->max_full && ctx->count > 0) {
        /* Keep looking for smaller samples; still allow one large later via
         * read_at only. */
        return 0;
    }
    if (ctx->count == ctx->cap) {
        size_t ncap = ctx->cap ? ctx->cap * 2 : 8;
        Sample *n = (Sample *)realloc(ctx->items, ncap * sizeof(Sample));
        if (n == NULL) {
            return -1;
        }
        ctx->items = n;
        ctx->cap = ncap;
    }
    s = &ctx->items[ctx->count++];
    s->id = info->id;
    snprintf(s->path, sizeof(s->path), "%s", info->path);
    s->lba = info->lba;
    s->sectors = info->sectors;
    s->form = info->form;
    s->bytes = info->bytes;
    s->host_size = (uint32_t)st.st_size;
    return 0;
}

static int find_multisetor(const CdfsMapInfo *info, void *user) {
    Sample *out = (Sample *)user;
    if (out->id != 0) {
        return 1;
    }
    if (info->sectors >= 2 && info->path && info->path[0] && info->lba != 0) {
        out->id = info->id;
        snprintf(out->path, sizeof(out->path), "%s", info->path);
        out->lba = info->lba;
        out->sectors = info->sectors;
        out->form = info->form;
        out->bytes = info->bytes;
        return 1;
    }
    return 0;
}

static void run_packed_suite(const char *root, const char *map, size_t samples,
                             size_t max_full) {
    CollectCtx ctx;
    Sample multi;
    size_t i;
    char host[1024];
    unsigned char *expect = NULL;
    size_t expect_len = 0;

    memset(&ctx, 0, sizeof(ctx));
    ctx.want = samples;
    ctx.max_full = max_full;
    ctx.root = root;

    if (cdfs_set_root(root) != 0) {
        fail("cdfs_set_root", root);
        return;
    }
    ok("cdfs_set_root");

    if (cdfs_load_map(map) != 0) {
        fail("cdfs_load_map", map ? map : "<default files_us/eu.json>");
        return;
    }
    ok("cdfs_load_map");

    if (cdfs_visit(collect_fn, &ctx) < 0) {
        fail("cdfs_visit collect", "oom or visit error");
        free(ctx.items);
        return;
    }
    if (ctx.count == 0) {
        fail("sample files", "no readable mapped files under root");
        free(ctx.items);
        return;
    }
    {
        char detail[64];
        snprintf(detail, sizeof(detail), "%zu sample(s)", ctx.count);
        ok(detail);
    }

    for (i = 0; i < ctx.count; i++) {
        if ((size_t)snprintf(host, sizeof(host), "%s/%s", root,
                             ctx.items[i].path) >= sizeof(host)) {
            fail("path join", ctx.items[i].path);
            continue;
        }
        check_read_id(ctx.items[i].id, host);
        if (read_host(host, &expect, &expect_len) == 0) {
            size_t off = expect_len > 4 ? expect_len / 3 : 0;
            size_t win = expect_len > 8 ? 8 : expect_len;
            if (win == 0) {
                win = 1;
            }
            check_read_at(ctx.items[i].id, expect, expect_len, off, win);
            /* Past-EOF window must zero-fill. */
            check_read_at(ctx.items[i].id, expect, expect_len, expect_len, 4);
            free(expect);
            expect = NULL;
        }
        check_lookup_lba(ctx.items[i].lba, ctx.items[i].id, ctx.items[i].path);
        if (ctx.items[i].sectors > 1) {
            check_lookup_lba(ctx.items[i].lba + 1, ctx.items[i].id,
                             ctx.items[i].path);
        }
    }

    memset(&multi, 0, sizeof(multi));
    cdfs_visit(find_multisetor, &multi);
    if (multi.id != 0) {
        if ((size_t)snprintf(host, sizeof(host), "%s/%s", root, multi.path) <
                sizeof(host) &&
            read_host(host, &expect, &expect_len) == 0) {
            char name[64];
            snprintf(name, sizeof(name), "multi-sector id=%u", (unsigned)multi.id);
            if (expect_len > max_full) {
                /* Large Form-2 etc.: window + LBA only. */
                size_t win = 2048;
                if (win > expect_len) {
                    win = expect_len;
                }
                check_read_at(multi.id, expect, expect_len, 0, win);
                check_lookup_lba(multi.lba, multi.id, multi.path);
                ok(name);
            } else {
                check_read_id(multi.id, host);
                check_lookup_lba(multi.lba + (multi.sectors > 1 ? 1 : 0), multi.id,
                                 multi.path);
                ok(name);
            }
            free(expect);
            expect = NULL;
        }
    } else {
        ok("multi-sector sample skipped (none in map)");
    }

    /* Unknown id must fail. */
    {
        unsigned char scratch[8];
        if (cdfs_read(0xFFFFFFFFu, scratch, sizeof(scratch)) == -1) {
            ok("unknown id rejected");
        } else {
            fail("unknown id rejected", "cdfs_read unexpectedly succeeded");
        }
    }

    cdfs_shutdown();
    free(ctx.items);
}

static void run_path_reject(const char *workdir) {
    char root[512];
    char map_path[512];
    char outside[512];
    const char *evil_dotdot =
        "{\n"
        "  \"version\": \"us\",\n"
        "  \"executable\": \"SLUS_014.36\",\n"
        "  \"files\": {\n"
        "    \"1\": {\n"
        "      \"path\": \"../secret.dat\",\n"
        "      \"lba\": 1,\n"
        "      \"sectors\": 1,\n"
        "      \"bytes\": 4,\n"
        "      \"form\": 1\n"
        "    }\n"
        "  }\n"
        "}\n";
    const char *evil_abs =
        "{\n"
        "  \"version\": \"us\",\n"
        "  \"executable\": \"SLUS_014.36\",\n"
        "  \"files\": {\n"
        "    \"1\": {\n"
        "      \"path\": \"/tmp/abs.dat\",\n"
        "      \"lba\": 1,\n"
        "      \"sectors\": 1,\n"
        "      \"bytes\": 3,\n"
        "      \"form\": 1\n"
        "    }\n"
        "  }\n"
        "}\n";

    snprintf(root, sizeof(root), "%s/reject_root", workdir);
    snprintf(outside, sizeof(outside), "%s/secret.dat", workdir);
    if (ensure_dir(root) != 0 || write_file(outside, "LEAK", 4) != 0) {
        fail("path reject setup", "mkdir/write");
        return;
    }

    snprintf(map_path, sizeof(map_path), "%s/evil_dotdot.json", workdir);
    if (write_file(map_path, evil_dotdot, strlen(evil_dotdot)) != 0) {
        fail("path reject setup", "evil map");
        return;
    }
    cdfs_shutdown();
    if (cdfs_set_root(root) != 0) {
        fail("path reject set_root", NULL);
        return;
    }
    /* Unsafe paths are dropped at load; empty table → load fails. */
    if (cdfs_load_map(map_path) != 0) {
        ok("map load rejects ../ escape");
    } else {
        unsigned char buf[8];
        int n = cdfs_read(1, buf, sizeof(buf));
        if (n < 0) {
            ok("map load rejects ../ escape");
        } else {
            fail("map load rejects ../ escape", "read succeeded");
        }
    }
    cdfs_shutdown();

    snprintf(map_path, sizeof(map_path), "%s/evil_abs.json", workdir);
    if (write_file(map_path, evil_abs, strlen(evil_abs)) != 0) {
        fail("path reject abs setup", NULL);
        return;
    }
    if (cdfs_set_root(root) != 0) {
        fail("path reject abs set_root", NULL);
        return;
    }
    if (cdfs_load_map(map_path) != 0) {
        ok("map load rejects absolute path");
    } else {
        fail("map load rejects absolute path", "load unexpectedly succeeded");
    }
    cdfs_shutdown();
}

static int write_mini_fixture(const char *workdir) {
    char root[512];
    char path[576];
    const char *map =
        "{\n"
        "  \"version\": \"us\",\n"
        "  \"executable\": \"SLUS_014.36\",\n"
        "  \"files\": {\n"
        "    \"1\": {\n"
        "      \"path\": \"AAA/PRO/ALPHA.PRO\",\n"
        "      \"lba\": 100,\n"
        "      \"sectors\": 1,\n"
        "      \"bytes\": 11,\n"
        "      \"form\": 1\n"
        "    },\n"
        "    \"2\": {\n"
        "      \"path\": \"AAA/DAT/BETA.DAT\",\n"
        "      \"lba\": 101,\n"
        "      \"sectors\": 2,\n"
        "      \"bytes\": 16,\n"
        "      \"form\": 1\n"
        "    },\n"
        "    \"10\": {\n"
        "      \"path\": \"AAA/PRO/GAMMA.PRO\",\n"
        "      \"lba\": 200,\n"
        "      \"sectors\": 1,\n"
        "      \"bytes\": 7,\n"
        "      \"form\": 1\n"
        "    }\n"
        "  }\n"
        "}\n";

    snprintf(root, sizeof(root), "%s/mini", workdir);
    snprintf(path, sizeof(path), "%s/AAA", root);
    if (ensure_dir(root) != 0 || ensure_dir(path) != 0) {
        return -1;
    }
    snprintf(path, sizeof(path), "%s/AAA/PRO", root);
    if (ensure_dir(path) != 0) {
        return -1;
    }
    snprintf(path, sizeof(path), "%s/AAA/DAT", root);
    if (ensure_dir(path) != 0) {
        return -1;
    }
    snprintf(path, sizeof(path), "%s/AAA/PRO/ALPHA.PRO", root);
    if (write_file(path, "alpha-bytes", 11) != 0) {
        return -1;
    }
    snprintf(path, sizeof(path), "%s/AAA/DAT/BETA.DAT", root);
    if (write_file(path, "beta-bytes!!!!!!", 16) != 0) {
        return -1;
    }
    snprintf(path, sizeof(path), "%s/AAA/PRO/GAMMA.PRO", root);
    if (write_file(path, "gamma!!", 7) != 0) {
        return -1;
    }
    snprintf(path, sizeof(path), "%s/files_us.json", root);
    if (write_file(path, map, strlen(map)) != 0) {
        return -1;
    }
    return 0;
}

static void usage(const char *argv0) {
    fprintf(stderr,
            "usage:\n"
            "  %s --self-test WORKDIR\n"
            "  %s --root DIR [--map PATH] [--samples N] [--max-full BYTES]\n"
            "\n"
            "Packed roots use the P1.6 layout (PC runtime/<ver> or SD\n"
            "…/switch/dw3/<ver>). Default samples=8, max-full=1048576.\n",
            argv0, argv0);
}

int main(int argc, char **argv) {
    const char *root = NULL;
    const char *map = NULL;
    const char *self_dir = NULL;
    size_t samples = 8;
    size_t max_full = 1024u * 1024u;
    int i;

    for (i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--root") == 0 && i + 1 < argc) {
            root = argv[++i];
        } else if (strcmp(argv[i], "--map") == 0 && i + 1 < argc) {
            map = argv[++i];
        } else if (strcmp(argv[i], "--self-test") == 0 && i + 1 < argc) {
            self_dir = argv[++i];
        } else if (strcmp(argv[i], "--samples") == 0 && i + 1 < argc) {
            samples = (size_t)strtoul(argv[++i], NULL, 0);
            if (samples == 0) {
                samples = 1;
            }
        } else if (strcmp(argv[i], "--max-full") == 0 && i + 1 < argc) {
            max_full = (size_t)strtoul(argv[++i], NULL, 0);
            if (max_full < 64) {
                max_full = 64;
            }
        } else if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            usage(argv[0]);
            return 0;
        } else {
            fprintf(stderr, "error: bad argument: %s\n", argv[i]);
            usage(argv[0]);
            return 2;
        }
    }

    if (self_dir != NULL) {
        char mini_root[512];
        if (ensure_dir(self_dir) != 0 || write_mini_fixture(self_dir) != 0) {
            fprintf(stderr, "error: cannot create self-test fixture under %s\n",
                    self_dir);
            return 1;
        }
        run_path_reject(self_dir);
        snprintf(mini_root, sizeof(mini_root), "%s/mini", self_dir);
        run_packed_suite(mini_root, NULL, 3, max_full);
    } else if (root != NULL) {
        run_packed_suite(root, map, samples, max_full);
    } else {
        usage(argv[0]);
        return 2;
    }

    printf("summary: %d ok, %d fail\n", g_oks, g_fails);
    return g_fails == 0 ? 0 : 1;
}
