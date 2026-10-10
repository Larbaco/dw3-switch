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

/* P3.8 batch 1 — early-game / connected from 780/785/790 */
extern char __start_dw3data_wstag200[];
extern char __stop_dw3data_wstag200[];
extern char __start_dw3bss_wstag200[];
extern char __stop_dw3bss_wstag200[];
extern char __start_dw3data_wstag201[];
extern char __stop_dw3data_wstag201[];
extern char __start_dw3bss_wstag201[];
extern char __stop_dw3bss_wstag201[];
extern char __start_dw3data_wstag206[];
extern char __stop_dw3data_wstag206[];
extern char __start_dw3bss_wstag206[];
extern char __stop_dw3bss_wstag206[];
extern char __start_dw3data_wstag210[];
extern char __stop_dw3data_wstag210[];
extern char __start_dw3bss_wstag210[];
extern char __stop_dw3bss_wstag210[];
extern char __start_dw3data_wstag212[];
extern char __stop_dw3data_wstag212[];
extern char __start_dw3bss_wstag212[];
extern char __stop_dw3bss_wstag212[];
extern char __start_dw3data_wstag220[];
extern char __stop_dw3data_wstag220[];
extern char __start_dw3bss_wstag220[];
extern char __stop_dw3bss_wstag220[];
extern char __start_dw3data_wstag225[];
extern char __stop_dw3data_wstag225[];
extern char __start_dw3bss_wstag225[];
extern char __stop_dw3bss_wstag225[];
extern char __start_dw3data_wstag230[];
extern char __stop_dw3data_wstag230[];
extern char __start_dw3bss_wstag230[];
extern char __stop_dw3bss_wstag230[];
extern char __start_dw3data_wstag235[];
extern char __stop_dw3data_wstag235[];
extern char __start_dw3bss_wstag235[];
extern char __stop_dw3bss_wstag235[];
extern char __start_dw3data_wstag240[];
extern char __stop_dw3data_wstag240[];
extern char __start_dw3bss_wstag240[];
extern char __stop_dw3bss_wstag240[];
extern char __start_dw3data_wstag245[];
extern char __stop_dw3data_wstag245[];
extern char __start_dw3bss_wstag245[];
extern char __stop_dw3bss_wstag245[];
extern char __start_dw3data_wstag250[];
extern char __stop_dw3data_wstag250[];
extern char __start_dw3bss_wstag250[];
extern char __stop_dw3bss_wstag250[];
extern char __start_dw3data_wstag255[];
extern char __stop_dw3data_wstag255[];
extern char __start_dw3bss_wstag255[];
extern char __stop_dw3bss_wstag255[];
/* P3.8 batch 2 — remaining early Central/Asuka + Amaterasu twins */
extern char __start_dw3data_wstag202[];
extern char __stop_dw3data_wstag202[];
extern char __start_dw3bss_wstag202[];
extern char __stop_dw3bss_wstag202[];
extern char __start_dw3data_wstag203[];
extern char __stop_dw3data_wstag203[];
extern char __start_dw3bss_wstag203[];
extern char __stop_dw3bss_wstag203[];
extern char __start_dw3data_wstag205[];
extern char __stop_dw3data_wstag205[];
extern char __start_dw3bss_wstag205[];
extern char __stop_dw3bss_wstag205[];
extern char __start_dw3data_wstag218[];
extern char __stop_dw3data_wstag218[];
extern char __start_dw3bss_wstag218[];
extern char __stop_dw3bss_wstag218[];
extern char __start_dw3data_wstag219[];
extern char __stop_dw3data_wstag219[];
extern char __start_dw3bss_wstag219[];
extern char __stop_dw3bss_wstag219[];
extern char __start_dw3data_wstag232[];
extern char __stop_dw3data_wstag232[];
extern char __start_dw3bss_wstag232[];
extern char __stop_dw3bss_wstag232[];
extern char __start_dw3data_wstag237[];
extern char __stop_dw3data_wstag237[];
extern char __start_dw3bss_wstag237[];
extern char __stop_dw3bss_wstag237[];
extern char __start_dw3data_wstag270[];
extern char __stop_dw3data_wstag270[];
extern char __start_dw3bss_wstag270[];
extern char __stop_dw3bss_wstag270[];
extern char __start_dw3data_wstag275[];
extern char __stop_dw3data_wstag275[];
extern char __start_dw3bss_wstag275[];
extern char __stop_dw3bss_wstag275[];
extern char __start_dw3data_wstag280[];
extern char __stop_dw3data_wstag280[];
extern char __start_dw3bss_wstag280[];
extern char __stop_dw3bss_wstag280[];
extern char __start_dw3data_wstag285[];
extern char __stop_dw3data_wstag285[];
extern char __start_dw3bss_wstag285[];
extern char __stop_dw3bss_wstag285[];
extern char __start_dw3data_wstag290[];
extern char __stop_dw3data_wstag290[];
extern char __start_dw3bss_wstag290[];
extern char __stop_dw3bss_wstag290[];
extern char __start_dw3data_wstag295[];
extern char __stop_dw3data_wstag295[];
extern char __start_dw3bss_wstag295[];
extern char __stop_dw3bss_wstag295[];
extern char __start_dw3data_wstag300[];
extern char __stop_dw3data_wstag300[];
extern char __start_dw3bss_wstag300[];
extern char __stop_dw3bss_wstag300[];
extern char __start_dw3data_wstag305[];
extern char __stop_dw3data_wstag305[];
extern char __start_dw3bss_wstag305[];
extern char __stop_dw3bss_wstag305[];
extern char __start_dw3data_wstag310[];
extern char __stop_dw3data_wstag310[];
extern char __start_dw3bss_wstag310[];
extern char __stop_dw3bss_wstag310[];
extern char __start_dw3data_wstag315[];
extern char __stop_dw3data_wstag315[];
extern char __start_dw3bss_wstag315[];
extern char __stop_dw3bss_wstag315[];
extern char __start_dw3data_wstag320[];
extern char __stop_dw3data_wstag320[];
extern char __start_dw3bss_wstag320[];
extern char __stop_dw3bss_wstag320[];
extern char __start_dw3data_wstag325[];
extern char __stop_dw3data_wstag325[];
extern char __start_dw3bss_wstag325[];
extern char __stop_dw3bss_wstag325[];
extern char __start_dw3data_wstag211[];
extern char __stop_dw3data_wstag211[];
extern char __start_dw3bss_wstag211[];
extern char __stop_dw3bss_wstag211[];
extern char __start_dw3data_wstag221[];
extern char __stop_dw3data_wstag221[];
extern char __start_dw3bss_wstag221[];
extern char __stop_dw3bss_wstag221[];
extern char __start_dw3data_wstag226[];
extern char __stop_dw3data_wstag226[];
extern char __start_dw3bss_wstag226[];
extern char __stop_dw3bss_wstag226[];
extern char __start_dw3data_wstag231[];
extern char __stop_dw3data_wstag231[];
extern char __start_dw3bss_wstag231[];
extern char __stop_dw3bss_wstag231[];
extern char __start_dw3data_wstag233[];
extern char __stop_dw3data_wstag233[];
extern char __start_dw3bss_wstag233[];
extern char __stop_dw3bss_wstag233[];
extern char __start_dw3data_wstag236[];
extern char __stop_dw3data_wstag236[];
extern char __start_dw3bss_wstag236[];
extern char __stop_dw3bss_wstag236[];
extern char __start_dw3data_wstag238[];
extern char __stop_dw3data_wstag238[];
extern char __start_dw3bss_wstag238[];
extern char __stop_dw3bss_wstag238[];
extern char __start_dw3data_wstag241[];
extern char __stop_dw3data_wstag241[];
extern char __start_dw3bss_wstag241[];
extern char __stop_dw3bss_wstag241[];
extern char __start_dw3data_wstag246[];
extern char __stop_dw3data_wstag246[];
extern char __start_dw3bss_wstag246[];
extern char __stop_dw3bss_wstag246[];
extern char __start_dw3data_wstag251[];
extern char __stop_dw3data_wstag251[];
extern char __start_dw3bss_wstag251[];
extern char __stop_dw3bss_wstag251[];
extern char __start_dw3data_wstag256[];
extern char __stop_dw3data_wstag256[];
extern char __start_dw3bss_wstag256[];
extern char __stop_dw3bss_wstag256[];
extern char __start_dw3data_wstag261[];
extern char __stop_dw3data_wstag261[];
extern char __start_dw3bss_wstag261[];
extern char __stop_dw3bss_wstag261[];


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

void dw3_overlay_bind_p38_batch1_sections(
    Dw3OverlayDesc *wstag200, Dw3OverlayDesc *wstag201, Dw3OverlayDesc *wstag206,
    Dw3OverlayDesc *wstag210, Dw3OverlayDesc *wstag212, Dw3OverlayDesc *wstag220,
    Dw3OverlayDesc *wstag225, Dw3OverlayDesc *wstag230, Dw3OverlayDesc *wstag235,
    Dw3OverlayDesc *wstag240, Dw3OverlayDesc *wstag245, Dw3OverlayDesc *wstag250,
    Dw3OverlayDesc *wstag255) {
    bind_section(wstag200, __start_dw3data_wstag200, __stop_dw3data_wstag200,
                 __start_dw3bss_wstag200, __stop_dw3bss_wstag200);
    bind_section(wstag201, __start_dw3data_wstag201, __stop_dw3data_wstag201,
                 __start_dw3bss_wstag201, __stop_dw3bss_wstag201);
    bind_section(wstag206, __start_dw3data_wstag206, __stop_dw3data_wstag206,
                 __start_dw3bss_wstag206, __stop_dw3bss_wstag206);
    bind_section(wstag210, __start_dw3data_wstag210, __stop_dw3data_wstag210,
                 __start_dw3bss_wstag210, __stop_dw3bss_wstag210);
    bind_section(wstag212, __start_dw3data_wstag212, __stop_dw3data_wstag212,
                 __start_dw3bss_wstag212, __stop_dw3bss_wstag212);
    bind_section(wstag220, __start_dw3data_wstag220, __stop_dw3data_wstag220,
                 __start_dw3bss_wstag220, __stop_dw3bss_wstag220);
    bind_section(wstag225, __start_dw3data_wstag225, __stop_dw3data_wstag225,
                 __start_dw3bss_wstag225, __stop_dw3bss_wstag225);
    bind_section(wstag230, __start_dw3data_wstag230, __stop_dw3data_wstag230,
                 __start_dw3bss_wstag230, __stop_dw3bss_wstag230);
    bind_section(wstag235, __start_dw3data_wstag235, __stop_dw3data_wstag235,
                 __start_dw3bss_wstag235, __stop_dw3bss_wstag235);
    bind_section(wstag240, __start_dw3data_wstag240, __stop_dw3data_wstag240,
                 __start_dw3bss_wstag240, __stop_dw3bss_wstag240);
    bind_section(wstag245, __start_dw3data_wstag245, __stop_dw3data_wstag245,
                 __start_dw3bss_wstag245, __stop_dw3bss_wstag245);
    bind_section(wstag250, __start_dw3data_wstag250, __stop_dw3data_wstag250,
                 __start_dw3bss_wstag250, __stop_dw3bss_wstag250);
    bind_section(wstag255, __start_dw3data_wstag255, __stop_dw3data_wstag255,
                 __start_dw3bss_wstag255, __stop_dw3bss_wstag255);
}

void dw3_overlay_bind_p38_batch2a_sections(
    Dw3OverlayDesc *wstag202,
    Dw3OverlayDesc *wstag203,
    Dw3OverlayDesc *wstag205,
    Dw3OverlayDesc *wstag218,
    Dw3OverlayDesc *wstag219,
    Dw3OverlayDesc *wstag232,
    Dw3OverlayDesc *wstag237,
    Dw3OverlayDesc *wstag270,
    Dw3OverlayDesc *wstag275,
    Dw3OverlayDesc *wstag280,
    Dw3OverlayDesc *wstag285,
    Dw3OverlayDesc *wstag290,
    Dw3OverlayDesc *wstag295,
    Dw3OverlayDesc *wstag300,
    Dw3OverlayDesc *wstag305,
    Dw3OverlayDesc *wstag310,
    Dw3OverlayDesc *wstag315,
    Dw3OverlayDesc *wstag320,
    Dw3OverlayDesc *wstag325) {
    bind_section(wstag202, __start_dw3data_wstag202, __stop_dw3data_wstag202,
                 __start_dw3bss_wstag202, __stop_dw3bss_wstag202);
    bind_section(wstag203, __start_dw3data_wstag203, __stop_dw3data_wstag203,
                 __start_dw3bss_wstag203, __stop_dw3bss_wstag203);
    bind_section(wstag205, __start_dw3data_wstag205, __stop_dw3data_wstag205,
                 __start_dw3bss_wstag205, __stop_dw3bss_wstag205);
    bind_section(wstag218, __start_dw3data_wstag218, __stop_dw3data_wstag218,
                 __start_dw3bss_wstag218, __stop_dw3bss_wstag218);
    bind_section(wstag219, __start_dw3data_wstag219, __stop_dw3data_wstag219,
                 __start_dw3bss_wstag219, __stop_dw3bss_wstag219);
    bind_section(wstag232, __start_dw3data_wstag232, __stop_dw3data_wstag232,
                 __start_dw3bss_wstag232, __stop_dw3bss_wstag232);
    bind_section(wstag237, __start_dw3data_wstag237, __stop_dw3data_wstag237,
                 __start_dw3bss_wstag237, __stop_dw3bss_wstag237);
    bind_section(wstag270, __start_dw3data_wstag270, __stop_dw3data_wstag270,
                 __start_dw3bss_wstag270, __stop_dw3bss_wstag270);
    bind_section(wstag275, __start_dw3data_wstag275, __stop_dw3data_wstag275,
                 __start_dw3bss_wstag275, __stop_dw3bss_wstag275);
    bind_section(wstag280, __start_dw3data_wstag280, __stop_dw3data_wstag280,
                 __start_dw3bss_wstag280, __stop_dw3bss_wstag280);
    bind_section(wstag285, __start_dw3data_wstag285, __stop_dw3data_wstag285,
                 __start_dw3bss_wstag285, __stop_dw3bss_wstag285);
    bind_section(wstag290, __start_dw3data_wstag290, __stop_dw3data_wstag290,
                 __start_dw3bss_wstag290, __stop_dw3bss_wstag290);
    bind_section(wstag295, __start_dw3data_wstag295, __stop_dw3data_wstag295,
                 __start_dw3bss_wstag295, __stop_dw3bss_wstag295);
    bind_section(wstag300, __start_dw3data_wstag300, __stop_dw3data_wstag300,
                 __start_dw3bss_wstag300, __stop_dw3bss_wstag300);
    bind_section(wstag305, __start_dw3data_wstag305, __stop_dw3data_wstag305,
                 __start_dw3bss_wstag305, __stop_dw3bss_wstag305);
    bind_section(wstag310, __start_dw3data_wstag310, __stop_dw3data_wstag310,
                 __start_dw3bss_wstag310, __stop_dw3bss_wstag310);
    bind_section(wstag315, __start_dw3data_wstag315, __stop_dw3data_wstag315,
                 __start_dw3bss_wstag315, __stop_dw3bss_wstag315);
    bind_section(wstag320, __start_dw3data_wstag320, __stop_dw3data_wstag320,
                 __start_dw3bss_wstag320, __stop_dw3bss_wstag320);
    bind_section(wstag325, __start_dw3data_wstag325, __stop_dw3data_wstag325,
                 __start_dw3bss_wstag325, __stop_dw3bss_wstag325);
}

void dw3_overlay_bind_p38_batch2b_sections(
    Dw3OverlayDesc *wstag211,
    Dw3OverlayDesc *wstag221,
    Dw3OverlayDesc *wstag226,
    Dw3OverlayDesc *wstag231,
    Dw3OverlayDesc *wstag233,
    Dw3OverlayDesc *wstag236,
    Dw3OverlayDesc *wstag238,
    Dw3OverlayDesc *wstag241,
    Dw3OverlayDesc *wstag246,
    Dw3OverlayDesc *wstag251,
    Dw3OverlayDesc *wstag256,
    Dw3OverlayDesc *wstag261) {
    bind_section(wstag211, __start_dw3data_wstag211, __stop_dw3data_wstag211,
                 __start_dw3bss_wstag211, __stop_dw3bss_wstag211);
    bind_section(wstag221, __start_dw3data_wstag221, __stop_dw3data_wstag221,
                 __start_dw3bss_wstag221, __stop_dw3bss_wstag221);
    bind_section(wstag226, __start_dw3data_wstag226, __stop_dw3data_wstag226,
                 __start_dw3bss_wstag226, __stop_dw3bss_wstag226);
    bind_section(wstag231, __start_dw3data_wstag231, __stop_dw3data_wstag231,
                 __start_dw3bss_wstag231, __stop_dw3bss_wstag231);
    bind_section(wstag233, __start_dw3data_wstag233, __stop_dw3data_wstag233,
                 __start_dw3bss_wstag233, __stop_dw3bss_wstag233);
    bind_section(wstag236, __start_dw3data_wstag236, __stop_dw3data_wstag236,
                 __start_dw3bss_wstag236, __stop_dw3bss_wstag236);
    bind_section(wstag238, __start_dw3data_wstag238, __stop_dw3data_wstag238,
                 __start_dw3bss_wstag238, __stop_dw3bss_wstag238);
    bind_section(wstag241, __start_dw3data_wstag241, __stop_dw3data_wstag241,
                 __start_dw3bss_wstag241, __stop_dw3bss_wstag241);
    bind_section(wstag246, __start_dw3data_wstag246, __stop_dw3data_wstag246,
                 __start_dw3bss_wstag246, __stop_dw3bss_wstag246);
    bind_section(wstag251, __start_dw3data_wstag251, __stop_dw3data_wstag251,
                 __start_dw3bss_wstag251, __stop_dw3bss_wstag251);
    bind_section(wstag256, __start_dw3data_wstag256, __stop_dw3data_wstag256,
                 __start_dw3bss_wstag256, __stop_dw3bss_wstag256);
    bind_section(wstag261, __start_dw3data_wstag261, __stop_dw3data_wstag261,
                 __start_dw3bss_wstag261, __stop_dw3bss_wstag261);
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

