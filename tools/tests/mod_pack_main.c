/* Host proofs of the MOD loader (port/mods/mod_pack.c). The PNG fixtures are
 * made by tools/tests/test_mod_pack.py (synthetic gradients, no game art) in
 * the folder given as argv[1]:
 *   good/       valid pack: mod.json (us only), map @2x and @4x halves, sprites
 *   badsize/    map PNG of the wrong size
 *   newformat/  mod.json "format": 99
 *   junk/       mod.json that is not JSON
 *   eu_only/    pack for region eu only
 *   truncated/  map PNG cut short
 * Plain C, no game code. */
#include "mod_pack.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int g_fail;
#define CHECK(c, ...) do { if (!(c)) { printf("FAIL %s:%d ", __FILE__, __LINE__); \
    printf(__VA_ARGS__); printf("\n"); g_fail = 1; } } while (0)

static void test_manifest(void) {
    Dw3ModManifest m;
    char why[DW3_MOD_WHY];
    const char *ok =
        "{ \"format\": 1, \"id\": \"p-1\", \"name\": \"P\", \"version\": \"0.1\","
        "  \"regions\": [\"us\", \"eu\"], \"future\": {\"x\": [1, {\"y\": \"}\"}]},"
        "  \"map\": { \"scales\": [2, 4], \"filter\": \"nearest\", \"reveal\": true },"
        "  \"generator\": \"g\" }";

    CHECK(dw3_mod_manifest_parse(ok, strlen(ok), &m, why, sizeof(why)) == 0, "ok: %s", why);
    CHECK(strcmp(m.id, "p-1") == 0 && m.region_us && m.region_eu, "ids");
    CHECK(m.scales == ((1u << 2) | (1u << 4)), "scales %x", m.scales);
    CHECK(strcmp(m.filter, "nearest") == 0 && m.reveal == 1, "filter/reveal");
    ok = "{\"format\":2,\"id\":\"x\"}";
    CHECK(dw3_mod_manifest_parse(ok, strlen(ok), &m, why, sizeof(why)) != 0 &&
          strstr(why, "format 2") != NULL, "newer format refused: %s", why);
    ok = "{\"format\":1}";
    CHECK(dw3_mod_manifest_parse(ok, strlen(ok), &m, why, sizeof(why)) != 0 &&
          strstr(why, "id") != NULL, "no id refused: %s", why);
    ok = "format: 1";
    CHECK(dw3_mod_manifest_parse(ok, strlen(ok), &m, why, sizeof(why)) != 0, "not JSON");
    ok = "{\"format\":1,\"id\":\"x\",\"regions\":[\"us\"";
    CHECK(dw3_mod_manifest_parse(ok, strlen(ok), &m, why, sizeof(why)) == 0 ||
          why[0] != 0, "truncated JSON does not crash");
    CHECK(dw3_mod_manifest_parse("", 0, &m, why, sizeof(why)) != 0, "empty");
}

static void path_to(char *out, size_t cap, const char *dir, const char *sub) {
    snprintf(out, cap, "%s/%s", dir, sub);
}

static void test_valid(const char *dir) {
    char mods[600], why[DW3_MOD_WHY];
    Dw3ModPack pack;
    Dw3ModImage img, spr;
    int scale = 0;

    path_to(mods, sizeof(mods), dir, "good_root");
    CHECK(dw3_mod_pack_find(mods, NULL, "us", &pack, why, sizeof(why)) == 0,
          "find: %s", why);
    CHECK(strcmp(pack.man.id, "synthetic") == 0, "id %s", pack.man.id);
    CHECK(dw3_mod_pack_find(mods, NULL, "eu", &pack, why, sizeof(why)) != 0 &&
          strstr(why, "region eu") != NULL, "no eu pack: %s", why);
    CHECK(dw3_mod_pack_find(mods, "good", "us", &pack, why, sizeof(why)) == 0,
          "find by name: %s", why);
    CHECK(dw3_mod_pack_find(mods, "nope", "us", &pack, why, sizeof(why)) != 0,
          "wanted name missing");
    CHECK(dw3_mod_pack_find(mods, NULL, "us", &pack, why, sizeof(why)) == 0 &&
          strcmp(pack.man.id, "synthetic") == 0, "find again: %s", why);
    /* 4x is the highest the manifest lists and the file exists */
    CHECK(dw3_mod_load_map(&pack, "us", 0, 8, 6, 4, &img, &scale, why, sizeof(why)) == 0 &&
          scale == 4 && img.w == 32 && img.h == 24, "map 4x: %s (%d)", why, scale);
    CHECK(img.rgba && img.rgba[3] == 255, "map is opaque");
    dw3_mod_image_free(&img);
    /* the budget caps the scale: 2x is chosen */
    CHECK(dw3_mod_load_map(&pack, "us", 1, 8, 6, 2, &img, &scale, why, sizeof(why)) == 0 &&
          scale == 2 && img.w == 16, "map 2x: %s (%d)", why, scale);
    dw3_mod_image_free(&img);
    CHECK(dw3_mod_load_sprite(&pack, "us", 0, "towns", 5, 4, 3, 2, &spr, why,
                              sizeof(why)) == 0 && spr.w == 12 && spr.h == 8,
          "sprite: %s", why);
    CHECK(dw3_mod_load_sprite(&pack, "us", 0, "towns", 5, 4, 3, 3, &img, why,
                              sizeof(why)) != 0 && strstr(why, "expected 12x12") != NULL,
          "sprite size mismatch: %s", why);
    CHECK(dw3_mod_load_sprite(&pack, "us", 0, "towns", 99, 4, 3, 2, &img, why,
                              sizeof(why)) != 0, "sprite missing");
    /* alpha blend: a half-transparent red sprite over a map */
    CHECK(dw3_mod_load_map(&pack, "us", 0, 8, 6, 4, &img, &scale, why, sizeof(why)) == 0, "map");
    if (img.rgba && spr.rgba) {
        uint8_t before[3];

        memcpy(before, img.rgba + ((size_t)4 * img.w + 4) * 4, 3);
        dw3_mod_blend(&img, &spr, 4, 4);
        CHECK(memcmp(before, img.rgba + ((size_t)4 * img.w + 4) * 4, 3) != 0 ||
              spr.rgba[3] == 0, "blend changed the pixel");
        dw3_mod_blend(&img, &spr, 30, 22); /* clipped at the corner: no crash */
        dw3_mod_blend(&img, &spr, -5, -5);
    }
    dw3_mod_image_free(&img);
    dw3_mod_image_free(&spr);
}

static void test_invalid(const char *dir) {
    char mods[600], why[DW3_MOD_WHY];
    Dw3ModPack pack;
    Dw3ModImage img;
    int scale = 0;

    path_to(mods, sizeof(mods), dir, "bad_root");
    /* wrong-sized map: the pack opens (the manifest is fine), the map is refused */
    CHECK(dw3_mod_pack_find(mods, "badsize", "us", &pack, why, sizeof(why)) == 0,
          "badsize opens: %s", why);
    CHECK(dw3_mod_load_map(&pack, "us", 0, 8, 6, 4, &img, &scale, why, sizeof(why)) != 0 &&
          strstr(why, "expected 32x24") != NULL && img.rgba == NULL,
          "wrong size refused: %s", why);
    CHECK(dw3_mod_pack_find(mods, "newformat", "us", &pack, why, sizeof(why)) != 0 &&
          strstr(why, "format 99") != NULL, "newer format: %s", why);
    CHECK(dw3_mod_pack_find(mods, "junk", "us", &pack, why, sizeof(why)) != 0,
          "junk manifest: %s", why);
    CHECK(dw3_mod_pack_find(mods, "eu_only", "us", &pack, why, sizeof(why)) != 0 &&
          strstr(why, "region us") != NULL, "region mismatch: %s", why);
    CHECK(dw3_mod_pack_find(mods, "truncated", "us", &pack, why, sizeof(why)) == 0,
          "truncated opens: %s", why);
    CHECK(dw3_mod_load_map(&pack, "us", 0, 8, 6, 4, &img, &scale, why, sizeof(why)) != 0 &&
          strstr(why, "not a valid PNG") != NULL, "truncated PNG refused: %s", why);
    /* a missing file, a missing folder, no folder at all */
    CHECK(dw3_mod_load_map(&pack, "us", 1, 8, 6, 4, &img, &scale, why, sizeof(why)) != 0,
          "missing half refused: %s", why);
    /* without a name the first folder (by name) with a valid manifest wins:
     * junk and newformat are skipped, badsize is first and its map is refused
     * later, which is where the caller falls back */
    CHECK(dw3_mod_pack_find(mods, NULL, "us", &pack, why, sizeof(why)) == 0 &&
          strstr(pack.dir, "badsize") != NULL, "first valid pack: %s", why);
    path_to(mods, sizeof(mods), dir, "does_not_exist");
    CHECK(dw3_mod_pack_find(mods, NULL, "us", &pack, why, sizeof(why)) != 0 &&
          strstr(why, "no mods folder") != NULL, "missing folder: %s", why);
    CHECK(dw3_mod_pack_find(NULL, NULL, "us", &pack, why, sizeof(why)) != 0, "NULL dir");
}

int main(int argc, char **argv) {
    const char *dir = argc > 1 ? argv[1] : ".";

    test_manifest();
    test_valid(dir);
    test_invalid(dir);
    if (!g_fail) printf("OK: mod pack loader\n");
    return g_fail;
}
