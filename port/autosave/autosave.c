/* P6.31 auto-save (off by default): policy, target and the write.
 * Design and the defaults chosen without the owner: docs/P6.31_AUTOSAVE.md.
 *
 * The game's card file has room for exactly three saves (header, icons, info
 * section and three data copies fill 0x7B00 of the 0x8000 bytes), so there is
 * no free slot for the auto-save. Default chosen, owner can change: the
 * auto-save goes to the slot the player last saved to or loaded from, and the
 * player's version of that slot is first copied aside (saveN.bin.pre-auto),
 * once, before the first auto-save writes over it.
 *
 * The game side (patches 0140, 0141) only asks dw3_autosave_field_idle at the
 * field's idle frame, and hands a snapshot to dw3_autosave_commit when it was
 * told to. With the option OFF the first call returns at once. */
#include "port_autosave.h"
#include "port_options.h"
#include "memcard_host.h"
#include "save_convert.h"

#include <stdio.h>
#include <string.h>

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

#define AS_PLAYTIME_HOURS 4
#define AS_PLAYTIME_MINUTES 6
#define AS_PLAYTIME_SECONDS 8
#define AS_SUMMARY_NAME 0x18

static int g_mode_override = -1;
static int g_port = -1;
static int g_slot = -1;
static int g_last_mode = -1;
/* 1 while the target slot holds what the player saved or loaded, 0 once an
 * auto-save wrote it (a later manual save or load sets it again). */
static int g_target_manual;
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

int dw3_autosave_target_port(void) {
    return g_port;
}

int dw3_autosave_target_slot(void) {
    return g_slot;
}

void dw3_autosave_test_reset(void) {
    g_mode_override = -1;
    g_port = -1;
    g_slot = -1;
    g_last_mode = -1;
    g_target_manual = 0;
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
    ctx.has_target = g_port >= 0 && g_slot >= 0;
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
        AS_INFO("dw3-autosave: area change to mode 0x%x: saving to card %d "
                "slot %d", f->mode, g_port, g_slot);
        return 1;
    }
    AS_INFO("dw3-autosave: mode 0x%x: not saved (%s)", f->mode,
            dw3_autosave_skip_name(v));
    return 0;
}

void dw3_autosave_note_loaded(int port, int slot) {
    g_port = port;
    g_slot = slot;
    g_last_mode = -1;
    g_target_manual = 1;
    if (dw3_autosave_mode() == DW3_AUTOSAVE_AREA) {
        AS_INFO("dw3-autosave: target is card %d slot %d (loaded)", port,
                slot);
    }
}

void dw3_autosave_note_saved(int port, int slot) {
    g_port = port;
    g_slot = slot;
    g_target_manual = 1;
    if (dw3_autosave_mode() == DW3_AUTOSAVE_AREA) {
        AS_INFO("dw3-autosave: target is card %d slot %d (saved)", port,
                slot);
    }
}

void dw3_autosave_title_started(void) {
    g_port = -1;
    g_slot = -1;
    g_last_mode = -1;
    g_target_manual = 0;
}

static int as_u16(const unsigned char *p) {
    return (short)(p[0] | (p[1] << 8));
}

static void as_put32(unsigned char *p, int v) {
    p[0] = (unsigned char)(v & 0xFF);
    p[1] = (unsigned char)((v >> 8) & 0xFF);
    p[2] = (unsigned char)((v >> 16) & 0xFF);
    p[3] = (unsigned char)((v >> 24) & 0xFF);
}

static long as_seconds(const unsigned char *time12) {
    return (long)as_u16(time12 + AS_PLAYTIME_HOURS) * 3600L +
           (long)as_u16(time12 + AS_PLAYTIME_MINUTES) * 60L +
           (long)as_u16(time12 + AS_PLAYTIME_SECONDS);
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

int dw3_autosave_commit(const Dw3AutosaveSnapshot *snap) {
    static unsigned char old_slot[SAVE_SLOT_STRIDE];
    unsigned char old_info[SAVE_INFO_SIZE];
    unsigned char new_info[SAVE_INFO_SIZE];
    unsigned char *summary;
    unsigned offset;
    int port = g_port;
    int slot = g_slot;
    int rc;

    if (snap == NULL || snap->save == NULL || snap->save_size <= 0 ||
        (unsigned)snap->save_size > SAVE_SLOT_STRIDE || port < 0 ||
        port >= MEMCARD_HOST_SLOTS || slot < 0 || slot >= SAVE_SLOT_COUNT) {
        AS_WARN("dw3-autosave: not saved (bad snapshot or no target)");
        return -1;
    }
    if (!memcard_host_has_file(port)) {
        AS_WARN("dw3-autosave: not saved: card %d has no save file", port);
        return -1;
    }
    offset = SAVE_DATA_OFFSET + (unsigned)slot * SAVE_SLOT_STRIDE;
    if (memcard_host_file_read(port, SAVE_INFO_OFFSET, old_info,
                               SAVE_INFO_SIZE) != SAVE_INFO_SIZE ||
        memcard_host_file_read(port, offset, old_slot, SAVE_SLOT_STRIDE) !=
            (int)SAVE_SLOT_STRIDE) {
        AS_WARN("dw3-autosave: not saved: card %d cannot be read", port);
        return -1;
    }
    /* Only a card the game wrote, and only the slot of this very game. */
    if (old_info[4] != 'D' || old_info[5] != 'M' || old_info[6] != 'W' ||
        old_info[7] != '3' ||
        old_info[0] != save_xor(old_info + 4, SAVE_INFO_SIZE - 4)) {
        AS_WARN("dw3-autosave: not saved: card %d info section is not a "
                "DW3 card", port);
        return -1;
    }
    summary = old_info + 8 + slot * SAVE_SUMMARY_SIZE;
    if (summary[0] == '\0' ||
        strncmp((const char *)summary, (const char *)snap->name,
                AS_SUMMARY_NAME) != 0 ||
        as_seconds(summary + 0x24) > as_seconds(snap->play_time)) {
        AS_WARN("dw3-autosave: not saved: card %d slot %d holds another "
                "game (name or play time differ)", port, slot);
        return -1;
    }
    if (g_target_manual &&
        memcard_host_snapshot(port, ".pre-auto") != 0) {
        AS_WARN("dw3-autosave: not saved: cannot keep your version of card "
                "%d slot %d aside (save%d.bin.pre-auto)", port, slot, port);
        return -1;
    }
    memcpy(new_info, old_info, SAVE_INFO_SIZE);
    as_fill_summary(new_info + 8 + slot * SAVE_SUMMARY_SIZE, snap);
    new_info[1] = (unsigned char)slot;
    new_info[0] = save_xor(new_info + 4, SAVE_INFO_SIZE - 4);
    if (memcard_host_file_write(port, offset, snap->save,
                                (size_t)snap->save_size) < 0 ||
        memcard_host_file_write(port, SAVE_INFO_OFFSET, new_info,
                                SAVE_INFO_SIZE) < 0) {
        (void)memcard_host_file_write(port, offset, old_slot,
                                      (size_t)snap->save_size);
        (void)memcard_host_file_write(port, SAVE_INFO_OFFSET, old_info,
                                      SAVE_INFO_SIZE);
        AS_WARN("dw3-autosave: write into RAM failed; card %d unchanged",
                port);
        return -1;
    }
    rc = memcard_host_flush(port);
    if (rc != 0) {
        /* The file on disk is the old one (the write goes to a .tmp first).
         * Put the old bytes back in RAM too, so nothing else can write the
         * auto-save later by accident, and try the old image once more. */
        (void)memcard_host_file_write(port, offset, old_slot,
                                      (size_t)snap->save_size);
        (void)memcard_host_file_write(port, SAVE_INFO_OFFSET, old_info,
                                      SAVE_INFO_SIZE);
        AS_WARN("dw3-autosave: FAILED to write card %d (%s kept as it was)",
                port, "your save");
        (void)memcard_host_flush(port);
        return -1;
    }
    g_target_manual = 0;
    g_count++;
    AS_INFO("dw3-autosave: wrote card %d slot %d (area mode 0x%x, #%u)",
            port, slot, snap->mode, g_count);
#if defined(DW3_CHEATS) && !defined(DW3_AUTOSAVE_TEST)
    /* P6.20: an auto-save made after a cheat carries the cheat mark. */
    dw3_cheats_save_written(port, slot, (int)offset, snap->save,
                            snap->save_size);
#endif
    return 0;
}
