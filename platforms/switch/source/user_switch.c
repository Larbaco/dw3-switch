/* P6.26 slice 1: who is playing on the Switch, and where their saves go.
 * Order (docs/PER_USER_SAVES.md): accountInitialize (fail -> shared),
 * accountGetPreselectedUser, the only user, pselShowUserSelector. A cancel or
 * an error means the game does not start and nothing is written.
 * Hardware behaviour is OLED-unverified: every step logs its facts. */
#include "user_switch.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <psyz/log.h>
#include <switch.h>

#include "port_user_saves.h"

static int g_account_open;
static char g_memdir[512];
static int g_memdir_set;
static char g_nickname[0x21];

static const char *applet_type_name(AppletType t) {
    switch (t) {
    case AppletType_Application: return "Application";
    case AppletType_SystemApplet: return "SystemApplet";
    case AppletType_LibraryApplet: return "LibraryApplet";
    case AppletType_OverlayApplet: return "OverlayApplet";
    case AppletType_SystemApplication: return "SystemApplication";
    default: return "other";
    }
}

static void uid_from_nx(Dw3Uid *o, const AccountUid *u) {
    o->uid[0] = u->uid[0];
    o->uid[1] = u->uid[1];
}

static int p_init(void *ctx) {
    Result rc;
    (void)ctx;
    rc = accountInitialize(AccountServiceType_Application);
    INFOF("user: accountInitialize rc=0x%x", (unsigned)rc);
    if (R_FAILED(rc)) {
        return (int)(rc ? rc : 1);
    }
    g_account_open = 1;
    return 0;
}

static int p_pre(void *ctx, Dw3Uid *out) {
    AccountUid u;
    Result rc;
    (void)ctx;
    memset(&u, 0, sizeof(u));
    rc = accountGetPreselectedUser(&u);
    INFOF("user: accountGetPreselectedUser rc=0x%x valid=%d", (unsigned)rc,
          R_SUCCEEDED(rc) && accountUidIsValid(&u));
    if (R_FAILED(rc)) {
        return (int)(rc ? rc : 1);
    }
    uid_from_nx(out, &u);
    return 0;
}

static int p_list(void *ctx, Dw3Uid *out, int max, int *count) {
    AccountUid uids[ACC_USER_LIST_SIZE];
    s32 total = 0;
    Result rc;
    int i;
    (void)ctx;
    memset(uids, 0, sizeof(uids));
    rc = accountListAllUsers(uids, ACC_USER_LIST_SIZE, &total);
    INFOF("user: accountListAllUsers rc=0x%x count=%d", (unsigned)rc, (int)total);
    if (R_FAILED(rc)) {
        return (int)(rc ? rc : 1);
    }
    if (total > ACC_USER_LIST_SIZE) {
        total = ACC_USER_LIST_SIZE;
    }
    for (i = 0; i < total && i < max; i++) {
        uid_from_nx(&out[i], &uids[i]);
    }
    *count = (int)total;
    return 0;
}

static int p_select(void *ctx, Dw3Uid *out) {
    AccountUid u;
    PselUserSelectionSettings st;
    Result rc;
    (void)ctx;
    memset(&u, 0, sizeof(u));
    memset(&st, 0, sizeof(st));
    INFOF("user: showing the system profile selector (pselShowUserSelector)");
    rc = pselShowUserSelector(&u, &st);
    INFOF("user: pselShowUserSelector rc=0x%x (module %u desc %u) valid=%d",
          (unsigned)rc, (unsigned)R_MODULE(rc), (unsigned)R_DESCRIPTION(rc),
          R_SUCCEEDED(rc) && accountUidIsValid(&u));
    if (R_SUCCEEDED(rc)) {
        uid_from_nx(out, &u);
        return 0;
    }
    /* libnx reports a cancel as a failed Result; whether it has a distinct
     * code is OLED-unverified. Cancel and error are handled the same way
     * (the game does not start), so only the log tells them apart. */
    return -1;
}

static void read_nickname(const Dw3Uid *u) {
    AccountUid nu;
    AccountProfile prof;
    AccountProfileBase base;

    g_nickname[0] = '\0';
    nu.uid[0] = u->uid[0];
    nu.uid[1] = u->uid[1];
    memset(&base, 0, sizeof(base));
    if (R_FAILED(accountGetProfile(&prof, nu))) {
        return;
    }
    if (R_SUCCEEDED(accountProfileGet(&prof, NULL, &base))) {
        memcpy(g_nickname, base.nickname, sizeof(base.nickname));
        g_nickname[sizeof(g_nickname) - 1] = '\0';
    }
    accountProfileClose(&prof);
}

static void log_prep(const char *tag, const Dw3UserPrep *prep, const char *legacy) {
    if (prep->err != 0) {
        ERRORF("user: %s migration/prepare FAILED: %s", tag, prep->errmsg);
    } else if (prep->created) {
        INFOF("user: %s migration: new region folder, copied %d legacy file(s) from %s",
              tag, prep->copied, legacy);
    } else {
        INFOF("user: %s region folder already exists: no migration", tag);
    }
}

int dw3_switch_user_boot(const char *root, const char *region) {
    Dw3UserProvider prov = {NULL, p_init, p_pre, p_list, p_select};
    Dw3UserResolved r;
    char base[512], legacy[600], other_legacy[600], dir[640], other_dir[640];
    const char *other = (strcmp(region, "eu") == 0) ? "us" : "eu";
    Dw3UserPrep prep;
    AppletType at;

    g_memdir_set = 0;
    at = appletGetAppletType();
    INFOF("user: applet type=%s (%d)", applet_type_name(at), (int)at);

#ifdef DW3_NX_REPLAY
    INFOF("user: skipped (replay build)");
    return DW3_USER_BOOT_SKIPPED;
#else
    if (getenv("DW3_MEMCARD_DIR") != NULL && getenv("DW3_MEMCARD_DIR")[0] != '\0') {
        INFOF("user: skipped (DW3_MEMCARD_DIR forces the save folder)");
        return DW3_USER_BOOT_SKIPPED;
    }
    if (dw3_user_saves_base_from_root(root, base, sizeof(base)) != 0) {
        ERRORF("user: no base folder above runtime root %s; using the legacy folder", root);
        return DW3_USER_BOOT_SKIPPED;
    }

    if (!dw3_switch_user_resolve(&prov, &r)) {
        INFOF("user: result=%s init_rc=0x%x preselect_rc=%d users=%d selector_rc=%d: "
              "the game does not start; nothing written",
              dw3_user_reason_name(r.reason), (unsigned)r.init_rc, r.preselect_rc,
              r.user_count, r.selector_rc);
        dw3_switch_user_exit();
        return DW3_USER_BOOT_STOP;
    }
    dw3_user_saves_set_current_hash(r.hash);
    INFOF("user: profile=%s reason=%s init_rc=0x%x preselect_rc=%d users=%d selector_rc=%d",
          r.hash, dw3_user_reason_name(r.reason), (unsigned)r.init_rc,
          r.preselect_rc, r.user_count, r.selector_rc);

    g_nickname[0] = '\0';
    if (r.reason != DW3_USER_SHARED_NO_ACCOUNT && r.reason != DW3_USER_SHARED_NO_USERS) {
        read_nickname(&r.uid);
    }

    snprintf(legacy, sizeof(legacy), "%s/memcard", root);
    snprintf(other_legacy, sizeof(other_legacy), "%s/%s/memcard", base, other);
    dw3_user_saves_prepare(base, r.key, region, legacy, g_nickname, &prep);
    log_prep(region, &prep, legacy);
    if (prep.err != 0) {
        /* Better to play on the shared legacy folder than to lose the session;
         * it is the pre-P6.26 behaviour. */
        ERRORF("user: falling back to the legacy save folder %s", legacy);
        dw3_switch_user_exit();
        return DW3_USER_BOOT_SKIPPED;
    }
    /* The other region: keeps the P6.25 cross-region import working from this
     * user's own folder (never from the legacy one, which is nobody's). */
    {
        Dw3UserPrep prep2;
        dw3_user_saves_prepare(base, r.key, other, other_legacy, g_nickname, &prep2);
        log_prep(other, &prep2, other_legacy);
    }
    if (dw3_user_saves_dir(dir, sizeof(dir), base, r.key, region) != 0 ||
        dw3_user_saves_dir(other_dir, sizeof(other_dir), base, r.key, other) != 0 ||
        strlen(dir) >= sizeof(g_memdir)) {
        ERRORF("user: save path too long; using the legacy folder");
        dw3_switch_user_exit();
        return DW3_USER_BOOT_SKIPPED;
    }
    memcpy(g_memdir, dir, strlen(dir) + 1);
    g_memdir_set = 1;
    /* memcard_host_setup_cross_import reads this when the user did not set it. */
    setenv("DW3_MEMCARD_IMPORT_DIR", other_dir, 0);
    INFOF("user: save dir=%s", g_memdir);
    return DW3_USER_BOOT_START;
#endif
}

const char *dw3_switch_user_memdir(void) { return g_memdir_set ? g_memdir : NULL; }

void dw3_switch_user_exit(void) {
    if (g_account_open) {
        accountExit();
        g_account_open = 0;
    }
}
