/* P6.26 account probe (diagnostic only). See account_probe.h. */
#include "account_probe.h"

#include <psyz.h>
#include <psyz/log.h>

#include <stdio.h>
#include <string.h>

#include <switch.h>

#define PROBE_MAX_USERS ACC_USER_LIST_SIZE

static const char *probe_applet_type_name(AppletType t) {
    switch (t) {
    case AppletType_None: return "None";
    case AppletType_Default: return "Default";
    case AppletType_Application: return "Application";
    case AppletType_SystemApplet: return "SystemApplet";
    case AppletType_LibraryApplet: return "LibraryApplet";
    case AppletType_OverlayApplet: return "OverlayApplet";
    case AppletType_SystemApplication: return "SystemApplication";
    default: return "?";
    }
}

/* "uid=<hi><lo>" as 32 hex digits, or "uid=invalid" for the all-zero uid. */
static void probe_uid_str(AccountUid uid, char *out, size_t n) {
    if (!accountUidIsValid(&uid)) {
        snprintf(out, n, "invalid(%016llx%016llx)",
                 (unsigned long long)uid.uid[0], (unsigned long long)uid.uid[1]);
    } else {
        snprintf(out, n, "%016llx%016llx", (unsigned long long)uid.uid[0],
                 (unsigned long long)uid.uid[1]);
    }
}

/* Nickname of a uid, or an "(rc=0x...)" note when the profile cannot be read. */
static void probe_nick(AccountUid uid, char *out, size_t n) {
    AccountProfile prof;
    AccountProfileBase base;
    Result rc;

    snprintf(out, n, "-");
    rc = accountGetProfile(&prof, uid);
    if (R_FAILED(rc)) {
        snprintf(out, n, "(profile rc=0x%08x)", (unsigned)rc);
        return;
    }
    memset(&base, 0, sizeof(base));
    rc = accountProfileGet(&prof, NULL, &base);
    if (R_SUCCEEDED(rc)) {
        base.nickname[sizeof(base.nickname) - 1] = '\0';
        snprintf(out, n, "%s", base.nickname);
    } else {
        snprintf(out, n, "(profile get rc=0x%08x)", (unsigned)rc);
    }
    accountProfileClose(&prof);
}

/* One line for a call that returns a single uid. */
static void probe_log_uid_result(const char *what, Result rc, AccountUid uid) {
    char id[48], nick[0x40];

    if (R_FAILED(rc)) {
        INFOF("account probe: %s rc=0x%08x", what, (unsigned)rc);
        return;
    }
    probe_uid_str(uid, id, sizeof(id));
    if (accountUidIsValid(&uid)) {
        probe_nick(uid, nick, sizeof(nick));
    } else {
        snprintf(nick, sizeof(nick), "-");
    }
    INFOF("account probe: %s rc=0x%08x uid=%s nickname=\"%s\"", what,
          (unsigned)rc, id, nick);
}

#ifdef DW3_ACCOUNT_PROBE_PSEL
static void probe_psel(void) {
    PselUserSelectionSettings set;
    AccountUid uid;
    Result rc;

    memset(&set, 0, sizeof(set));
    memset(&uid, 0, sizeof(uid));
    INFOF("account probe: psel opening pselShowUserSelector "
          "(DW3_ACCOUNT_PROBE_PSEL; the choice is ignored)");
    rc = pselShowUserSelector(&uid, &set);
    if (R_SUCCEEDED(rc)) {
        probe_log_uid_result("psel chosen", rc, uid);
    } else {
        /* A cancel from the selector shows up as an error code (the applet
         * returns no user); log it as is and let the owner read the context. */
        INFOF("account probe: psel rc=0x%08x (module=%u desc=%u; cancel or "
              "error, no user chosen)",
              (unsigned)rc, (unsigned)R_MODULE(rc), (unsigned)R_DESCRIPTION(rc));
    }
}
#endif

void dw3_nx_account_probe(void) {
    AppletType at = appletGetAppletType();
    AccountUid uid;
    AccountUid uids[PROBE_MAX_USERS];
    s32 count = 0, listed = 0, i;
    Result rc;

    INFOF("account probe: begin");
    INFOF("account probe: appletGetAppletType=%s (%d)",
          probe_applet_type_name(at), (int)at);

    rc = accountInitialize(AccountServiceType_Application);
    INFOF("account probe: accountInitialize(Application) rc=0x%08x",
          (unsigned)rc);
    if (R_FAILED(rc)) {
        INFOF("account probe: end (account service unavailable)");
        return;
    }

    memset(&uid, 0, sizeof(uid));
    rc = accountGetPreselectedUser(&uid);
    probe_log_uid_result("accountGetPreselectedUser", rc, uid);

    rc = accountGetUserCount(&count);
    INFOF("account probe: accountGetUserCount rc=0x%08x count=%d",
          (unsigned)rc, (int)count);

    memset(uids, 0, sizeof(uids));
    rc = accountListAllUsers(uids, PROBE_MAX_USERS, &listed);
    INFOF("account probe: accountListAllUsers rc=0x%08x listed=%d",
          (unsigned)rc, (int)listed);
    if (R_SUCCEEDED(rc)) {
        for (i = 0; i < listed && i < PROBE_MAX_USERS; i++) {
            char id[48], nick[0x40];
            probe_uid_str(uids[i], id, sizeof(id));
            probe_nick(uids[i], nick, sizeof(nick));
            INFOF("account probe:   user[%d] uid=%s nickname=\"%s\"", (int)i,
                  id, nick);
        }
    }

    memset(&uid, 0, sizeof(uid));
    rc = accountGetLastOpenedUser(&uid);
    probe_log_uid_result("accountGetLastOpenedUser", rc, uid);

    memset(&uid, 0, sizeof(uid));
    rc = accountTrySelectUserWithoutInteraction(&uid, false);
    probe_log_uid_result("accountTrySelectUserWithoutInteraction(false)", rc,
                         uid);

#ifdef DW3_ACCOUNT_PROBE_PSEL
    probe_psel();
#endif

    accountExit();
    INFOF("account probe: end");
}
