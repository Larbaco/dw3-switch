/*
 * Bind host overlay descriptors to named .data/.bss sections produced by
 * tools/prefix_stage_obj.py (stages) and ld -r + objcopy (mode overlays).
 *
 * GNU ld provides __start_<sec> / __stop_<sec> for referenced orphan sections.
 */
#ifndef PORT_HOST
#error "port/overlays/overlay_sections.c is host-only"
#endif

#include "overlay_host.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern char __start_dw3data_fieldstg[];
extern char __stop_dw3data_fieldstg[];
extern char __start_dw3bss_fieldstg[];
extern char __stop_dw3bss_fieldstg[];

extern char __start_dw3data_stplnmet[];
extern char __stop_dw3data_stplnmet[];
extern char __start_dw3bss_stplnmet[];
extern char __stop_dw3bss_stplnmet[];

extern char __start_dw3data_wstag260[];
extern char __stop_dw3data_wstag260[];
extern char __start_dw3bss_wstag260[];
extern char __stop_dw3bss_wstag260[];

extern char __start_dw3data_wstag780[];
extern char __stop_dw3data_wstag780[];
extern char __start_dw3bss_wstag780[];
extern char __stop_dw3bss_wstag780[];

extern char __start_dw3data_wstag785[];
extern char __stop_dw3data_wstag785[];
extern char __start_dw3bss_wstag785[];
extern char __stop_dw3bss_wstag785[];

extern char __start_dw3data_wstag790[];
extern char __stop_dw3data_wstag790[];
extern char __start_dw3bss_wstag790[];
extern char __stop_dw3bss_wstag790[];

extern char __start_dw3data_fightstg[];
extern char __stop_dw3data_fightstg[];
extern char __start_dw3bss_fightstg[];
extern char __stop_dw3bss_fightstg[];

extern char __start_dw3data_wfightmn[];
extern char __stop_dw3data_wfightmn[];
extern char __start_dw3bss_wfightmn[];
extern char __stop_dw3bss_wfightmn[];

extern char __start_dw3data_stfgtrep[];
extern char __stop_dw3data_stfgtrep[];
extern char __start_dw3bss_stfgtrep[];
extern char __stop_dw3bss_stfgtrep[];

extern char __start_dw3data_ststatus[];
extern char __stop_dw3data_ststatus[];
extern char __start_dw3bss_ststatus[];
extern char __stop_dw3bss_ststatus[];

extern char __start_dw3data_stitshop[];
extern char __stop_dw3data_stitshop[];
extern char __start_dw3bss_stitshop[];
extern char __stop_dw3bss_stitshop[];

extern char __start_dw3data_stgtrain[];
extern char __stop_dw3data_stgtrain[];
extern char __start_dw3bss_stgtrain[];
extern char __stop_dw3bss_stgtrain[];

extern char __start_dw3data_stgdglab[];
extern char __stop_dw3data_stgdglab[];
extern char __start_dw3bss_stgdglab[];
extern char __stop_dw3bss_stgdglab[];

extern char __start_dw3data_stgmcard[];
extern char __stop_dw3data_stgmcard[];
extern char __start_dw3bss_stgmcard[];
extern char __stop_dw3bss_stgmcard[];

extern char __start_dw3data_stdgname[];
extern char __stop_dw3data_stdgname[];
extern char __start_dw3bss_stdgname[];
extern char __stop_dw3bss_stdgname[];

extern char __start_dw3data_cardgame[];
extern char __stop_dw3data_cardgame[];
extern char __start_dw3bss_cardgame[];
extern char __stop_dw3bss_cardgame[];

extern char __start_dw3data_stcrddek[];
extern char __stop_dw3data_stcrddek[];
extern char __start_dw3bss_stcrddek[];
extern char __stop_dw3bss_stcrddek[];

extern char __start_dw3data_stcrdshp[];
extern char __stop_dw3data_stcrdshp[];
extern char __start_dw3bss_stcrdshp[];
extern char __stop_dw3bss_stcrdshp[];

extern char __start_dw3data_stcrdabm[];
extern char __stop_dw3data_stcrdabm[];
extern char __start_dw3bss_stcrdabm[];
extern char __stop_dw3bss_stcrdabm[];

extern char __start_dw3data_stagslct[];
extern char __stop_dw3data_stagslct[];
extern char __start_dw3bss_stagslct[];
extern char __stop_dw3bss_stagslct[];

static void bind_section(Dw3OverlayDesc *desc, char *data, char *data_end, char *bss,
                         char *bss_end) {
    size_t data_size;
    size_t bss_size;
    void *init;

    if (desc == NULL) {
        return;
    }
    data_size = (size_t)(data_end - data);
    bss_size = (size_t)(bss_end - bss);

    desc->data = data;
    desc->data_size = data_size;
    desc->bss = bss;
    desc->bss_size = bss_size;

    if (data_size == 0) {
        desc->data_init = NULL;
        fprintf(stderr, "dw3: overlay file=0x%x empty .data section\n",
                (unsigned)desc->file_id);
        return;
    }
    init = malloc(data_size);
    if (init == NULL) {
        fprintf(stderr, "dw3: overlay snapshot malloc(%zu) failed file=0x%x\n", data_size,
                (unsigned)desc->file_id);
        desc->data_init = NULL;
        return;
    }
    memcpy(init, data, data_size);
    desc->data_init = init;
    fprintf(stderr, "dw3: overlay file=0x%x data=%zu bss=%zu snapshot=%p\n",
            (unsigned)desc->file_id, data_size, bss_size, init);
}

void dw3_overlay_bind_field_sections(Dw3OverlayDesc *fieldstg, Dw3OverlayDesc *stplnmet,
                                     Dw3OverlayDesc *wstag260, Dw3OverlayDesc *wstag780,
                                     Dw3OverlayDesc *wstag785, Dw3OverlayDesc *wstag790) {
    bind_section(fieldstg, __start_dw3data_fieldstg, __stop_dw3data_fieldstg,
                 __start_dw3bss_fieldstg, __stop_dw3bss_fieldstg);
    bind_section(stplnmet, __start_dw3data_stplnmet, __stop_dw3data_stplnmet,
                 __start_dw3bss_stplnmet, __stop_dw3bss_stplnmet);
    bind_section(wstag260, __start_dw3data_wstag260, __stop_dw3data_wstag260,
                 __start_dw3bss_wstag260, __stop_dw3bss_wstag260);
    bind_section(wstag780, __start_dw3data_wstag780, __stop_dw3data_wstag780,
                 __start_dw3bss_wstag780, __stop_dw3bss_wstag780);
    bind_section(wstag785, __start_dw3data_wstag785, __stop_dw3data_wstag785,
                 __start_dw3bss_wstag785, __stop_dw3bss_wstag785);
    bind_section(wstag790, __start_dw3data_wstag790, __stop_dw3data_wstag790,
                 __start_dw3bss_wstag790, __stop_dw3bss_wstag790);
}

void dw3_overlay_bind_battle_sections(Dw3OverlayDesc *fightstg, Dw3OverlayDesc *wfightmn,
                                      Dw3OverlayDesc *stfgtrep) {
    bind_section(fightstg, __start_dw3data_fightstg, __stop_dw3data_fightstg,
                 __start_dw3bss_fightstg, __stop_dw3bss_fightstg);
    bind_section(wfightmn, __start_dw3data_wfightmn, __stop_dw3data_wfightmn,
                 __start_dw3bss_wfightmn, __stop_dw3bss_wfightmn);
    bind_section(stfgtrep, __start_dw3data_stfgtrep, __stop_dw3data_stfgtrep,
                 __start_dw3bss_stfgtrep, __stop_dw3bss_stfgtrep);
}

void dw3_overlay_bind_menu_sections(Dw3OverlayDesc *ststatus, Dw3OverlayDesc *stitshop,
                                    Dw3OverlayDesc *stgtrain, Dw3OverlayDesc *stgdglab,
                                    Dw3OverlayDesc *stgmcard, Dw3OverlayDesc *stdgname) {
    bind_section(ststatus, __start_dw3data_ststatus, __stop_dw3data_ststatus,
                 __start_dw3bss_ststatus, __stop_dw3bss_ststatus);
    bind_section(stitshop, __start_dw3data_stitshop, __stop_dw3data_stitshop,
                 __start_dw3bss_stitshop, __stop_dw3bss_stitshop);
    bind_section(stgtrain, __start_dw3data_stgtrain, __stop_dw3data_stgtrain,
                 __start_dw3bss_stgtrain, __stop_dw3bss_stgtrain);
    bind_section(stgdglab, __start_dw3data_stgdglab, __stop_dw3data_stgdglab,
                 __start_dw3bss_stgdglab, __stop_dw3bss_stgdglab);
    bind_section(stgmcard, __start_dw3data_stgmcard, __stop_dw3data_stgmcard,
                 __start_dw3bss_stgmcard, __stop_dw3bss_stgmcard);
    bind_section(stdgname, __start_dw3data_stdgname, __stop_dw3data_stdgname,
                 __start_dw3bss_stdgname, __stop_dw3bss_stdgname);
}

void dw3_overlay_bind_cardgame_sections(Dw3OverlayDesc *cardgame, Dw3OverlayDesc *stcrddek,
                                        Dw3OverlayDesc *stcrdshp, Dw3OverlayDesc *stcrdabm) {
    bind_section(cardgame, __start_dw3data_cardgame, __stop_dw3data_cardgame,
                 __start_dw3bss_cardgame, __stop_dw3bss_cardgame);
    bind_section(stcrddek, __start_dw3data_stcrddek, __stop_dw3data_stcrddek,
                 __start_dw3bss_stcrddek, __stop_dw3bss_stcrddek);
    bind_section(stcrdshp, __start_dw3data_stcrdshp, __stop_dw3data_stcrdshp,
                 __start_dw3bss_stcrdshp, __stop_dw3bss_stcrdshp);
    bind_section(stcrdabm, __start_dw3data_stcrdabm, __stop_dw3data_stcrdabm,
                 __start_dw3bss_stcrdabm, __stop_dw3bss_stcrdabm);
}

void dw3_overlay_bind_stagslct_sections(Dw3OverlayDesc *stagslct) {
    bind_section(stagslct, __start_dw3data_stagslct, __stop_dw3data_stagslct,
                 __start_dw3bss_stagslct, __stop_dw3bss_stagslct);
}

