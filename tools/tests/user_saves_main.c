/* P6.26: user resolution (fake provider), uid formatting, per-user folders and
 * the legacy copy (port/saves/user_saves.c). Usage: user_saves_test <scratch dir>
 * (must exist and be empty). */
#include "port_user_saves.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

static int g_fail;

#define CHECK(cond)                                                          \
    do {                                                                     \
        if (!(cond)) {                                                       \
            fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #cond);          \
            g_fail = 1;                                                      \
        }                                                                    \
    } while (0)

typedef struct {
    int init_rc;
    int pre_rc;
    Dw3Uid pre;
    int list_rc;
    int nusers;
    Dw3Uid users[8];
    int sel_rc;
    Dw3Uid sel;
    int sel_calls;
} Fake;

static int f_init(void *c) { return ((Fake *)c)->init_rc; }
static int f_pre(void *c, Dw3Uid *o) {
    Fake *f = c;
    *o = f->pre;
    return f->pre_rc;
}
static int f_list(void *c, Dw3Uid *o, int max, int *n) {
    Fake *f = c;
    int i;
    for (i = 0; i < f->nusers && i < max; i++) {
        o[i] = f->users[i];
    }
    *n = f->nusers;
    return f->list_rc;
}
static int f_sel(void *c, Dw3Uid *o) {
    Fake *f = c;
    f->sel_calls++;
    *o = f->sel;
    return f->sel_rc;
}

static Dw3UserProvider prov(Fake *f) {
    Dw3UserProvider p = {f, f_init, f_pre, f_list, f_sel};
    return p;
}

static Dw3Uid U(uint64_t a, uint64_t b) {
    Dw3Uid u = {{a, b}};
    return u;
}

static void touch(const char *path, const char *text) {
    FILE *fp = fopen(path, "wb");
    if (fp == NULL) {
        fprintf(stderr, "cannot write %s\n", path);
        g_fail = 1;
        return;
    }
    fputs(text, fp);
    fclose(fp);
}

static int read_all(const char *path, char *buf, size_t n) {
    FILE *fp = fopen(path, "rb");
    size_t r;
    if (fp == NULL) {
        return -1;
    }
    r = fread(buf, 1, n - 1, fp);
    buf[r] = '\0';
    fclose(fp);
    return (int)r;
}

static int exists(const char *path) {
    struct stat st;
    return stat(path, &st) == 0;
}

static void test_resolve(void) {
    Dw3UserResolved r;
    Fake f;
    Dw3UserProvider p;
    char hex[40];

    /* uid formatter: uid[0] then uid[1], 16 hex digits each. */
    dw3_uid_to_hex(&(Dw3Uid){{0x0123456789abcdefull, 0x00000000000000ffull}}, hex);
    CHECK(strcmp(hex, "0123456789abcdef00000000000000ff") == 0);

    /* Account init fails -> shared, the game starts. */
    memset(&f, 0, sizeof(f));
    f.init_rc = 5;
    p = prov(&f);
    CHECK(dw3_switch_user_resolve(&p, &r) == 1);
    CHECK(r.reason == DW3_USER_SHARED_NO_ACCOUNT && strcmp(r.key, "shared") == 0);
    CHECK(r.init_rc == 5 && r.preselect_rc == DW3_USER_RC_UNASKED);

    /* Preselected wins, even with several users, and no selector is shown. */
    memset(&f, 0, sizeof(f));
    f.pre = U(1, 2);
    f.nusers = 3;
    p = prov(&f);
    CHECK(dw3_switch_user_resolve(&p, &r) == 1);
    CHECK(r.reason == DW3_USER_PRESELECTED && f.sel_calls == 0);
    CHECK(strcmp(r.key, "00000000000000010000000000000002") == 0);
    CHECK(strcmp(r.hash, "462054ec") == 0 && r.user_count == -1);

    /* Preselect error, one user: that user, no selector. */
    memset(&f, 0, sizeof(f));
    f.pre_rc = 0x1234;
    f.nusers = 1;
    f.users[0] = U(0xaabbccdd11223344ull, 7);
    p = prov(&f);
    CHECK(dw3_switch_user_resolve(&p, &r) == 1);
    CHECK(r.reason == DW3_USER_SINGLE && f.sel_calls == 0 && r.user_count == 1);
    CHECK(r.preselect_rc == 0x1234);
    CHECK(strcmp(r.hash, "1e5c9d58") == 0);

    /* Preselect "ok" with an all-zero uid does not count. */
    memset(&f, 0, sizeof(f));
    f.nusers = 1;
    f.users[0] = U(9, 9);
    p = prov(&f);
    CHECK(dw3_switch_user_resolve(&p, &r) == 1 && r.reason == DW3_USER_SINGLE);

    /* Several users: the selector decides. */
    memset(&f, 0, sizeof(f));
    f.pre_rc = 1;
    f.nusers = 3;
    f.users[0] = U(1, 1);
    f.users[1] = U(2, 2);
    f.users[2] = U(3, 3);
    f.sel = U(2, 2);
    p = prov(&f);
    CHECK(dw3_switch_user_resolve(&p, &r) == 1);
    CHECK(r.reason == DW3_USER_SELECTOR && f.sel_calls == 1 && r.user_count == 3);
    CHECK(strcmp(r.key, "00000000000000020000000000000002") == 0);

    /* Selector cancelled: the game does not start, no key. */
    f.sel_rc = 1;
    CHECK(dw3_switch_user_resolve(&p, &r) == 0);
    CHECK(r.reason == DW3_USER_CANCELLED && r.key[0] == '\0' && r.selector_rc == 1);

    /* Selector error: same, different reason. */
    f.sel_rc = -7;
    CHECK(dw3_switch_user_resolve(&p, &r) == 0);
    CHECK(r.reason == DW3_USER_ERROR && r.key[0] == '\0');

    /* Selector "succeeds" with an empty uid: treated as an error. */
    f.sel_rc = 0;
    f.sel = U(0, 0);
    CHECK(dw3_switch_user_resolve(&p, &r) == 0 && r.reason == DW3_USER_ERROR);

    /* Listing fails: no start. No users listed: shared. */
    memset(&f, 0, sizeof(f));
    f.pre_rc = 1;
    f.list_rc = 9;
    p = prov(&f);
    CHECK(dw3_switch_user_resolve(&p, &r) == 0 && r.reason == DW3_USER_ERROR);
    f.list_rc = 0;
    CHECK(dw3_switch_user_resolve(&p, &r) == 1 && r.reason == DW3_USER_SHARED_NO_USERS);

    /* PC profile: default, override, sanitising. */
    unsetenv("DW3_SAVE_PROFILE");
    CHECK(dw3_pc_user_resolve(NULL, &r) == 1 && strcmp(r.key, "default") == 0);
    setenv("DW3_SAVE_PROFILE", "Ana b/../x", 1);
    CHECK(dw3_pc_user_resolve(NULL, &r) == 1 && strcmp(r.key, "Ana_b_.._x") == 0);
    CHECK(dw3_pc_user_resolve("..", &r) == 1 && strcmp(r.key, "default") == 0);
    CHECK(dw3_pc_user_resolve("kid", &r) == 1 && strcmp(r.key, "kid") == 0);
    unsetenv("DW3_SAVE_PROFILE");
}

static void test_short_id(void) {
    char id[DW3_USER_HASH_LEN];
    Dw3UserResolved r;
    Fake f;
    Dw3UserProvider p;

    /* Known vectors (FNV-1a 32 over the 16 uid bytes, checked against an
     * independent Python implementation). */
    dw3_uid_short_id(&(Dw3Uid){{1, 2}}, id);
    CHECK(strcmp(id, "462054ec") == 0);
    dw3_uid_short_id(&(Dw3Uid){{0x100000000047359aull, 0xa05a2b7c39f747bfull}}, id);
    CHECK(strcmp(id, "8d8d4548") == 0);

    /* Switch accounts share the high bytes 0x10000000...: the short id must
     * still tell them apart (the old key-prefix id did not). */
    dw3_uid_short_id(&(Dw3Uid){{0x1000000000000000ull, 0x1111}}, id);
    CHECK(strcmp(id, "da46519f") == 0);
    dw3_uid_short_id(&(Dw3Uid){{0x1000000000000000ull, 0x2222}}, id);
    CHECK(strcmp(id, "9d754fcd") == 0);
    CHECK(strlen(id) == 8);

    /* The resolved short id goes with the uid, never the nickname or the key. */
    memset(&f, 0, sizeof(f));
    f.pre = U(0x1000000000000000ull, 0x1111);
    p = prov(&f);
    CHECK(dw3_switch_user_resolve(&p, &r) == 1);
    CHECK(strcmp(r.hash, "da46519f") == 0);
    CHECK(strcmp(r.key, "10000000000000000000000000001111") == 0);
    f.pre = U(0x1000000000000000ull, 0x2222);
    CHECK(dw3_switch_user_resolve(&p, &r) == 1);
    CHECK(strcmp(r.hash, "9d754fcd") == 0);
    CHECK(strcmp(r.hash, "10000000") != 0);
}

static void test_dirs(const char *scratch) {
    char out[512], base[1024], legacy[1100], p[1300], buf[256], tiny[8];
    Dw3UserPrep prep;
    const char *key = "00000000000000010000000000000002";

    CHECK(dw3_user_saves_dir(out, sizeof(out), "sdmc:/switch/dw3", key, "eu") == 0);
    CHECK(strcmp(out, "sdmc:/switch/dw3/saves/00000000000000010000000000000002/eu/memcard") == 0);
    CHECK(dw3_user_saves_dir(tiny, sizeof(tiny), "a", "b", "us") == -1);
    CHECK(dw3_user_saves_base_from_root("sdmc:/switch/dw3/us", out, sizeof(out)) == 0 &&
          strcmp(out, "sdmc:/switch/dw3") == 0);
    CHECK(dw3_user_saves_base_from_root("runtime/us/", out, sizeof(out)) == 0 &&
          strcmp(out, "runtime") == 0);
    CHECK(dw3_user_saves_base_from_root("us", out, sizeof(out)) == -1);

    snprintf(base, sizeof(base), "%s/dw3", scratch);
    mkdir(base, 0777);
    snprintf(legacy, sizeof(legacy), "%s/us", base);
    mkdir(legacy, 0777);
    snprintf(legacy, sizeof(legacy), "%s/us/memcard", base);
    mkdir(legacy, 0777);
    snprintf(p, sizeof(p), "%s/save0.bin", legacy);
    touch(p, "SAVE0");
    snprintf(p, sizeof(p), "%s/autosave.bin", legacy);
    touch(p, "AUTO");
    snprintf(p, sizeof(p), "%s/autosave.cheats", legacy);
    touch(p, "CH");
    snprintf(p, sizeof(p), "%s/unrelated.txt", legacy);
    touch(p, "NO");

    /* First launch of profile A: legacy files are COPIED. */
    CHECK(dw3_user_saves_prepare(base, "AAAA", "us", legacy, "Ana", &prep) == 0);
    CHECK(prep.created == 1 && prep.copied == 3 && prep.err == 0);
    snprintf(p, sizeof(p), "%s/saves/AAAA/us/memcard/save0.bin", base);
    CHECK(read_all(p, buf, sizeof(buf)) == 5 && strcmp(buf, "SAVE0") == 0);
    snprintf(p, sizeof(p), "%s/saves/AAAA/us/memcard/autosave.cheats", base);
    CHECK(exists(p));
    snprintf(p, sizeof(p), "%s/saves/AAAA/us/memcard/unrelated.txt", base);
    CHECK(!exists(p));
    snprintf(p, sizeof(p), "%s/saves/AAAA/us/imported.txt", base);
    CHECK(read_all(p, buf, sizeof(buf)) > 0 && strstr(buf, "files_copied=3") != NULL);
    snprintf(p, sizeof(p), "%s/saves/AAAA/profile.txt", base);
    CHECK(read_all(p, buf, sizeof(buf)) > 0 && strstr(buf, "nickname=Ana") != NULL &&
          strstr(buf, "first_seen=20") != NULL);
    snprintf(p, sizeof(p), "%s/saves/AAAA/us.tmp", base);
    CHECK(!exists(p));
    /* The originals are untouched. */
    snprintf(p, sizeof(p), "%s/save0.bin", legacy);
    CHECK(read_all(p, buf, sizeof(buf)) == 5);

    /* The player changes the save; a second launch must not overwrite it. */
    snprintf(p, sizeof(p), "%s/saves/AAAA/us/memcard/save0.bin", base);
    touch(p, "NEWER");
    CHECK(dw3_user_saves_prepare(base, "AAAA", "us", legacy, "Other", &prep) == 0);
    CHECK(prep.created == 0 && prep.copied == 0);
    CHECK(read_all(p, buf, sizeof(buf)) == 5 && strcmp(buf, "NEWER") == 0);
    snprintf(p, sizeof(p), "%s/saves/AAAA/profile.txt", base);
    CHECK(read_all(p, buf, sizeof(buf)) > 0 && strstr(buf, "nickname=Ana") != NULL);

    /* A second profile gets its own copy; separate from A's later edits. */
    CHECK(dw3_user_saves_prepare(base, "BBBB", "us", legacy, "Bia\nx", &prep) == 0);
    CHECK(prep.created == 1 && prep.copied == 3);
    snprintf(p, sizeof(p), "%s/saves/BBBB/us/memcard/save0.bin", base);
    CHECK(read_all(p, buf, sizeof(buf)) == 5 && strcmp(buf, "SAVE0") == 0);
    snprintf(p, sizeof(p), "%s/saves/BBBB/profile.txt", base);
    CHECK(read_all(p, buf, sizeof(buf)) > 0 && strstr(buf, "nickname=Bia x\n") != NULL);

    /* Region with no legacy folder: an empty region folder, imported.txt says 0. */
    CHECK(dw3_user_saves_prepare(base, "BBBB", "eu", "", NULL, &prep) == 0);
    CHECK(prep.created == 1 && prep.copied == 0);
    snprintf(p, sizeof(p), "%s/saves/BBBB/eu/memcard", base);
    CHECK(exists(p));

    /* Hash for the crash report. */
    CHECK(dw3_user_profile_hash()[0] == '\0');
    dw3_user_saves_set_current_hash("aabbccdd");
    CHECK(strcmp(dw3_user_profile_hash(), "aabbccdd") == 0);
}

int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "usage: user_saves_test <scratch dir>\n");
        return 2;
    }
    test_resolve();
    test_short_id();
    test_dirs(argv[1]);
    if (g_fail) {
        return 1;
    }
    printf("OK: user_saves\n");
    return 0;
}
