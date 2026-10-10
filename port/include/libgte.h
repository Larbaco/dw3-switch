#ifndef PORT_LIBGTE_H
#define PORT_LIBGTE_H

/*
 * PSY-Z libgte omits a few Psy-Q constants/prototypes the engine uses.
 * Include the real header, then fill the gaps for host compilation.
 */
#include_next <libgte.h>

#ifndef ONE
#define ONE 4096 /* GTE fixed-point 1.0 */
#endif

#ifndef PORT_LIBGTE_EXTRAS_DECLS
#define PORT_LIBGTE_EXTRAS_DECLS
SVECTOR *ApplyMatrixSV(MATRIX *m, SVECTOR *v0, SVECTOR *v1);
#endif

#endif /* PORT_LIBGTE_H */
