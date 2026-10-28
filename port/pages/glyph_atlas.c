/* P6.30 glyph atlas lookups (port_atlas.h). */
#include "port_atlas.h"
#include "port_pages.h"

#include <stddef.h>

#include "help_atlas.inc"

typedef struct AtlasFace {
    const Dw3AtlasGlyph *g;
    int n;
    /* Latin-1 direct index, built on first use. */
    short latin1[256];
    int built;
} AtlasFace;

static AtlasFace g_face[DW3_ATLAS_FACES];

static void build(int face) {
    AtlasFace *f = &g_face[face];
    int i;

    if (f->built) {
        return;
    }
    f->g = face == 0 ? atlas_g0 : face == 1 ? atlas_g1 : atlas_g2;
    f->n = face == 0 ? (int)(sizeof(atlas_g0) / sizeof(atlas_g0[0]))
           : face == 1 ? (int)(sizeof(atlas_g1) / sizeof(atlas_g1[0]))
                       : (int)(sizeof(atlas_g2) / sizeof(atlas_g2[0]));
    for (i = 0; i < 256; i++) {
        f->latin1[i] = -1;
    }
    for (i = 0; i < f->n; i++) {
        if (f->g[i].cp < 256) {
            f->latin1[f->g[i].cp] = (short)i;
        }
    }
    f->built = 1;
}

static int find(int face, unsigned cp) {
    AtlasFace *f;
    int lo;
    int hi;

    if (face < 0 || face >= DW3_ATLAS_FACES) {
        face = 0;
    }
    build(face);
    f = &g_face[face];
    if (cp < 256) {
        return f->latin1[cp];
    }
    /* sorted by code point */
    lo = 0;
    hi = f->n - 1;
    while (lo <= hi) {
        int mid = (lo + hi) / 2;

        if (f->g[mid].cp == cp) {
            return mid;
        }
        if (f->g[mid].cp < cp) {
            lo = mid + 1;
        } else {
            hi = mid - 1;
        }
    }
    return -1;
}

int dw3_atlas_has(int face, unsigned cp) {
    return find(face, cp) >= 0;
}

const Dw3AtlasGlyph *dw3_atlas_glyph(int face, unsigned cp) {
    int i = find(face, cp);

    if (face < 0 || face >= DW3_ATLAS_FACES) {
        face = 0;
    }
    if (i < 0) {
        i = find(face, 0xFFFD);
    }
    return &g_face[face].g[i < 0 ? 0 : i];
}

const unsigned char *dw3_atlas_coverage(const Dw3AtlasGlyph *g) {
    return atlas_cov + g->off;
}

int dw3_atlas_ascent(int face) {
    return atlas_metrics[face >= 0 && face < DW3_ATLAS_FACES ? face : 0][1];
}

int dw3_atlas_height(int face) {
    return atlas_metrics[face >= 0 && face < DW3_ATLAS_FACES ? face : 0][0];
}

int dw3_atlas_measure(int face, const char *s, int len) {
    int w = 0;
    int i = 0;

    while (i < len) {
        unsigned cp;
        int u = dw3_pages_utf8(s + i, len - i, &cp);

        if (u <= 0) {
            break;
        }
        w += dw3_atlas_glyph(face, cp)->adv;
        i += u;
    }
    return w;
}

unsigned dw3_atlas_bytes(void) {
    return (unsigned)sizeof(atlas_cov);
}

int dw3_atlas_glyph_count(void) {
    return (int)(sizeof(atlas_g0) / sizeof(atlas_g0[0]) +
                 sizeof(atlas_g1) / sizeof(atlas_g1[0]) +
                 sizeof(atlas_g2) / sizeof(atlas_g2[0]));
}
