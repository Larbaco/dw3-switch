/* P6.26 per-user saves (slice 1). See user_saves.h and docs/PER_USER_SAVES.md. */
#include "port_user_saves.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>

static char g_current_hash[DW3_USER_HASH_LEN];

const char *dw3_user_reason_name(Dw3UserReason r) {
    switch (r) {
    case DW3_USER_PRESELECTED: return "preselected";
    case DW3_USER_SINGLE: return "single-user";
    case DW3_USER_SELECTOR: return "selector";
    case DW3_USER_SHARED_NO_ACCOUNT: return "shared(account-init-failed)";
    case DW3_USER_SHARED_NO_USERS: return "shared(no-users)";
    case DW3_USER_PC_PROFILE: return "pc-profile";
    case DW3_USER_CANCELLED: return "cancelled";
    case DW3_USER_ERROR: return "error";
    }
    return "?";
}

void dw3_uid_to_hex(const Dw3Uid *u, char *out) {
    snprintf(out, DW3_UID_HEX_LEN + 1, "%016llx%016llx",
             (unsigned long long)u->uid[0], (unsigned long long)u->uid[1]);
}

void dw3_uid_short_id(const Dw3Uid *u, char *out) {
    static const uint32_t k_fnv_offset = 0x811c9dc5u;
    static const uint32_t k_fnv_prime = 0x01000193u;
    uint32_t h = k_fnv_offset;
    int word;
    int shift;

    for (word = 0; word < 2; word++) {
        for (shift = 56; shift >= 0; shift -= 8) {
            h ^= (uint32_t)((u->uid[word] >> shift) & 0xffu);
            h *= k_fnv_prime;
        }
    }
    snprintf(out, DW3_USER_HASH_LEN, "%08x", (unsigned)h);
}

static int uid_valid(const Dw3Uid *u) { return u->uid[0] != 0 || u->uid[1] != 0; }

static void set_key(Dw3UserResolved *o, const char *key) {
    size_t n;

    snprintf(o->key, sizeof(o->key), "%s", key);
    n = strlen(o->key);
    if (n > DW3_USER_HASH_LEN - 1) {
        n = DW3_USER_HASH_LEN - 1;
    }
    memcpy(o->hash, o->key, n);
    o->hash[n] = '\0';
}

static void set_uid(Dw3UserResolved *o, const Dw3Uid *u, Dw3UserReason why) {
    char hex[DW3_UID_HEX_LEN + 1];

    o->uid = *u;
    dw3_uid_to_hex(u, hex);
    set_key(o, hex);
    dw3_uid_short_id(u, o->hash); /* replaces the key prefix set by set_key */
    o->reason = why;
}

static void resolved_init(Dw3UserResolved *o) {
    memset(o, 0, sizeof(*o));
    o->preselect_rc = DW3_USER_RC_UNASKED;
    o->selector_rc = DW3_USER_RC_UNASKED;
    o->user_count = -1;
}

int dw3_switch_user_resolve(const Dw3UserProvider *p, Dw3UserResolved *o) {
    Dw3Uid uids[8];
    Dw3Uid one;
    int n = 0;
    int rc;

    resolved_init(o);
    o->init_rc = (p->account_init != NULL) ? p->account_init(p->ctx) : -1;
    if (o->init_rc != 0) {
        set_key(o, "shared");
        o->reason = DW3_USER_SHARED_NO_ACCOUNT;
        return 1;
    }
    if (p->preselected != NULL) {
        memset(&one, 0, sizeof(one));
        o->preselect_rc = p->preselected(p->ctx, &one);
        if (o->preselect_rc == 0 && uid_valid(&one)) {
            set_uid(o, &one, DW3_USER_PRESELECTED);
            return 1;
        }
    } else {
        o->preselect_rc = -1;
    }
    memset(uids, 0, sizeof(uids));
    if (p->list_users == NULL ||
        p->list_users(p->ctx, uids, (int)(sizeof(uids) / sizeof(uids[0])), &n) != 0) {
        o->reason = DW3_USER_ERROR;
        return 0;
    }
    o->user_count = n;
    if (n == 1 && uid_valid(&uids[0])) {
        set_uid(o, &uids[0], DW3_USER_SINGLE);
        return 1;
    }
    if (n <= 0) {
        set_key(o, "shared");
        o->reason = DW3_USER_SHARED_NO_USERS;
        return 1;
    }
    memset(&one, 0, sizeof(one));
    rc = (p->show_selector != NULL) ? p->show_selector(p->ctx, &one) : -1;
    o->selector_rc = rc;
    if (rc == 0 && uid_valid(&one)) {
        set_uid(o, &one, DW3_USER_SELECTOR);
        return 1;
    }
    o->reason = (rc == 1) ? DW3_USER_CANCELLED : DW3_USER_ERROR;
    return 0;
}

int dw3_pc_user_resolve(const char *profile, Dw3UserResolved *o) {
    char key[DW3_USER_KEY_MAX];
    const char *src = profile;
    size_t i;

    resolved_init(o);
    if (src == NULL || src[0] == '\0') {
        src = getenv("DW3_SAVE_PROFILE");
    }
    if (src == NULL || src[0] == '\0') {
        src = "default";
    }
    for (i = 0; src[i] != '\0' && i + 1 < sizeof(key); i++) {
        char c = src[i];
        int ok = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
                 (c >= '0' && c <= '9') || c == '_' || c == '-' || c == '.';
        key[i] = ok ? c : '_';
    }
    key[i] = '\0';
    if (strcmp(key, ".") == 0 || strcmp(key, "..") == 0) {
        strcpy(key, "default");
    }
    set_key(o, key);
    o->reason = DW3_USER_PC_PROFILE;
    return 1;
}

int dw3_user_saves_dir(char *out, size_t n, const char *base, const char *key,
                       const char *region) {
    int w = snprintf(out, n, "%s/saves/%s/%s/memcard", base, key, region);

    return (w < 0 || (size_t)w >= n) ? -1 : 0;
}

int dw3_user_saves_base_from_root(const char *root, char *out, size_t n) {
    size_t len = strlen(root);
    size_t i;

    while (len > 1 && (root[len - 1] == '/' || root[len - 1] == '\\')) {
        len--;
    }
    for (i = len; i > 0; i--) {
        if (root[i - 1] == '/' || root[i - 1] == '\\') {
            break;
        }
    }
    if (i <= 1 || i - 1 >= n) {
        return -1;
    }
    memcpy(out, root, i - 1);
    out[i - 1] = '\0';
    return 0;
}

static int is_dir(const char *path) {
    struct stat st;

    return stat(path, &st) == 0 && S_ISDIR(st.st_mode);
}

/* mkdir -p. Prefixes that cannot be created ("sdmc:") are skipped; only the
 * last component decides success. */
static int mkdir_p(const char *path) {
    char buf[512];
    size_t i;

    if (strlen(path) >= sizeof(buf)) {
        return -1;
    }
    strcpy(buf, path);
    for (i = 1; buf[i] != '\0'; i++) {
        if (buf[i] == '/') {
            buf[i] = '\0';
            if (!is_dir(buf)) {
                (void)mkdir(buf, 0777);
            }
            buf[i] = '/';
        }
    }
    if (mkdir(buf, 0777) != 0 && errno != EEXIST && !is_dir(buf)) {
        return -1;
    }
    return 0;
}

/* 0 copied, 1 source absent, -1 error. */
static int copy_file(const char *from, const char *to) {
    FILE *in = fopen(from, "rb");
    FILE *out;
    char buf[4096];
    size_t n;
    int rc = 0;

    if (in == NULL) {
        return 1;
    }
    out = fopen(to, "wb");
    if (out == NULL) {
        fclose(in);
        return -1;
    }
    while ((n = fread(buf, 1, sizeof(buf), in)) > 0) {
        if (fwrite(buf, 1, n, out) != n) {
            rc = -1;
            break;
        }
    }
    if (ferror(in)) {
        rc = -1;
    }
    fclose(in);
    if (fclose(out) != 0) {
        rc = -1;
    }
    return rc;
}

static void fail(Dw3UserPrep *prep, const char *msg, const char *path) {
    prep->err = -1;
    snprintf(prep->errmsg, sizeof(prep->errmsg), "%.30s: %.60s", msg, path);
}

static void write_profile_txt(const char *userdir, const char *nickname) {
    char path[1100];
    char nick[0x21 * 2];
    FILE *fp;
    time_t t = time(NULL);
    struct tm tmv;
    size_t i, j = 0;

    snprintf(path, sizeof(path), "%s/profile.txt", userdir);
    fp = fopen(path, "rb");
    if (fp != NULL) {
        fclose(fp);
        return;
    }
    for (i = 0; nickname != NULL && nickname[i] != '\0' && j + 1 < sizeof(nick); i++) {
        nick[j++] = (nickname[i] == '\n' || nickname[i] == '\r') ? ' ' : nickname[i];
    }
    nick[j] = '\0';
    fp = fopen(path, "wb");
    if (fp == NULL) {
        return; /* informational only */
    }
    if (gmtime_r(&t, &tmv) != NULL) {
        fprintf(fp, "nickname=%s\nfirst_seen=%04d-%02d-%02d\n", nick,
                tmv.tm_year + 1900, tmv.tm_mon + 1, tmv.tm_mday);
    } else {
        fprintf(fp, "nickname=%s\nfirst_seen=unknown\n", nick);
    }
    fclose(fp);
}

static const char *const k_legacy_files[] = {
    "save0.bin", "save1.bin", "autosave.bin",
    "save0.cheats", "save1.cheats", "autosave.cheats",
};

int dw3_user_saves_prepare(const char *base, const char *key, const char *region,
                           const char *legacy_memcard, const char *nickname,
                           Dw3UserPrep *prep) {
    char userdir[1024], regdir[1100], tmpdir[1100], tmpmem[1200];
    char from[1300], to[1300];
    size_t i;
    int w;
    FILE *fp;

    memset(prep, 0, sizeof(*prep));
    w = snprintf(userdir, sizeof(userdir), "%s/saves/%s", base, key);
    if (w < 0 || (size_t)w >= sizeof(userdir)) {
        fail(prep, "path too long", key);
        return -1;
    }
    snprintf(regdir, sizeof(regdir), "%s/%s", userdir, region);
    snprintf(tmpdir, sizeof(tmpdir), "%s/%s.tmp", userdir, region);
    snprintf(tmpmem, sizeof(tmpmem), "%s/memcard", tmpdir);

    if (mkdir_p(userdir) != 0) {
        fail(prep, "cannot create", userdir);
        return -1;
    }
    write_profile_txt(userdir, nickname);
    if (is_dir(regdir)) {
        return 0; /* known profile and region: nothing to import */
    }

    if (mkdir_p(tmpmem) != 0) {
        fail(prep, "cannot create", tmpmem);
        return -1;
    }
    if (legacy_memcard != NULL && legacy_memcard[0] != '\0') {
        for (i = 0; i < sizeof(k_legacy_files) / sizeof(k_legacy_files[0]); i++) {
            int rc;

            snprintf(from, sizeof(from), "%s/%s", legacy_memcard, k_legacy_files[i]);
            snprintf(to, sizeof(to), "%s/%s", tmpmem, k_legacy_files[i]);
            rc = copy_file(from, to);
            if (rc < 0) {
                fail(prep, "copy failed", from);
                return -1;
            }
            if (rc == 0) {
                prep->copied++;
                prep->legacy_files++;
            }
        }
    }
    snprintf(to, sizeof(to), "%s/imported.txt", tmpdir);
    fp = fopen(to, "wb");
    if (fp != NULL) {
        fprintf(fp, "source=%s\nfiles_copied=%d\nmode=copy (originals untouched)\n",
                legacy_memcard != NULL ? legacy_memcard : "(none)", prep->copied);
        fclose(fp);
    }
    if (rename(tmpdir, regdir) != 0) {
        fail(prep, "rename failed", regdir);
        return -1;
    }
    prep->created = 1;
    return 0;
}

void dw3_user_saves_set_current_hash(const char *hash) {
    snprintf(g_current_hash, sizeof(g_current_hash), "%s", hash != NULL ? hash : "");
}

const char *dw3_user_profile_hash(void) { return g_current_hash; }
