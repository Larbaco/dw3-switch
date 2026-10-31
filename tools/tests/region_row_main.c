/* Host test of P6.38: the Region at start row's restart window, end to end on
 * the host: the registry (port/options/port_options.c, built as a combined
 * image's game copy: DW3_REGION_IMAGE), the Yes / No window (confirm.c) and the
 * policy (region_relaunch.c), with stand-ins for the pad, the saves, the dialog
 * artwork and the host's relaunch hooks. The running region is the US (no
 * VERSION_EU). Usage: region_row_test <scratch dir> (must exist and be empty). */
#include "dw3_region.h"
#include "dw3_region_relaunch.h"
#include "port_confirm.h"
#include "port_options.h"
#include "port_options_groups.h"
#include "title_overlay_art.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum { BIT_UP = 4, BIT_DOWN = 6, BIT_CIRCLE = 12, BIT_CROSS = 13, BIT_TRIANGLE = 14 };
void dw3_port_options_test_set_pressed(unsigned mask);
void dw3_port_options_test_goto_row(int reg);
void dw3_port_confirm_test_set_pressed(unsigned mask);

static int g_fail;
#define CHECK(cond)                                                          \
    do {                                                                     \
        if (!(cond)) {                                                       \
            fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #cond);          \
            g_fail = 1;                                                      \
        }                                                                    \
    } while (0)

/* --- stand-ins --- */
static int g_flush_rc, g_flush_calls, g_section_open;
static int g_check_rc = DW3_RELAUNCH_OK;
static int g_request_rc, g_request_calls, g_request_target;
static int g_view_active, g_view_kind = -1, g_view_focus = -1;
static int g_quit_requests, g_restart_requests;

void dw3_port_audio_apply(int music_step, int sfx_step) { (void)music_step; (void)sfx_step; }
int memcard_host_flush_all(void) { g_flush_calls++; return g_flush_rc; }
int memcard_host_section_open_any(void) { return g_section_open; }
void dw3_port_restart_request(void) { g_restart_requests++; }
void dw3_host_request_quit(void) { g_quit_requests++; }
void dw3_port_confirm_test_sound(int kind) { (void)kind; }
void dw3_pc_dialog_set(int active, int kind, int focus) {
    g_view_active = active;
    g_view_kind = kind;
    g_view_focus = focus;
}
int dw3_host_relaunch_check(Dw3Region target, char *why, size_t cap) {
    (void)target;
    snprintf(why, cap, g_check_rc == DW3_RELAUNCH_OK ? "test host: exec" :
                       g_check_rc == DW3_RELAUNCH_NO_DATA ? "no data (test)" : "no host (test)");
    return g_check_rc;
}
int dw3_host_relaunch_request(Dw3Region target, char *why, size_t cap) {
    g_request_calls++;
    g_request_target = (int)target;
    snprintf(why, cap, g_request_rc == 0 ? "test host: exec" : "test host refused");
    return g_request_rc;
}

static void press(int bit) {
    dw3_port_options_test_set_pressed(1u << bit);
    dw3_port_confirm_test_set_pressed(1u << bit);
    dw3_port_options_tick();
    dw3_port_options_test_set_pressed(0);
    dw3_port_confirm_test_set_pressed(0);
}

/* The window ignores the press that opened it: one idle frame. */
static void idle(void) {
    dw3_port_options_tick();
}

static int row_value(void) { return dw3_port_options_level(DW3_PORT_OPT_REGION); }

static void read_cfg(const char *dir, Dw3Region *pref, Dw3Region *last) {
    dw3_region_cfg_read(dir, pref, last);
}

static void reset_stubs(void) {
    g_flush_rc = g_flush_calls = g_section_open = 0;
    g_check_rc = DW3_RELAUNCH_OK;
    g_request_rc = g_request_calls = g_request_target = 0;
    g_quit_requests = 0;
}

/* Open Options at `in_game`, on the Region row (Gameplay's last row). */
static void open_on_row(int in_game) {
    if (dw3_port_options_is_open()) {
        dw3_port_options_close();
    }
    if (in_game) {
        dw3_port_options_open_in_game();
    } else {
        dw3_port_options_open();
    }
    dw3_port_options_test_goto_row(DW3_PORT_OPT_REGION);
}

int main(int argc, char **argv) {
    const char *dir;
    Dw3Region pref, last;
    int i;

    if (argc != 2) {
        fprintf(stderr, "usage: %s <scratch dir>\n", argv[0]);
        return 2;
    }
    dir = argv[1];
    setenv("DW3_CONFIG_DIR", dir, 1); /* both cfg files in one folder, as the PC replays */
    (void)dw3_port_options_persist_init(dir);
    CHECK(dw3_port_options_region_base() != NULL);

    /* 1. Ask -> US: the running region (US), nothing to restart, no window. */
    reset_stubs();
    open_on_row(0);
    CHECK(row_value() == 0);
    press(BIT_CROSS);
    CHECK(row_value() == 1);
    press(BIT_UP); /* leave the row */
    CHECK(!dw3_port_confirm_is_open());
    press(BIT_DOWN); /* back on the row */
    CHECK(!dw3_port_confirm_is_open());

    /* 2. US -> EU, leave the row: the window, the cursor on No, nothing else yet. */
    press(BIT_CROSS);
    CHECK(row_value() == 2);
    CHECK(!dw3_port_confirm_is_open()); /* still on the row: no question yet */
    press(BIT_UP);
    CHECK(dw3_port_confirm_is_open() && dw3_port_confirm_owned_is_open());
    CHECK(g_view_active && g_view_kind == DW3_DIALOG_RESTART_EU && g_view_focus == 0);
    idle();
    idle();
    CHECK(dw3_port_confirm_is_open()); /* it waits for an answer */
    /* the title and field menus' own tick leaves it alone */
    dw3_port_confirm_test_set_pressed(1u << BIT_CROSS);
    dw3_port_confirm_tick();
    dw3_port_confirm_test_set_pressed(0);
    CHECK(dw3_port_confirm_is_open());
    /* No (the default) */
    press(BIT_CROSS);
    CHECK(!dw3_port_confirm_is_open() && !g_view_active);
    CHECK(g_request_calls == 0 && g_flush_calls == 0 && g_quit_requests == 0);
    CHECK(dw3_port_options_is_open());
    CHECK(row_value() == 2); /* the choice stays saved: takes effect at the next start */
    read_cfg(dir, &pref, &last);
    CHECK(pref == DW3_REGION_EU);

    /* 3. Walking the cursor over the row without changing it asks nothing. */
    press(BIT_DOWN); /* on the row */
    press(BIT_UP);   /* off it */
    CHECK(!dw3_port_confirm_is_open());

    /* 4. Ask never asks. */
    press(BIT_DOWN);
    press(BIT_CROSS); /* EU -> Ask (wraps) */
    CHECK(row_value() == 0);
    press(BIT_UP);
    CHECK(!dw3_port_confirm_is_open());

    /* 5. Circle / Triangle cancel the window like No. */
    press(BIT_DOWN);
    press(BIT_CROSS);
    press(BIT_CROSS); /* Ask -> US -> EU */
    CHECK(row_value() == 2);
    press(BIT_UP);
    CHECK(dw3_port_confirm_is_open());
    idle();
    press(BIT_CIRCLE);
    CHECK(!dw3_port_confirm_is_open() && g_request_calls == 0);

    /* 6. Back to the group list also leaves the row (the window is up over the list,
     * Options stays open until it is answered). */
    press(BIT_DOWN);
    press(BIT_CROSS); /* EU -> Ask */
    press(BIT_CROSS); /* Ask -> US */
    press(BIT_CROSS); /* US -> EU */
    CHECK(row_value() == 2);
    press(BIT_TRIANGLE);
    CHECK(dw3_port_confirm_is_open() && dw3_port_options_group() < 0);
    idle();
    press(BIT_TRIANGLE); /* cancel */
    CHECK(!dw3_port_confirm_is_open() && dw3_port_options_is_open());

    /* 7. Yes: the saves are flushed, the cfg gets the region, the host is asked. */
    reset_stubs();
    open_on_row(0);
    CHECK(row_value() == 2);
    press(BIT_CROSS); /* EU -> Ask */
    press(BIT_CROSS); /* Ask -> US */
    press(BIT_CROSS); /* US -> EU */
    press(BIT_UP);
    CHECK(dw3_port_confirm_is_open());
    idle();
    press(BIT_DOWN); /* cursor to Yes */
    CHECK(g_view_focus == 1);
    press(BIT_CROSS);
    CHECK(!dw3_port_confirm_is_open());
    CHECK(g_flush_calls == 1 && g_request_calls == 1 && g_request_target == DW3_REGION_EU);
    read_cfg(dir, &pref, &last);
    CHECK(pref == DW3_REGION_EU && last == DW3_REGION_EU);

    /* 8. In game the same question carries the progress warning (its own artwork kind). */
    reset_stubs();
    open_on_row(1);
    press(BIT_CROSS); /* EU -> Ask */
    press(BIT_CROSS); /* Ask -> US */
    press(BIT_CROSS); /* US -> EU */
    press(BIT_UP);
    CHECK(dw3_port_confirm_is_open() && g_view_kind == DW3_DIALOG_RESTART_EU_GAME);
    idle();
    press(BIT_CROSS); /* No */
    CHECK(!dw3_port_confirm_is_open() && g_request_calls == 0);

    /* 9. A save that cannot be flushed (or is being written) stops the relaunch and says so. */
    reset_stubs();
    g_flush_rc = -1;
    open_on_row(0);
    press(BIT_CROSS);
    press(BIT_CROSS);
    press(BIT_CROSS); /* EU -> Ask -> US -> EU */
    press(BIT_UP);
    idle();
    press(BIT_DOWN);
    press(BIT_CROSS); /* Yes */
    CHECK(g_flush_calls == 1 && g_request_calls == 0);
    CHECK(dw3_port_confirm_is_open() && g_view_kind == DW3_DIALOG_NOTICE);
    idle();
    press(BIT_CROSS); /* OK */
    CHECK(!dw3_port_confirm_is_open());
    g_flush_rc = 0;
    g_section_open = 1;
    open_on_row(0);
    press(BIT_CROSS);
    press(BIT_CROSS);
    press(BIT_CROSS);
    press(BIT_UP);
    idle();
    press(BIT_DOWN);
    press(BIT_CROSS);
    CHECK(g_request_calls == 0 && g_view_kind == DW3_DIALOG_NOTICE);
    idle();
    press(BIT_TRIANGLE);
    CHECK(!dw3_port_confirm_is_open());

    /* 10. The host refuses: the notice, today's behaviour. */
    reset_stubs();
    g_request_rc = -1;
    open_on_row(0);
    for (i = 0; i < 3; i++) {
        press(BIT_CROSS);
    }
    press(BIT_UP);
    idle();
    press(BIT_DOWN);
    press(BIT_CROSS);
    CHECK(g_request_calls == 1 && g_view_kind == DW3_DIALOG_NOTICE);
    idle();
    press(BIT_CROSS);
    CHECK(!dw3_port_confirm_is_open());

    /* 11. A host that cannot relaunch at all: the notice without a question. */
    reset_stubs();
    g_check_rc = DW3_RELAUNCH_NO_HOST;
    open_on_row(0);
    for (i = 0; i < 3; i++) {
        press(BIT_CROSS);
    }
    press(BIT_UP);
    CHECK(dw3_port_confirm_is_open() && g_view_kind == DW3_DIALOG_NOTICE);
    idle();
    press(BIT_CROSS);
    CHECK(!dw3_port_confirm_is_open() && g_request_calls == 0 && g_flush_calls == 0);

    /* 12. No data for the target: no window at all. */
    reset_stubs();
    g_check_rc = DW3_RELAUNCH_NO_DATA;
    open_on_row(0);
    for (i = 0; i < 3; i++) {
        press(BIT_CROSS);
    }
    press(BIT_UP);
    CHECK(!dw3_port_confirm_is_open());

    /* 13. The sub-screens the window is not for: Quit and Return still work (kind 0 / 1). */
    reset_stubs();
    dw3_port_options_close();
    dw3_port_confirm_open(DW3_CONFIRM_QUIT);
    CHECK(dw3_port_confirm_is_open() && !dw3_port_confirm_owned_is_open());
    dw3_port_confirm_close();

    if (g_fail) {
        return 1;
    }
    printf("OK: region row restart\n");
    return 0;
}
