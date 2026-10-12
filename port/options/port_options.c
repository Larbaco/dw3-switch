/* P9.2 port options shell — stays on MODE_TITLE; never sets menu choice. */
#include "port_options.h"

#include <stdio.h>
#include <string.h>

#ifndef DW3_PORT_OPTIONS_TEST
#include "port_text.h"
#include <libgpu.h>
#include <psyz/video.h>
#include <stdlib.h>
#include "engine/pad.h"
#endif

enum { DW3_PORT_OPT_STUBS = 2 };

typedef struct Dw3PortOptStub {
    const char *label;
    int value; /* 0/1; document-only / safe no-op until P9.3 */
} Dw3PortOptStub;

static int g_open;
static int g_cursor;
static int g_open_frames;
static int g_logged_open;
static Dw3PortOptStub g_stubs[DW3_PORT_OPT_STUBS] = {
    {"Widescreen (stub)", 0},
    {"Fast-forward (stub)", 0},
};

#ifdef DW3_PORT_OPTIONS_TEST
/* Button bits match engine/pad.h. */
enum {
    PO_BIT_SELECT = 0,
    PO_BIT_UP = 4,
    PO_BIT_DOWN = 6,
    PO_BIT_CIRCLE = 12,
    PO_BIT_CROSS = 13,
    PO_BIT_TRIANGLE = 14
};
static unsigned g_test_pressed;
void dw3_port_options_test_set_pressed(unsigned mask) {
    g_test_pressed = mask;
}
static int po_pressed(int bit) {
    return (int)((g_test_pressed >> bit) & 1u);
}
#else
static int po_pressed(int bit) {
    return PAD_PRESSED(bit);
}
#endif

void dw3_port_options_open(void) {
    if (g_open) {
        return;
    }
    g_open = 1;
    g_cursor = 0;
    g_open_frames = 0;
    g_logged_open = 0;
    fprintf(stderr, "dw3-port-options: open (stay MODE_TITLE)\n");
    fflush(stderr);
}

void dw3_port_options_close(void) {
    if (!g_open) {
        return;
    }
    g_open = 0;
    g_open_frames = 0;
    fprintf(stderr, "dw3-port-options: close (resume runOptions)\n");
    fflush(stderr);
}

int dw3_port_options_is_open(void) {
    return g_open != 0;
}

int dw3_port_options_stub_count(void) {
    return DW3_PORT_OPT_STUBS;
}

const char *dw3_port_options_stub_label(int index) {
    if (index < 0 || index >= DW3_PORT_OPT_STUBS) {
        return NULL;
    }
    return g_stubs[index].label;
}

int dw3_port_options_stub_value(int index) {
    if (index < 0 || index >= DW3_PORT_OPT_STUBS) {
        return 0;
    }
    return g_stubs[index].value != 0;
}

void dw3_port_options_stub_set(int index, int value) {
    if (index < 0 || index >= DW3_PORT_OPT_STUBS) {
        return;
    }
    g_stubs[index].value = value != 0;
}

void dw3_port_options_tick(void) {
    if (!g_open) {
        return;
    }
    g_open_frames++;
    if (!g_logged_open && g_open_frames == 1) {
        g_logged_open = 1;
        fprintf(stderr, "dw3-port-options: shell tick (Select opened)\n");
        fflush(stderr);
    }

#ifdef DW3_PORT_OPTIONS_TEST
    if (po_pressed(PO_BIT_TRIANGLE) || po_pressed(PO_BIT_SELECT)) {
        dw3_port_options_close();
        return;
    }
    if (po_pressed(PO_BIT_UP)) {
        if (g_cursor > 0) {
            g_cursor--;
        }
    }
    if (po_pressed(PO_BIT_DOWN)) {
        if (g_cursor < DW3_PORT_OPT_STUBS - 1) {
            g_cursor++;
        }
    }
    if (po_pressed(PO_BIT_CROSS) || po_pressed(PO_BIT_CIRCLE)) {
        int v = g_stubs[g_cursor].value ? 0 : 1;
        g_stubs[g_cursor].value = v;
        fprintf(stderr, "dw3-port-options: stub[%d] %s -> %d (no-op)\n",
                g_cursor, g_stubs[g_cursor].label, v);
        fflush(stderr);
    }
#else
    if (po_pressed(PAD_TRIANGLE) || po_pressed(PAD_SELECT)) {
        dw3_port_options_close();
        return;
    }
    if (po_pressed(PAD_UP) || PAD_REPEATED(PAD_UP)) {
        if (g_cursor > 0) {
            g_cursor--;
        }
    }
    if (po_pressed(PAD_DOWN) || PAD_REPEATED(PAD_DOWN)) {
        if (g_cursor < DW3_PORT_OPT_STUBS - 1) {
            g_cursor++;
        }
    }
    if (po_pressed(PAD_CROSS) || po_pressed(PAD_CIRCLE)) {
        int v = g_stubs[g_cursor].value ? 0 : 1;
        g_stubs[g_cursor].value = v;
        fprintf(stderr, "dw3-port-options: stub[%d] %s -> %d (no-op)\n",
                g_cursor, g_stubs[g_cursor].label, v);
        fflush(stderr);
    }
#endif
}

#ifndef DW3_PORT_OPTIONS_TEST
static void dw3_port_options_maybe_fb_dump(void) {
    const char *path;
    unsigned char *rgb;
    int w = 0;
    int h = 0;
    FILE *fp;
    size_t nbytes;
    static int dumped;

    if (dumped || g_open_frames < 45) {
        return;
    }
    path = getenv("DW3_PORT_OPTIONS_FB_DUMP");
    if (path == NULL || path[0] == '\0') {
        return;
    }
    rgb = Psyz_VideoAllocCapturedFrame(&w, &h);
    if (rgb == NULL || w <= 0 || h <= 0) {
        rgb = Psyz_VideoAllocVramDump(&w, &h);
    }
    if (rgb == NULL || w <= 0 || h <= 0) {
        fprintf(stderr, "dw3-port-options: FB dump failed\n");
        fflush(stderr);
        return;
    }
    nbytes = (size_t)w * (size_t)h * 3u;
    fp = fopen(path, "wb");
    if (fp == NULL) {
        fprintf(stderr, "dw3-port-options: cannot write %s\n", path);
        fflush(stderr);
        free(rgb);
        return;
    }
    fprintf(fp, "P6\n%d %d\n255\n", w, h);
    fwrite(rgb, 1, nbytes, fp);
    fclose(fp);
    free(rgb);
    dumped = 1;
    fprintf(stderr, "dw3-port-options: TTB FB dump %dx%d -> %s\n", w, h,
            path);
    fflush(stderr);
    if (getenv("DW3_PORT_OPTIONS_QUIT_AFTER_DUMP") != NULL) {
        exit(0);
    }
}
#endif

void dw3_port_options_draw(void) {
#ifdef DW3_PORT_OPTIONS_TEST
    (void)g_cursor;
#else
    short ox = 0;
    short oy = 0;
    DISPENV env;
    char line1[48];
    char line2[48];
    char line3[48];
    const char *lines[6];
    int n = 0;
    int i;

    if (!g_open) {
        return;
    }
    GetDispEnv(&env);
    ox = env.disp.x;
    oy = env.disp.y;

    lines[n++] = "Port options";
    lines[n++] = "------------";
    for (i = 0; i < DW3_PORT_OPT_STUBS && n < 5; i++) {
        char *buf = (i == 0) ? line1 : line2;
        snprintf(buf, 48, "%c %s: %s", (i == g_cursor) ? '>' : ' ',
                 g_stubs[i].label, g_stubs[i].value ? "ON" : "OFF");
        lines[n++] = buf;
    }
    snprintf(line3, sizeof(line3), "Triangle/Backspace: Back");
    lines[n++] = line3;

    dw3_port_text_draw_panel((int)ox + 32, (int)oy + 40, 256, 72, lines, n);
    dw3_port_options_maybe_fb_dump();
#endif
}

void dw3_port_options_draw_hint(void) {
#ifdef DW3_PORT_OPTIONS_TEST
    return;
#else
    short ox = 0;
    short oy = 0;
    DISPENV env;
    const char *lines[1];

    if (g_open) {
        return;
    }
    GetDispEnv(&env);
    ox = env.disp.x;
    oy = env.disp.y;
    lines[0] = "Select/Backspace: Port options";
    dw3_port_text_draw_panel((int)ox + 40, (int)oy + 210, 240, 20, lines, 1);
#endif
}
