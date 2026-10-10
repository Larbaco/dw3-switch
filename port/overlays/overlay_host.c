#ifndef PORT_HOST
#error "port/overlays/overlay_host.c is host-only"
#endif

#include "overlay_host.h"

#include <string.h>

static const Dw3OverlayDesc *g_table;
static size_t g_count;

void dw3_overlay_set_table(const Dw3OverlayDesc *table, size_t count) {
    g_table = table;
    g_count = count;
}

const Dw3OverlayDesc *dw3_overlay_find(int32_t file_id) {
    size_t i;

    if (g_table == NULL || file_id == 0) {
        return NULL;
    }
    for (i = 0; i < g_count; i++) {
        if (g_table[i].file_id == file_id) {
            return &g_table[i];
        }
    }
    return NULL;
}

int dw3_overlay_restore(int32_t file_id) {
    const Dw3OverlayDesc *desc = dw3_overlay_find(file_id);

    if (desc == NULL) {
        return -1;
    }
    if (desc->data != NULL && desc->data_init != NULL && desc->data_size > 0) {
        memcpy(desc->data, desc->data_init, desc->data_size);
    }
    if (desc->bss != NULL && desc->bss_size > 0) {
        memset(desc->bss, 0, desc->bss_size);
    }
    return 0;
}
