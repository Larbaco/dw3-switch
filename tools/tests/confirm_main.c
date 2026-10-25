/* Host test of port/options/confirm.c (P6.27 / P6.28): the Yes / No window's
 * state machine with stubs for the pad, the sound, the save flush, the P6.9
 * restart request and the host's quit. Built with -DDW3_PORT_CONFIRM_TEST. */
#include "port_confirm.h"

#include <stdio.h>
#include <string.h>

/* The pad bits of confirm.c's test mode (the engine's PAD_* numbers). */
enum { UP = 4, DOWN = 6, LEFT = 7, CIRCLE = 12, CROSS = 13, TRIANGLE = 14 };
void dw3_port_confirm_test_set_pressed(unsigned mask);

/* --- stubs --- */
static int g_flush_rc;
static int g_flush_calls;
static int g_restart_enabled;
static int g_restart_requests;
static int g_quit_requests;
static int g_sound[3];
static int g_view_active;
static int g_view_kind = -1;
static int g_view_focus = -1;
static int g_view_calls;

int memcard_host_flush_all(void) {
    g_flush_calls++;
    return g_flush_rc;
}
int dw3_port_options_restart_enabled(void) { return g_restart_enabled; }
void dw3_port_restart_request(void) { g_restart_requests++; }
void dw3_host_request_quit(void) { g_quit_requests++; }
void dw3_port_confirm_test_sound(int kind) { g_sound[kind]++; }
void dw3_pc_dialog_set(int active, int kind, int focus) {
    g_view_active = active;
    g_view_kind = kind;
    g_view_focus = focus;
    g_view_calls++;
}

static int g_fail;
#define CHECK(cond)                                                        \
    do {                                                                   \
        if (!(cond)) {                                                     \
            fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #cond);        \
            g_fail = 1;                                                    \
        }                                                                  \
    } while (0)

static void press(int bit) {
    dw3_port_confirm_test_set_pressed(1u << bit);
    dw3_port_confirm_tick();
    dw3_port_confirm_test_set_pressed(0);
}

static void reset(void) {
    dw3_port_confirm_close();
    g_flush_rc = 0;
    g_flush_calls = 0;
    g_restart_requests = 0;
    g_quit_requests = 0;
    memset(g_sound, 0, sizeof(g_sound));
}

int main(void) {
    const char *label;

    CHECK(dw3_port_confirm_available());
    CHECK(dw3_port_title_quit_enabled());

    /* The field row does not depend on the Restart hotkey setting. */
    g_restart_enabled = 0;
    CHECK(dw3_port_field_return_enabled());
    g_restart_enabled = 1;
    CHECK(dw3_port_field_return_enabled());

    /* Closed: a tick does nothing. */
    reset();
    press(CROSS);
    CHECK(!dw3_port_confirm_is_open());
    CHECK(g_quit_requests == 0 && g_restart_requests == 0);

    /* Quit: opens on No; the press that opened it does not answer it. */
    reset();
    dw3_port_confirm_open(DW3_CONFIRM_QUIT);
    CHECK(dw3_port_confirm_is_open());
    CHECK(g_view_active == 1 && g_view_kind == 0 && g_view_focus == 0);
    press(CROSS); /* first tick after the open: ignored */
    CHECK(dw3_port_confirm_is_open());
    CHECK(g_quit_requests == 0);
    /* Cross on No: closes, nothing happens. */
    press(CROSS);
    CHECK(!dw3_port_confirm_is_open());
    CHECK(g_view_active == 0);
    CHECK(g_quit_requests == 0 && g_flush_calls == 0 && g_restart_requests == 0);

    /* Down moves to Yes, Up back to No; Left and Right too. */
    reset();
    dw3_port_confirm_open(DW3_CONFIRM_QUIT);
    press(UP); /* the guard frame */
    press(DOWN);
    CHECK(g_view_focus == 1);
    press(UP);
    CHECK(g_view_focus == 0);
    press(LEFT);
    CHECK(g_view_focus == 1);
    CHECK(g_sound[0] == 3);
    /* Yes: the saves are flushed, then the host is asked, once. */
    press(CROSS);
    CHECK(!dw3_port_confirm_is_open());
    CHECK(g_flush_calls == 1);
    CHECK(g_quit_requests == 1);
    CHECK(g_restart_requests == 0);

    /* A failed flush cancels the quit and keeps the game. */
    reset();
    g_flush_rc = -1;
    dw3_port_confirm_open(DW3_CONFIRM_QUIT);
    press(DOWN); /* guard */
    press(DOWN);
    press(CROSS);
    CHECK(g_flush_calls == 1);
    CHECK(g_quit_requests == 0);
    CHECK(!dw3_port_confirm_is_open());

    /* Triangle and Circle cancel. */
    reset();
    dw3_port_confirm_open(DW3_CONFIRM_QUIT);
    press(DOWN); /* guard */
    press(DOWN); /* Yes */
    press(TRIANGLE);
    CHECK(!dw3_port_confirm_is_open());
    CHECK(g_quit_requests == 0 && g_flush_calls == 0);
    dw3_port_confirm_open(DW3_CONFIRM_RETURN);
    press(DOWN);
    press(CIRCLE);
    CHECK(!dw3_port_confirm_is_open());
    CHECK(g_restart_requests == 0);

    /* Return to title: Yes asks the P6.9 reset (it does the flushing). */
    reset();
    dw3_port_confirm_open(DW3_CONFIRM_RETURN);
    CHECK(g_view_kind == 1 && g_view_focus == 0);
    press(DOWN); /* guard */
    press(DOWN);
    press(CROSS);
    CHECK(g_restart_requests == 1);
    CHECK(g_quit_requests == 0);
    CHECK(g_flush_calls == 0);
    CHECK(!dw3_port_confirm_is_open());

    /* Opening twice keeps the first window; close is idempotent. */
    reset();
    dw3_port_confirm_open(DW3_CONFIRM_QUIT);
    dw3_port_confirm_open(DW3_CONFIRM_RETURN);
    CHECK(g_view_kind == 0);
    dw3_port_confirm_close();
    dw3_port_confirm_close();
    CHECK(!dw3_port_confirm_is_open());

    /* The field row's text: the game's font codes, capitals, 01 01 = space. */
    label = dw3_port_field_return_label(2);
    CHECK((unsigned char)label[0] == 33 && (unsigned char)label[1] == 22 &&
          (unsigned char)label[2] == 33 && (unsigned char)label[3] == 25 &&
          (unsigned char)label[4] == 18);
    CHECK((unsigned char)label[5] == 1 && (unsigned char)label[6] == 1);
    CHECK((unsigned char)label[7] == 32); /* S */
    CHECK(strlen(label) == 13); /* TITLE, a two-byte space, SCREEN */
    {
        char english[40];
        strcpy(english, dw3_port_field_return_label(2));
        CHECK(strcmp(english, dw3_port_field_return_label(0)) == 0);
        CHECK(strcmp(english, dw3_port_field_return_label(1)) == 0);
        CHECK(strcmp(english, dw3_port_field_return_label(3)) != 0);
        strcpy(english, dw3_port_field_return_label(5));
        CHECK(strcmp(english, dw3_port_field_return_label(6)) != 0);
    }
    /* Every language's label is only letters and the two-byte space. */
    {
        int lang;
        for (lang = 1; lang <= 6; lang++) {
            const unsigned char *p = (const unsigned char *)dw3_port_field_return_label(lang);
            CHECK(*p != 0);
            while (*p) {
                if (*p == 1) {
                    CHECK(p[1] == 1);
                    p += 2;
                } else {
                    CHECK(*p >= 14 && *p <= 39);
                    p++;
                }
            }
        }
    }

    if (g_fail) {
        return 1;
    }
    printf("OK: confirm state machine\n");
    return 0;
}
