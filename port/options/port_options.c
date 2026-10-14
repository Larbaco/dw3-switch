/* P9.2 port options shell — stays on MODE_TITLE; never sets menu choice. */
#include "port_options.h"
#include "port_turbo.h"

#include <stdio.h>
#include <string.h>

#ifndef DW3_PORT_OPTIONS_TEST
#include "port_text.h"
#include <libgpu.h>
#include "engine/pad.h"
/* Psyz_VideoAlloc* capture is PC/PSY-Z only — compile out on Switch. */
#if defined(DW3_PC_HOST)
#include <psyz/video.h>
#include <stdlib.h>
#endif
#endif

enum {
    DW3_PORT_OPT_COUNT = 3,
    DW3_PORT_OPT_WIDESCREEN = 0,
    DW3_PORT_OPT_FAST_FORWARD = 1,
    DW3_PORT_OPT_RESTART = 2
};

typedef struct Dw3PortOptStub {
    const char *label;
    int value; /* 0/1, or 0/2..4 for Fast-forward; 0 is stock behaviour */
} Dw3PortOptStub;

static int g_open;
static int g_cursor;
static int g_open_frames;
static int g_logged_open;
static Dw3PortOptStub g_stubs[DW3_PORT_OPT_COUNT] = {
    {"Widescreen", 0},
    /* P6.4: the speed R3 toggles to in field and battle. OFF disables R3;
     * the game itself still starts at normal speed. */
    {"Fast-forward", DW3_TURBO_SPEED_DEFAULT},
    {"Restart hotkey", 0},
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

int dw3_port_options_count(void) {
    return DW3_PORT_OPT_COUNT;
}

const char *dw3_port_options_label(int index) {
    if (index < 0 || index >= DW3_PORT_OPT_COUNT) {
        return NULL;
    }
    return g_stubs[index].label;
}

int dw3_port_options_value(int index) {
    if (index < 0 || index >= DW3_PORT_OPT_COUNT) {
        return 0;
    }
    return g_stubs[index].value != 0;
}

void dw3_port_options_set(int index, int value) {
    if (index < 0 || index >= DW3_PORT_OPT_COUNT) {
        return;
    }
    g_stubs[index].value = value != 0;
}

int dw3_port_options_level(int index) {
    if (index < 0 || index >= DW3_PORT_OPT_COUNT) {
        return 0;
    }
    return g_stubs[index].value;
}

void dw3_port_options_set_level(int index, int level) {
    if (index < 0 || index >= DW3_PORT_OPT_COUNT) {
        return;
    }
    g_stubs[index].value = level;
}

/* Confirm on a row: ON/OFF, or OFF -> 2x -> 3x -> 4x -> OFF for Fast-forward. */
static void po_toggle(int index) {
    int v;

    if (index == DW3_PORT_OPT_FAST_FORWARD) {
        v = g_stubs[index].value;
        v = v < DW3_TURBO_SPEED_MIN ? DW3_TURBO_SPEED_MIN : v + 1;
        if (v > DW3_TURBO_SPEED_MAX) {
            v = 0;
        }
        g_stubs[index].value = v;
        if (v) {
            fprintf(stderr, "dw3-port-options: fast-forward %dx (R3)\n", v);
        } else {
            fprintf(stderr, "dw3-port-options: fast-forward off\n");
        }
        fflush(stderr);
        return;
    }
    v = g_stubs[index].value ? 0 : 1;
    g_stubs[index].value = v;
    fprintf(stderr, "dw3-port-options: %s %s\n",
            index == DW3_PORT_OPT_RESTART ? "restart hotkey" : "widescreen",
            v ? "enabled" : "disabled");
    fflush(stderr);
}

int dw3_port_options_restart_enabled(void) {
    return g_stubs[DW3_PORT_OPT_RESTART].value != 0;
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
    /* Close is Triangle only — Select is the open chord on the title menu. */
    if (po_pressed(PO_BIT_TRIANGLE)) {
        dw3_port_options_close();
        return;
    }
    if (po_pressed(PO_BIT_UP)) {
        if (g_cursor > 0) {
            g_cursor--;
        }
    }
    if (po_pressed(PO_BIT_DOWN)) {
        if (g_cursor < DW3_PORT_OPT_COUNT - 1) {
            g_cursor++;
        }
    }
    if (po_pressed(PO_BIT_CROSS) || po_pressed(PO_BIT_CIRCLE)) {
        po_toggle(g_cursor);
    }
#else
    if (po_pressed(PAD_TRIANGLE)) {
        dw3_port_options_close();
        return;
    }
    if (po_pressed(PAD_UP) || PAD_REPEATED(PAD_UP)) {
        if (g_cursor > 0) {
            g_cursor--;
        }
    }
    if (po_pressed(PAD_DOWN) || PAD_REPEATED(PAD_DOWN)) {
        if (g_cursor < DW3_PORT_OPT_COUNT - 1) {
            g_cursor++;
        }
    }
    if (po_pressed(PAD_CROSS) || po_pressed(PAD_CIRCLE)) {
        po_toggle(g_cursor);
    }
#endif
}

#if !defined(DW3_PORT_OPTIONS_TEST) && defined(DW3_PC_HOST)
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
    char lines_buf[DW3_PORT_OPT_COUNT][48];
    char back_line[48];
    const char *lines[7];
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
    for (i = 0; i < DW3_PORT_OPT_COUNT && n < 6; i++) {
        char *buf = lines_buf[i];
        if (i == DW3_PORT_OPT_FAST_FORWARD && g_stubs[i].value) {
            snprintf(buf, sizeof(lines_buf[i]), "%c %s: %dx (R3)",
                     (i == g_cursor) ? '>' : ' ', g_stubs[i].label,
                     g_stubs[i].value);
        } else {
            snprintf(buf, sizeof(lines_buf[i]), "%c %s: %s",
                     (i == g_cursor) ? '>' : ' ', g_stubs[i].label,
                     g_stubs[i].value ? "ON" : "OFF");
        }
        lines[n++] = buf;
    }
    snprintf(back_line, sizeof(back_line), "Triangle (S): Back");
    lines[n++] = back_line;

    dw3_port_text_draw_panel((int)ox + 32, (int)oy + 32, 256, 88, lines, n);
#if defined(DW3_PC_HOST)
    dw3_port_options_maybe_fb_dump();
#endif
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
