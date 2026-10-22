#ifndef PORT_HOST
#error "port/overlays/overlay_host.c is host-only"
#endif

#include "overlay_host.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const Dw3OverlayDesc *g_table;
static size_t g_count;
static int32_t g_mode_file_id;
static int32_t g_sub_file_id;
static int g_strict = -1; /* -1: not decided yet, read the environment */

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

void dw3_overlay_set_strict(int on) { g_strict = on ? 1 : 0; }

int dw3_overlay_strict(void) {
    const char *env;

    if (g_strict < 0) {
        /* DW3_OVERLAY_STRICT=0/1 wins; otherwise a replay run (DW3_REPLAY,
         * P6.15) is strict. The Switch replay build calls
         * dw3_overlay_set_strict(1) because it has no environment. */
        env = getenv("DW3_OVERLAY_STRICT");
        if (env != NULL && env[0] != '\0') {
            g_strict = env[0] != '0';
        } else {
            env = getenv("DW3_REPLAY");
            g_strict = env != NULL && env[0] != '\0';
        }
    }
    return g_strict;
}

int dw3_overlay_restore(int32_t file_id) {
    const Dw3OverlayDesc *desc = dw3_overlay_find(file_id);
    const char *kind;
    const char *why;

    if (desc == NULL) {
        /* Stable marker for P4.6 OLED nxlink sweeps (see docs/P4.6_MODULES_STATUS.md)
         * and the P6.15 replay runners: keep this line's format. */
        fprintf(stderr, "dw3: overlay restore file=0x%x kind=unknown UNKNOWN\n",
                (unsigned)file_id);
        why = g_table == NULL ? "no overlay table registered"
              : file_id == 0  ? "file id 0"
                              : "file id not in the overlay table";
        fprintf(stderr,
                "dw3: ERROR overlay restore failed: file=0x%x (%d): %s; its .data/.bss "
                "were not reset and the id is not recorded as loaded\n",
                (unsigned)file_id, (int)file_id, why);
        if (dw3_overlay_strict()) {
            fprintf(stderr,
                    "dw3: FATAL overlay restore file=0x%x failed in strict mode "
                    "(DW3_OVERLAY_STRICT / replay run); aborting\n",
                    (unsigned)file_id);
            fflush(stderr);
            abort();
        }
        fflush(stderr);
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

int dw3_overlay_load_mode(int32_t *loaded_mode, int32_t *loaded_sub, int32_t mode_index,
                          int32_t file_id) {
    if (*loaded_mode == mode_index) {
        return 1;
    }
    /* PS1: a new mode overlay always drops the sub-overlay latch. */
    *loaded_sub = -1;
    if (file_id == 0) {
        /* Mode slots without an overlay file (MODE_OVERLAY_FILES[i] == 0). */
        *loaded_mode = mode_index;
        return 1;
    }
    if (dw3_overlay_restore(file_id) != 0) {
        /* Not loaded: forget the old mode too, so the next entry into any
         * mode (including the previous one) restores again, as the PS1
         * memcpy would have overwritten OVERLAY_ADDRESS. */
        *loaded_mode = -1;
        return -1;
    }
    *loaded_mode = mode_index;
    return 0;
}

int dw3_overlay_load_sub(int32_t *loaded_sub, int32_t file_id) {
    if (*loaded_sub == file_id) {
        return 1;
    }
    if (dw3_overlay_restore(file_id) != 0) {
        *loaded_sub = -1;
        return -1;
    }
    *loaded_sub = file_id;
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
