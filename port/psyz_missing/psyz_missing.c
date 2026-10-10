/*
 * Minimal host stubs for Psy-Q APIs that PSY-Z declares but does not yet
 * implement (or omits from the build). Enough for Phase 2 title bring-up;
 * movies stay skipped via MODE_TITLE.
 */
#include <libgs.h>
#include <libgte.h>
#include <libgpu.h>
#include <libetc.h>
#include <libsnd.h>

/* --- libgs --- */

MATRIX GsWSMATRIX;
MATRIX GsLIGHTWSMATRIX;

void GsInit3D(void) {}

void GsSetProjection(long h) { (void)h; }

void GsSetLightMode(int mode) { (void)mode; }

/* PSY-Z has no host GsSetFlatLight; accept Psy-Q GsF_LIGHT (port/include). */
int Dw3_GsSetFlatLight(int id, GsF_LIGHT *light) {
    (void)id;
    (void)light;
    return 0;
}

void GsSetRefView2(GsRVIEW2 *pv) { (void)pv; }

/* Not declared in PSY-Z libgs.h yet. */
void GsGetTimInfo(unsigned long *addr, GsIMAGE *img);

void GsGetTimInfo(unsigned long *addr, GsIMAGE *img) {
    unsigned long *p;
    unsigned long flag;
    unsigned long bsize;
    unsigned short *u;

    if (addr == NULL || img == NULL) {
        return;
    }
    img->x = img->y = img->w = img->h = 0;
    img->pixel = 0;
    img->clut = 0;
    p = addr;
    flag = *p++;
    if (flag & 8u) {
        bsize = *p;
        u = (unsigned short *)p;
        /* CLUT: size(2) + x,y,w,h (8) = 12 bytes header, then entries */
        img->clut = u + 6;
        p = (unsigned long *)((unsigned char *)p + bsize);
    }
    bsize = *p++;
    u = (unsigned short *)p;
    img->x = (short)u[0];
    img->y = (short)u[1];
    img->w = (short)u[2];
    img->h = (short)u[3];
    img->pixel = u + 4;
    (void)bsize;
    (void)flag;
}

/* --- libgpu (draw interrupt helpers unused on host GPU path) --- */

u_long *BreakDraw(void) { return 0; }

void ContinueDraw(u_long *inst_ot, u_long *cont_ot) {
    (void)inst_ot;
    (void)cont_ot;
}

int IsIdleGPU(int maxcount) {
    (void)maxcount;
    return 0;
}

/* --- libgte --- */

MATRIX *RotMatrixYXZ_gte(SVECTOR *r, MATRIX *m) { return RotMatrixYXZ(r, m); }

MATRIX *RotMatrixZYX_gte(SVECTOR *r, MATRIX *m) { return RotMatrixZYX(r, m); }

/* Psy-Q signature: SVECTOR out (not VECTOR). */
SVECTOR *ApplyMatrixSV(MATRIX *m, SVECTOR *v0, SVECTOR *v1) {
    VECTOR tmp;

    if (v0 == NULL || v1 == NULL) {
        return v1;
    }
    ApplyMatrix(m, v0, &tmp);
    v1->vx = (short)tmp.vx;
    v1->vy = (short)tmp.vy;
    v1->vz = (short)tmp.vz;
    return v1;
}

/* libpad: see psyz_pad_host.c (linked into dw3-pc so PadGetState wins). */

/* libmcrd: see port/memcard/memcard_libmcrd.c (MemCard* → .mcr via _card_*). */

/* libsnd SsSepSetDecrescendo — implemented in vendor/psyz midiread_host.c */

/* libpress DecDCTvlc* — see port/movies/dct_vlc.c (bsdec). */
