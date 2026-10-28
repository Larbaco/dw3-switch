/* MOD map upscale (docs/MOD_MAP_UPSCALE.md): dumps the map view's art from the
 * user's own extracted game files, for tools/mods/extract_map_assets.py.
 *
 *   map_assets_dump SHEET_FILE IMAGE_FILE OUTDIR HALF
 *
 * SHEET_FILE / IMAGE_FILE are STWMP<n>CS.BIN / STWMP<n>TM.BIN of the runtime
 * tree. Writes, in PAM (P7, RGB_ALPHA, which Pillow reads):
 *   OUTDIR/base_h<HALF>.pam                      the bare map, opaque
 *   OUTDIR/towns_h<HALF>_<id>.pam, areas_h<HALF>_<id>.pam   one file per frame
 *       of the reveal sprites (sheet entry 1 = towns, entry 2 = visited-area
 *       icons), the size of the frame's box, VRAM colour 0 transparent
 *   OUTDIR/frames_h<HALF>.txt                    one line per frame:
 *                                                kind id x0 y0 w h
 * Nothing here is committed or shipped: the output is derived from the disc. */
#include "map_view_core.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static uint8_t *slurp(const char *path, size_t *len) {
    FILE *f = fopen(path, "rb");
    uint8_t *p;
    long n;

    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    n = ftell(f);
    fseek(f, 0, SEEK_SET);
    p = (uint8_t *)malloc((size_t)n + 1);
    if (p && fread(p, 1, (size_t)n, f) != (size_t)n) {
        free(p);
        p = NULL;
    }
    fclose(f);
    *len = (size_t)n;
    return p;
}

static int rd32(const uint8_t *p) {
    return (int)((uint32_t)p[0] | ((uint32_t)p[1] << 8) |
                 ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24));
}

static int write_pam(const char *path, const uint8_t *rgba, int w, int h) {
    FILE *f = fopen(path, "wb");

    if (!f) return -1;
    fprintf(f, "P7\nWIDTH %d\nHEIGHT %d\nDEPTH 4\nMAXVAL 255\nTUPLTYPE RGB_ALPHA\n"
            "ENDHDR\n", w, h);
    fwrite(rgba, 1, (size_t)w * (size_t)h * 4, f);
    fclose(f);
    return 0;
}

int main(int argc, char **argv) {
    size_t sn, in;
    uint8_t *sheet, *imgs;
    uint16_t *vram;
    int half, e, off[3], i;
    MvImage base;
    MvBox box;
    char path[1024];
    FILE *list;

    if (argc < 5) {
        fprintf(stderr, "usage: %s SHEET IMAGES OUTDIR HALF\n", argv[0]);
        return 2;
    }
    half = atoi(argv[4]);
    sheet = slurp(argv[1], &sn);
    imgs = slurp(argv[2], &in);
    vram = (uint16_t *)calloc((size_t)MV_VRAM_W * MV_VRAM_H, 2);
    if (!sheet || !imgs || !vram || sn < 16) {
        fprintf(stderr, "cannot read %s / %s\n", argv[1], argv[2]);
        return 3;
    }
    if (dw3_mv_disc_load_map_vram(imgs, in, vram) <= 0) {
        fprintf(stderr, "no TIMs placed from %s\n", argv[2]);
        return 3;
    }
    for (e = 0; e < 3; ++e) {
        off[e] = rd32(sheet + 4 * e);
        if (off[e] <= 0 || (size_t)off[e] >= sn) {
            fprintf(stderr, "bad entry %d offset %d\n", e, off[e]);
            return 3;
        }
    }
    if (dw3_mv_compose(vram, (const int32_t *)(sheet + off[0]), sn - (size_t)off[0],
                       0, 0x280, 0, 0x140, 0x100, &base, &box) != 0) {
        fprintf(stderr, "map compose failed\n");
        return 3;
    }
    snprintf(path, sizeof(path), "%s/base_h%d.pam", argv[3], half);
    if (write_pam(path, base.rgba, base.w, base.h) != 0) return 4;
    printf("base h%d: %dx%d box %d,%d\n", half, base.w, base.h, box.x0, box.y0);
    dw3_mv_image_free(&base);

    /* the reveal sprites' pixels: the game loads entry 1 (towns) and entry 2
     * (areas) at y 0x100 */
    if (dw3_mv_disc_load_entry(imgs, in, 1, 0x380, 0x100, vram) <= 0 ||
        dw3_mv_disc_load_entry(imgs, in, 2, 0x300, 0x100, vram) <= 0) {
        fprintf(stderr, "no TIMs placed from entries 1 / 2 of %s\n", argv[2]);
        return 3;
    }
    snprintf(path, sizeof(path), "%s/frames_h%d.txt", argv[3], half);
    list = fopen(path, "w");
    if (!list) return 4;
    for (e = 1; e <= 2; ++e) {
        const uint8_t *sh = sheet + off[e];
        size_t len = sn - (size_t)off[e];
        size_t ids_off = (size_t)rd32(sh + 4);
        int ids[64], n = 0, tex_x;

        for (i = 0; i < 64 && ids_off + (size_t)i < len; ++i) {
            if (sh[ids_off + (size_t)i] == 0xff) break;
            ids[n++] = sh[ids_off + (size_t)i];
        }
        tex_x = e == 1 ? 0x380 : 0x300;
        for (i = 0; i < n; ++i) {
            MvImage tmp;

            if (dw3_mv_sheet_frame_box((const int32_t *)sh, len, ids[i], &box) != 0)
                continue;
            memset(&tmp, 0, sizeof(tmp));
            tmp.w = box.x1 - box.x0;
            tmp.h = box.y1 - box.y0;
            tmp.rgba = (uint8_t *)calloc((size_t)tmp.w * (size_t)tmp.h, 4);
            if (dw3_mv_blit_frame(vram, (const int32_t *)sh, len, ids[i], tex_x,
                                  0x100, 0x140, 0x100, &tmp, -box.x0, -box.y0) != 0) {
                free(tmp.rgba);
                continue;
            }
            snprintf(path, sizeof(path), "%s/%s_h%d_%d.pam", argv[3],
                     e == 1 ? "towns" : "areas", half, ids[i]);
            write_pam(path, tmp.rgba, tmp.w, tmp.h);
            fprintf(list, "%s %d %d %d %d %d\n", e == 1 ? "towns" : "areas",
                    ids[i], box.x0, box.y0, tmp.w, tmp.h);
            free(tmp.rgba);
        }
        printf("%s h%d: %d frames\n", e == 1 ? "towns" : "areas", half, n);
    }
    fclose(list);
    free(sheet);
    free(imgs);
    free(vram);
    return 0;
}
