#ifndef PORT_LIBGS_H
#define PORT_LIBGS_H

/*
 * PSY-Z libgs is missing a few globals/prototypes the engine references.
 * Declarations only — definitions live in the shim / later port glue.
 */
#include_next <libgs.h>

#ifndef PORT_LIBGS_EXTRAS_DECLS
#define PORT_LIBGS_EXTRAS_DECLS
extern MATRIX GsWSMATRIX;
extern MATRIX GsWSMATRIX_ORG;
extern MATRIX GsLIGHTWSMATRIX;
void GsGetTimInfo(u_long *im, GsIMAGE *tim);
#endif

#endif /* PORT_LIBGS_H */
