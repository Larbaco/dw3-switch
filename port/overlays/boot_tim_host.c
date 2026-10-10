/*
 * Load runtime boot TIM into STAGE_VRAM (SUB_OVERLAY_ADDRESS) so game main
 * can GsGetTimInfo / LoadImage like on PS1.
 */
#ifndef PORT_HOST
#error "port/overlays/boot_tim_host.c is host-only"
#endif

#include "boot_tim_host.h"

#include <stdio.h>
#include <string.h>

extern unsigned char STAGE_VRAM[];

enum { DW3_HOST_STAGE_BYTES = 0x80000 };

int dw3_host_load_boot_tim(const char *path) {
    FILE *fp;
    long sz;
    size_t n;

    if (path == NULL || path[0] == '\0') {
        return -1;
    }
    fp = fopen(path, "rb");
    if (fp == NULL) {
        return -1;
    }
    if (fseek(fp, 0, SEEK_END) != 0) {
        fclose(fp);
        return -1;
    }
    sz = ftell(fp);
    if (sz <= 0 || (size_t)sz > (size_t)DW3_HOST_STAGE_BYTES) {
        fclose(fp);
        return -1;
    }
    if (fseek(fp, 0, SEEK_SET) != 0) {
        fclose(fp);
        return -1;
    }
    memset(STAGE_VRAM, 0, DW3_HOST_STAGE_BYTES);
    n = fread(STAGE_VRAM, 1, (size_t)sz, fp);
    fclose(fp);
    if (n != (size_t)sz) {
        return -1;
    }
    /* TIM magic */
    if (STAGE_VRAM[0] != 0x10 || STAGE_VRAM[1] != 0x00 ||
        STAGE_VRAM[2] != 0x00 || STAGE_VRAM[3] != 0x00) {
        return -1;
    }
    return 0;
}
