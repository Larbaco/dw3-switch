/*
 * Weak stubs for mode overlays not yet linked into the host image.
 * Linked overlays must not appear here — weak defs block archive pull.
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
/* FIGHTSTG_start comes from P3.3 host overlay objects — do not weak-stub. */
DW3_MODE_STUB(WFIGHTTS_start)
DW3_MODE_STUB(CARDGAME_start)
DW3_MODE_STUB(SOUNDTST_start)
DW3_MODE_STUB(SHOCKTST_start)
/* STGTRAIN / STDGNAME / STGMCARD / STGDGLAB / STITSHOP / STSTATUS: P3.4 linked. */
DW3_MODE_STUB(STCRDABM_start)
DW3_MODE_STUB(STCRDSHP_start)
/* STFGTREP_start comes from P3.3 host overlay objects — do not weak-stub. */
DW3_MODE_STUB(STAGSLCT_start)
DW3_MODE_STUB(CNTY_SEL_start)
