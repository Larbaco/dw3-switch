/* P6.30 baked glyph atlas: coverage bitmaps of Latin-1 and a few symbols in
 * three faces (tools/generate_help_assets.py, port/pages/help_atlas.inc), laid
 * out at run time by the page engine (port_pages.h). The game needs no font
 * file: the bitmaps are in the image's read-only data. */
#ifndef DW3_PORT_ATLAS_H
#define DW3_PORT_ATLAS_H

#ifdef __cplusplus
extern "C" {
#endif

enum {
    DW3_ATLAS_BODY = 0,   /* 24 px */
    DW3_ATLAS_BOLD = 1,   /* 24 px bold */
    DW3_ATLAS_TITLE = 2,  /* 29 px condensed bold */
    DW3_ATLAS_FACES = 3
};

typedef struct Dw3AtlasGlyph {
    unsigned short cp;
    unsigned char adv;        /* advance in pixels */
    unsigned char w, h;       /* coverage bitmap size (0 for a blank) */
    signed char left;         /* bitmap x from the pen */
    signed char top;          /* bitmap top above the baseline */
    unsigned int off;         /* offset in the coverage table */
} Dw3AtlasGlyph;

/* Glyph of a code point (U+FFFD's when the face has none), never NULL. */
const Dw3AtlasGlyph *dw3_atlas_glyph(int face, unsigned cp);
/* 1 when the face has the code point itself. */
int dw3_atlas_has(int face, unsigned cp);
/* w * h bytes of 8-bit coverage. */
const unsigned char *dw3_atlas_coverage(const Dw3AtlasGlyph *g);
int dw3_atlas_ascent(int face);
int dw3_atlas_height(int face);
/* Width in pixels of UTF-8 text of len bytes. */
int dw3_atlas_measure(int face, const char *s, int len);
/* Bytes of the coverage table and glyph count (the log line). */
unsigned dw3_atlas_bytes(void);
int dw3_atlas_glyph_count(void);

#ifdef __cplusplus
}
#endif

#endif
