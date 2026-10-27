#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L /* clock_gettime under -std=c11 */
#endif
/* P6.31 auto-save (off by default): policy, the file and the load.
 * Design: docs/P6.31_AUTOSAVE.md.
 *
 * Owner decision 2026-10-26: the auto-save is its own card file in the card
 * folder (autosave.bin, the same 32 KiB card format, one save in slot 0). It
 * never touches save0.bin / save1.bin. The title screen's Continue loads it
 * when it exists.
 *
 * The game side (patches 0140, 0141, 0142) asks dw3_autosave_field_idle at the
 * field's idle frame, hands a snapshot to dw3_autosave_commit when told to, and
 * loads through dw3_autosave_load. With the option OFF the first call returns
 * at once. */
#include "port_autosave.h"
#include "port_options.h"
#include "memcard_host.h"
#include "save_convert.h"

#include <stdio.h>
#include <string.h>
#include <time.h>

#ifdef DW3_AUTOSAVE_TEST
#define AS_INFO(...) (fprintf(stderr, __VA_ARGS__), fputc('\n', stderr))
#define AS_WARN(...) (fprintf(stderr, __VA_ARGS__), fputc('\n', stderr))
#else
#include <psyz/log.h>
#define AS_INFO(...) INFOF(__VA_ARGS__)
#define AS_WARN(...) WARNF(__VA_ARGS__)
#ifdef DW3_CHEATS
#include "port_cheats.h"
#endif
#endif

#define AS_SUMMARY_NAME 0x18
#define AS_FILE_NAME "DW3-AUTOSAVE"

static int g_mode_override = -1;
static int g_last_mode = -1;
static int g_file_ok = -1; /* cached validity of autosave.bin, -1 = unknown */
static unsigned g_count;

const char *dw3_autosave_skip_name(Dw3AutosaveVerdict v) {
    static const char *const names[DW3_AUTOSAVE_SKIP_COUNT] = {
        "go", "option off", "not in a saveable field", "field menu not open yet",
        "battle", "event or menu busy", "transition", "first area (baseline)",
        "same area", "no slot used yet", "card or slot not usable"};

    return (unsigned)v < (unsigned)DW3_AUTOSAVE_SKIP_COUNT ? names[v] : "?";
}

int dw3_autosave_verdict_is_transient(Dw3AutosaveVerdict v) {
    return v == DW3_AUTOSAVE_SKIP_BATTLE || v == DW3_AUTOSAVE_SKIP_BUSY ||
           v == DW3_AUTOSAVE_SKIP_TRANSITION;
}

Dw3AutosaveVerdict dw3_autosave_decide(const Dw3AutosaveCtx *ctx) {
    if (ctx->option != DW3_AUTOSAVE_AREA) {
        return DW3_AUTOSAVE_SKIP_OFF;
    }
    if (!ctx->in_field) {
        return DW3_AUTOSAVE_SKIP_NOT_FIELD;
    }
    if (ctx->battle) {
        return DW3_AUTOSAVE_SKIP_BATTLE;
    }
    if (!ctx->menu_allowed) {
        return DW3_AUTOSAVE_SKIP_NO_MENU;
    }
    if (ctx->busy) {
        return DW3_AUTOSAVE_SKIP_BUSY;
    }
    if (ctx->frozen) {
        return DW3_AUTOSAVE_SKIP_TRANSITION;
    }
    if (ctx->last_mode < 0) {
        return DW3_AUTOSAVE_SKIP_BASELINE;
    }
    if (ctx->mode == ctx->last_mode) {
        return DW3_AUTOSAVE_SKIP_SAME_AREA;
    }
    if (!ctx->has_target) {
        return DW3_AUTOSAVE_SKIP_NO_TARGET;
    }
    return DW3_AUTOSAVE_GO;
}

int dw3_autosave_mode(void) {
    if (g_mode_override >= 0) {
        return g_mode_override;
    }
#ifdef DW3_AUTOSAVE_TEST
    return DW3_AUTOSAVE_OFF;
#else
    return dw3_port_options_level(DW3_PORT_OPT_AUTOSAVE);
#endif
}

void dw3_autosave_test_reset(void) {
    g_mode_override = -1;
    g_last_mode = -1;
    g_file_ok = -1;
    g_count = 0;
}

void dw3_autosave_test_set_mode(int mode) {
    g_mode_override = mode;
}

int dw3_autosave_field_idle(const Dw3AutosaveField *f) {
    Dw3AutosaveCtx ctx;
    Dw3AutosaveVerdict v;

    if (dw3_autosave_mode() != DW3_AUTOSAVE_AREA) {
        return 0; /* OFF: nothing is read, counted or logged */
    }
    ctx.option = DW3_AUTOSAVE_AREA;
    ctx.in_field = dw3_autosave_area_of_mode(f->mode) != 0;
    ctx.menu_allowed = f->progress >= 4;
    ctx.battle = f->battle_starting != 0;
    ctx.busy = f->busy != 0 || f->acting != 0 || f->menu_open != 0;
    ctx.frozen = f->frozen != 0;
    ctx.mode = f->mode;
    ctx.last_mode = g_last_mode;
    ctx.has_target = 1; /* its own file: always a place to write */
    v = dw3_autosave_decide(&ctx);
    if (dw3_autosave_verdict_is_transient(v) ||
        v == DW3_AUTOSAVE_SKIP_SAME_AREA) {
        return 0; /* the area change, if any, is still pending */
    }
    if (v == DW3_AUTOSAVE_SKIP_NOT_FIELD) {
        return 0;
    }
    g_last_mode = f->mode;
    if (v == DW3_AUTOSAVE_GO) {
        AS_INFO("dw3-autosave: area change to mode 0x%x: saving to "
                "autosave.bin", f->mode);
        return 1;
    }
    AS_INFO("dw3-autosave: mode 0x%x: not saved (%s)", f->mode,
            dw3_autosave_skip_name(v));
    return 0;
}

void dw3_autosave_note_loaded(void) {
    g_last_mode = -1; /* loading is not an area change */
}

/* Whether autosave.bin holds a save: a DW3 card (info magic and checksum),
 * slot 0 named, and a data slot with a good checksum and version byte.
 * Returns its SAVE_REGION_*, or 0. */
static int as_check_file(unsigned char *out_slot) {
    static unsigned char slot[SAVE_SLOT_STRIDE];
    unsigned char info[SAVE_INFO_SIZE];
    int region;

    if (memcard_host_ensure(MEMCARD_HOST_AUTO) != 0 ||
        !memcard_host_has_file(MEMCARD_HOST_AUTO) ||
        memcard_host_file_read(MEMCARD_HOST_AUTO, SAVE_INFO_OFFSET, info,
                               SAVE_INFO_SIZE) != SAVE_INFO_SIZE ||
        memcard_host_file_read(MEMCARD_HOST_AUTO, SAVE_DATA_OFFSET, slot,
                               SAVE_SLOT_STRIDE) != (int)SAVE_SLOT_STRIDE) {
        return 0;
    }
    if (info[4] != 'D' || info[5] != 'M' || info[6] != 'W' || info[7] != '3' ||
        info[0] != save_xor(info + 4, SAVE_INFO_SIZE - 4) || info[8] == '\0') {
        return 0;
    }
    region = save_slot_region(slot);
    if (region != SAVE_REGION_NONE && out_slot != NULL) {
        memcpy(out_slot, slot, SAVE_SLOT_STRIDE);
    }
    return region;
}

void dw3_autosave_title_started(void) {
    g_last_mode = -1;
    g_file_ok = as_check_file(NULL) != 0;
    if (g_file_ok) {
        AS_INFO("dw3-autosave: autosave.bin found: Continue loads it");
    }
}

int dw3_autosave_file_present(void) {
    if (g_file_ok < 0) {
        g_file_ok = as_check_file(NULL) != 0;
    }
    return g_file_ok;
}

int dw3_autosave_load(unsigned char *dst, int size, int region) {
    static unsigned char slot[SAVE_SLOT_STRIDE];

    if (dst == NULL || size <= 0 || (unsigned)size > SAVE_SLOT_STRIDE ||
        as_check_file(slot) != region) {
        AS_WARN("dw3-autosave: Continue: autosave.bin cannot be loaded");
        return -1;
    }
    memcpy(dst, slot, (size_t)size);
    AS_INFO("dw3-autosave: Continue loads autosave.bin");
#if defined(DW3_CHEATS) && !defined(DW3_AUTOSAVE_TEST)
    /* P6.20: the cheat mark follows the file (autosave.cheats). */
    dw3_cheats_save_loaded(MEMCARD_HOST_AUTO, 0, dst, size);
#endif
    dw3_autosave_note_loaded();
    return 0;
}

static void as_put32(unsigned char *p, int v) {
    p[0] = (unsigned char)(v & 0xFF);
    p[1] = (unsigned char)((v >> 8) & 0xFF);
    p[2] = (unsigned char)((v >> 16) & 0xFF);
    p[3] = (unsigned char)((v >> 24) & 0xFF);
}

/* The 0x44-byte summary the save list shows (MemCardSave). */
static void as_fill_summary(unsigned char *s, const Dw3AutosaveSnapshot *snap) {
    int i;

    memset(s, 0, SAVE_SUMMARY_SIZE);
    strncpy((char *)s, (const char *)snap->name, AS_SUMMARY_NAME);
    as_put32(s + 0x18, dw3_autosave_area_of_mode(snap->mode));
    as_put32(s + 0x1C, DW3_AUTOSAVE_PLACE);
    as_put32(s + 0x20, snap->money);
    memcpy(s + 0x24, snap->play_time, 12);
    for (i = 0; i < 3; i++) {
        as_put32(s + 0x30 + 4 * i, snap->partners[i]);
        s[0x3C + 2 * i] = (unsigned char)(snap->levels[i] & 0xFF);
        s[0x3C + 2 * i + 1] = (unsigned char)((snap->levels[i] >> 8) & 0xFF);
    }
}

static double as_now_ms(void) {
    struct timespec ts;

    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec * 1000.0 + (double)ts.tv_nsec / 1.0e6;
}

int dw3_autosave_commit(const Dw3AutosaveSnapshot *snap) {
    double t_start = as_now_ms(), t_flush;
    static unsigned char old_slot[SAVE_SLOT_STRIDE];
    unsigned char old_info[SAVE_INFO_SIZE];
    unsigned char new_info[SAVE_INFO_SIZE];
    int had_file;

    if (snap == NULL || snap->save == NULL || snap->save_size <= 0 ||
        (unsigned)snap->save_size > SAVE_SLOT_STRIDE) {
        AS_WARN("dw3-autosave: not saved (bad snapshot)");
        return -1;
    }
    (void)memcard_host_ensure(MEMCARD_HOST_AUTO);
    had_file = memcard_host_has_file(MEMCARD_HOST_AUTO);
    if (had_file) {
        if (memcard_host_file_read(MEMCARD_HOST_AUTO, SAVE_INFO_OFFSET,
                                   old_info, SAVE_INFO_SIZE) !=
                SAVE_INFO_SIZE ||
            memcard_host_file_read(MEMCARD_HOST_AUTO, SAVE_DATA_OFFSET,
                                   old_slot, SAVE_SLOT_STRIDE) !=
                (int)SAVE_SLOT_STRIDE) {
            AS_WARN("dw3-autosave: not saved: autosave.bin cannot be read");
            return -1;
        }
    } else if (memcard_host_create_file(MEMCARD_HOST_AUTO, AS_FILE_NAME) !=
               0) {
        AS_WARN("dw3-autosave: not saved: cannot create autosave.bin");
        return -1;
    }
    /* The file is ours: the info section is built whole (one save in slot 0,
     * the other two summaries empty). A new game overwrites the old one. */
    memset(new_info, 0, SAVE_INFO_SIZE);
    new_info[4] = 'D';
    new_info[5] = 'M';
    new_info[6] = 'W';
    new_info[7] = '3';
    as_fill_summary(new_info + 8, snap);
    new_info[0] = save_xor(new_info + 4, SAVE_INFO_SIZE - 4);
    if (memcard_host_file_write(MEMCARD_HOST_AUTO, SAVE_DATA_OFFSET, snap->save,
                                (size_t)snap->save_size) < 0 ||
        memcard_host_file_write(MEMCARD_HOST_AUTO, SAVE_INFO_OFFSET, new_info,
                                SAVE_INFO_SIZE) < 0) {
        AS_WARN("dw3-autosave: write into RAM failed; autosave.bin unchanged");
        goto rollback;
    }
    t_flush = as_now_ms();
    if (memcard_host_flush(MEMCARD_HOST_AUTO) != 0) {
        AS_WARN("dw3-autosave: FAILED to write autosave.bin (the file on "
                "disk is as it was)");
        goto rollback;
    }
    g_count++;
    g_file_ok = 1;
    AS_INFO("dw3-autosave: wrote autosave.bin (area mode 0x%x, #%u) in %.1f "
            "ms (SD write %.1f ms, the game was stalled that long); option "
            "Auto-save=%d",
            snap->mode, g_count, as_now_ms() - t_start, as_now_ms() - t_flush,
            dw3_autosave_mode());
#if defined(DW3_CHEATS) && !defined(DW3_AUTOSAVE_TEST)
    /* P6.20: an auto-save made after a cheat carries the cheat mark. */
    dw3_cheats_save_written(MEMCARD_HOST_AUTO, 0, SAVE_DATA_OFFSET, snap->save,
                            snap->save_size);
#endif
    return 0;
rollback:
    /* The disk still has the old file (rename-atomic). Put RAM back too. */
    if (had_file) {
        (void)memcard_host_file_write(MEMCARD_HOST_AUTO, SAVE_DATA_OFFSET,
                                      old_slot, SAVE_SLOT_STRIDE);
        (void)memcard_host_file_write(MEMCARD_HOST_AUTO, SAVE_INFO_OFFSET,
                                      old_info, SAVE_INFO_SIZE);
        (void)memcard_host_flush(MEMCARD_HOST_AUTO);
    } else {
        (void)memcard_host_format_slot(MEMCARD_HOST_AUTO);
    }
    return -1;
}
