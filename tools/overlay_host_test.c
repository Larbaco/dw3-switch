/*
 * Host test for P2.6 static overlay restore semantics.
 *
 * Two fixture overlays: switching file ids restores .data and zeros .bss.
 * Loading the same id twice preserves mutations.
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

#define MODE_A 2
#define MODE_B 3

static unsigned char ovl_a_data[8];
static const unsigned char ovl_a_data_init[8] = {
    0xA0, 0xA1, 0xA2, 0xA3, 0xA4, 0xA5, 0xA6, 0xA7};
static unsigned char ovl_a_bss[4];

static unsigned char ovl_b_data[8];
static const unsigned char ovl_b_data_init[8] = {
    0xB0, 0xB1, 0xB2, 0xB3, 0xB4, 0xB5, 0xB6, 0xB7};
static unsigned char ovl_b_bss[4];

static void *entry_a(void) { return NULL; }
static void *entry_b(void) { return NULL; }

static const Dw3OverlayDesc g_fixtures[] = {
    {FILE_OVL_A, entry_a, ovl_a_data, ovl_a_data_init, sizeof(ovl_a_data),
     ovl_a_bss, sizeof(ovl_a_bss)},
    {FILE_OVL_B, entry_b, ovl_b_data, ovl_b_data_init, sizeof(ovl_b_data),
     ovl_b_bss, sizeof(ovl_b_bss)},
};

/* Mirrors OVERLAY_LOADER + loadModeOverlay / loadSubOverlay host paths. */
static int32_t g_mode = -1;
static int32_t g_sub = -1;

static int load_mode(int32_t mode_index, int32_t file_id) {
    if (g_mode != mode_index) {
        g_mode = mode_index;
        g_sub = -1;
        if (file_id != 0) {
            return dw3_overlay_restore(file_id);
        }
    }
    return 1; /* preserved / skipped */
}

static int load_sub(int32_t file_id) {
    if (g_sub != file_id) {
        g_sub = file_id;
        return dw3_overlay_restore(file_id);
    }
    return 1; /* preserved */
}

static int fail(const char *msg) {
    fprintf(stderr, "FAIL: %s\n", msg);
    return 1;
}

int main(void) {
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

    /* Sub-overlay: same twice preserves; different resets. */
    g_sub = -1;
    memset(ovl_b_data, 0xFF, sizeof(ovl_b_data));
    memset(ovl_b_bss, 0xEE, sizeof(ovl_b_bss));
    if (load_sub(FILE_OVL_B) != 0) {
        return fail("sub load B");
    }
    ovl_b_data[1] = 0x55;
    ovl_b_bss[1] = 0x66;
    if (load_sub(FILE_OVL_B) != 1) {
        return fail("same sub should preserve");
    }
    if (ovl_b_data[1] != 0x55 || ovl_b_bss[1] != 0x66) {
        return fail("same sub mutated state lost");
    }
    if (load_sub(FILE_OVL_A) != 0) {
        return fail("sub switch to A");
    }
    if (memcmp(ovl_a_data, ovl_a_data_init, sizeof(ovl_a_data)) != 0) {
        return fail("sub A data not restored");
    }

    puts("OK: overlay reset vs preserve");
    return 0;
}
