/* P6.27 / P6.28: the Yes / No confirmation of Quit game (title menu) and
 * Return to title (field menu). The window is drawn by the native overlay
 * (title_overlay_art.c, dw3_pc_dialog_art); this file is its small state
 * machine, the pad, and what a Yes does. See docs/PORT_OPTIONS.md
 * "Quit and return to title". */
#include "port_confirm.h"

#include "memcard_host.h"
#include "port_options.h"
#include "port_options_groups.h"
#include "port_region_relaunch.h"
#include "port_restart.h"

#include <stdio.h>
#include <string.h>

#if defined(DW3_PORT_CONFIRM_TEST)
/* Host test: the pad, the sound and the host's quit are stubs of the test. */
#define CF_INFO(...) (fprintf(stderr, __VA_ARGS__), fputc('\n', stderr))
#define CF_WARN(...) (fprintf(stderr, __VA_ARGS__), fputc('\n', stderr))
enum { CF_UP = 4, CF_RIGHT = 5, CF_DOWN = 6, CF_LEFT = 7, CF_CIRCLE = 12,
       CF_CROSS = 13, CF_TRIANGLE = 14 };
static unsigned g_test_pressed;
void dw3_port_confirm_test_set_pressed(unsigned mask) { g_test_pressed = mask; }
static int cf_pressed(int bit) { return (int)((g_test_pressed >> bit) & 1u); }
static void cf_sound(int kind) { extern void dw3_port_confirm_test_sound(int); dw3_port_confirm_test_sound(kind); }
#else
#include <psyz/log.h>
#include "engine/pad.h"
#include "engine/sound.h"
#define CF_INFO(...) INFOF(__VA_ARGS__)
#define CF_WARN(...) WARNF(__VA_ARGS__)
enum { CF_UP = PAD_UP, CF_RIGHT = PAD_RIGHT, CF_DOWN = PAD_DOWN, CF_LEFT = PAD_LEFT,
       CF_CIRCLE = PAD_CIRCLE, CF_CROSS = PAD_CROSS, CF_TRIANGLE = PAD_TRIANGLE };
static int cf_pressed(int bit) { return PAD_PRESSED(bit); }
static void cf_sound(int kind) {
    static const s32 id[3] = {SOUND_CURSOR, SOUND_SELECT, SOUND_MENU_CANCEL};
    if (SOUND.playSound != NULL) {
        SOUND.playSound(id[kind]);
    }
}
#endif
enum { CF_SND_CURSOR = 0, CF_SND_SELECT = 1, CF_SND_CANCEL = 2 };

#if defined(DW3_TITLE_NATIVE_OVERLAY) || defined(DW3_PORT_CONFIRM_TEST)
#include "title_overlay_art.h"
#define CF_NATIVE 1
/* The host's own way out (platforms/pc: the window's close request;
 * platforms/switch: the HOME quit flag, so the game loop returns and the
 * NRO exits to hbmenu). */
extern void dw3_host_request_quit(void);
#else
#define CF_NATIVE 0
#endif

static int g_open;
static int g_kind;
static int g_focus; /* 0 = No (the default), 1 = Yes */
static int g_just_opened;
static int g_region_target; /* P6.38: 1 US / 2 EU, for DW3_CONFIRM_REGION */
static int g_region_in_game;
static int g_ask_what; /* D1: DW3_PORT_ASK_* of a DW3_CONFIRM_ASK window */

static int kind_owned(int kind) {
    return kind == DW3_CONFIRM_REGION || kind == DW3_CONFIRM_NOTICE ||
           kind == DW3_CONFIRM_ASK;
}

static const char *kind_name(int kind) {
    return kind == DW3_CONFIRM_RETURN ? "return to title"
           : kind == DW3_CONFIRM_REGION ? "restart in region"
           : kind == DW3_CONFIRM_NOTICE ? "restart notice"
           : kind == DW3_CONFIRM_ASK
               ? (g_ask_what == DW3_PORT_ASK_SHARED_SAVES ? "shared saves"
                                                          : "stage sweep")
               : "quit";
}

#if CF_NATIVE
static int dialog_art_kind(void) {
    switch (g_kind) {
    case DW3_CONFIRM_RETURN:
        return DW3_DIALOG_RETURN;
    case DW3_CONFIRM_REGION:
        return DW3_DIALOG_RESTART_US + (g_region_target == 2 ? 1 : 0) +
               (g_region_in_game ? 2 : 0);
    case DW3_CONFIRM_NOTICE:
        return DW3_DIALOG_NOTICE;
    case DW3_CONFIRM_ASK:
        return g_ask_what == DW3_PORT_ASK_SHARED_SAVES ? DW3_DIALOG_SHARED_SAVES
                                                       : DW3_DIALOG_SWEEP;
    default:
        return DW3_DIALOG_QUIT;
    }
}
#endif

static void push_view(void) {
#if CF_NATIVE
    dw3_pc_dialog_set(g_open, dialog_art_kind(), g_focus);
#endif
}

int dw3_port_confirm_available(void) {
    return CF_NATIVE;
}

int dw3_port_title_quit_enabled(void) {
    return dw3_port_confirm_available();
}

int dw3_port_field_return_enabled(void) {
    return dw3_port_confirm_available();
}

void dw3_port_confirm_open(int kind) {
    if (g_open || !dw3_port_confirm_available()) {
        return;
    }
    g_open = 1;
    g_kind = kind == DW3_CONFIRM_RETURN ? DW3_CONFIRM_RETURN : DW3_CONFIRM_QUIT;
    g_focus = 0;
    g_just_opened = 1;
    push_view();
    CF_INFO("dw3-port-confirm: open %s (cursor on No)", kind_name(g_kind));
}

int dw3_port_confirm_is_open(void) {
    return g_open != 0;
}

#if DW3_OPT_REGION_ROW
void dw3_port_confirm_open_region(int target, int in_game) {
    if (g_open || !dw3_port_confirm_available()) {
        return;
    }
    g_open = 1;
    g_kind = DW3_CONFIRM_REGION;
    g_region_target = target == 2 ? 2 : 1;
    g_region_in_game = in_game != 0;
    g_focus = 0;
    g_just_opened = 1;
    push_view();
    CF_INFO("dw3-port-confirm: open restart in %s (%s, cursor on No)",
            g_region_target == 2 ? "EU" : "US", in_game ? "in game" : "title");
}

void dw3_port_confirm_open_notice(void) {
    if (g_open || !dw3_port_confirm_available()) {
        return;
    }
    g_open = 1;
    g_kind = DW3_CONFIRM_NOTICE;
    g_focus = 1; /* the one button, OK, is lit */
    g_just_opened = 1;
    push_view();
    CF_INFO("dw3-port-confirm: open restart notice (OK)");
}
#else
void dw3_port_confirm_open_region(int target, int in_game) {
    (void)target;
    (void)in_game;
}
void dw3_port_confirm_open_notice(void) {}
#endif

/* D1: the Yes / No window of Shared saves and Stage sweep. Cursor on No. */
int dw3_port_confirm_open_ask(int what) {
    if (g_open || !dw3_port_confirm_available()) {
        return 0;
    }
    g_open = 1;
    g_kind = DW3_CONFIRM_ASK;
    g_ask_what = what;
    g_focus = 0;
    g_just_opened = 1;
    push_view();
    CF_INFO("dw3-port-confirm: open %s (cursor on No)", kind_name(g_kind));
    return 1;
}

int dw3_port_confirm_owned_is_open(void) {
    return g_open && kind_owned(g_kind);
}

void dw3_port_confirm_close(void) {
    if (!g_open) {
        return;
    }
    g_open = 0;
    g_just_opened = 0;
    push_view();
}

/* Quit game: the saves go to the card files first, as on HOME. A failed
 * flush cancels the quit (unlike HOME, the player can still save again). */
static void accept_quit(void) {
    if (memcard_host_flush_all() != 0) {
        CF_WARN("dw3-port-quit: save flush failed; quit cancelled");
        dw3_port_confirm_close();
        return;
    }
    CF_INFO("dw3-port-quit: saves flushed; asking the host to quit");
#if CF_NATIVE
    dw3_host_request_quit();
#endif
    dw3_port_confirm_close();
}

/* Return to title: the P6.9 reset, run by dw3_port_restart_poll() right after
 * the next pad update (it flushes the saves and cancels on a failure). */
static void accept_return(void) {
    CF_INFO("dw3-port-confirm: return to title confirmed");
    dw3_port_restart_request();
    dw3_port_confirm_close();
}

static void confirm_tick(void) {
    if (!g_open) {
        return;
    }
    if (g_just_opened) {
        /* The press that opened the window must not also answer it. */
        g_just_opened = 0;
        return;
    }
    if (g_kind == DW3_CONFIRM_NOTICE) {
        /* One button: any answer closes it. */
        if (cf_pressed(CF_CROSS) || cf_pressed(CF_TRIANGLE) || cf_pressed(CF_CIRCLE)) {
            cf_sound(cf_pressed(CF_CROSS) ? CF_SND_SELECT : CF_SND_CANCEL);
            CF_INFO("dw3-port-confirm: OK (restart notice)");
            dw3_port_confirm_close();
            dw3_port_region_relaunch_notice_done();
        }
        return;
    }
    if (cf_pressed(CF_UP) || cf_pressed(CF_DOWN) || cf_pressed(CF_LEFT) ||
        cf_pressed(CF_RIGHT)) {
        g_focus = !g_focus;
        cf_sound(CF_SND_CURSOR);
        push_view();
        CF_INFO("dw3-port-confirm: cursor on %s", g_focus ? "Yes" : "No");
    } else if (cf_pressed(CF_CROSS)) {
        cf_sound(CF_SND_SELECT);
        if (g_focus) {
            CF_INFO("dw3-port-confirm: Yes (%s)", kind_name(g_kind));
            if (g_kind == DW3_CONFIRM_ASK) {
                const int what = g_ask_what;

                dw3_port_confirm_close();
                dw3_port_options_ask_answer(what, 1);
            } else if (g_kind == DW3_CONFIRM_REGION) {
                /* Close first: a failed relaunch opens the notice in its place. */
                const int target = g_region_target;

                dw3_port_confirm_close();
                dw3_port_region_relaunch_yes(target);
            } else if (g_kind == DW3_CONFIRM_RETURN) {
                accept_return();
            } else {
                accept_quit();
            }
        } else {
            CF_INFO("dw3-port-confirm: No (%s); nothing changed", kind_name(g_kind));
            dw3_port_confirm_close();
            if (g_kind == DW3_CONFIRM_REGION) {
                dw3_port_region_relaunch_no(g_region_target, 0);
            } else if (g_kind == DW3_CONFIRM_ASK) {
                dw3_port_options_ask_answer(g_ask_what, 0);
            }
        }
    } else if (cf_pressed(CF_TRIANGLE) || cf_pressed(CF_CIRCLE)) {
        cf_sound(CF_SND_CANCEL);
        CF_INFO("dw3-port-confirm: cancelled (%s); nothing changed", kind_name(g_kind));
        dw3_port_confirm_close();
        if (g_kind == DW3_CONFIRM_REGION) {
            dw3_port_region_relaunch_no(g_region_target, 1);
        } else if (g_kind == DW3_CONFIRM_ASK) {
            dw3_port_options_ask_answer(g_ask_what, 0);
        }
    }
}

void dw3_port_confirm_tick(void) {
    if (g_open && kind_owned(g_kind)) {
        return; /* the Options screen ticks these (dw3_port_confirm_tick_owned) */
    }
    confirm_tick();
}

void dw3_port_confirm_tick_owned(void) {
    if (g_open && kind_owned(g_kind)) {
        confirm_tick();
    }
}

/* The field menu's row text. The game's font encoding of a string table:
 * digits 4..13, A..Z 14..39, a..z 40..65, and 01 01 for a space. The words
 * are in capitals like the other rows and use no accent (the glyph of an
 * accent is a two-byte code that this file does not carry). */
const char *dw3_port_field_return_label(int language) {
    static char out[40];
    const char *text;
    char *p = out;

    switch (language) {
    case 3: text = "RETOUR TITRE"; break;
    case 4: text = "TORNA AL TITOLO"; break;
    case 5: text = "TITELBILD"; break;
    case 6: text = "VOLVER AL INICIO"; break;
    default: text = "TITLE SCREEN"; break;
    }
    for (; *text != '\0' && p < out + sizeof(out) - 3; text++) {
        if (*text >= 'A' && *text <= 'Z') {
            *p++ = (char)(14 + *text - 'A');
        } else if (*text >= 'a' && *text <= 'z') {
            *p++ = (char)(40 + *text - 'a');
        } else if (*text >= '0' && *text <= '9') {
            *p++ = (char)(4 + *text - '0');
        } else {
            *p++ = 1;
            *p++ = 1;
        }
    }
    *p = '\0';
    return out;
}
