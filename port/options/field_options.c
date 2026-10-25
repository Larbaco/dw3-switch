/* P9.7 (slice 1): the port Options screen from the field menu's Options row.
 * The screen itself (rows, pad, persistence, live apply) is port_options.c; this
 * file is what the field menu needs on top of it: the row's text, opening the
 * screen in the menu's language, keeping the native overlay showing it while
 * the field (not the title) is the mode, and hiding it again on close. See
 * docs/PORT_OPTIONS.md "Options in game". */
#include "port_field_options.h"

#include "port_confirm.h"
#include "port_options.h"

#include <stdio.h>

#if defined(DW3_TITLE_NATIVE_OVERLAY) || defined(DW3_FIELD_OPTIONS_TEST)
#include "title_overlay_art.h"
/* The overlays' state hook (title_menu_overlay.c, nx_title_overlay.c): with
 * the Options screen open the PC overlay shows it whatever `active` says, and
 * (0, -2) hides it again. The Switch overlay follows dw3_port_options_is_open()
 * by itself and ignores the call while it is open. */
extern void dw3_pc_title_menu_set_state(int active, int selected);
#define FO_NATIVE 1
#else
#define FO_NATIVE 0
#endif

int dw3_port_field_options_enabled(void) {
    return dw3_port_confirm_available();
}

/* The game's font encoding of a string table: digits 4..13, A..Z 14..39,
 * a..z 40..65 and 01 01 for a space (the same as the Return to title row). The
 * words are capitals without accents. */
const char *dw3_port_field_options_label(int language) {
    static char out[40];
    const char *text;
    char *p = out;

    switch (language) {
    case 3: text = "OPTIONS"; break;
    case 4: text = "OPZIONI"; break;
    case 5: text = "OPTIONEN"; break;
    case 6: text = "OPCIONES"; break;
    default: text = "OPTIONS"; break;
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

#if FO_NATIVE
/* LANGUAGE to the authored Options lettering, as title_logo_capture.c does. */
static int lettering_language(int language) {
    switch (language) {
    case 3: return DW3_TITLE_LANG_FR;
    case 4: return DW3_TITLE_LANG_IT;
    case 5: return DW3_TITLE_LANG_DE;
    case 6: return DW3_TITLE_LANG_ES;
    default: return DW3_TITLE_LANG_EN;
    }
}
#endif

/* Show (or refresh) the screen on the PC overlay; a no-op elsewhere. */
static void show_view(void) {
#if FO_NATIVE
    dw3_pc_title_menu_set_state(1, -1);
#endif
}

static void hide_view(void) {
#if FO_NATIVE
    dw3_pc_title_menu_set_state(0, -2);
#endif
}

void dw3_port_field_options_open(int language) {
    if (!dw3_port_field_options_enabled() || dw3_port_options_is_open()) {
        return;
    }
#if FO_NATIVE
    /* The field never ran the title's label capture on a disc booted straight
     * into a map (the replay start-mode hook); set the words here too. */
    dw3_pc_title_overlay_set_options_language(lettering_language(language));
#else
    (void)language;
#endif
    dw3_port_options_open_in_game();
    show_view();
}

int dw3_port_field_options_is_open(void) {
    return dw3_port_options_is_open() && dw3_port_options_in_game();
}

void dw3_port_field_options_tick(void) {
    if (!dw3_port_field_options_is_open()) {
        return;
    }
    /* The screen's own tick reads the pad (Triangle closes it). */
    dw3_port_options_tick();
    if (dw3_port_options_is_open()) {
        show_view();
    } else {
        hide_view();
    }
}
