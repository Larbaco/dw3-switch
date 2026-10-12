#ifndef PORT_HOST
#error "port/overlays/overlay_host.c is host-only"
#endif

#include "overlay_host.h"

#include <stdio.h>
#include <string.h>

static const Dw3OverlayDesc *g_table;
static size_t g_count;
static int32_t g_mode_file_id;
static int32_t g_sub_file_id;

void dw3_overlay_set_table(const Dw3OverlayDesc *table, size_t count) {
    g_table = table;
    g_count = count;
    g_mode_file_id = 0;
    g_sub_file_id = 0;
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
    const char *kind;

    if (desc == NULL) {
        /* Stable marker for P4.6 OLED nxlink sweeps (see docs/P4.6_MODULES_STATUS.md). */
        fprintf(stderr, "dw3: overlay restore file=0x%x kind=unknown UNKNOWN\n",
                (unsigned)file_id);
        return -1;
    }
    if (desc->data != NULL && desc->data_init != NULL && desc->data_size > 0) {
        memcpy(desc->data, desc->data_init, desc->data_size);
    }
    if (desc->bss != NULL && desc->bss_size > 0) {
        memset(desc->bss, 0, desc->bss_size);
    }
    /* Mode overlays own an entry point; sub-overlays (WFIGHTMN, stages, …)
     * must not clobber the tracked mode file used by the mesh gate. */
    if (desc->entry != NULL) {
        g_mode_file_id = file_id;
        kind = "mode";
    } else {
        g_sub_file_id = file_id;
        kind = "sub";
    }
    /* kind=mode|sub is for nxlink sweep parsers; entry pointer stays for triage. */
    fprintf(stderr,
            "dw3: overlay restore file=0x%x kind=%s data=%zu bss=%zu entry=%p\n",
            (unsigned)file_id, kind, desc->data_size, desc->bss_size,
            (void *)desc->entry);
    return 0;
}

int32_t dw3_overlay_current_mode_file(void) { return g_mode_file_id; }

int32_t dw3_overlay_current_sub_file(void) { return g_sub_file_id; }

int dw3_host_menu_force_once(void) {
    static int done;
    if (done) {
        return 0;
    }
    done = 1;
    return 1;
}
