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
#include <stdio.h>

/* --- libgs --- */

MATRIX GsWSMATRIX;
MATRIX GsLIGHTWSMATRIX;

static void dw3_matrix_identity(MATRIX *m) {
    int i;
    int j;

    for (i = 0; i < 3; i++) {
        for (j = 0; j < 3; j++) {
            m->m[i][j] = (i == j) ? 4096 : 0;
        }
        m->t[i] = 0;
    }
}

void GsInit3D(void) {
    /* BSS leaves both matrices at 0. A zero world-screen matrix collapses
     * every RTPS to one point, which is how the battle scene vanished. */
    dw3_matrix_identity(&GsWSMATRIX);
    dw3_matrix_identity(&GsLIGHTWSMATRIX);
}

void GsSetProjection(long h) {
    /* layerLoadView passes the battle camera's proj here. H == 0 breaks RTPS. */
    if (h > 0) {
        SetGeomScreen(h);
    }
}

void GsSetLightMode(int mode) { (void)mode; }

/* PSY-Z has no host GsSetFlatLight; accept Psy-Q GsF_LIGHT (port/include).
 * Directional colors are still a follow-up. SetBackColor (real) supplies
 * ambient, and GsInit3D leaves GsLIGHTWSMATRIX as identity so ncs is not
 * fed a zero matrix. */
int Dw3_GsSetFlatLight(int id, GsF_LIGHT *light) {
    (void)id;
    (void)light;
    return 0;
}

/* Fixed-point (4096 = 1.0) rigid transforms in 64-bit, so camera
 * translations of 0x1400 and more neither wrap a short (ApplyMatrix and
 * CompMatrix narrow their vector input to SVECTOR) nor overflow. */
static void dw3_mat_mul(const MATRIX *a, const MATRIX *b, MATRIX *out) {
    MATRIX r;
    int i;
    int j;

    for (i = 0; i < 3; i++) {
        long long t = 0;

        for (j = 0; j < 3; j++) {
            r.m[i][j] = (short)(((long long)a->m[i][0] * b->m[0][j] +
                                 (long long)a->m[i][1] * b->m[1][j] +
                                 (long long)a->m[i][2] * b->m[2][j]) >>
                                12);
        }
        t = ((long long)a->m[i][0] * b->t[0] + (long long)a->m[i][1] * b->t[1] +
             (long long)a->m[i][2] * b->t[2]) >>
            12;
        r.t[i] = (int)(t + a->t[i]);
    }
    *out = r;
}

/* Inverse of a rotation + translation: transpose, and -R^T t. */
static void dw3_mat_invert_rigid(const MATRIX *m, MATRIX *out) {
    MATRIX r;
    int i;

    for (i = 0; i < 3; i++) {
        r.m[i][0] = m->m[0][i];
        r.m[i][1] = m->m[1][i];
        r.m[i][2] = m->m[2][i];
    }
    for (i = 0; i < 3; i++) {
        r.t[i] = (int)(-(((long long)r.m[i][0] * m->t[0] +
                          (long long)r.m[i][1] * m->t[1] +
                          (long long)r.m[i][2] * m->t[2]) >>
                         12));
    }
    *out = r;
}

/* The view of a camera at vp looking at vr, both in super's space.
 * ratan2(y, x) is atan2(y, x) (decomp ratan.c; Psy-Q header). RotMatrix's
 * view +Z row is (-sin yaw, sin pitch, cos yaw * cos pitch), so the yaw that
 * aims +Z at (dx, dz) is -ratan2(dx, dz), and the pitch that includes dy is
 * +ratan2(dy, xz). The opposite signs leave the reference off the view axis
 * (battle vp=-2432,-1920,-11136 lands at view x≈7458 instead of ≈0).
 * Battle stores rz << 12. The roll turns the picture around the line of
 * sight, so it is applied after yaw and pitch: RotMatrix(pitch, yaw, roll)
 * is Rx·Ry·Rz, which would roll about the world Z axis before the yaw. */
static void dw3_look_at(const GsRVIEW2 *pv, MATRIX *out) {
    long dx = pv->vrx - pv->vpx;
    long dy = pv->vry - pv->vpy;
    long dz = pv->vrz - pv->vpz;
    long xz = SquareRoot0(dx * dx + dz * dz);
    SVECTOR ang;
    MATRIX look;
    MATRIX roll;
    int i;

    if (xz == 0) {
        xz = 1;
    }
    ang.vx = (short)ratan2(dy, xz);
    ang.vy = (short)(-ratan2(dx, dz));
    ang.vz = 0;
    ang.pad = 0;
    RotMatrix(&ang, &look);
    for (i = 0; i < 3; i++) {
        look.t[i] = 0;
    }

    ang.vx = 0;
    ang.vy = 0;
    ang.vz = (short)(pv->rz >> 12);
    RotMatrix(&ang, &roll);
    for (i = 0; i < 3; i++) {
        roll.t[i] = 0;
    }
    dw3_mat_mul(&roll, &look, out);

    /* Translation: the eye maps to the origin. */
    for (i = 0; i < 3; i++) {
        out->t[i] = (int)(-(((long long)out->m[i][0] * pv->vpx +
                             (long long)out->m[i][1] * pv->vpy +
                             (long long)out->m[i][2] * pv->vpz) >>
                            12));
    }
}

/* GsSetRefView2 puts the camera in super's space: the viewpoint sits at
 * super(vp) in the world, and turns and moves with super. The world-screen
 * matrix is therefore look · super⁻¹ (super's chain up to the world taken
 * as one transform). FIGHTSTG's shot camera turns around the fighters by
 * stepping super's rot.vy (shot_camera.c turnAroundFighters) and moves
 * along z through super's tz (viewPartnerSide). Composing super after the
 * look instead (CompMatrix(super, look)) turned the picture in screen
 * space, the battle idle camera panning on the spot instead of orbiting. */
void GsSetRefView2(GsRVIEW2 *pv) {
    MATRIX local;
    MATRIX super_lw;
    MATRIX super_inv;
    GsCOORDINATE2 *c;
    static int logged;

    if (pv == NULL) {
        return;
    }
    dw3_look_at(pv, &local);
    if (pv->super != NULL) {
        super_lw = pv->super->coord;
        for (c = pv->super->super; c != NULL; c = c->super) {
            dw3_mat_mul(&c->coord, &super_lw, &super_lw);
        }
        dw3_mat_invert_rigid(&super_lw, &super_inv);
        dw3_mat_mul(&local, &super_inv, &GsWSMATRIX);
    } else {
        GsWSMATRIX = local;
    }
    if (!logged) {
        logged = 1;
        fprintf(stderr,
                "dw3: GsSetRefView2 vp=%d,%d,%d vr=%d,%d,%d "
                "m00=%d m11=%d m22=%d t=%d,%d,%d\n",
                pv->vpx, pv->vpy, pv->vpz, pv->vrx, pv->vry, pv->vrz,
                GsWSMATRIX.m[0][0], GsWSMATRIX.m[1][1], GsWSMATRIX.m[2][2],
                GsWSMATRIX.t[0], GsWSMATRIX.t[1], GsWSMATRIX.t[2]);
    }
}

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
