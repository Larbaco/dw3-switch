/*
 * Bind host overlay descriptors to named .data/.bss sections produced by
 * tools/prefix_stage_obj.py (stages) and ld -r + objcopy (FIELDSTG / STPLNMET).
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
                                     Dw3OverlayDesc *wstag785) {
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
}
