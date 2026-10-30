/* P10.6 Switch glyphs: replace the font's PS button icons (cells of
 * FONT_ICONS_1) in VRAM with Switch glyphs that follow the button map.
 * docs/P10.6_GLYPHS.md. C90: this file is built as gnu89 with the rest of port/. */
#include "port_glyphs.h"

#include "mod_pack.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* --- art ---------------------------------------------------------------- */

/* tools/generate_switch_glyphs.py writes this: one 144-digit string per glyph
 * (the PNG files of assets/glyphs are the same art). */
static const char g_art_text[DW3_GLYPH_COUNT][DW3_GLYPH_PIXELS + 1] = {
#include "switch_glyph_art.inc"
};

static const char *const g_glyph_name[DW3_GLYPH_COUNT] = {
    "A", "B", "X", "Y", "L", "R", "ZL", "ZR"};

/* The icon cells of the four PS buttons: the Shift-JIS symbol of each is
 * 0x817E (x), 0x819B (circle), 0x81A0 (square), 0x81A2 (triangle), and the
 * drawn cell is icons[n - 1] with n the position in FONT_ICON_MAP
 * (docs/P10.6_GLYPHS.md section 1). */
static const unsigned char g_cell_button[DW3_GLYPH_CELLS] = {
    DW3_BTN_CROSS, DW3_BTN_CIRCLE, DW3_BTN_SQUARE, DW3_BTN_TRIANGLE};
static const unsigned char g_cell_n[DW3_GLYPH_CELLS] = {27, 47, 50, 52};

/* Census (docs/P10.6_GLYPHS.md "Census"): the US cell n = 27 is a plain
 * multiplication sign 8 px wide, not a button, so the US Cross keeps the
 * original icon whatever the layout; EU draws it as a button disc. */
#define GL_US_CROSS_PATCHED 0

/* Role -> palette index of the font CLUT. The patch writes indices only and
 * never the palette.
 * US: the icon is drawn with the text colour, indices 1, 2 (the body of the
 *   ramp) and 15 (the shadow); clear is index 0. The rim and the letter are
 *   index 2 on a clear body (an open ring with a letter), the shade 15 like
 *   the original's.
 * EU: the icon palette (rows 224..231, eight identical copies, own colours
 *   whatever the text colour) has no neutral light grey: its greys are indices
 *   1 (0x2D6B), 2 (0x1084, nearly black), 3 (0x39CE), 5 (0x2529), and the lightest
 *   colour that is not a tint of red, green or pink is index 9 (0x5E6F, a light
 *   steel grey). Rim and letter 9, body 2, shade 3: a dark disc with a light rim
 *   and letter. A grey letter (index 3) was tried first and read too dim. */
static const unsigned char g_map_us[DW3_ROLE_COUNT] = {0, 2, 0, 2, 15};
static const unsigned char g_map_eu[DW3_ROLE_COUNT] = {0, 9, 2, 9, 3};

int dw3_glyph_from_cap(const char *cap) {
    int i;

    if (cap == NULL) {
        return DW3_GLYPH_NONE;
    }
    for (i = 0; i < DW3_GLYPH_COUNT; i++) {
        if (strcmp(cap, g_glyph_name[i]) == 0) {
            return i;
        }
    }
    return DW3_GLYPH_NONE;
}

const char *dw3_glyph_name(int glyph) {
    return glyph >= 0 && glyph < DW3_GLYPH_COUNT ? g_glyph_name[glyph] : "?";
}

const unsigned char *dw3_glyph_art(int glyph) {
    static unsigned char roles[DW3_GLYPH_COUNT][DW3_GLYPH_PIXELS];
    static int decoded;
    int g;
    int i;

    if (glyph < 0 || glyph >= DW3_GLYPH_COUNT) {
        return NULL;
    }
    if (!decoded) {
        for (g = 0; g < DW3_GLYPH_COUNT; g++) {
            for (i = 0; i < DW3_GLYPH_PIXELS; i++) {
                roles[g][i] = (unsigned char)(g_art_text[g][i] - '0');
            }
        }
        decoded = 1;
    }
    return roles[glyph];
}

const unsigned char *dw3_glyph_index_map(int eu) {
    return eu ? g_map_eu : g_map_us;
}

int dw3_glyph_pack(const unsigned char roles[DW3_GLYPH_PIXELS],
                   const unsigned char map[DW3_ROLE_COUNT],
                   unsigned char out[DW3_GLYPH_PACKED]) {
    int i;

    for (i = 0; i < DW3_GLYPH_PIXELS; i++) {
        if (roles[i] >= DW3_ROLE_COUNT || map[roles[i]] > 15) {
            return -1;
        }
    }
    for (i = 0; i < DW3_GLYPH_PIXELS; i += 2) {
        out[i / 2] = (unsigned char)(map[roles[i]] | (map[roles[i + 1]] << 4));
    }
    return 0;
}

/* The five role colours of the PNG (tools/generate_switch_glyphs.py ROLE_RGB);
 * index 0 is the transparent one. */
static const unsigned char g_role_rgb[DW3_ROLE_COUNT][3] = {
    {0, 0, 0}, {224, 224, 224}, {40, 40, 48}, {255, 255, 255}, {112, 112, 120}};

int dw3_glyph_roles_from_rgba(const unsigned char *rgba, int w, int h,
                              unsigned char out[DW3_GLYPH_PIXELS], char *why,
                              unsigned why_cap) {
    int i;
    int r;

    if (w != DW3_GLYPH_W || h != DW3_GLYPH_H) {
        if (why != NULL && why_cap > 0) {
            snprintf(why, why_cap, "%dx%d, not %dx%d", w, h, DW3_GLYPH_W,
                     DW3_GLYPH_H);
        }
        return -1;
    }
    for (i = 0; i < DW3_GLYPH_PIXELS; i++) {
        const unsigned char *p = rgba + i * 4;

        if (p[3] == 0) {
            out[i] = DW3_ROLE_CLEAR;
            continue;
        }
        for (r = 1; r < DW3_ROLE_COUNT; r++) {
            if (p[0] == g_role_rgb[r][0] && p[1] == g_role_rgb[r][1] &&
                p[2] == g_role_rgb[r][2]) {
                break;
            }
        }
        if (r == DW3_ROLE_COUNT) {
            if (why != NULL && why_cap > 0) {
                snprintf(why, why_cap,
                         "pixel %d,%d is %d,%d,%d, not a role colour", i % w,
                         i / w, p[0], p[1], p[2]);
            }
            return -1;
        }
        out[i] = (unsigned char)r;
    }
    return 0;
}

int dw3_glyph_art_from_png(const char *path,
                           unsigned char out[DW3_GLYPH_PIXELS], char *why,
                           unsigned why_cap) {
    Dw3ModImage img;
    int rc;

    memset(&img, 0, sizeof(img));
    if (dw3_mod_png_load(path, &img, why, why_cap) != 0) {
        return -1;
    }
    rc = dw3_glyph_roles_from_rgba(img.rgba, img.w, img.h, out, why, why_cap);
    dw3_mod_image_free(&img);
    return rc;
}

int dw3_glyph_plan(const unsigned char map[DW3_BTN_COUNT], int eu,
                   Dw3GlyphCell out[DW3_GLYPH_CELLS]) {
    int i;
    int patched = 0;

    for (i = 0; i < DW3_GLYPH_CELLS; i++) {
        out[i].ps_button = g_cell_button[i];
        out[i].n = g_cell_n[i];
        out[i].glyph = dw3_glyph_from_cap(
            dw3_button_cap_label(map, g_cell_button[i]));
        if (!eu && g_cell_button[i] == DW3_BTN_CROSS && !GL_US_CROSS_PATCHED) {
            out[i].glyph = DW3_GLYPH_NONE;
        }
        if (out[i].glyph != DW3_GLYPH_NONE) {
            patched++;
        }
    }
    return patched;
}

void dw3_glyph_plan_text(const Dw3GlyphCell cells[DW3_GLYPH_CELLS], char *out,
                         unsigned cap) {
    unsigned n = 0;
    int i;

    if (cap == 0) {
        return;
    }
    out[0] = '\0';
    for (i = 0; i < DW3_GLYPH_CELLS; i++) {
        int w = snprintf(out + n, cap - n, "%sn=%d:%s", i ? " " : "",
                         cells[i].n,
                         cells[i].glyph == DW3_GLYPH_NONE
                             ? "orig" : dw3_glyph_name(cells[i].glyph));

        if (w < 0 || (unsigned)w >= cap - n) {
            return;
        }
        n += (unsigned)w;
    }
}

/* The page-turn prompt: the font codes of the letters L, R and the digit 1 (the
 * same bytes in US and EU: "L1" is 19 05, "R1" is 1F 05). */
static const unsigned char g_pt_button[DW3_PAGETURN_COUNT] = {DW3_BTN_L1,
                                                              DW3_BTN_R1};
static const char g_pt_string[DW3_PAGETURN_COUNT][4] = {
    {1, DW3_PAGETURN_N0, 0, 0}, {1, DW3_PAGETURN_N0 + 1, 0, 0}};

int dw3_glyph_pageturn_match(const char *str) {
    if (str == NULL || (unsigned char)str[1] != 0x05 || str[2] != 0) {
        return -1;
    }
    if ((unsigned char)str[0] == 0x19) {
        return 0;
    }
    return (unsigned char)str[0] == 0x1F ? 1 : -1;
}

int dw3_glyph_pageturn_plan(const unsigned char map[DW3_BTN_COUNT],
                            int out[DW3_PAGETURN_COUNT]) {
    int i;
    int patched = 0;

    for (i = 0; i < DW3_PAGETURN_COUNT; i++) {
        out[i] = dw3_glyph_from_cap(dw3_button_cap_label(map, g_pt_button[i]));
        patched += out[i] != DW3_GLYPH_NONE;
    }
    return patched;
}

const char *dw3_glyph_pageturn_string(int side) {
    return side >= 0 && side < DW3_PAGETURN_COUNT ? g_pt_string[side] : NULL;
}

/* --- host part: the FONT.load wrapper, the VRAM upload, the census ------- */

#ifndef DW3_GLYPHS_TEST
#include <psyz/log.h>
#include <libgpu.h>

#include "engine/text.h"
#include "overlay_host.h"
#include "port_options.h"

#define GL_LOG(...) INFOF(__VA_ARGS__)
#define GL_WARN(...) WARNF(__VA_ARGS__)

#if VERSION_EU
#define GL_EU 1
#else
#define GL_EU 0
#endif

/* The font archive's pages are loaded at x = 0x140 (loadFont), a page is 64
 * halfwords wide and a cell's u is in 4 bpp texels, 4 to a halfword. */
enum { GL_FONT_X = 0x140, GL_PAGE_HW = 64, GL_STYLE = 1 };

static void (*g_orig_load)(void);
static int g_font_loaded;          /* the font is in VRAM (any load) */
static int g_loads;                /* font loads seen */
static int g_logged_off;           /* the off line was logged once */
static int g_cur[DW3_GLYPH_CELLS]; /* glyph in VRAM per cell, NONE = original */
static int g_have_orig[DW3_GLYPH_CELLS];
static unsigned char g_orig[DW3_GLYPH_CELLS][DW3_GLYPH_PACKED];
/* Page-turn prompt (L1 / R1): the size-1 icon table is replaced by a copy with
 * two more cells while the option is on, the originals come back when it is off. */
static Glyph g_pt_icons[114 + DW3_PAGETURN_COUNT];
static Glyph *g_pt_orig_icons;     /* the disc's table, NULL = not installed */
static s16 g_pt_orig_count;
static int g_pt_cur[DW3_PAGETURN_COUNT]; /* glyph in VRAM per side */
static struct {
    int side;
    int glyph;
    int32_t mode;
} g_pt_seen[16];                   /* (side, glyph, scene) of the use lines logged */
static int g_pt_seen_n;
static int g_pt_warned;
static int g_art_ready;
static unsigned char g_art[DW3_GLYPH_COUNT][DW3_GLYPH_PIXELS];
static int g_art_file[DW3_GLYPH_COUNT]; /* 1 = read from the override folder */

/* The folder of the owner's own glyph PNGs: sdmc:/switch/dw3/glyphs on the
 * Switch (like mods/), <config dir>/glyphs on the PC. */
static int gl_override_dir(char *out, size_t cap) {
#ifdef __SWITCH__
    snprintf(out, cap, "sdmc:/switch/dw3/glyphs");
    return 1;
#else
    const char *dir = dw3_port_options_cfg_dir();

    if (dir == NULL || dir[0] == '\0') {
        return 0;
    }
    snprintf(out, cap, "%s/glyphs", dir);
    return 1;
#endif
}

/* The art of every glyph, once: the compiled-in default, overridden by a PNG of
 * the same name in the override folder. A missing file is silent (it is the
 * normal case); a bad file logs one line and keeps the default. */
static void gl_art_load(void) {
    char dir[480];
    char path[512];
    int have_dir;
    int g;
    int files = 0;

    if (g_art_ready) {
        return;
    }
    have_dir = gl_override_dir(dir, sizeof(dir));
    for (g = 0; g < DW3_GLYPH_COUNT; g++) {
        memcpy(g_art[g], dw3_glyph_art(g), DW3_GLYPH_PIXELS);
        g_art_file[g] = 0;
        if (have_dir) {
            char why[96];
            char name[8];
            unsigned char px[DW3_GLYPH_PIXELS];
            int k;

            for (k = 0; dw3_glyph_name(g)[k] != '\0' && k < 6; k++) {
                char c = dw3_glyph_name(g)[k];

                name[k] = (char)(c >= 'A' && c <= 'Z' ? c + 32 : c);
            }
            name[k] = '\0';
            snprintf(path, sizeof(path), "%s/%s.png", dir, name);
            if (dw3_glyph_art_from_png(path, px, why, sizeof(why)) == 0) {
                memcpy(g_art[g], px, DW3_GLYPH_PIXELS);
                g_art_file[g] = 1;
                files++;
            } else if (strstr(why, "cannot open") == NULL &&
                       strstr(why, "No such file") == NULL) {
                GL_WARN("dw3-glyphs: %s: %s; using the bundled %s", path, why,
                        dw3_glyph_name(g));
            }
        }
    }
    g_art_ready = 1;
    if (files > 0) {
        GL_LOG("dw3-glyphs: %d glyph(s) read from %s", files, dir);
    }
}

/* The font page cell of icon n of the size-1 table: VRAM rectangle. 0 = usable. */
static int gl_cell_rect(int n, RECT *r) {
    const Glyph *c;

    if (FONT.styles == NULL || FONT.styles[GL_STYLE].icons == NULL ||
        n < 1 || n > FONT.styles[GL_STYLE].iconCount) {
        return -1;
    }
    c = &FONT.styles[GL_STYLE].icons[n - 1];
    if (c->page > 1 || (c->u & 3) != 0 || c->w != DW3_GLYPH_W ||
        c->h != DW3_GLYPH_H) {
        return -1;
    }
    r->x = (short)(GL_FONT_X + c->page * GL_PAGE_HW + c->u / 4);
    r->y = (short)c->v;
    r->w = (short)(c->w / 4);
    r->h = (short)c->h;
    return 0;
}

static void gl_upload(const RECT *r, unsigned char *bytes) {
    RECT rect = *r;

    LoadImage(&rect, (u_long *)bytes);
}

/* Put the cell in `want` (a glyph, or NONE = the original). 1 = VRAM changed. */
static int gl_set_cell(int i, int n, int want, int eu) {
    RECT r;
    unsigned char packed[DW3_GLYPH_PACKED];

    if (g_cur[i] == want) {
        return 0;
    }
    if (gl_cell_rect(n, &r) != 0) {
        GL_WARN("dw3-glyphs: icon n=%d cell is not a 12x12 whole-halfword cell; "
                "left as is", n);
        g_cur[i] = want; /* do not retry every refresh */
        return 0;
    }
    if (want == DW3_GLYPH_NONE) {
        if (!g_have_orig[i]) {
            return 0;
        }
        gl_upload(&r, g_orig[i]);
        g_cur[i] = DW3_GLYPH_NONE;
        return 1;
    }
    if (!g_have_orig[i]) {
        DrawSync(0);
        StoreImage(&r, (u_long *)g_orig[i]);
        DrawSync(0);
        g_have_orig[i] = 1;
    }
    if (dw3_glyph_pack(g_art[want], dw3_glyph_index_map(eu), packed) != 0) {
        return 0;
    }
    gl_upload(&r, packed);
    g_cur[i] = want;
    return 1;
}

/* --- page-turn prompt: the table, the two cells, the string hook ---------- */

/* The size-1 icon table with two more cells, or the disc's own. 1 = the extended
 * table is in use. The new cells copy the metrics of the Triangle icon (n = 52,
 * a 12x12 button cell of both regions: same palette rows, baseline and advance)
 * and point at blank font-page texels (docs/P10.6_GLYPHS.md). */
static int gl_pt_table(int on) {
    TextStyle *st = &FONT.styles[GL_STYLE];
    int side;

    if (!on) {
        if (g_pt_orig_icons != NULL) {
            st->icons = g_pt_orig_icons;
            st->iconCount = g_pt_orig_count;
            g_pt_orig_icons = NULL;
        }
        return 0;
    }
    if (g_pt_orig_icons != NULL) {
        return 1;
    }
    if (st->icons == NULL || st->iconCount != 114) {
        if (!g_pt_warned) {
            GL_WARN("dw3-glyphs: size-1 icon table has %d icons, not 114; "
                    "page-turn prompt left as text", st->iconCount);
            g_pt_warned = 1;
        }
        return 0;
    }
    memcpy(g_pt_icons, st->icons, 114 * sizeof(Glyph));
    for (side = 0; side < DW3_PAGETURN_COUNT; side++) {
        Glyph *g = &g_pt_icons[114 + side];

        *g = st->icons[52 - 1];
        g->page = 0;
        g->u = DW3_PAGETURN_U;
        g->v = (u8)(DW3_PAGETURN_V0 + side * DW3_PAGETURN_V_STEP);
        g->w = DW3_GLYPH_W;
        g->h = DW3_GLYPH_H;
    }
    g_pt_orig_icons = st->icons;
    g_pt_orig_count = st->iconCount;
    st->icons = g_pt_icons;
    st->iconCount = 114 + DW3_PAGETURN_COUNT;
    return 1;
}

/* Draw glyph `want` (or none) into the spare cell of `side`. 1 = changed. */
static int gl_pt_set(int side, int want, int eu) {
    RECT r;
    unsigned char packed[DW3_GLYPH_PACKED];

    if (g_pt_cur[side] == want) {
        return 0;
    }
    if (want != DW3_GLYPH_NONE) {
        if (dw3_glyph_pack(g_art[want], dw3_glyph_index_map(eu), packed) != 0) {
            return 0;
        }
        r.x = (short)(GL_FONT_X + DW3_PAGETURN_U / 4);
        r.y = (short)(DW3_PAGETURN_V0 + side * DW3_PAGETURN_V_STEP);
        r.w = DW3_GLYPH_W / 4;
        r.h = DW3_GLYPH_H;
        gl_upload(&r, packed);
    }
    g_pt_cur[side] = want;
    return 1;
}

const char *dw3_port_text_override(const void *style, const char *str) {
    int side;
    int32_t mode;
    int i;

    if (g_pt_orig_icons == NULL || str == NULL ||
        style != (const void *)&FONT.styles[GL_STYLE]) {
        return str;
    }
    side = dw3_glyph_pageturn_match(str);
    if (side < 0 || g_pt_cur[side] == DW3_GLYPH_NONE) {
        return str;
    }
    mode = dw3_overlay_current_mode_file();
    for (i = 0; i < g_pt_seen_n; i++) {
        if (g_pt_seen[i].side == side && g_pt_seen[i].glyph == g_pt_cur[side] &&
            g_pt_seen[i].mode == mode) {
            return dw3_glyph_pageturn_string(side);
        }
    }
    if (g_pt_seen_n < 16) {
        const char *name = dw3_overlay_file_name(mode);

        g_pt_seen[g_pt_seen_n].side = side;
        g_pt_seen[g_pt_seen_n].glyph = g_pt_cur[side];
        g_pt_seen[g_pt_seen_n].mode = mode;
        g_pt_seen_n++;
        GL_LOG("dw3-glyphs: page-turn prompt %s drawn as glyph %s "
               "(scene=%s 0x%X)", side ? "R1" : "L1",
               dw3_glyph_name(g_pt_cur[side]), name != NULL ? name : "?",
               (unsigned)mode);
    }
    return dw3_glyph_pageturn_string(side);
}

static void gl_pt_refresh(int want_on, const unsigned char *map,
                          const char *why) {
    int plan[DW3_PAGETURN_COUNT];
    int changed = 0;
    int side;

    dw3_glyph_pageturn_plan(map, plan);
    if (want_on && gl_pt_table(1)) {
        for (side = 0; side < DW3_PAGETURN_COUNT; side++) {
            changed += gl_pt_set(side, plan[side], GL_EU);
        }
    } else {
        gl_pt_table(0);
        for (side = 0; side < DW3_PAGETURN_COUNT; side++) {
            changed += gl_pt_set(side, DW3_GLYPH_NONE, GL_EU);
        }
    }
    if (changed) {
        g_pt_seen_n = 0; /* the next use of each prompt is logged again */
    }
    if (want_on && (changed || (why != NULL && strcmp(why, "font load") == 0))) {
        GL_LOG("dw3-glyphs: page-turn L1=%s R1=%s (cells n=%d n=%d) [%s]",
               g_pt_cur[0] == DW3_GLYPH_NONE ? "text" : dw3_glyph_name(g_pt_cur[0]),
               g_pt_cur[1] == DW3_GLYPH_NONE ? "text" : dw3_glyph_name(g_pt_cur[1]),
               DW3_PAGETURN_N0, DW3_PAGETURN_N0 + 1, why != NULL ? why : "?");
    } else if (!want_on && changed) {
        GL_LOG("dw3-glyphs: page-turn off [%s]", why != NULL ? why : "?");
    }
}

const unsigned char *dw3_glyphs_hud_art(int glyph) {
    if (glyph < 0 || glyph >= DW3_GLYPH_COUNT) {
        return NULL;
    }
    gl_art_load();
    return g_art[glyph];
}

void dw3_glyphs_refresh(const char *why) {
    const int want_on =
        dw3_port_options_level(DW3_PORT_OPT_SWITCH_GLYPHS) != 0;
    const unsigned char *map = dw3_button_map_current();
    Dw3GlyphCell cells[DW3_GLYPH_CELLS];
    char text[96];
    int changed = 0;
    int patched = 0;
    int i;

    if (!g_font_loaded) {
        return;
    }
    dw3_glyph_plan(map, GL_EU, cells);
    if (want_on) {
        gl_art_load();
    }
    gl_pt_refresh(want_on, map, why);
    for (i = 0; i < DW3_GLYPH_CELLS; i++) {
        changed += gl_set_cell(i, cells[i].n,
                               want_on ? cells[i].glyph : DW3_GLYPH_NONE,
                               GL_EU);
        patched += g_cur[i] != DW3_GLYPH_NONE;
    }
    if (!changed && !(why != NULL && strcmp(why, "font load") == 0 &&
                      (want_on || !g_logged_off))) {
        return;
    }
    if (!want_on) {
        g_logged_off = 1;
    }
    dw3_glyph_plan_text(cells, text, sizeof(text));
    GL_LOG("dw3-glyphs: %s, map=%s, patched %d cells (%s, %s) [%s]",
           want_on ? "on" : "off", dw3_button_layout_name(dw3_button_map_layout()),
           patched, GL_EU ? "eu" : "us", text, why != NULL ? why : "?");
}

/* FONT.load wrapper: the font is loaded, its cells are the disc's again, so the
 * backup and the state start over, then the option decides. */
static void gl_font_load(void) {
    int i;

    g_orig_load();
    DrawSync(0);
    g_loads++;
    g_font_loaded = 1;
    for (i = 0; i < DW3_GLYPH_CELLS; i++) {
        g_cur[i] = DW3_GLYPH_NONE;
        g_have_orig[i] = 0;
    }
    for (i = 0; i < DW3_PAGETURN_COUNT; i++) {
        g_pt_cur[i] = DW3_GLYPH_NONE; /* the spare cells are blank again */
    }
    dw3_glyphs_refresh("font load");
}

void dw3_glyphs_init(void) {
    if (FONT.load == gl_font_load) {
        return;
    }
    g_orig_load = FONT.load;
    FONT.load = gl_font_load;
}

/* --- census ---------------------------------------------------------------- */

enum { GL_CENSUS_MAX = 4096 };

typedef struct GlCensusKey {
    const void *data;
    int pos;
    int n;
    int style;
} GlCensusKey;

static GlCensusKey g_census[GL_CENSUS_MAX];
static int g_census_n;
static int g_census_flag = -1;
static int g_census_total;

void dw3_port_icon_census(int n, int style, const unsigned char *data, int pos,
                          int sjis) {
    char ctx[96];
    int32_t mode;
    int32_t sub;
    const char *name;
    int i;
    int lo;
    int k = 0;

    if (g_census_flag < 0) {
        const char *e = getenv("DW3_ICON_CENSUS");

        g_census_flag = e != NULL && e[0] == '1';
    }
    if (!g_census_flag || data == NULL) {
        return;
    }
    g_census_total++;
    for (i = 0; i < g_census_n; i++) {
        if (g_census[i].data == data && g_census[i].pos == pos &&
            g_census[i].n == n && g_census[i].style == style) {
            return;
        }
    }
    if (g_census_n < GL_CENSUS_MAX) {
        g_census[g_census_n].data = data;
        g_census[g_census_n].pos = pos;
        g_census[g_census_n].n = n;
        g_census[g_census_n].style = style;
        g_census_n++;
    }
    /* Bytes around the icon: up to 8 before, then the icon's two bytes and up to
     * 8 after, to the end of the string. Hex only, so a log never carries the
     * disc's text as text. */
    lo = pos > 8 ? pos - 8 : 0;
    for (i = lo; i < pos && k < (int)sizeof(ctx) - 4; i++) {
        k += snprintf(ctx + k, sizeof(ctx) - (unsigned)k, "%02X", data[i]);
    }
    k += snprintf(ctx + k, sizeof(ctx) - (unsigned)k, "|%02X%02X|",
                  data[pos], data[pos + 1]);
    for (i = pos + 2; i < pos + 10 && data[i] != 0 && k < (int)sizeof(ctx) - 4;
         i++) {
        k += snprintf(ctx + k, sizeof(ctx) - (unsigned)k, "%02X", data[i]);
    }
    mode = dw3_overlay_current_mode_file();
    sub = dw3_overlay_current_sub_file();
    name = dw3_overlay_file_name(mode);
    GL_LOG("dw3-icon-census: n=%d style=%d %s scene=%s(0x%X) sub=0x%X "
           "string=%p+%d ctx=%s (distinct %d, draws %d)",
           n, style, sjis ? "sjis" : "01n", name != NULL ? name : "?",
           (unsigned)mode, (unsigned)sub, (const void *)data, pos, ctx,
           g_census_n, g_census_total);
}
#endif /* !DW3_GLYPHS_TEST */
