/* P6.32 phase 0: the full-screen map screen on the PC SDL3 GPU present.
 *
 * Draws over the whole 16:9 picture: a black backdrop, the map texture (the
 * captured map, or a CPU-scaled copy of it) placed and zoomed by a viewport
 * larger than the window with a scissor to the picture, then the HUD canvas.
 * It reuses the title overlay's pipeline and quad buffer
 * (title_menu_overlay.c, dw3_pc_overlay_gpu_ctx). The state, the scalers and
 * the pad live in port/map_view/. */
#include <SDL3/SDL.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "map_view.h"

typedef struct Dw3PcOverlayGpu {
    SDL_GPUDevice *device;
    SDL_Window *window;
    SDL_GPUGraphicsPipeline *pipeline;
    SDL_GPUBuffer *vertices;
    unsigned quad_first; /* first vertex of a full 0..1 quad in `vertices` */
    float pic_x, pic_y, pic_w, pic_h; /* the whole picture in window pixels */
} Dw3PcOverlayGpu;
extern int dw3_pc_overlay_gpu_ctx(Dw3PcOverlayGpu *ctx);

static SDL_GPUTexture *g_map_tex, *g_hud_tex, *g_black_tex;
static SDL_GPUTexture *g_bank_tex[DW3_MV_BANK_N]; /* the game's cursor and balloon */
static unsigned g_bank_gen = ~0u;
static SDL_GPUSampler *g_linear, *g_nearest;
static SDL_GPUDevice *g_dev;
static unsigned g_map_gen = ~0u, g_hud_gen = ~0u;
static int g_map_w, g_map_h, g_idle, g_logged_draw;

static void release_all(void) {
    if (!g_dev) return;
    if (g_map_tex) SDL_ReleaseGPUTexture(g_dev, g_map_tex);
    if (g_hud_tex) SDL_ReleaseGPUTexture(g_dev, g_hud_tex);
    if (g_black_tex) SDL_ReleaseGPUTexture(g_dev, g_black_tex);
    if (g_linear) SDL_ReleaseGPUSampler(g_dev, g_linear);
    if (g_nearest) SDL_ReleaseGPUSampler(g_dev, g_nearest);
    {
        int i;

        for (i = 0; i < DW3_MV_BANK_N; ++i) {
            if (g_bank_tex[i]) SDL_ReleaseGPUTexture(g_dev, g_bank_tex[i]);
            g_bank_tex[i] = NULL;
        }
        g_bank_gen = ~0u;
    }
    g_map_tex = g_hud_tex = g_black_tex = NULL;
    g_linear = g_nearest = NULL;
    g_map_gen = g_hud_gen = ~0u;
    g_map_w = g_map_h = 0;
    g_dev = NULL;
}

static SDL_GPUTexture *make_texture(SDL_GPUDevice *dev, int w, int h) {
    const SDL_GPUTextureCreateInfo info = {
        .type = SDL_GPU_TEXTURETYPE_2D,
        .format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM,
        .usage = SDL_GPU_TEXTUREUSAGE_SAMPLER,
        .width = (Uint32)w,
        .height = (Uint32)h,
        .layer_count_or_depth = 1,
        .num_levels = 1,
        .sample_count = SDL_GPU_SAMPLECOUNT_1,
    };
    return SDL_CreateGPUTexture(dev, &info);
}

static SDL_GPUSampler *make_sampler(SDL_GPUDevice *dev, SDL_GPUFilter f) {
    const SDL_GPUSamplerCreateInfo info = {
        .min_filter = f,
        .mag_filter = f,
        .mipmap_mode = SDL_GPU_SAMPLERMIPMAPMODE_NEAREST,
        .address_mode_u = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE,
        .address_mode_v = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE,
        .address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE,
    };
    return SDL_CreateGPUSampler(dev, &info);
}

static int upload(SDL_GPUDevice *dev, SDL_GPUTexture *tex, const uint8_t *rgba,
                  int w, int h) {
    const size_t bytes = (size_t)w * (size_t)h * 4;
    const SDL_GPUTransferBufferCreateInfo ti = {
        .usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD, .size = (Uint32)bytes,
    };
    SDL_GPUTransferBuffer *tb = SDL_CreateGPUTransferBuffer(dev, &ti);
    uint8_t *mapped;
    SDL_GPUCommandBuffer *cmd;
    SDL_GPUCopyPass *copy;
    SDL_GPUTextureTransferInfo src;
    SDL_GPUTextureRegion dst;

    if (!tb) return -1;
    mapped = (uint8_t *)SDL_MapGPUTransferBuffer(dev, tb, false);
    if (!mapped) {
        SDL_ReleaseGPUTransferBuffer(dev, tb);
        return -1;
    }
    memcpy(mapped, rgba, bytes);
    SDL_UnmapGPUTransferBuffer(dev, tb);
    cmd = SDL_AcquireGPUCommandBuffer(dev);
    if (!cmd) {
        SDL_ReleaseGPUTransferBuffer(dev, tb);
        return -1;
    }
    copy = SDL_BeginGPUCopyPass(cmd);
    memset(&src, 0, sizeof(src));
    src.transfer_buffer = tb;
    src.pixels_per_row = (Uint32)w;
    src.rows_per_layer = (Uint32)h;
    memset(&dst, 0, sizeof(dst));
    dst.texture = tex;
    dst.w = (Uint32)w;
    dst.h = (Uint32)h;
    dst.d = 1;
    SDL_UploadToGPUTexture(copy, &src, &dst, false);
    SDL_EndGPUCopyPass(copy);
    SDL_SubmitGPUCommandBuffer(cmd);
    SDL_WaitForGPUIdle(dev);
    SDL_ReleaseGPUTransferBuffer(dev, tb);
    return 0;
}

/* Frame callback (outside the render pass): bring the textures up to date. */
void dw3_pc_map_view_prepare(void) {
    Dw3MapViewDraw d;
    Dw3PcOverlayGpu ctx;

    dw3_map_view_get_draw(&d);
    if (!d.active) {
        if (g_dev && ++g_idle >= 30) {
            fprintf(stderr, "dw3-map: pc textures released\n");
            release_all();
        }
        g_logged_draw = 0;
        return;
    }
    g_idle = 0;
    if (!dw3_pc_overlay_gpu_ctx(&ctx) || !ctx.device) return;
    if (g_dev != ctx.device) {
        release_all();
        g_dev = ctx.device;
    }
    if (!g_linear) g_linear = make_sampler(g_dev, SDL_GPU_FILTER_LINEAR);
    if (!g_nearest) g_nearest = make_sampler(g_dev, SDL_GPU_FILTER_NEAREST);
    if (!g_black_tex) {
        const uint8_t black[4] = {0, 0, 0, 255};

        g_black_tex = make_texture(g_dev, 1, 1);
        if (g_black_tex) upload(g_dev, g_black_tex, black, 1, 1);
    }
    if (!g_hud_tex) g_hud_tex = make_texture(g_dev, DW3_MV_HUD_W, DW3_MV_HUD_H);
    if (d.tex_gen != g_map_gen && d.tex_rgba && d.tex_w > 0) {
        if (!g_map_tex || g_map_w != d.tex_w || g_map_h != d.tex_h) {
            if (g_map_tex) SDL_ReleaseGPUTexture(g_dev, g_map_tex);
            g_map_tex = make_texture(g_dev, d.tex_w, d.tex_h);
            g_map_w = d.tex_w;
            g_map_h = d.tex_h;
        }
        if (g_map_tex && upload(g_dev, g_map_tex, d.tex_rgba, d.tex_w, d.tex_h) == 0) {
            g_map_gen = d.tex_gen;
            dw3_map_view_tex_taken(d.tex_gen);
            fprintf(stderr, "dw3-map: pc texture %dx%d uploaded (%s)\n", d.tex_w,
                    d.tex_h, d.linear ? "linear" : "nearest");
        }
    }
    if (d.bank_gen != g_bank_gen) {
        int i;

        for (i = 0; i < DW3_MV_BANK_N; ++i) {
            if (g_bank_tex[i]) SDL_ReleaseGPUTexture(g_dev, g_bank_tex[i]);
            g_bank_tex[i] = NULL;
            if (!d.bank[i].rgba || d.bank[i].w <= 0) continue;
            g_bank_tex[i] = make_texture(g_dev, d.bank[i].w, d.bank[i].h);
            if (g_bank_tex[i] &&
                upload(g_dev, g_bank_tex[i], d.bank[i].rgba, d.bank[i].w,
                       d.bank[i].h) != 0) {
                SDL_ReleaseGPUTexture(g_dev, g_bank_tex[i]);
                g_bank_tex[i] = NULL;
            }
        }
        g_bank_gen = d.bank_gen;
        fprintf(stderr, "dw3-map: pc sprite bank uploaded (gen %u)\n", d.bank_gen);
    }
    if (d.hud_gen != g_hud_gen && d.hud_rgba && g_hud_tex &&
        upload(g_dev, g_hud_tex, d.hud_rgba, DW3_MV_HUD_W, DW3_MV_HUD_H) == 0) {
        g_hud_gen = d.hud_gen;
    }
}

void dw3_pc_map_view_destroy(void) { release_all(); }

/* One pass over the picture, clipped to `clip` (window pixels): the backdrop,
 * the map, the game's sprites on it (map space), the HUD canvas. */
static void draw_layers(SDL_GPURenderPass *pass, const Dw3PcOverlayGpu *ctx,
                        const Dw3MapViewDraw *d, const SDL_Rect *clip) {
    SDL_GPUViewport vp;
    SDL_GPUTextureSamplerBinding tb;
    int i;

    if (clip->w <= 0 || clip->h <= 0) return;
    memset(&vp, 0, sizeof(vp));
    vp.min_depth = 0.0f;
    vp.max_depth = 1.0f;
    SDL_SetGPUScissor(pass, clip);

    /* backdrop */
    vp.x = ctx->pic_x; vp.y = ctx->pic_y; vp.w = ctx->pic_w; vp.h = ctx->pic_h;
    SDL_SetGPUViewport(pass, &vp);
    tb.texture = g_black_tex; tb.sampler = g_nearest;
    SDL_BindGPUFragmentSamplers(pass, 0, &tb, 1);
    SDL_DrawGPUPrimitives(pass, 6, 1, ctx->quad_first, 0);

    /* the map: the quad fills the viewport, which is the map's rectangle */
    vp.x = ctx->pic_x + d->x0 * ctx->pic_w;
    vp.y = ctx->pic_y + d->y0 * ctx->pic_h;
    vp.w = (d->x1 - d->x0) * ctx->pic_w;
    vp.h = (d->y1 - d->y0) * ctx->pic_h;
    SDL_SetGPUViewport(pass, &vp);
    tb.texture = g_map_tex; tb.sampler = d->linear ? g_linear : g_nearest;
    SDL_BindGPUFragmentSamplers(pass, 0, &tb, 1);
    SDL_DrawGPUPrimitives(pass, 6, 1, ctx->quad_first, 0);

    /* the cursor and the balloon, nearest, in map space like the map */
    for (i = 0; i < d->nspr; ++i) {
        const Dw3MapViewSprite *sp = &d->spr[i];

        if (sp->bank < 0 || sp->bank >= DW3_MV_BANK_N || !g_bank_tex[sp->bank])
            continue;
        vp.x = ctx->pic_x + sp->x0 * ctx->pic_w;
        vp.y = ctx->pic_y + sp->y0 * ctx->pic_h;
        vp.w = (sp->x1 - sp->x0) * ctx->pic_w;
        vp.h = (sp->y1 - sp->y0) * ctx->pic_h;
        SDL_SetGPUViewport(pass, &vp);
        tb.texture = g_bank_tex[sp->bank]; tb.sampler = g_nearest;
        SDL_BindGPUFragmentSamplers(pass, 0, &tb, 1);
        SDL_DrawGPUPrimitives(pass, 6, 1, ctx->quad_first, 0);
    }

    /* the HUD canvas over the whole picture */
    vp.x = ctx->pic_x; vp.y = ctx->pic_y; vp.w = ctx->pic_w; vp.h = ctx->pic_h;
    SDL_SetGPUViewport(pass, &vp);
    tb.texture = g_hud_tex; tb.sampler = g_nearest;
    SDL_BindGPUFragmentSamplers(pass, 0, &tb, 1);
    SDL_DrawGPUPrimitives(pass, 6, 1, ctx->quad_first, 0);
}

/* Render callback (inside the pass, over the finished game picture). */
void dw3_pc_map_view_render(SDL_GPURenderPass *pass) {
    Dw3MapViewDraw d;
    Dw3PcOverlayGpu ctx;
    SDL_GPUBufferBinding binding;
    SDL_Rect scissor, pic;
    int ww = 0, wh = 0;

    dw3_map_view_get_draw(&d);
    if (!d.active || !g_map_tex || !g_black_tex || !g_hud_tex ||
        g_map_gen != d.tex_gen || !g_linear || !g_nearest) return;
    if (!dw3_pc_overlay_gpu_ctx(&ctx) || !ctx.pipeline || !ctx.vertices) return;
    SDL_GetWindowSizeInPixels(ctx.window, &ww, &wh);
    binding.buffer = ctx.vertices;
    binding.offset = 0;
    SDL_BindGPUGraphicsPipeline(pass, ctx.pipeline);
    SDL_BindGPUVertexBuffers(pass, 0, &binding, 1);
    pic.x = (int)(ctx.pic_x + 0.5f);
    pic.y = (int)(ctx.pic_y + 0.5f);
    pic.w = (int)(ctx.pic_w + 0.5f);
    pic.h = (int)(ctx.pic_h + 0.5f);
    if (d.hole) {
        /* the game's name box: its rectangle of the picture is left to the
         * game's own pixels, so the picture is drawn in up to four strips
         * around it */
        SDL_Rect h, r;
        int hx0 = (int)(ctx.pic_x + d.hx0 * ctx.pic_w + 0.5f);
        int hx1 = (int)(ctx.pic_x + d.hx1 * ctx.pic_w + 0.5f);
        int hy0 = (int)(ctx.pic_y + d.hy0 * ctx.pic_h + 0.5f);
        int hy1 = (int)(ctx.pic_y + d.hy1 * ctx.pic_h + 0.5f);

        if (hx0 < pic.x) hx0 = pic.x;
        if (hy0 < pic.y) hy0 = pic.y;
        if (hx1 > pic.x + pic.w) hx1 = pic.x + pic.w;
        if (hy1 > pic.y + pic.h) hy1 = pic.y + pic.h;
        h.x = hx0; h.y = hy0; h.w = hx1 - hx0; h.h = hy1 - hy0;
        r.x = pic.x; r.y = pic.y; r.w = pic.w; r.h = h.y - pic.y;
        draw_layers(pass, &ctx, &d, &r);                       /* above */
        r.y = h.y + h.h; r.h = pic.y + pic.h - r.y;
        draw_layers(pass, &ctx, &d, &r);                       /* below */
        r.x = pic.x; r.y = h.y; r.w = h.x - pic.x; r.h = h.h;
        draw_layers(pass, &ctx, &d, &r);                       /* left */
        r.x = h.x + h.w; r.w = pic.x + pic.w - r.x;
        draw_layers(pass, &ctx, &d, &r);                       /* right */
    } else {
        draw_layers(pass, &ctx, &d, &pic);
    }
    scissor.x = 0; scissor.y = 0; scissor.w = ww; scissor.h = wh;
    SDL_SetGPUScissor(pass, &scissor);
    if (!g_logged_draw) {
        g_logged_draw = 1;
        fprintf(stderr, "dw3-map: pc draw picture %.0f,%.0f %.0fx%.0f in window "
                "%dx%d\n", (double)ctx.pic_x, (double)ctx.pic_y,
                (double)ctx.pic_w, (double)ctx.pic_h, ww, wh);
    }
}
