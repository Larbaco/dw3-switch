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

/*
 * gtemac.h helpers used by FIGHTSTG draw/mesh. On host, engine/gte.h does not
 * pull gtemac.h; map the names onto PSY-Z's CompMatrix / AverageZ*.
 */
#ifndef gte_CompMatrix
#define gte_CompMatrix(r1, r2, r3) CompMatrix((r1), (r2), (r3))
#endif

#ifndef gte_AverageZ3
#define gte_AverageZ3(r1, r2, r3, r4)                                          \
    do {                                                                       \
        *(r4) = AverageZ3((r1), (r2), (r3));                                   \
    } while (0)
#endif

#ifndef gte_AverageZ4
#define gte_AverageZ4(r1, r2, r3, r4, r5)                                      \
    do {                                                                       \
        *(r5) = AverageZ4((r1), (r2), (r3), (r4));                             \
    } while (0)
#endif

#endif /* PORT_LIBGTE_H */
