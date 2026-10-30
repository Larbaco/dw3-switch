/* P6.32 phase 7: the help line of the full-screen map view. See map_view_help.h. */
#include "map_view_help.h"

#include <stdio.h>
#include <string.h>

#include "port_glyphs.h"

/* The two lines per language. {zo} {zi} the zoom buttons, {sp} {sn} the scaler
 * buttons, {ce} centre on your area, {hud} the bars, {back} the cancel button.
 * English is the PoC's text byte for byte, so the pictures that show it did not
 * change. MACHINE-DRAFTED, NEEDS NATIVE REVIEW for FR, DE, IT, ES: written by an
 * AI agent from the English, with the words the help pages use (docs/P6.30_HELP.md):
 * FR filtre / zone / retour, DE Skalierer / Gebiet, IT ridimensionatore / area,
 * ES escalador / zona. Capitals and ASCII only (the HUD font): DE writes UE for
 * the umlaut, the others drop their accents. The scaler word is the one of the
 * help pages, abbreviated where the line would not fit (IT). */
static const char *const g_lines[MV_HELP_LANGS][MV_HELP_LINES] = {
    {"PAD/L-STICK CURSOR  R-STICK/{zo}/{zi} ZOOM  {sp}/{sn} SCALER",
     "{ce} CENTRE ON YOUR AREA  {hud} HUD  {back} BACK"},
    {"PAD/STICK G. CURSEUR  STICK D./{zo}/{zi} ZOOM  {sp}/{sn} FILTRE",
     "{ce} CENTRER SUR VOTRE ZONE  {hud} HUD  {back} RETOUR"},
    {"PAD/L-STICK CURSOR  R-STICK/{zo}/{zi} ZOOM  {sp}/{sn} SKALIERER",
     "{ce} AUF DEINEM GEBIET ZENTRIEREN  {hud} HUD  {back} ZURUECK"},
    {"PAD/STICK SIN. CURSORE  STICK DES./{zo}/{zi} ZOOM  {sp}/{sn} RIDIMENS.",
     "{ce} CENTRA SULLA TUA AREA  {hud} HUD  {back} INDIETRO"},
    {"PAD/STICK IZQ. CURSOR  STICK DER./{zo}/{zi} ZOOM  {sp}/{sn} ESCALADOR",
     "{ce} CENTRAR EN TU ZONA  {hud} HUD  {back} VOLVER"},
};

static const char *const g_slot[MV_HS_COUNT] = {"{zo}", "{zi}", "{sp}", "{sn}",
                                                "{ce}", "{hud}", "{back}"};

int dw3_mv_help_lang(int game_language) {
    switch (game_language) {
    case 3: return MV_HELP_FR;
    case 4: return MV_HELP_IT;
    case 5: return MV_HELP_DE;
    case 6: return MV_HELP_ES;
    default: return MV_HELP_EN;
    }
}

const char *dw3_mv_help_lang_name(int lang) {
    static const char *const n[MV_HELP_LANGS] = {"en", "fr", "de", "it", "es"};

    return lang >= 0 && lang < MV_HELP_LANGS ? n[lang] : "?";
}

/* The PS word of a position, in capitals; Triangle is TRI as it always was on
 * this line. */
static void ps_word(int source, char out[MV_HELP_NAME_CAP]) {
    const char *w = dw3_button_ps_name(source);
    int i;

    if (strcmp(w, "triangle") == 0) {
        w = "tri";
    }
    for (i = 0; w[i] != '\0' && i + 1 < MV_HELP_NAME_CAP; i++) {
        out[i] = (char)(w[i] >= 'a' && w[i] <= 'z' ? w[i] - 'a' + 'A' : w[i]);
    }
    out[i] = '\0';
}

void dw3_mv_help_name(const unsigned char map[DW3_BTN_COUNT], int ps,
                      const MvHelpMode *mode, char out[MV_HELP_NAME_CAP]) {
    if (ps < 0 || ps >= DW3_BTN_COUNT) {
        /* Select is not remappable: the Switch's Minus, the PC's Select */
        snprintf(out, MV_HELP_NAME_CAP, "%s", mode->switch_names ? "MINUS" : "SELECT");
        return;
    }
    if (mode->switch_names) {
        const char *cap = dw3_button_cap_label(map, ps);
        const int g = mode->glyphs ? dw3_glyph_from_cap(cap) : DW3_GLYPH_NONE;

        if (g != DW3_GLYPH_NONE) {
            out[0] = (char)(MV_HELP_GLYPH0 + g);
            out[1] = '\0';
        } else {
            snprintf(out, MV_HELP_NAME_CAP, "%s", cap);
        }
        return;
    }
    ps_word(map[ps], out);
}

void dw3_mv_help_names(const unsigned char map[DW3_BTN_COUNT], const MvBindings *b,
                       const MvHelpMode *mode,
                       char names[MV_HS_COUNT][MV_HELP_NAME_CAP]) {
    static const int act[5] = {MV_ACT_ZOOM_OUT, MV_ACT_ZOOM_IN, MV_ACT_SCALER_PREV,
                               MV_ACT_SCALER_NEXT, MV_ACT_CENTRE};
    int i;

    for (i = 0; i < 5; i++) {
        dw3_mv_help_name(map, dw3_mv_bind_ps_button(b->bit[act[i]]), mode, names[i]);
    }
    dw3_mv_help_name(map, DW3_BTN_CIRCLE, mode, names[MV_HS_HUD]);
    dw3_mv_help_name(map, DW3_BTN_TRIANGLE, mode, names[MV_HS_BACK]);
}

int dw3_mv_help_line(int lang, int line,
                     const char names[MV_HS_COUNT][MV_HELP_NAME_CAP], char *out,
                     size_t cap) {
    const char *t;
    size_t n = 0;

    if (lang < 0 || lang >= MV_HELP_LANGS || line < 0 || line >= MV_HELP_LINES ||
        out == NULL || cap == 0) {
        return -1;
    }
    t = g_lines[lang][line];
    while (*t != '\0') {
        int s;

        if (*t == '{') {
            for (s = 0; s < MV_HS_COUNT; s++) {
                const size_t l = strlen(g_slot[s]);

                if (strncmp(t, g_slot[s], l) == 0) {
                    const size_t nl = strlen(names[s]);

                    if (n + nl + 1 > cap) {
                        return -1;
                    }
                    memcpy(out + n, names[s], nl);
                    n += nl;
                    t += l;
                    break;
                }
            }
            if (s < MV_HS_COUNT) {
                continue;
            }
        }
        if (n + 2 > cap) {
            return -1;
        }
        out[n++] = *t++;
    }
    out[n] = '\0';
    return (int)n;
}

int dw3_mv_help_width(const char *text) {
    int w = 0;

    for (; *text != '\0'; text++) {
        w += (unsigned char)*text >= MV_HELP_GLYPH0 ? MV_HELP_GLYPH_ADV : MV_HELP_CHAR_W;
    }
    return w;
}

const char *dw3_mv_help_log_text(const char *text, char *out, size_t cap) {
    size_t n = 0;

    for (; *text != '\0' && n + 5 < cap; text++) {
        const unsigned char c = (unsigned char)*text;

        if (c >= MV_HELP_GLYPH0) {
            n += (size_t)snprintf(out + n, cap - n, "[%s]",
                                  dw3_glyph_name(c - MV_HELP_GLYPH0));
        } else {
            out[n++] = (char)c;
        }
    }
    out[n] = '\0';
    return out;
}
