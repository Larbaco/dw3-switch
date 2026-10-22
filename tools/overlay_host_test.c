/*
 * Host test for P2.6 static overlay restore semantics.
 *
 * Two fixture overlays: switching file ids restores .data and zeros .bss.
 * Loading the same id twice preserves mutations. An unknown file id fails,
 * is not recorded as loaded, and aborts in strict mode.
 *
 * Build (from repo root):
 *   gcc -std=c11 -Wall -Wextra -Werror -O2 -DPORT_HOST=1 \
 *     -o build/overlay_host_test tools/overlay_host_test.c \
 *     port/overlays/overlay_host.c -Iport/overlays
 */
#include "overlay_host.h"

#include <stdio.h>
#include <string.h>

#define FILE_OVL_A 0x101
#define FILE_OVL_B 0x102
#define FILE_OVL_SUB 0x103

#define FILE_OVL_UNKNOWN 0x1FB /* e.g. WFIGHTTS: not in any host table */

#define MODE_A 2
#define MODE_B 3
#define MODE_UNKNOWN 8

static unsigned char ovl_a_data[8];
static const unsigned char ovl_a_data_init[8] = {
    0xA0, 0xA1, 0xA2, 0xA3, 0xA4, 0xA5, 0xA6, 0xA7};
static unsigned char ovl_a_bss[4];

static unsigned char ovl_b_data[8];
static const unsigned char ovl_b_data_init[8] = {
    0xB0, 0xB1, 0xB2, 0xB3, 0xB4, 0xB5, 0xB6, 0xB7};
static unsigned char ovl_b_bss[4];

static unsigned char ovl_sub_data[8];
static const unsigned char ovl_sub_data_init[8] = {
    0xC0, 0xC1, 0xC2, 0xC3, 0xC4, 0xC5, 0xC6, 0xC7};
static unsigned char ovl_sub_bss[4];

static void *entry_a(void) { return NULL; }
static void *entry_b(void) { return NULL; }

static const Dw3OverlayDesc g_fixtures[] = {
    {FILE_OVL_A, entry_a, ovl_a_data, ovl_a_data_init, sizeof(ovl_a_data),
     ovl_a_bss, sizeof(ovl_a_bss)},
    {FILE_OVL_B, entry_b, ovl_b_data, ovl_b_data_init, sizeof(ovl_b_data),
     ovl_b_bss, sizeof(ovl_b_bss)},
    /* Sub-overlay: entry NULL (like WFIGHTMN). Must not update mode file. */
    {FILE_OVL_SUB, NULL, ovl_sub_data, ovl_sub_data_init, sizeof(ovl_sub_data),
     ovl_sub_bss, sizeof(ovl_sub_bss)},
};

/* OVERLAY_LOADER.mode / .subOverlay; the loaders are the real host halves
 * of loadModeOverlay / loadSubOverlay (patches 0008 + 0060). */
static int32_t g_mode = -1;
static int32_t g_sub = -1;

static int load_mode(int32_t mode_index, int32_t file_id) {
    return dw3_overlay_load_mode(&g_mode, &g_sub, mode_index, file_id);
}

static int load_sub(int32_t file_id) { return dw3_overlay_load_sub(&g_sub, file_id); }

static int fail(const char *msg) {
    fprintf(stderr, "FAIL: %s\n", msg);
    return 1;
}

/* A failed restore must not be recorded as loaded (patch 0060). */
static int check_unknown(void) {
    /* Sub: A-sub loaded, then an unknown id fails and clears the latch, so
     * the same unknown id is tried (and fails) again, and the previous sub
     * is restored again (the PS1 memcpy of the unknown file overwrote it). */
    g_mode = -1;
    g_sub = -1;
    if (load_mode(MODE_A, FILE_OVL_A) != 0 || load_sub(FILE_OVL_SUB) != 0) {
        return fail("unknown: setup");
    }
    if (load_sub(FILE_OVL_UNKNOWN) != -1) {
        return fail("unknown sub should fail");
    }
    if (g_sub != -1) {
        return fail("unknown sub id recorded as loaded");
    }
    if (load_sub(FILE_OVL_UNKNOWN) != -1) {
        return fail("unknown sub not retried");
    }
    ovl_sub_data[2] = 0x99;
    if (load_sub(FILE_OVL_SUB) != 0) {
        return fail("sub after unknown should restore");
    }
    if (ovl_sub_data[2] != ovl_sub_data_init[2] || g_sub != FILE_OVL_SUB) {
        return fail("sub not restored after unknown");
    }
    if (load_sub(0) != -1 || g_sub != -1) {
        return fail("sub file id 0 should fail and not be recorded");
    }

    /* Mode: an unknown mode overlay fails and clears both latches; going back
     * to the previous mode restores it again. */
    ovl_a_data[3] = 0x44;
    if (load_mode(MODE_UNKNOWN, FILE_OVL_UNKNOWN) != -1) {
        return fail("unknown mode should fail");
    }
    if (g_mode != -1 || g_sub != -1) {
        return fail("unknown mode recorded as loaded");
    }
    if (load_mode(MODE_A, FILE_OVL_A) != 0) {
        return fail("mode A after unknown should restore");
    }
    if (ovl_a_data[3] != ovl_a_data_init[3] || g_mode != MODE_A) {
        return fail("mode A not restored after unknown");
    }
    /* Mode slot without a file: recorded, no restore. */
    if (load_mode(0, 0) != 1 || g_mode != 0) {
        return fail("file-less mode slot");
    }
    return 0;
}

/* Strict mode: an unknown restore aborts (run as `overlay_host_test strict`). */
static int run_strict(void) {
    dw3_overlay_set_table(g_fixtures, sizeof(g_fixtures) / sizeof(g_fixtures[0]));
    if (!dw3_overlay_strict()) {
        return fail("strict mode not enabled by the environment");
    }
    (void)dw3_overlay_restore(FILE_OVL_UNKNOWN);
    return fail("strict unknown restore returned");
}

int main(int argc, char **argv) {
    if (argc > 1 && strcmp(argv[1], "strict") == 0) {
        return run_strict();
    }
    /* Non-strict unless the caller says otherwise (DW3_REPLAY may be set). */
    dw3_overlay_set_strict(0);
    dw3_overlay_set_table(g_fixtures,
                          sizeof(g_fixtures) / sizeof(g_fixtures[0]));
    g_mode = -1;
    g_sub = -1;

    /* Fresh load of A: data matches init, bss zero. */
    memset(ovl_a_data, 0xFF, sizeof(ovl_a_data));
    memset(ovl_a_bss, 0xFF, sizeof(ovl_a_bss));
    if (load_mode(MODE_A, FILE_OVL_A) != 0) {
        return fail("load A");
    }
    if (dw3_overlay_current_mode_file() != FILE_OVL_A) {
        return fail("mode file not A after load A");
    }
    if (memcmp(ovl_a_data, ovl_a_data_init, sizeof(ovl_a_data)) != 0) {
        return fail("A data not restored");
    }
    if (ovl_a_bss[0] || ovl_a_bss[1] || ovl_a_bss[2] || ovl_a_bss[3]) {
        return fail("A bss not zeroed");
    }

    /* Mutate A, reload same mode: preserve. */
    ovl_a_data[0] = 0x11;
    ovl_a_bss[0] = 0x22;
    if (load_mode(MODE_A, FILE_OVL_A) != 1) {
        return fail("same mode should preserve");
    }
    if (ovl_a_data[0] != 0x11 || ovl_a_bss[0] != 0x22) {
        return fail("same mode mutated state lost");
    }

    /* Switch to B: B restored/zeroed; A left as mutated (different slot). */
    memset(ovl_b_data, 0xFF, sizeof(ovl_b_data));
    memset(ovl_b_bss, 0xFF, sizeof(ovl_b_bss));
    if (load_mode(MODE_B, FILE_OVL_B) != 0) {
        return fail("load B");
    }
    if (dw3_overlay_current_mode_file() != FILE_OVL_B) {
        return fail("mode file not B after load B");
    }
    if (memcmp(ovl_b_data, ovl_b_data_init, sizeof(ovl_b_data)) != 0) {
        return fail("B data not restored");
    }
    if (ovl_b_bss[0] || ovl_b_bss[1] || ovl_b_bss[2] || ovl_b_bss[3]) {
        return fail("B bss not zeroed");
    }
    if (ovl_a_data[0] != 0x11) {
        return fail("A unexpectedly changed when loading B");
    }

    /* Back to A: id/mode changed → reset A again. */
    if (load_mode(MODE_A, FILE_OVL_A) != 0) {
        return fail("reload A after B");
    }
    if (memcmp(ovl_a_data, ovl_a_data_init, sizeof(ovl_a_data)) != 0) {
        return fail("A not reset after mode change");
    }
    if (ovl_a_bss[0] || ovl_a_bss[1] || ovl_a_bss[2] || ovl_a_bss[3]) {
        return fail("A bss not zeroed after mode change");
    }

    /* Sub-overlay (entry NULL): same twice preserves; does not touch mode. */
    g_sub = -1;
    memset(ovl_sub_data, 0xFF, sizeof(ovl_sub_data));
    memset(ovl_sub_bss, 0xEE, sizeof(ovl_sub_bss));
    if (load_sub(FILE_OVL_SUB) != 0) {
        return fail("sub load SUB");
    }
    if (dw3_overlay_current_mode_file() != FILE_OVL_A) {
        return fail("sub load clobbered mode file");
    }
    if (dw3_overlay_current_sub_file() != FILE_OVL_SUB) {
        return fail("sub load did not latch stage/sub file");
    }
    if (memcmp(ovl_sub_data, ovl_sub_data_init, sizeof(ovl_sub_data)) != 0) {
        return fail("SUB data not restored");
    }
    if (ovl_sub_bss[0] || ovl_sub_bss[1] || ovl_sub_bss[2] || ovl_sub_bss[3]) {
        return fail("SUB bss not zeroed");
    }
    ovl_sub_data[1] = 0x55;
    ovl_sub_bss[1] = 0x66;
    if (load_sub(FILE_OVL_SUB) != 1) {
        return fail("same sub should preserve");
    }
    if (ovl_sub_data[1] != 0x55 || ovl_sub_bss[1] != 0x66) {
        return fail("same sub mutated state lost");
    }
    if (dw3_overlay_current_mode_file() != FILE_OVL_A) {
        return fail("mode file changed after same sub");
    }

    /* Force a second restore of SUB (new tracked id) via mode B then A+sub. */
    if (load_mode(MODE_B, FILE_OVL_B) != 0) {
        return fail("load B before sub reset");
    }
    if (load_mode(MODE_A, FILE_OVL_A) != 0) {
        return fail("reload A before sub reset");
    }
    g_sub = -1;
    ovl_sub_data[1] = 0x77;
    if (load_sub(FILE_OVL_SUB) != 0) {
        return fail("sub reset after mode bounce");
    }
    if (ovl_sub_data[1] != ovl_sub_data_init[1]) {
        return fail("SUB not reset after mode bounce");
    }
    if (dw3_overlay_current_mode_file() != FILE_OVL_A) {
        return fail("mode file not A after sub reset");
    }

    if (check_unknown() != 0) {
        return 1;
    }

    puts("OK: overlay reset vs preserve");
    return 0;
}
