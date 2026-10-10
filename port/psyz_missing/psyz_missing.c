/*
 * Minimal host stubs for Psy-Q APIs that PSY-Z declares but does not yet
 * implement (or omits from the build). Enough for Phase 2 title bring-up;
 * movies stay skipped via MODE_TITLE.
 */
#include <libgs.h>
#include <libgte.h>
#include <libetc.h>
#include <libmcrd.h>
#include <libsnd.h>
#include <libpress.h>

/* --- libgs --- */

MATRIX GsWSMATRIX;
MATRIX GsLIGHTWSMATRIX;

void GsInit3D(void) {}

void GsSetProjection(long h) { (void)h; }

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

/* --- libgte --- */

MATRIX *RotMatrixYXZ_gte(SVECTOR *r, MATRIX *m) { return RotMatrixYXZ(r, m); }

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

/* --- libmcrd --- */

void MemCardInit(long val) { (void)val; }

void MemCardStart(void) {}

long MemCardSync(long mode, long *cmds, long *result) {
    (void)mode;
    if (cmds) {
        *cmds = 0;
    }
    if (result) {
        *result = 0;
    }
    return 1;
}

long MemCardExist(long chan) {
    (void)chan;
    return 1;
}

long MemCardAccept(long chan) {
    (void)chan;
    return 1;
}

long MemCardReadFile(
    long chan, char *file, unsigned long *adrs, long offset, long bytes) {
    (void)chan;
    (void)file;
    (void)adrs;
    (void)offset;
    (void)bytes;
    return 1;
}

long MemCardWriteFile(
    long chan, char *file, unsigned long *adrs, long offset, long bytes) {
    (void)chan;
    (void)file;
    (void)adrs;
    (void)offset;
    (void)bytes;
    return 1;
}

long MemCardGetDirentry(long chan, char *name, struct DIRENTRY *dir,
                        long *files, long ofs, long max) {
    (void)chan;
    (void)name;
    (void)dir;
    (void)ofs;
    (void)max;
    if (files) {
        *files = 0;
    }
    return 0;
}

long MemCardCreateFile(long chan, char *filename, long blocks) {
    (void)chan;
    (void)filename;
    (void)blocks;
    return 0;
}

long MemCardFormat(long chan) {
    (void)chan;
    return 0;
}

long MemCardUnformat(long chan) {
    (void)chan;
    return 0;
}

/* libsnd SsSepSetDecrescendo — implemented in vendor/psyz midiread_host.c */

/* --- libpress (movies skipped at MODE_TITLE) --- */

int DecDCTvlc2(u_long *bs, u_long *buf, DECDCTTAB table) {
    (void)bs;
    (void)buf;
    (void)table;
    return 0;
}

void DecDCTvlcBuild(u_short *table) { (void)table; }
