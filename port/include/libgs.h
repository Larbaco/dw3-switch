#ifndef PORT_LIBGS_H
#define PORT_LIBGS_H

/*
 * PSY-Z libgs is missing a few globals/prototypes the engine references.
 * Also: PSY-Z's GsF_LIGHT nests direction in an SVECTOR; Psy-Q / DW3 use
 * flat int vx,vy,vz. Rename PSY-Z's typedef away, then provide the Psy-Q
 * layout the battle lights code expects. GsSetFlatLight is redirected to a
 * host stub (PSY-Z has no host definition yet).
 */
#define GsF_LIGHT PortPsyz_GsF_LIGHT
#include_next <libgs.h>
#undef GsF_LIGHT

typedef struct {
    int vx, vy, vz;
    unsigned char r, g, b;
} GsF_LIGHT;

#ifndef PORT_LIBGS_EXTRAS_DECLS
#define PORT_LIBGS_EXTRAS_DECLS
extern MATRIX GsWSMATRIX;
extern MATRIX GsWSMATRIX_ORG;
extern MATRIX GsLIGHTWSMATRIX;
void GsGetTimInfo(u_long *im, GsIMAGE *tim);
void GsSetLightMode(int mode);
int Dw3_GsSetFlatLight(int id, GsF_LIGHT *light);
#endif

#define GsSetFlatLight(id, lt) Dw3_GsSetFlatLight((id), (lt))

#endif /* PORT_LIBGS_H */
