/* Host test for the text pack loader (port/l10n/l10n_core.c) on a synthetic runtime built by
 * tools/tests/test_l10n_loader.py. Usage: l10n_test <runtime/eu> <scenario>. Prints
 * "LOG ..." lines (the loader's log) and "CHECK name ok|FAIL"; exits 1 on any FAIL. */
#include "cdfs.h"
#include "l10n_core.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define N 16

static int g_sectors[N];
static unsigned short g_counts[N];
static int g_fail;

static void log_line(const char *line) {
    printf("LOG %s\n", line);
}

static void check(const char *name, int ok) {
    printf("CHECK %s %s\n", name, ok ? "ok" : "FAIL");
    if (!ok) {
        g_fail = 1;
    }
}

static long file_size(const char *path) {
    FILE *f = fopen(path, "rb");
    long n;

    if (!f) {
        return -1;
    }
    fseek(f, 0, SEEK_END);
    n = ftell(f);
    fclose(f);
    return n;
}

static int same_file(uint32_t id, const char *path) {
    long n = file_size(path);
    unsigned char *a, *b;
    FILE *f;
    int ok;

    if (n <= 0 || (long)cdfs_file_size(id) != n) {
        return 0;
    }
    a = malloc((size_t)n);
    b = malloc((size_t)n);
    f = fopen(path, "rb");
    ok = f && fread(a, 1, (size_t)n, f) == (size_t)n && cdfs_read(id, b, (size_t)n) == (int)n && !memcmp(a, b, (size_t)n);
    if (f) {
        fclose(f);
    }
    free(a);
    free(b);
    return ok;
}

int main(int argc, char **argv) {
    Dw3L10n l;
    char path[512];
    const char *root, *scen;
    int snap_s[N];
    unsigned short snap_c[N];
    uint32_t en_size1, en_size2;
    int i, n;

    if (argc < 3) {
        return 2;
    }
    root = argv[1];
    scen = argv[2];
    for (i = 0; i < N; i++) {
        g_sectors[i] = 1000 + i * 10;
        g_counts[i] = (unsigned short)(1 + i % 3);
    }
    memcpy(snap_s, g_sectors, sizeof(snap_s));
    memcpy(snap_c, g_counts, sizeof(snap_c));
    dw3_l10n_set_log(log_line);
    if (cdfs_set_root(root) != 0 || cdfs_load_map(NULL) != 0) {
        printf("cannot load the runtime map under %s\n", root);
        return 2;
    }
    en_size1 = cdfs_file_size(1);
    en_size2 = cdfs_file_size(2);
    dw3_l10n_core_bind(&l, g_sectors, g_counts, strcmp(scen, "badid") == 0 ? 2 : N, "ptbr", "eu");

    if (strncmp(scen, "keys", 4) == 0) {
        dw3_l10n_core_set_keys(&l, 1);
    }
    n = dw3_l10n_core_load(&l, root);
    if (strcmp(scen, "keys") == 0 || strcmp(scen, "keysmissing") == 0) {
        check("load", n == 1 && l.pack_ok);
        check("keys_ok", l.keys_ok == (strcmp(scen, "keys") == 0));
        check("apply", dw3_l10n_core_apply(&l) == 2 && l.applied);
        check("keys_on", l.keys_on == l.keys_ok);
        snprintf(path, sizeof(path), "%s/l10n/ptbr/1.%sbin", root, l.keys_on ? "k." : "");
        check("reads_variant", same_file(1, path));
        check("counts_follow_variant", g_counts[1] == (l.keys_on ? l.pack.files[0].keys_sectors : l.pack.files[0].sectors));
        check("restore", dw3_l10n_core_restore(&l) == 2 && cdfs_file_size(1) == en_size1);
        return g_fail;
    }
    if (strcmp(scen, "ok") == 0 || strcmp(scen, "twice") == 0 || strcmp(scen, "bigger") == 0) {
        check("load", n == 1 && l.pack_ok && l.reason[0] == 0);
        check("has_id", dw3_l10n_core_has_id(&l, 1) && dw3_l10n_core_has_id(&l, 2) && !dw3_l10n_core_has_id(&l, 3));
        check("untouched_before_apply", !memcmp(snap_s, g_sectors, sizeof(snap_s)) && cdfs_file_size(1) == en_size1);
        check("apply", dw3_l10n_core_apply(&l) == 2 && l.applied);
        check("counts", g_counts[1] == l.pack.files[0].sectors && g_counts[2] == l.pack.files[1].sectors);
        if (strcmp(scen, "bigger") == 0) {
            check("grew", g_counts[1] == 2);
        }
        check("virtual_lba", g_sectors[1] >= DW3_L10N_VLBA_BASE && g_sectors[2] == g_sectors[1] + (int)l.pack.files[0].sectors);
        check("others_untouched", g_sectors[0] == snap_s[0] && g_sectors[3] == snap_s[3] && g_counts[3] == snap_c[3]);
        snprintf(path, sizeof(path), "%s/l10n/ptbr/1.bin", root);
        check("reads_pack_1", same_file(1, path));
        snprintf(path, sizeof(path), "%s/l10n/ptbr/2.bin", root);
        check("reads_pack_2", same_file(2, path));
        check("override_flag", cdfs_override_active(1) && cdfs_override_active(2) && !cdfs_override_active(3));
        if (strcmp(scen, "twice") == 0) {
            check("apply_again", dw3_l10n_core_apply(&l) == 2);
        }
        check("restore", dw3_l10n_core_restore(&l) == 2 && !l.applied);
        check("restore_again", dw3_l10n_core_restore(&l) == 0);
        check("arrays_back", !memcmp(snap_s, g_sectors, sizeof(snap_s)) && !memcmp(snap_c, g_counts, sizeof(snap_c)));
        check("english_back", cdfs_file_size(1) == en_size1 && cdfs_file_size(2) == en_size2 &&
                                  !cdfs_override_active(1) && !cdfs_override_active(2));
        check("reapply", dw3_l10n_core_apply(&l) == 2 && cdfs_override_active(1));
        check("restore_last", dw3_l10n_core_restore(&l) == 2 && cdfs_file_size(1) == en_size1);
    } else {
        check("not_ok", n == 0 && !l.pack_ok);
        printf("REASON %s\n", l.reason);
        check("apply_refused", dw3_l10n_core_apply(&l) == -1 && !l.applied);
        check("arrays_untouched", !memcmp(snap_s, g_sectors, sizeof(snap_s)) && !memcmp(snap_c, g_counts, sizeof(snap_c)));
        check("english_read", cdfs_file_size(1) == en_size1 && !cdfs_override_active(1));
        check("restore_noop", dw3_l10n_core_restore(&l) == 0);
    }
    return g_fail;
}
