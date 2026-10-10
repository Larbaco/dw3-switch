/*
 * Weak stubs for mode overlays not yet linked into the host image.
 * STDWTITL_start comes from title sources; FIELDSTG / STPLNMET from P3.2
 * host overlay objects (do not stub those — weak defs block archive pull).
 */
#ifndef PORT_HOST
#error "port/overlays/mode_stubs.c is host-only"
#endif

typedef struct Task Task;

#define DW3_MODE_STUB(name)                                                    \
    Task *name(void) __attribute__((weak));                                    \
    Task *name(void) {                                                         \
        return (Task *)0;                                                      \
    }

DW3_MODE_STUB(STCRDDEK_start)
DW3_MODE_STUB(FIGHTSTG_start)
DW3_MODE_STUB(CARDGAME_start)
DW3_MODE_STUB(SOUNDTST_start)
DW3_MODE_STUB(SHOCKTST_start)
DW3_MODE_STUB(STGTRAIN_start)
DW3_MODE_STUB(STDGNAME_start)
DW3_MODE_STUB(STGMCARD_start)
DW3_MODE_STUB(STGDGLAB_createScene)
DW3_MODE_STUB(STITSHOP_start)
DW3_MODE_STUB(STSTATUS_start)
DW3_MODE_STUB(STCRDABM_start)
DW3_MODE_STUB(STCRDSHP_start)
DW3_MODE_STUB(STFGTREP_start)
DW3_MODE_STUB(STAGSLCT_start)
DW3_MODE_STUB(CNTY_SEL_start)
