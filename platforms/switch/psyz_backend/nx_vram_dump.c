/* One-shot SD dumps of the present display rect (CPU VRAM + mesh FBO).
 * host_hello Makefile excludes this TU; CMake game image compiles it when
 * DW3_NX_DUMP_VRAM=1 (needs port/overlays for FIGHTSTG). */
#include "nx_vram_dump.h"

#if DW3_NX_DUMP_VRAM

#include "nx_battle_mode.h"
#include "nx_mesh.h"
#include "nx_vram_pack.h"
#include "overlay_host.h"
#include "nx.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* US / EU FILE_FIGHTSTG — dump only during battle mode. */
#define DW3_FILE_FIGHTSTG_US 0x159
#define DW3_FILE_FIGHTSTG_EU 0x167

/* preview.10: first mesh+ui present had 21 tris and mesh_nonblack=0; real
 * fight-menu frames flush thousands. Wait past the empty/clear prologue. */
#define DW3_NX_DUMP_MIN_MESH_PRIMS 2000ull

#define DW3_NX_DUMP_DIR "sdmc:/switch/dw3"
#define DW3_NX_DUMP_VRAM_RGB555 DW3_NX_DUMP_DIR "/dump_vram_display.rgb555"
#define DW3_NX_DUMP_VRAM_PPM DW3_NX_DUMP_DIR "/dump_vram_display.ppm"
#define DW3_NX_DUMP_MESH_RGBA DW3_NX_DUMP_DIR "/dump_mesh_fbo.rgba"
#define DW3_NX_DUMP_MESH_PPM DW3_NX_DUMP_DIR "/dump_mesh_fbo.ppm"
#define DW3_NX_DUMP_MESH_ALT_PPM DW3_NX_DUMP_DIR "/dump_mesh_fbo_y0.ppm"

static int s_dumped;

static int fightstg_active(void) {
    int32_t mode_file = dw3_overlay_current_mode_file();

    return mode_file == DW3_FILE_FIGHTSTG_US ||
           mode_file == DW3_FILE_FIGHTSTG_EU;
}

static int write_all(FILE *fp, const void *data, size_t n) {
    return fp && data && n > 0 && fwrite(data, 1, n, fp) == n;
}

static int write_ppm_rgb(const char *path, int w, int h, const uint8_t *rgb,
                         size_t rgb_bytes) {
    FILE *fp;
    char header[64];
    int hdr_len;

    if (!path || !rgb || w <= 0 || h <= 0) {
        return 0;
    }
    if (rgb_bytes < (size_t)w * (size_t)h * 3u) {
        return 0;
    }
    fp = fopen(path, "wb");
    if (!fp) {
        return 0;
    }
    hdr_len = snprintf(header, sizeof(header), "P6\n%d %d\n255\n", w, h);
    if (hdr_len <= 0 || !write_all(fp, header, (size_t)hdr_len) ||
        !write_all(fp, rgb, (size_t)w * (size_t)h * 3u)) {
        fclose(fp);
        return 0;
    }
    fclose(fp);
    return 1;
}

static void rgb555_rect_stats(const uint16_t *vram, int dx, int dy, int dw,
                              int dh, uint32_t *out_nonzero,
                              uint32_t *out_nonblack, uint32_t *out_sum_r,
                              uint32_t *out_sum_g, uint32_t *out_sum_b) {
    int row;
    uint32_t nonzero = 0;
    uint32_t nonblack = 0;
    uint32_t sum_r = 0;
    uint32_t sum_g = 0;
    uint32_t sum_b = 0;

    for (row = 0; row < dh; row++) {
        const uint16_t *s =
            vram + ((size_t)(dy + row) * (size_t)DW3_NX_VRAM_W) + (size_t)dx;
        int col;

        for (col = 0; col < dw; col++) {
            uint16_t v = s[col];
            unsigned r = v & 0x1Fu;
            unsigned g = (v >> 5) & 0x1Fu;
            unsigned b = (v >> 10) & 0x1Fu;

            if (v != 0) {
                nonzero++;
            }
            if (r | g | b) {
                nonblack++;
            }
            sum_r += r;
            sum_g += g;
            sum_b += b;
        }
    }
    *out_nonzero = nonzero;
    *out_nonblack = nonblack;
    *out_sum_r = sum_r;
    *out_sum_g = sum_g;
    *out_sum_b = sum_b;
}

static void rgba_stats(const uint8_t *rgba, int n, uint32_t *out_nonzero,
                       uint32_t *out_nonblack, uint32_t *out_sum_r,
                       uint32_t *out_sum_g, uint32_t *out_sum_b) {
    int i;
    uint32_t nonzero = 0;
    uint32_t nonblack = 0;
    uint32_t sum_r = 0;
    uint32_t sum_g = 0;
    uint32_t sum_b = 0;

    for (i = 0; i < n; i++) {
        const uint8_t *p = rgba + (size_t)i * 4u;
        unsigned r = p[0];
        unsigned g = p[1];
        unsigned b = p[2];

        if (r | g | b | p[3]) {
            nonzero++;
        }
        if (r | g | b) {
            nonblack++;
        }
        sum_r += r;
        sum_g += g;
        sum_b += b;
    }
    *out_nonzero = nonzero;
    *out_nonblack = nonblack;
    *out_sum_r = sum_r;
    *out_sum_g = sum_g;
    *out_sum_b = sum_b;
}

static int dump_vram_rgb555(const uint16_t *vram, int dx, int dy, int dw,
                            int dh) {
    FILE *fp;
    int row;
    size_t row_bytes = (size_t)dw * sizeof(uint16_t);

    fp = fopen(DW3_NX_DUMP_VRAM_RGB555, "wb");
    if (!fp) {
        return 0;
    }
    for (row = 0; row < dh; row++) {
        const uint16_t *s =
            vram + ((size_t)(dy + row) * (size_t)DW3_NX_VRAM_W) + (size_t)dx;

        if (!write_all(fp, s, row_bytes)) {
            fclose(fp);
            return 0;
        }
    }
    fclose(fp);
    return 1;
}

static int dump_vram_ppm(const uint16_t *vram, int dx, int dy, int dw, int dh) {
    uint8_t *rgb;
    size_t n = (size_t)dw * (size_t)dh;
    size_t i;
    int row;
    int ok;

    rgb = (uint8_t *)malloc(n * 3u);
    if (!rgb) {
        return 0;
    }
    i = 0;
    for (row = 0; row < dh; row++) {
        const uint16_t *s =
            vram + ((size_t)(dy + row) * (size_t)DW3_NX_VRAM_W) + (size_t)dx;
        int col;

        for (col = 0; col < dw; col++) {
            uint16_t v = s[col];
            unsigned r5 = v & 0x1Fu;
            unsigned g5 = (v >> 5) & 0x1Fu;
            unsigned b5 = (v >> 10) & 0x1Fu;

            rgb[i++] = (uint8_t)((r5 * 255u + 15u) / 31u);
            rgb[i++] = (uint8_t)((g5 * 255u + 15u) / 31u);
            rgb[i++] = (uint8_t)((b5 * 255u + 15u) / 31u);
        }
    }
    ok = write_ppm_rgb(DW3_NX_DUMP_VRAM_PPM, dw, dh, rgb, n * 3u);
    free(rgb);
    return ok;
}

static int dump_mesh_ppm(const char *path, const uint8_t *rgba, int dw,
                         int dh) {
    uint8_t *rgb;
    size_t n = (size_t)dw * (size_t)dh;
    size_t i;
    int ok;

    rgb = (uint8_t *)malloc(n * 3u);
    if (!rgb) {
        return 0;
    }
    for (i = 0; i < n; i++) {
        rgb[i * 3u + 0u] = rgba[i * 4u + 0u];
        rgb[i * 3u + 1u] = rgba[i * 4u + 1u];
        rgb[i * 3u + 2u] = rgba[i * 4u + 2u];
    }
    ok = write_ppm_rgb(path, dw, dh, rgb, n * 3u);
    free(rgb);
    return ok;
}

static int read_mesh_band(int dx, int dy, int dw, int dh, uint8_t **out_rgba,
                          size_t *out_bytes, uint32_t *nonblack,
                          uint32_t *sum_r, uint32_t *sum_g, uint32_t *sum_b) {
    size_t need = (size_t)dw * (size_t)dh * 4u;
    uint8_t *rgba;
    size_t got;
    uint32_t nz = 0;

    *out_rgba = NULL;
    *out_bytes = 0;
    *nonblack = 0;
    *sum_r = *sum_g = *sum_b = 0;
    rgba = (uint8_t *)malloc(need);
    if (!rgba) {
        return 0;
    }
    got = dw3_nx_mesh_read_rect_rgba(dx, dy, dw, dh, rgba, need);
    if (got != need) {
        free(rgba);
        return 0;
    }
    rgba_stats(rgba, dw * dh, &nz, nonblack, sum_r, sum_g, sum_b);
    *out_rgba = rgba;
    *out_bytes = need;
    return 1;
}

void dw3_nx_vram_dump_battle_once(const uint16_t *vram, int dx, int dy, int dw,
                                  int dh, int mesh_active,
                                  uint64_t prims_flushed) {
    uint8_t *mesh_rgba = NULL;
    uint8_t *mesh_alt = NULL;
    size_t mesh_bytes = 0;
    size_t mesh_alt_bytes = 0;
    size_t vram_bytes;
    int vram_ok = 0;
    int vram_ppm_ok = 0;
    int mesh_ok = 0;
    int mesh_ppm_ok = 0;
    int mesh_alt_ok = 0;
    uint32_t v_nonzero = 0;
    uint32_t v_nonblack = 0;
    uint32_t v_sr = 0;
    uint32_t v_sg = 0;
    uint32_t v_sb = 0;
    uint32_t m_nonzero = 0;
    uint32_t m_nonblack = 0;
    uint32_t m_sr = 0;
    uint32_t m_sg = 0;
    uint32_t m_sb = 0;
    uint32_t alt_nonblack = 0;
    uint32_t alt_sr = 0;
    uint32_t alt_sg = 0;
    uint32_t alt_sb = 0;
    uint32_t pixels;
    int alt_y = -1;

    if (s_dumped || !vram || dw <= 0 || dh <= 0) {
        return;
    }
    if (!fightstg_active()) {
        return;
    }
    /* GPU battle modes: the first FIGHTSTG presents are still opaque (no mesh
     * yet). Wait for a mesh+ui frame so the dump pairs CPU VRAM with the FBO. */
    if (!mesh_active && !dw3_nx_battle_mode_use_soft()) {
        return;
    }
    /* Mesh path: skip the prologue present (preview.10: 21 tris, all black).
     * DW3_DUMP_MIN_PRIMS overrides the threshold (steady Fight-menu frame). */
    if (mesh_active) {
        static uint64_t min_prims;

        if (min_prims == 0) {
            const char *env = getenv("DW3_DUMP_MIN_PRIMS");

            min_prims = DW3_NX_DUMP_MIN_MESH_PRIMS;
            if (env != NULL && env[0] != '\0') {
                unsigned long long v = strtoull(env, NULL, 0);

                if (v > 0) {
                    min_prims = (uint64_t)v;
                }
            }
        }
        if (prims_flushed < min_prims) {
            return;
        }
    }

    s_dumped = 1;
    pixels = (uint32_t)dw * (uint32_t)dh;
    vram_bytes = (size_t)pixels * sizeof(uint16_t);

    vram_ok = dump_vram_rgb555(vram, dx, dy, dw, dh);
    vram_ppm_ok = dump_vram_ppm(vram, dx, dy, dw, dh);
    rgb555_rect_stats(vram, dx, dy, dw, dh, &v_nonzero, &v_nonblack, &v_sr,
                      &v_sg, &v_sb);

    if (mesh_active && dw3_nx_mesh_available()) {
        if (read_mesh_band(dx, dy, dw, dh, &mesh_rgba, &mesh_bytes, &m_nonblack,
                           &m_sr, &m_sg, &m_sb)) {
            FILE *fp = fopen(DW3_NX_DUMP_MESH_RGBA, "wb");

            if (fp && write_all(fp, mesh_rgba, mesh_bytes)) {
                mesh_ok = 1;
            }
            if (fp) {
                fclose(fp);
            }
            mesh_ppm_ok =
                dump_mesh_ppm(DW3_NX_DUMP_MESH_PPM, mesh_rgba, dw, dh);
            free(mesh_rgba);
            mesh_rgba = NULL;
        }
        /* Other double-buffer band: if display is y=256, also dump y=0. */
        alt_y = (dy >= 240) ? 0 : 256;
        if (alt_y + dh <= DW3_NX_VRAM_H &&
            read_mesh_band(dx, alt_y, dw, dh, &mesh_alt, &mesh_alt_bytes,
                           &alt_nonblack, &alt_sr, &alt_sg, &alt_sb)) {
            mesh_alt_ok =
                dump_mesh_ppm(DW3_NX_DUMP_MESH_ALT_PPM, mesh_alt, dw, dh);
            free(mesh_alt);
            mesh_alt = NULL;
        }
    }
    (void)alt_sr;
    (void)alt_sg;
    (void)alt_sb;
    (void)m_nonzero;
    (void)v_nonzero;

    psyz_log(1, "nx_vram_dump.c", __LINE__, __func__,
             "vram dump: mesh_active=%d prims=%llu rect=%d,%d %dx%d "
             "vram_rgb555=%s (%zu B) vram_ppm=%s mesh_rgba=%s (%zu B) "
             "mesh_ppm=%s alt_ppm=%s vram_nonblack=%u/%u "
             "meanRGB555=%.1f,%.1f,%.1f mesh_nonblack=%u/%u "
             "meanRGBA8=%.1f,%.1f,%.1f alt_y=%d alt_nonblack=%u/%u "
             "paths=" DW3_NX_DUMP_DIR "/",
             mesh_active, (unsigned long long)prims_flushed, dx, dy, dw, dh,
             vram_ok ? "ok" : "fail", vram_bytes, vram_ppm_ok ? "ok" : "fail",
             mesh_ok ? "ok" : "fail", mesh_ok ? mesh_bytes : 0u,
             mesh_ppm_ok ? "ok" : "fail", mesh_alt_ok ? "ok" : "fail",
             v_nonblack, pixels,
             pixels ? (double)v_sr / (double)pixels : 0.0,
             pixels ? (double)v_sg / (double)pixels : 0.0,
             pixels ? (double)v_sb / (double)pixels : 0.0, m_nonblack, pixels,
             pixels ? (double)m_sr / (double)pixels : 0.0,
             pixels ? (double)m_sg / (double)pixels : 0.0,
             pixels ? (double)m_sb / (double)pixels : 0.0, alt_y, alt_nonblack,
             pixels);
}

#endif /* DW3_NX_DUMP_VRAM */
