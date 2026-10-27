/* P6.31 host test: the auto-save policy, the idle-frame state machine and the
 * write to its own file (autosave.bin, the player's card untouched), the
 * load, rollback on a failed flush. Synthetic card images only, built here. Usage: autosave_test <dir>.
 * Prints "OK: autosave" and exits 0, or "FAIL: <what>" and exits 1. */
#include "port_autosave.h"
#include "memcard_host.h"
#include "save_convert.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#define SAVE_BYTES 0x26BC
#define MODE_A 0x233
#define MODE_B 0x234

static int fails;

#define CHECK(cond, what)                                              \
    do {                                                               \
        if (!(cond)) {                                                 \
            printf("FAIL: %s (line %d)\n", what, __LINE__);            \
            fails++;                                                   \
        }                                                              \
    } while (0)

static char g_dir[512];

static void put16(unsigned char *p, int v) {
    p[0] = (unsigned char)(v & 0xFF);
    p[1] = (unsigned char)((v >> 8) & 0xFF);
}

static void set_time(unsigned char *t12, int h, int m, int s) {
    memset(t12, 0, 12);
    put16(t12 + 4, h);
    put16(t12 + 6, m);
    put16(t12 + 8, s);
}

/* A card with three saves: slot s holds bytes of value 0x10 + s, named
 * "Tai", played 1:00:10 + s minutes. */
static void make_card(int port) {
    unsigned char info[SAVE_INFO_SIZE];
    unsigned char slotbuf[SAVE_SLOT_STRIDE];
    int s;

    memset(info, 0, sizeof(info));
    memcpy(info + 4, "DMW3", 4);
    for (s = 0; s < 3; s++) {
        unsigned char *sum = info + 8 + s * SAVE_SUMMARY_SIZE;

        strcpy((char *)sum, "Tai");
        sum[4] = (unsigned char)(0xA0 + s); /* junk after the NUL: the game's strcpy leaves it */
        set_time(sum + 0x24, 1, s, 10);
        memset(slotbuf, 0x10 + s, sizeof(slotbuf));
        memcard_host_file_write(port, SAVE_DATA_OFFSET + s * SAVE_SLOT_STRIDE,
                                slotbuf, sizeof(slotbuf));
    }
    info[1] = 0;
    info[0] = save_xor(info + 4, SAVE_INFO_SIZE - 4);
    memcard_host_file_write(port, SAVE_INFO_OFFSET, info, sizeof(info));
}

static void snapshot(Dw3AutosaveSnapshot *snap, unsigned char *save, int mode,
                     int minutes, const char *name) {
    memset(snap, 0, sizeof(*snap));
    memset(save, 0x77, SAVE_BYTES);
    snap->save = save;
    snap->save_size = SAVE_BYTES;
    snap->mode = mode;
    strcpy((char *)snap->name, name);
    snap->money = 1234;
    set_time(snap->play_time, 1, minutes, 30);
    snap->partners[0] = 3;
    snap->levels[0] = 12;
}

static void image(int port, unsigned char *out) {
    memcard_host_file_read(port, 0, out, MEMCARD_FLAT_SIZE);
}

static int read_file(const char *path, unsigned char *out, size_t n) {
    FILE *fp = fopen(path, "rb");
    size_t got;

    if (fp == NULL) {
        return -1;
    }
    got = fread(out, 1, n, fp);
    fclose(fp);
    return got == n ? 0 : -1;
}

static void test_policy(void) {
    Dw3AutosaveCtx c;

    memset(&c, 0, sizeof(c));
    c.option = DW3_AUTOSAVE_AREA;
    c.in_field = 1;
    c.menu_allowed = 1;
    c.mode = MODE_B;
    c.last_mode = MODE_A;
    c.has_target = 1;
    CHECK(dw3_autosave_decide(&c) == DW3_AUTOSAVE_GO, "all clear: go");
    c.option = DW3_AUTOSAVE_OFF;
    CHECK(dw3_autosave_decide(&c) == DW3_AUTOSAVE_SKIP_OFF, "off");
    c.option = DW3_AUTOSAVE_AREA;
    c.in_field = 0;
    CHECK(dw3_autosave_decide(&c) == DW3_AUTOSAVE_SKIP_NOT_FIELD, "not a field");
    c.in_field = 1;
    c.battle = 1;
    CHECK(dw3_autosave_decide(&c) == DW3_AUTOSAVE_SKIP_BATTLE, "battle");
    c.busy = 1;
    CHECK(dw3_autosave_decide(&c) == DW3_AUTOSAVE_SKIP_BATTLE, "battle beats busy");
    c.battle = 0;
    CHECK(dw3_autosave_decide(&c) == DW3_AUTOSAVE_SKIP_BUSY, "event or menu");
    c.busy = 0;
    c.frozen = 1;
    CHECK(dw3_autosave_decide(&c) == DW3_AUTOSAVE_SKIP_TRANSITION, "transition");
    c.frozen = 0;
    c.menu_allowed = 0;
    CHECK(dw3_autosave_decide(&c) == DW3_AUTOSAVE_SKIP_NO_MENU, "story before the menu");
    c.menu_allowed = 1;
    c.last_mode = -1;
    CHECK(dw3_autosave_decide(&c) == DW3_AUTOSAVE_SKIP_BASELINE, "first area");
    c.last_mode = MODE_B;
    CHECK(dw3_autosave_decide(&c) == DW3_AUTOSAVE_SKIP_SAME_AREA, "same area");
    c.last_mode = MODE_A;
    c.has_target = 0;
    CHECK(dw3_autosave_decide(&c) == DW3_AUTOSAVE_SKIP_NO_TARGET, "verdict kept (unused by the port)");
    CHECK(dw3_autosave_verdict_is_transient(DW3_AUTOSAVE_SKIP_BATTLE) &&
              dw3_autosave_verdict_is_transient(DW3_AUTOSAVE_SKIP_BUSY) &&
              dw3_autosave_verdict_is_transient(DW3_AUTOSAVE_SKIP_TRANSITION),
          "transient verdicts");
    CHECK(!dw3_autosave_verdict_is_transient(DW3_AUTOSAVE_SKIP_NO_TARGET) &&
              !dw3_autosave_verdict_is_transient(DW3_AUTOSAVE_GO),
          "final verdicts");
}

static Dw3AutosaveField idle(int mode) {
    Dw3AutosaveField f;

    memset(&f, 0, sizeof(f));
    f.mode = mode;
    f.progress = 6;
    return f;
}

static void test_state_machine(void) {
    Dw3AutosaveField f;

    dw3_autosave_test_reset();
    f = idle(MODE_A);
    CHECK(dw3_autosave_field_idle(&f) == 0, "option off: never");
    dw3_autosave_test_set_mode(DW3_AUTOSAVE_AREA);
    dw3_autosave_note_loaded();
    CHECK(dw3_autosave_field_idle(&f) == 0, "loaded area is the baseline, not a change");
    CHECK(dw3_autosave_field_idle(&f) == 0, "same area again");
    f.battle_starting = 1;
    f.mode = MODE_B;
    CHECK(dw3_autosave_field_idle(&f) == 0, "battle: not now");
    f.battle_starting = 0;
    f.frozen = 1;
    CHECK(dw3_autosave_field_idle(&f) == 0, "banner: not now");
    f.frozen = 0;
    f.busy = 1;
    CHECK(dw3_autosave_field_idle(&f) == 0, "event: not now");
    f.busy = 0;
    CHECK(dw3_autosave_field_idle(&f) == 1, "the change is still pending: go");
    CHECK(dw3_autosave_field_idle(&f) == 0, "and consumed");
    f.mode = MODE_A;
    CHECK(dw3_autosave_field_idle(&f) == 1, "back to the first area: a change");
    /* Story before progress 4: noted, never saved, and not saved later for it. */
    f.mode = MODE_B;
    f.progress = 3;
    CHECK(dw3_autosave_field_idle(&f) == 0, "no field menu yet");
    f.progress = 6;
    CHECK(dw3_autosave_field_idle(&f) == 0, "the change was consumed while it was blocked");
    /* A mode the save list does not know is not a saveable field. */
    f.mode = 0x4FF;
    CHECK(dw3_autosave_field_idle(&f) == 0, "unknown mode");
    /* A new game: no baseline, but no slot is needed any more (own file). */
    dw3_autosave_title_started();
    f.mode = MODE_A;
    CHECK(dw3_autosave_field_idle(&f) == 0, "new game first area");
    f.mode = MODE_B;
    CHECK(dw3_autosave_field_idle(&f) == 1, "a new game saves too, no slot needed");
}

static void test_areas(void) {
    CHECK(dw3_autosave_area_of_mode(0x233) == 0x0D, "Bulk Swamp area");
    CHECK(dw3_autosave_area_of_mode(0x200) == 0x0B, "first mode");
    CHECK(dw3_autosave_area_of_mode(0x1FF) == 0, "below the table");
    CHECK(dw3_autosave_area_of_mode(0x2EF) == 0, "above the table");
}

static void game_bytes(unsigned char *save, int fill) {
    memset(save, fill, SAVE_BYTES);
    save[2] = SAVE_VERSION_US;
    save[0] = save_xor(save + 4, SAVE_BYTES - 4);
}

static void test_commit_and_load(void) {
    static unsigned char card_before[MEMCARD_FLAT_SIZE];
    static unsigned char card_after[MEMCARD_FLAT_SIZE];
    static unsigned char auto_img[MEMCARD_FLAT_SIZE];
    static unsigned char disk[MEMCARD_FLAT_SIZE];
    unsigned char save[SAVE_BYTES];
    unsigned char back[SAVE_BYTES];
    Dw3AutosaveSnapshot snap;
    char path[600];
    char card_path[600];
    char pre_path[620];

    snprintf(path, sizeof(path), "%s/autosave.bin", g_dir);
    snprintf(card_path, sizeof(card_path), "%s/save0.bin", g_dir);
    snprintf(pre_path, sizeof(pre_path), "%s/save0.bin.pre-auto", g_dir);
    CHECK(memcard_host_set_dir(g_dir) == 0, "set dir");
    CHECK(memcard_host_ensure(0) == 0, "ensure");
    CHECK(memcard_host_create_file(0, "BASLUS-01436DMW3-USA") == 0, "create card");
    make_card(0);
    CHECK(memcard_host_flush(0) == 0, "first flush");
    image(0, card_before);
    dw3_autosave_test_reset();
    CHECK(!dw3_autosave_file_present(), "no auto-save file yet");
    CHECK(dw3_autosave_load(back, SAVE_BYTES, SAVE_REGION_US) == -1,
          "nothing to continue");

    snapshot(&snap, save, MODE_B, 30, "Tai");
    game_bytes(save, 0x77);
    CHECK(dw3_autosave_commit(&snap) == 0, "commit creates autosave.bin");
    CHECK(access(path, F_OK) == 0, "autosave.bin exists");
    CHECK(read_file(path, auto_img, MEMCARD_FLAT_SIZE) == 0, "autosave.bin is a 32 KiB card");
    CHECK(memcmp(auto_img + SAVE_DATA_OFFSET, save, SAVE_BYTES) == 0, "slot 0 holds the snapshot");
    CHECK(memcmp(auto_img + SAVE_INFO_OFFSET + 4, "DMW3", 4) == 0, "info magic");
    CHECK(auto_img[SAVE_INFO_OFFSET] == save_xor(auto_img + SAVE_INFO_OFFSET + 4,
                                                 SAVE_INFO_SIZE - 4),
          "info checksum is right");
    {
        const unsigned char *sum = auto_img + SAVE_INFO_OFFSET + 8;

        CHECK(strcmp((const char *)sum, "Tai") == 0, "summary name");
        CHECK(sum[0x18] == dw3_autosave_area_of_mode(MODE_B), "summary area");
        CHECK(sum[0x1C] == DW3_AUTOSAVE_PLACE, "summary place");
        CHECK(sum[0x20] == (1234 & 0xFF) && sum[0x21] == (1234 >> 8), "summary money");
        CHECK(sum[0x30] == 3 && sum[0x3C] == 12, "summary partner and level");
    }
    /* The player's card is never touched, and no .pre-auto copy exists. */
    image(0, card_after);
    CHECK(memcmp(card_before, card_after, MEMCARD_FLAT_SIZE) == 0, "save0.bin in RAM untouched");
    CHECK(read_file(card_path, disk, MEMCARD_FLAT_SIZE) == 0 &&
              memcmp(disk, card_before, MEMCARD_FLAT_SIZE) == 0,
          "save0.bin on disk untouched");
    CHECK(access(pre_path, F_OK) != 0, "no .pre-auto copy any more");
    CHECK(dw3_autosave_file_present(), "the file is present after a write");

    /* Continue: the game bytes come back, checked again. */
    memset(back, 0, sizeof(back));
    CHECK(dw3_autosave_load(back, SAVE_BYTES, SAVE_REGION_US) == 0, "load");
    CHECK(memcmp(back, save, SAVE_BYTES) == 0, "the loaded bytes are the snapshot");
    CHECK(dw3_autosave_load(back, SAVE_BYTES, SAVE_REGION_EU) == -1, "other region refused");

    /* Another name and a shorter play time: a new game overwrites it. */
    game_bytes(save, 0x55);
    snapshot(&snap, save, MODE_A, 1, "Agumon");
    game_bytes(save, 0x55);
    CHECK(dw3_autosave_commit(&snap) == 0, "a new game overwrites the auto-save");
    CHECK(read_file(path, auto_img, MEMCARD_FLAT_SIZE) == 0 &&
              memcmp(auto_img + SAVE_DATA_OFFSET, save, SAVE_BYTES) == 0,
          "second write replaced it");
    CHECK(strcmp((const char *)auto_img + SAVE_INFO_OFFSET + 8, "Agumon") == 0, "new name");
    image(0, card_after);
    CHECK(memcmp(card_before, card_after, MEMCARD_FLAT_SIZE) == 0, "save0.bin still untouched");
    CHECK(dw3_autosave_commit(NULL) == -1, "no snapshot");
}

static void test_failure(void) {
    static unsigned char before[MEMCARD_FLAT_SIZE];
    static unsigned char disk[MEMCARD_FLAT_SIZE];
    unsigned char save[SAVE_BYTES];
    unsigned char back[SAVE_BYTES];
    Dw3AutosaveSnapshot snap;
    char path[600];

    snprintf(path, sizeof(path), "%s/autosave.bin", g_dir);
    CHECK(memcard_host_flush(MEMCARD_HOST_AUTO) == 0, "clean before the failure test");
    CHECK(read_file(path, before, MEMCARD_FLAT_SIZE) == 0, "read the file");
    snapshot(&snap, save, MODE_B, 59, "Tai");
    game_bytes(save, 0x33);
    memcard_host_test_fail_flush(1);
    CHECK(dw3_autosave_commit(&snap) == -1, "a failed flush is reported");
    CHECK(read_file(path, disk, MEMCARD_FLAT_SIZE) == 0 && memcmp(disk, before, MEMCARD_FLAT_SIZE) == 0,
          "the file on disk is the old auto-save");
    CHECK(dw3_autosave_load(back, SAVE_BYTES, SAVE_REGION_US) == 0 && back[4] == 0x55,
          "and it still loads (RAM rolled back)");
    memcard_host_test_fail_flush(0);
    CHECK(memcard_host_flush(MEMCARD_HOST_AUTO) == 0 && !memcard_host_flush_failed(MEMCARD_HOST_AUTO),
          "not left locked");

    /* A real failure: the folder cannot be written. */
    if (geteuid() != 0) {
        chmod(g_dir, 0555);
        CHECK(dw3_autosave_commit(&snap) == -1, "read-only folder: nothing written");
        chmod(g_dir, 0755);
        CHECK(read_file(path, disk, MEMCARD_FLAT_SIZE) == 0 && memcmp(disk, before, MEMCARD_FLAT_SIZE) == 0,
              "read-only: the file is intact");
    }
}

static void test_bad_file(void) {
    static unsigned char img[MEMCARD_FLAT_SIZE];
    char path[600];
    unsigned char back[SAVE_BYTES];

    snprintf(path, sizeof(path), "%s/autosave.bin", g_dir);
    memcard_host_shutdown();
    CHECK(memcard_host_set_dir(g_dir) == 0, "set dir again");
    CHECK(read_file(path, img, MEMCARD_FLAT_SIZE) == 0, "read");
    img[SAVE_DATA_OFFSET + 100] ^= 0xFF; /* a bad data checksum */
    {
        FILE *fp = fopen(path, "wb");

        CHECK(fp != NULL && fwrite(img, 1, MEMCARD_FLAT_SIZE, fp) == MEMCARD_FLAT_SIZE, "write bad");
        if (fp != NULL) {
            fclose(fp);
        }
    }
    dw3_autosave_test_reset();
    dw3_autosave_title_started();
    CHECK(!dw3_autosave_file_present(), "a corrupt file is not offered");
    CHECK(dw3_autosave_load(back, SAVE_BYTES, SAVE_REGION_US) == -1, "and does not load");
}

int main(int argc, char **argv) {
    if (argc < 2) {
        return 2;
    }
    snprintf(g_dir, sizeof(g_dir), "%s", argv[1]);
    test_policy();
    test_areas();
    test_state_machine();
    test_commit_and_load();
    test_failure();
    test_bad_file();
    memcard_host_shutdown();
    if (fails) {
        printf("%d check(s) failed\n", fails);
        return 1;
    }
    printf("OK: autosave\n");
    return 0;
}
