/* P6.26 per-user saves (slice 1): who is playing, and where their saves live.
 * Design: docs/PER_USER_SAVES.md. Platform independent: the Switch account
 * calls hide behind Dw3UserProvider (platforms/switch/source/user_switch.c),
 * and tests use a fake provider. Nothing here touches libnx. */
#ifndef DW3_USER_SAVES_H
#define DW3_USER_SAVES_H

#include <stddef.h>
#include <stdint.h>

/* 128-bit Switch account uid (same shape as libnx AccountUid). */
typedef struct {
    uint64_t uid[2];
} Dw3Uid;

#define DW3_UID_HEX_LEN 32
#define DW3_USER_KEY_MAX 64 /* folder name: 32 hex, "shared" or a PC profile */
#define DW3_USER_HASH_LEN 9 /* 8 chars of short id + NUL */

typedef enum {
    DW3_USER_PRESELECTED = 0,   /* the system preselected a user (forwarder with selector) */
    DW3_USER_SINGLE,            /* one account on the console */
    DW3_USER_SELECTOR,          /* the player picked one in the system selector */
    DW3_USER_SHARED_NO_ACCOUNT, /* account service failed: the shared profile */
    DW3_USER_SHARED_NO_USERS,   /* the service answered but lists no user */
    DW3_USER_PC_PROFILE,        /* PC: DW3_SAVE_PROFILE or "default" */
    DW3_USER_CANCELLED,         /* selector cancelled: the game does not start */
    DW3_USER_ERROR              /* selector or list failed: the game does not start */
} Dw3UserReason;

/* Account operations. Return 0 on success. show_selector returns 0 (picked),
 * 1 (cancelled by the player) or < 0 (error). Every hook may be NULL: a NULL
 * hook counts as "failed". */
typedef struct {
    void *ctx;
    int (*account_init)(void *ctx);
    int (*preselected)(void *ctx, Dw3Uid *out);
    int (*list_users)(void *ctx, Dw3Uid *out, int max, int *count);
    int (*show_selector)(void *ctx, Dw3Uid *out);
} Dw3UserProvider;

#define DW3_USER_RC_UNASKED (-1000)

typedef struct {
    Dw3UserReason reason;
    int init_rc;        /* account_init result */
    int preselect_rc;   /* preselected result, DW3_USER_RC_UNASKED when not asked */
    int user_count;     /* -1 when not listed */
    int selector_rc;    /* show_selector result, DW3_USER_RC_UNASKED when not shown */
    Dw3Uid uid;         /* valid for PRESELECTED / SINGLE / SELECTOR */
    char key[DW3_USER_KEY_MAX];   /* folder name; "" when the game must not start */
    char hash[DW3_USER_HASH_LEN]; /* short id for logs: dw3_uid_short_id (Switch), key prefix (PC) */
} Dw3UserResolved;

/* Resolve order: init (fail -> shared), preselected, the only user, the
 * selector. Returns 1 when the game may start (out->key set), 0 on cancel or
 * error (out->key empty, nothing must be written). */
int dw3_switch_user_resolve(const Dw3UserProvider *p, Dw3UserResolved *out);

/* PC: key from `profile` (when not NULL/empty), else DW3_SAVE_PROFILE, else
 * "default"; characters outside [A-Za-z0-9_.-] become '_'. Always returns 1. */
int dw3_pc_user_resolve(const char *profile, Dw3UserResolved *out);

const char *dw3_user_reason_name(Dw3UserReason r);

/* 32 lowercase hex digits: uid[0] then uid[1], %016llx each. out >= 33 bytes. */
void dw3_uid_to_hex(const Dw3Uid *u, char *out);

/* Short id for logs and the crash report: FNV-1a 32-bit over the 16 uid bytes
 * in the order of dw3_uid_to_hex, printed as 8 lowercase hex digits. Every
 * Switch account uid starts with the same high bytes, so the folder-name
 * prefix would be the same for everyone; the full uid is hashed instead. The
 * nickname is never part of it. out >= DW3_USER_HASH_LEN bytes. */
void dw3_uid_short_id(const Dw3Uid *u, char *out);

/* ---- folders ---- */

/* "<base>/saves/<key>/<region>/memcard"; 0 on success, -1 when out is short. */
int dw3_user_saves_dir(char *out, size_t n, const char *base, const char *key,
                       const char *region);

/* Parent of a runtime root (".../dw3/us" -> ".../dw3"); -1 if there is none. */
int dw3_user_saves_base_from_root(const char *root, char *out, size_t n);

typedef struct {
    int created;        /* the region folder did not exist and was created */
    int legacy_files;   /* legacy files found */
    int copied;         /* files copied */
    int err;            /* 0 ok, -1 an error (see errmsg) */
    char errmsg[96];
} Dw3UserPrep;

/* Make `<base>/saves/<key>/<region>/memcard` ready, write profile.txt once
 * (nickname may be NULL; first_seen is today's date), and when the region
 * folder is new COPY the files of `legacy_memcard` into it (never move or
 * delete) and write imported.txt. The folder is built under a temporary name
 * and renamed, so a stop half way leaves no half-imported region. Returns 0 on
 * success. */
int dw3_user_saves_prepare(const char *base, const char *key, const char *region,
                           const char *legacy_memcard, const char *nickname,
                           Dw3UserPrep *prep);

/* The profile hash for the crash report ("" until set). */
void dw3_user_saves_set_current_hash(const char *hash);
const char *dw3_user_profile_hash(void);

#endif
