/* Host check for the P6.1 side gutters of PSY-Z's CPU rasterizer
 * (sdl3_software.c), the renderer the Switch host draws with. */
#include <psyz.h>
#include "../internal.h"
#include "rasterizer.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* The rasterizer asks the video layer about dithering; nothing else. */
PsyzDitherMode Psyz_VideoGetDitheringMode(void) { return PSYZ_DITHER_OFF; }
void psyz_log(unsigned int level, const char *file, unsigned int line,
              const char *func, const char *fmt, ...) {
    (void)level;
    (void)file;
    (void)line;
    (void)func;
    (void)fmt;
}

#define RED 0x001F
#define GREEN 0x03E0
#define BLUE 0x7C00
#define G 53

static int failures;

static void check(int cond, const char *what) {
    if (!cond) {
        fprintf(stderr, "FAIL: %s\n", what);
        failures++;
    }
}

static unsigned xy(int x, int y) {
    return ((unsigned)(y & 0xFFFF) << 16) | (unsigned)(x & 0xFFFF);
}

static void tile(int x, int y, int w, int h, unsigned bgr) {
    u_long p[3];

    p[0] = (0x60u << 24) | bgr;
    p[1] = xy(x, y);
    p[2] = xy(w, h);
    software_rasterizer.push_prim(p, 3);
}

static void quad(int x0, int y0, int x1, int y1, unsigned bgr) {
    u_long p[5];

    p[0] = (0x28u << 24) | bgr;
    p[1] = xy(x0, y0);
    p[2] = xy(x1, y0);
    p[3] = xy(x0, y1);
    p[4] = xy(x1, y1);
    software_rasterizer.push_prim(p, 5);
}

static u16 plane(int side, int x, int y) {
    return SoftwareRasterizer_GetGutterPlane(side)[y * PSYZ_GUTTER_MAX + x];
}

static u16 vram(int x, int y) {
    return SoftwareRasterizer_GetVram()[y * VRAM_W + x];
}

static void area(int x0, int y0, int x1, int y1) {
    software_rasterizer.area_start(x0, y0);
    software_rasterizer.area_end(x1, y1);
}

int main(void) {
    PS1_RECT rect;

    SoftwareRasterizer_Reset();
    software_rasterizer.offset(0, 0);
    area(0, 0, 319, 239);
    SoftwareRasterizer_SetGutter(G, 0, 320, 240);

    /* A background clear of the drawing area covers both gutters. */
    tile(0, 0, 320, 240, 0xF8);
    check(vram(0, 0) == RED && vram(319, 239) == RED, "clear fills VRAM");
    check(vram(320, 0) == 0, "clear stays inside the drawing area");
    check(plane(0, 0, 0) == RED && plane(0, G - 1, 239) == RED,
          "clear fills the left gutter");
    check(plane(1, 0, 0) == RED && plane(1, G - 1, 239) == RED,
          "clear fills the right gutter");
    check(plane(0, G, 0) == 0 && plane(0, 0, 240) == 0,
          "clear stays inside the gutter rows and columns");

    /* Geometry past the edges lands in the gutters, past them nowhere. */
    quad(-40, 10, 360, 20, 0xF800);
    check(vram(0, 15) == GREEN && vram(319, 15) == GREEN, "quad in VRAM");
    check(vram(320, 15) == 0 && vram(359, 15) == 0, "quad not past VRAM area");
    check(plane(0, G - 40, 15) == GREEN && plane(0, G - 41, 15) == RED,
          "quad reaches 40 px into the left gutter");
    check(plane(1, 0, 15) == GREEN && plane(1, 39, 15) == GREEN &&
              plane(1, 40, 15) == RED,
          "quad reaches 40 px into the right gutter");

    /* A partial drawing area takes no gutters. */
    area(0, 0, 159, 239);
    quad(-40, 30, 360, 40, 0xF800);
    check(plane(0, G - 1, 35) == RED && plane(1, 0, 35) == RED,
          "narrow area leaves the gutters alone");
    check(vram(159, 35) == GREEN && vram(160, 35) == RED,
          "narrow area clips as before");

    /* A display-wide band of rows (the area banner closing from the top and
       the bottom) takes the gutters in those rows only. */
    area(0, 100, 319, 139);
    tile(0, 100, 320, 40, 0xF800);
    check(plane(0, 0, 100) == GREEN && plane(1, G - 1, 139) == GREEN,
          "band clear fills its gutter rows");
    check(plane(0, 0, 99) == RED && plane(1, G - 1, 140) == RED,
          "band clear stays inside its rows");
    quad(-40, 90, 360, 150, 0xF80000);
    check(plane(0, G - 1, 99) == RED && plane(0, G - 1, 120) == BLUE,
          "band geometry is clipped to its rows in the gutters");

    /* An empty area (end before start, a 0x0 layer clip) draws nothing. */
    area(160, 120, 159, 119);
    tile(0, 0, 320, 240, 0xF800);
    check(vram(0, 0) == RED && vram(160, 120) == BLUE &&
              plane(0, 0, 0) == RED,
          "empty area draws nothing");

    /* The second buffer: offset and area 256 rows down. */
    software_rasterizer.offset(0, 256);
    area(0, 256, 319, 495);
    quad(-10, 0, 330, 5, 0xF800);
    check(plane(0, G - 1, 258) == GREEN && plane(1, 9, 258) == GREEN,
          "second buffer draws into its own gutter rows");

    /* A display-wide ClearImage clears the gutter rows too. */
    rect.x = 0;
    rect.y = 256;
    rect.w = 320;
    rect.h = 240;
    software_rasterizer.clear_image(&rect, 0, 0, 0);
    check(plane(0, G - 1, 258) == 0 && plane(1, 9, 258) == 0,
          "ClearImage clears the gutters");
    check(plane(0, 0, 0) == RED, "ClearImage keeps the other buffer");

    SoftwareRasterizer_ClearGutterRows(0, 240);
    check(plane(0, G - 40, 15) == 0 && plane(1, 0, 15) == 0,
          "ClearGutterRows blacks the rows out");

    /* CopyGutters: after a MoveImage of the display to x 640, the gutter
     * columns land beside the copy, as one image 2 * G + 320 wide, also when
     * the drawing that made them is still deferred. */
    for (int defer = 0; defer < 2; defer++) {
        SoftwareRasterizer_SetDefer(defer);
        software_rasterizer.offset(0, 0);
        area(0, 0, 319, 239);
        tile(0, 0, 320, 240, 0xF8);
        quad(-40, 100, 360, 110, 0xF800);
        rect.x = 0;
        rect.y = 0;
        rect.w = 320;
        rect.h = 240;
        software_rasterizer.move_image(&rect, 640, 0);
        check(SoftwareRasterizer_CopyGutters(0, 240, 640, 0) == G,
              "CopyGutters returns the gutter width");
        check(vram(640 - G, 0) == RED && vram(639, 239) == RED &&
                  vram(960, 0) == RED && vram(960 + G - 1, 239) == RED,
              "CopyGutters copies both strips of the clear");
        check(vram(640 - 40, 105) == GREEN && vram(640 - 41, 105) == RED &&
                  vram(960 + 39, 105) == GREEN && vram(960 + 40, 105) == RED,
              "CopyGutters copies the geometry past the edges");
        check(vram(640, 105) == GREEN && vram(959, 0) == RED,
              "CopyGutters leaves the MoveImage copy alone");
        check(vram(640 - G - 1, 0) == 0 && vram(960 + G, 0) == 0,
              "CopyGutters writes nothing past the strips");
        check(SoftwareRasterizer_CopyGutters(0, 240, 10, 0) == -1 &&
                  SoftwareRasterizer_CopyGutters(0, 240, 1000, 0) == -1,
              "CopyGutters refuses strips outside VRAM");
        SoftwareRasterizer_SetDefer(0);
        rect.x = 640 - G;
        rect.y = 0;
        rect.w = 320 + 2 * G;
        rect.h = 240;
        software_rasterizer.clear_image(&rect, 0, 0, 0);
    }

    /* Off: the gutters are dropped and nothing is drawn past the area. */
    software_rasterizer.offset(0, 0);
    area(0, 0, 319, 239);
    tile(0, 0, 320, 240, 0xF8);
    SoftwareRasterizer_SetGutter(0, 0, 320, 240);
    check(plane(0, 0, 0) == 0, "turning gutters off clears them");
    quad(-40, 50, 360, 60, 0xF800);
    check(plane(0, G - 1, 55) == 0 && plane(1, 0, 55) == 0,
          "no gutter writes while off");
    tile(0, 0, 320, 240, 0xF8);
    check(plane(0, 0, 0) == 0 && vram(320, 0) == 0,
          "clear does not grow while off");
    check(SoftwareRasterizer_CopyGutters(0, 240, 640, 0) == 0,
          "CopyGutters is a no-op while off");

    if (failures) {
        return 1;
    }
    printf("OK: software rasterizer side gutters\n");
    return 0;
}
