#include "title_overlay_art.h"

#include <SDL3/SDL.h>
#include <psyz/overlay.h>
#include <psyz/overlay_sdl3_gpu.h>
#include "port_options.h"

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(_WIN32)
#include "shaders/title_overlay_vert_dxil.h"
#include "shaders/title_overlay_frag_dxil.h"
#define OVERLAY_VERT title_overlay_vert_dxil
#define OVERLAY_VERT_LEN title_overlay_vert_dxil_len
#define OVERLAY_FRAG title_overlay_frag_dxil
#define OVERLAY_FRAG_LEN title_overlay_frag_dxil_len
#define OVERLAY_SHADER_FORMAT SDL_GPU_SHADERFORMAT_DXIL
#elif defined(__APPLE__)
#include "shaders/title_overlay_vert_msl.h"
#include "shaders/title_overlay_frag_msl.h"
#define OVERLAY_VERT title_overlay_vert_msl
#define OVERLAY_VERT_LEN title_overlay_vert_msl_len
#define OVERLAY_FRAG title_overlay_frag_msl
#define OVERLAY_FRAG_LEN title_overlay_frag_msl_len
#define OVERLAY_SHADER_FORMAT SDL_GPU_SHADERFORMAT_MSL
#else
#include "shaders/title_overlay_vert_spv.h"
#include "shaders/title_overlay_frag_spv.h"
#define OVERLAY_VERT title_overlay_vert_spv
#define OVERLAY_VERT_LEN title_overlay_vert_spv_len
#define OVERLAY_FRAG title_overlay_frag_spv
#define OVERLAY_FRAG_LEN title_overlay_frag_spv_len
#define OVERLAY_SHADER_FORMAT SDL_GPU_SHADERFORMAT_SPIRV
#endif

/* PRESS START, the three focused menu rows, and the Port options screen. */
enum { CANVAS_W = 960, CANVAS_H = 720, STATE_COUNT = 5, STATE_OPTIONS = 4 };
#define ART_TEXTURE_H (CANVAS_H * STATE_COUNT)

typedef struct OverlayVertex {
    float position[2];
    float uv[2];
} OverlayVertex;

static SDL_Window *g_window;
static SDL_GPUDevice *g_device;
static SDL_GPUGraphicsPipeline *g_pipeline;
static SDL_GPUBuffer *g_vertices;
static SDL_GPUTexture *g_texture;
static SDL_GPUTexture *g_logo_texture;
static SDL_GPUTexture *g_background_texture;
static SDL_GPUSampler *g_background_sampler;
static int g_background_ready;
static SDL_GPUSampler *g_sampler;
static SDL_GPUSampler *g_logo_sampler;
static int g_selection;
static int g_overlay_active;
static int g_logo_active;
static int g_logo_texture_ready;
static int g_presentation_size_checked;
static unsigned g_art_generation;

/* The 3x mode renders a 960x720 image. Start that opt-in mode in a 16:9
 * window so the image and native-resolution UI remain 1:1 with 160px bars. */
static void set_three_x_presentation_size(void) {
    const char *internal_res;
    float density;
    int width, height;

    if (g_presentation_size_checked || !g_window ||
        (SDL_GetWindowFlags(g_window) & SDL_WINDOW_HIDDEN)) return;
    internal_res = getenv("DW3_PC_INTERNAL_RES");
    if (!internal_res || strcmp(internal_res, "3") != 0) {
        g_presentation_size_checked = 1;
        return;
    }
    g_presentation_size_checked = 1;
    SDL_GetWindowSizeInPixels(g_window, &width, &height);
    if (width == 1280 && height == 720) return;
    density = SDL_GetWindowPixelDensity(g_window);
    if (density <= 0.0f) density = 1.0f;
    SDL_SetWindowSize(g_window,
                      (int)(1280.0f / density + 0.5f),
                      (int)(720.0f / density + 0.5f));
    SDL_SetWindowPosition(g_window, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
    fprintf(stderr,
            "dw3-title-overlay: 3x presentation window set to 1280x720 pixels\n");
}

static SDL_GPUShader *create_shader(const unsigned char *code, unsigned len,
                                    SDL_GPUShaderStage stage,
                                    Uint32 sampler_count) {
    const SDL_GPUShaderCreateInfo info = {
        .code = code,
        .code_size = len,
        .format = OVERLAY_SHADER_FORMAT,
#if defined(__APPLE__)
        .entrypoint = "main0",
#else
        .entrypoint = "main",
#endif
        .stage = stage,
        .num_samplers = sampler_count,
    };
    return SDL_CreateGPUShader(g_device, &info);
}

static void release_gpu_resources(void) {
    g_presentation_size_checked = 0;
    if (!g_device) return;
    if (g_pipeline) SDL_ReleaseGPUGraphicsPipeline(g_device, g_pipeline);
    if (g_vertices) SDL_ReleaseGPUBuffer(g_device, g_vertices);
    if (g_texture) SDL_ReleaseGPUTexture(g_device, g_texture);
    if (g_logo_texture) SDL_ReleaseGPUTexture(g_device, g_logo_texture);
    if (g_background_texture) SDL_ReleaseGPUTexture(g_device, g_background_texture);
    if (g_background_sampler) SDL_ReleaseGPUSampler(g_device, g_background_sampler);
    if (g_sampler) SDL_ReleaseGPUSampler(g_device, g_sampler);
    if (g_logo_sampler) SDL_ReleaseGPUSampler(g_device, g_logo_sampler);
    g_pipeline = NULL;
    g_vertices = NULL;
    g_texture = NULL;
    g_logo_texture = NULL;
    g_background_texture = NULL;
    g_background_sampler = NULL;
    g_background_ready = 0;
    g_sampler = NULL;
    g_logo_sampler = NULL;
    g_window = NULL;
    g_device = NULL;
    g_overlay_active = 0;
    g_logo_active = 0;
    g_logo_texture_ready = 0;
}

static int upload_art_and_quad(void) {
    const size_t canvas_bytes = (size_t)CANVAS_W * CANVAS_H * 4;
    const size_t texture_bytes = canvas_bytes * STATE_COUNT;
    uint8_t *pixels = (uint8_t *)malloc(texture_bytes);
    if (!pixels) return -1;
    g_art_generation = dw3_pc_title_overlay_generation();
    for (int state = 0; state < STATE_COUNT; ++state) {
        const int selected = state == 0 ? -1 :
                             state == STATE_OPTIONS ? DW3_TITLE_ART_OPTIONS :
                             state - 1;
        dw3_pc_title_overlay_art(pixels + (size_t)state * canvas_bytes,
                                 CANVAS_W * 4, selected);
    }

    const SDL_GPUTextureCreateInfo texture_info = {
        .type = SDL_GPU_TEXTURETYPE_2D,
        .format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM,
        .usage = SDL_GPU_TEXTUREUSAGE_SAMPLER,
        .width = CANVAS_W,
        .height = ART_TEXTURE_H,
        .layer_count_or_depth = 1,
        .num_levels = 1,
        .sample_count = SDL_GPU_SAMPLECOUNT_1,
    };
    g_texture = SDL_CreateGPUTexture(g_device, &texture_info);
    const SDL_GPUSamplerCreateInfo sampler_info = {
        .min_filter = SDL_GPU_FILTER_LINEAR,
        .mag_filter = SDL_GPU_FILTER_LINEAR,
        .mipmap_mode = SDL_GPU_SAMPLERMIPMAPMODE_NEAREST,
        .address_mode_u = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE,
        .address_mode_v = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE,
        .address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE,
    };
    g_sampler = SDL_CreateGPUSampler(g_device, &sampler_info);
    const SDL_GPUTextureCreateInfo logo_texture_info = {
        .type = SDL_GPU_TEXTURETYPE_2D,
        .format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM,
        .usage = SDL_GPU_TEXTUREUSAGE_SAMPLER,
        .width = 720,
        .height = 326,
        .layer_count_or_depth = 1,
        .num_levels = 1,
        .sample_count = SDL_GPU_SAMPLECOUNT_1,
    };
    g_logo_texture = SDL_CreateGPUTexture(g_device, &logo_texture_info);
    const SDL_GPUSamplerCreateInfo logo_sampler_info = {
        .min_filter = SDL_GPU_FILTER_LINEAR,
        .mag_filter = SDL_GPU_FILTER_LINEAR,
        .mipmap_mode = SDL_GPU_SAMPLERMIPMAPMODE_NEAREST,
        .address_mode_u = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE,
        .address_mode_v = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE,
        .address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE,
    };
    g_logo_sampler = SDL_CreateGPUSampler(g_device, &logo_sampler_info);
    if (!g_texture || !g_sampler || !g_logo_texture || !g_logo_sampler) {
        free(pixels);
        return -1;
    }

    const SDL_GPUTextureCreateInfo background_info = {
        .type = SDL_GPU_TEXTURETYPE_2D,
        .format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM,
        .usage = SDL_GPU_TEXTUREUSAGE_SAMPLER,
        .width = 320, .height = 240,
        .layer_count_or_depth = 1, .num_levels = 1,
        .sample_count = SDL_GPU_SAMPLECOUNT_1,
    };
    g_background_texture = SDL_CreateGPUTexture(g_device, &background_info);
    SDL_GPUSamplerCreateInfo background_sampler_info = sampler_info;
    background_sampler_info.min_filter = SDL_GPU_FILTER_NEAREST;
    background_sampler_info.mag_filter = SDL_GPU_FILTER_NEAREST;
    g_background_sampler = SDL_CreateGPUSampler(g_device, &background_sampler_info);
    if (!g_background_texture || !g_background_sampler) {
        free(pixels);
        return -1;
    }

    OverlayVertex quads[STATE_COUNT * 6 + 24];
    for (int row = 0; row < STATE_COUNT; ++row) {
        const float u0 = 0.0f;
        const float u1 = 1.0f;
        const float v0 = (float)(row * CANVAS_H) / (float)ART_TEXTURE_H;
        const float v1 = (float)((row + 1) * CANVAS_H) /
                         (float)ART_TEXTURE_H;
        const OverlayVertex a = {{0.0f, 0.0f}, {u0, v0}};
        const OverlayVertex b = {{1.0f, 0.0f}, {u1, v0}};
        const OverlayVertex c = {{1.0f, 1.0f}, {u1, v1}};
        const OverlayVertex d = {{0.0f, 1.0f}, {u0, v1}};
        OverlayVertex *q = &quads[row * 6];
        q[0] = a; q[1] = b; q[2] = c;
        q[3] = a; q[4] = c; q[5] = d;
    }
    {
        const OverlayVertex a = {{120.0f / CANVAS_W, 47.0f / CANVAS_H}, {0, 0}};
        const OverlayVertex b = {{840.0f / CANVAS_W, 47.0f / CANVAS_H}, {1, 0}};
        const OverlayVertex c = {{840.0f / CANVAS_W, 373.0f / CANVAS_H}, {1, 1}};
        const OverlayVertex d = {{120.0f / CANVAS_W, 373.0f / CANVAS_H}, {0, 1}};
        OverlayVertex *q = &quads[STATE_COUNT * 6];
        q[0] = a; q[1] = b; q[2] = c;
        q[3] = a; q[4] = c; q[5] = d;
    }

    /* Extend only the isolated animated backdrop. Each side samples the
     * original outer sixth in reverse so its seam meets the native edge.
     * The center (and every foreground sprite) keeps its original geometry. */
    for (int side = 0; side < 2; ++side) {
        const float u0 = side ? 1.0f : 1.0f / 6.0f;
        const float u1 = side ? 5.0f / 6.0f : 0.0f;
        const OverlayVertex a = {{0, 0}, {u0, 0}};
        const OverlayVertex b = {{1, 0}, {u1, 0}};
        const OverlayVertex c = {{1, 1}, {u1, 1}};
        const OverlayVertex d = {{0, 1}, {u0, 1}};
        OverlayVertex *q = &quads[(STATE_COUNT + 1 + side) * 6];
        q[0] = a; q[1] = b; q[2] = c;
        q[3] = a; q[4] = c; q[5] = d;
    }

    /* The options screen's dim, for the backdrop's side extensions: one texel
     * of its uniformly dimmed corner stretched over the quad. */
    {
        const float u = 0.5f / CANVAS_W;
        const float v = (STATE_OPTIONS * CANVAS_H + 0.5f) / ART_TEXTURE_H;
        const OverlayVertex a = {{0, 0}, {u, v}};
        const OverlayVertex b = {{1, 0}, {u, v}};
        const OverlayVertex c = {{1, 1}, {u, v}};
        const OverlayVertex d = {{0, 1}, {u, v}};
        OverlayVertex *q = &quads[(STATE_COUNT + 3) * 6];
        q[0] = a; q[1] = b; q[2] = c;
        q[3] = a; q[4] = c; q[5] = d;
    }

    SDL_GPUShader *vs = create_shader(OVERLAY_VERT, OVERLAY_VERT_LEN,
                                      SDL_GPU_SHADERSTAGE_VERTEX, 0);
    SDL_GPUShader *fs = create_shader(OVERLAY_FRAG, OVERLAY_FRAG_LEN,
                                      SDL_GPU_SHADERSTAGE_FRAGMENT, 1);
    if (!vs || !fs) {
        if (vs) SDL_ReleaseGPUShader(g_device, vs);
        if (fs) SDL_ReleaseGPUShader(g_device, fs);
        free(pixels);
        return -1;
    }
    const SDL_GPUVertexBufferDescription vb_desc = {
        .slot = 0,
        .pitch = sizeof(OverlayVertex),
        .input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX,
    };
    const SDL_GPUVertexAttribute attrs[] = {
        {.location = 0, .buffer_slot = 0,
         .format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2,
         .offset = offsetof(OverlayVertex, position)},
        {.location = 1, .buffer_slot = 0,
         .format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2,
         .offset = offsetof(OverlayVertex, uv)},
    };
    const SDL_GPUColorTargetBlendState blend = {
        .src_color_blendfactor = SDL_GPU_BLENDFACTOR_SRC_ALPHA,
        .dst_color_blendfactor = SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA,
        .color_blend_op = SDL_GPU_BLENDOP_ADD,
        .src_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE,
        .dst_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA,
        .alpha_blend_op = SDL_GPU_BLENDOP_ADD,
        .enable_blend = true,
    };
    const SDL_GPUColorTargetDescription target = {
        .format = SDL_GetGPUSwapchainTextureFormat(g_device, g_window),
        .blend_state = blend,
    };
    const SDL_GPUGraphicsPipelineCreateInfo pipeline_info = {
        .vertex_shader = vs,
        .fragment_shader = fs,
        .vertex_input_state = {
            .vertex_buffer_descriptions = &vb_desc,
            .num_vertex_buffers = 1,
            .vertex_attributes = attrs,
            .num_vertex_attributes = 2,
        },
        .primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST,
        .target_info = {
            .color_target_descriptions = &target,
            .num_color_targets = 1,
        },
    };
    g_pipeline = SDL_CreateGPUGraphicsPipeline(g_device, &pipeline_info);
    SDL_ReleaseGPUShader(g_device, vs);
    SDL_ReleaseGPUShader(g_device, fs);
    if (!g_pipeline) {
        free(pixels);
        return -1;
    }

    const SDL_GPUBufferCreateInfo buffer_info = {
        .usage = SDL_GPU_BUFFERUSAGE_VERTEX,
        .size = sizeof(quads),
    };
    g_vertices = SDL_CreateGPUBuffer(g_device, &buffer_info);
    const SDL_GPUTransferBufferCreateInfo transfer_info = {
        .usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD,
        .size = (Uint32)(texture_bytes + sizeof(quads)),
    };
    SDL_GPUTransferBuffer *transfer =
        SDL_CreateGPUTransferBuffer(g_device, &transfer_info);
    if (!g_vertices || !transfer) {
        if (transfer) SDL_ReleaseGPUTransferBuffer(g_device, transfer);
        free(pixels);
        return -1;
    }
    uint8_t *mapped = (uint8_t *)SDL_MapGPUTransferBuffer(g_device, transfer,
                                                           false);
    if (!mapped) {
        SDL_ReleaseGPUTransferBuffer(g_device, transfer);
        free(pixels);
        return -1;
    }
    memcpy(mapped, pixels, texture_bytes);
    memcpy(mapped + texture_bytes, quads, sizeof(quads));
    free(pixels);
    SDL_UnmapGPUTransferBuffer(g_device, transfer);

    SDL_GPUCommandBuffer *cmd = SDL_AcquireGPUCommandBuffer(g_device);
    if (!cmd) {
        SDL_ReleaseGPUTransferBuffer(g_device, transfer);
        return -1;
    }
    SDL_GPUCopyPass *copy = SDL_BeginGPUCopyPass(cmd);
    const SDL_GPUTextureTransferInfo texture_src = {
        .transfer_buffer = transfer,
        .pixels_per_row = CANVAS_W,
        .rows_per_layer = ART_TEXTURE_H,
    };
    const SDL_GPUTextureRegion texture_dst = {
        .texture = g_texture,
        .w = CANVAS_W,
        .h = ART_TEXTURE_H,
        .d = 1,
    };
    SDL_UploadToGPUTexture(copy, &texture_src, &texture_dst, false);
    const SDL_GPUTransferBufferLocation buffer_src = {
        .transfer_buffer = transfer,
        .offset = (Uint32)texture_bytes,
    };
    const SDL_GPUBufferRegion buffer_dst = {
        .buffer = g_vertices,
        .size = sizeof(quads),
    };
    SDL_UploadToGPUBuffer(copy, &buffer_src, &buffer_dst, true);
    SDL_EndGPUCopyPass(copy);
    SDL_SubmitGPUCommandBuffer(cmd);
    SDL_WaitForGPUIdle(g_device);
    SDL_ReleaseGPUTransferBuffer(g_device, transfer);
    fprintf(stderr,
            "dw3-title-overlay: %dx%d RGBA texture (%zu bytes), "
            "4 states, 24 vertices and 1 draw/frame\n",
            CANVAS_W, ART_TEXTURE_H, texture_bytes);
    return 0;
}

static void init_overlay(SDL_Window *window, SDL_GPUDevice *device) {
    g_presentation_size_checked = 0;
    g_window = window;
    g_device = device;
    if (upload_art_and_quad() != 0) {
        fprintf(stderr, "dw3-title-overlay: GPU setup failed: %s\n",
                SDL_GetError());
        release_gpu_resources();
    }
}

static int upload_rgba(SDL_GPUTexture *texture, const uint8_t *rgba,
                       int width, int height, int pitch) {
    if (!rgba || !g_device || !texture || pitch < width * 4) return -1;
    const size_t bytes = (size_t)pitch * (size_t)height;
    const SDL_GPUTransferBufferCreateInfo transfer_info = {
        .usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD,
        .size = (Uint32)bytes,
    };
    SDL_GPUTransferBuffer *transfer =
        SDL_CreateGPUTransferBuffer(g_device, &transfer_info);
    if (!transfer) return -1;
    uint8_t *mapped = (uint8_t *)SDL_MapGPUTransferBuffer(g_device, transfer,
                                                           false);
    if (!mapped) {
        SDL_ReleaseGPUTransferBuffer(g_device, transfer);
        return -1;
    }
    memcpy(mapped, rgba, bytes);
    SDL_UnmapGPUTransferBuffer(g_device, transfer);
    SDL_GPUCommandBuffer *cmd = SDL_AcquireGPUCommandBuffer(g_device);
    if (!cmd) {
        SDL_ReleaseGPUTransferBuffer(g_device, transfer);
        return -1;
    }
    SDL_GPUCopyPass *copy = SDL_BeginGPUCopyPass(cmd);
    const SDL_GPUTextureTransferInfo src = {
        .transfer_buffer = transfer,
        .pixels_per_row = (Uint32)(pitch / 4),
        .rows_per_layer = (Uint32)height,
    };
    const SDL_GPUTextureRegion dst = {
        .texture = texture,
        .w = (Uint32)width,
        .h = (Uint32)height,
        .d = 1,
    };
    SDL_UploadToGPUTexture(copy, &src, &dst, false);
    SDL_EndGPUCopyPass(copy);
    if (!SDL_SubmitGPUCommandBuffer(cmd)) {
        SDL_ReleaseGPUTransferBuffer(g_device, transfer);
        return -1;
    }
    SDL_WaitForGPUIdle(g_device);
    SDL_ReleaseGPUTransferBuffer(g_device, transfer);
    return 0;
}

int dw3_pc_title_menu_set_logo_rgba(const uint8_t *rgba, int width,
                                  int height, int pitch) {
    if (width != 720 || height != 326) return -1;
    if (upload_rgba(g_logo_texture, rgba, width, height, pitch) != 0) return -1;
    g_logo_texture_ready = 1;
    return 0;
}

/* Regenerate the states when EU labels or the options screen's values change. */
static void refresh_art(void) {
    const size_t canvas_bytes = (size_t)CANVAS_W * CANVAS_H * 4;
    const size_t texture_bytes = canvas_bytes * STATE_COUNT;
    SDL_GPUTransferBuffer *transfer;
    SDL_GPUCommandBuffer *cmd;
    uint8_t *mapped;

    if (!g_device || !g_texture ||
        g_art_generation == dw3_pc_title_overlay_generation()) return;
    const SDL_GPUTransferBufferCreateInfo transfer_info = {
        .usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD,
        .size = (Uint32)texture_bytes,
    };
    transfer = SDL_CreateGPUTransferBuffer(g_device, &transfer_info);
    if (!transfer) return;
    mapped = (uint8_t *)SDL_MapGPUTransferBuffer(g_device, transfer, false);
    if (!mapped) {
        SDL_ReleaseGPUTransferBuffer(g_device, transfer);
        return;
    }
    g_art_generation = dw3_pc_title_overlay_generation();
    for (int state = 0; state < STATE_COUNT; ++state) {
        const int selected = state == 0 ? -1 :
                             state == STATE_OPTIONS ? DW3_TITLE_ART_OPTIONS :
                             state - 1;
        dw3_pc_title_overlay_art(mapped + (size_t)state * canvas_bytes,
                                 CANVAS_W * 4, selected);
    }
    SDL_UnmapGPUTransferBuffer(g_device, transfer);
    cmd = SDL_AcquireGPUCommandBuffer(g_device);
    if (!cmd) {
        SDL_ReleaseGPUTransferBuffer(g_device, transfer);
        return;
    }
    SDL_GPUCopyPass *copy = SDL_BeginGPUCopyPass(cmd);
    const SDL_GPUTextureTransferInfo src = {
        .transfer_buffer = transfer,
        .pixels_per_row = CANVAS_W,
        .rows_per_layer = ART_TEXTURE_H,
    };
    const SDL_GPUTextureRegion dst = {
        .texture = g_texture,
        .w = CANVAS_W,
        .h = ART_TEXTURE_H,
        .d = 1,
    };
    SDL_UploadToGPUTexture(copy, &src, &dst, false);
    SDL_EndGPUCopyPass(copy);
    SDL_SubmitGPUCommandBuffer(cmd);
    SDL_WaitForGPUIdle(g_device);
    SDL_ReleaseGPUTransferBuffer(g_device, transfer);
}

int dw3_pc_title_background_rgba(const uint8_t *rgba, int width,
                               int height, int pitch) {
    if (!rgba) { g_background_ready = 0; return 0; }
    if (width != 320 || height != 240) return -1;
    if (upload_rgba(g_background_texture, rgba, width, height, pitch) != 0) return -1;
    g_background_ready = 1;
    return 0;
}

void dw3_pc_title_menu_logo_set_active(int active) {
    g_logo_active = active && g_logo_texture_ready;
}

/* The SDL3 GPU shader has no tint input; the edge fade is not followed here. */
void dw3_pc_title_menu_set_fade(int level) { (void)level; }

void dw3_pc_title_menu_set_state(int active, int selected) {
    /* The options screen replaces the menu and the logo while it is open. */
    if (dw3_port_options_is_open() && g_pipeline && g_texture && g_vertices &&
        g_sampler) {
        refresh_art();
        g_selection = STATE_OPTIONS;
        g_overlay_active = 1;
        return;
    }
    if (!active || !g_pipeline || !g_texture || !g_vertices || !g_sampler) {
        g_overlay_active = 0;
        return;
    }
    if (selected == -2) {
        g_overlay_active = 0;
        return;
    }
    if (selected < 0) g_selection = 0;
    else if (selected >= 2) g_selection = 3;
    else g_selection = selected + 1;
    refresh_art();
    g_overlay_active = 1;
}

static void render_overlay(SDL_GPUCommandBuffer *cmd, SDL_GPURenderPass *pass) {
    set_three_x_presentation_size();
    if ((!g_overlay_active && !g_logo_active && !g_background_ready) || !g_pipeline || !g_texture || !g_vertices ||
        !g_sampler || !g_window) return;
    int window_w = 0, window_h = 0;
    SDL_GetWindowSizeInPixels(g_window, &window_w, &window_h);
    if (window_w <= 0 || window_h <= 0) return;
    int viewport_w = window_w;
    int viewport_h = viewport_w * 3 / 4;
    if (viewport_h > window_h) {
        viewport_h = window_h;
        viewport_w = viewport_h * 4 / 3;
    }
    const SDL_GPUViewport viewport = {
        .x = (float)(window_w - viewport_w) / 2.0f,
        .y = (float)(window_h - viewport_h) / 2.0f,
        .w = (float)viewport_w,
        .h = (float)viewport_h,
        .min_depth = 0.0f,
        .max_depth = 1.0f,
    };
    SDL_BindGPUGraphicsPipeline(pass, g_pipeline);
    const SDL_GPUBufferBinding binding = {.buffer = g_vertices, .offset = 0};
    SDL_BindGPUVertexBuffers(pass, 0, &binding, 1);
    if (g_background_ready && g_background_texture && g_background_sampler &&
        viewport.x > 0.0f) {
        const SDL_GPUTextureSamplerBinding background_binding = {
            .texture = g_background_texture, .sampler = g_background_sampler,
        };
        SDL_BindGPUFragmentSamplers(pass, 0, &background_binding, 1);
        for (int side = 0; side < 2; ++side) {
            SDL_GPUViewport side_viewport = viewport;
            side_viewport.x = side ? viewport.x + viewport.w : 0.0f;
            side_viewport.w = viewport.x;
            SDL_SetGPUViewport(pass, &side_viewport);
            SDL_DrawGPUPrimitives(pass, 6, 1, (STATE_COUNT + 1 + side) * 6, 0);
        }
        if (g_overlay_active && g_selection == STATE_OPTIONS) {
            const SDL_GPUTextureSamplerBinding dim_binding = {
                .texture = g_texture, .sampler = g_sampler,
            };
            SDL_BindGPUFragmentSamplers(pass, 0, &dim_binding, 1);
            for (int side = 0; side < 2; ++side) {
                SDL_GPUViewport side_viewport = viewport;
                side_viewport.x = side ? viewport.x + viewport.w : 0.0f;
                side_viewport.w = viewport.x;
                SDL_SetGPUViewport(pass, &side_viewport);
                SDL_DrawGPUPrimitives(pass, 6, 1, (STATE_COUNT + 3) * 6, 0);
            }
        }
    }
    SDL_SetGPUViewport(pass, &viewport);
    if (g_logo_active && g_logo_texture_ready && g_logo_texture &&
        g_logo_sampler && !dw3_port_options_is_open()) {
        const SDL_GPUTextureSamplerBinding logo_binding = {
            .texture = g_logo_texture,
            .sampler = g_logo_sampler,
        };
        SDL_BindGPUFragmentSamplers(pass, 0, &logo_binding, 1);
        SDL_DrawGPUPrimitives(pass, 6, 1, STATE_COUNT * 6, 0);
    }
    if (!g_overlay_active) return;
    const SDL_GPUTextureSamplerBinding sampler_binding = {
        .texture = g_texture,
        .sampler = g_sampler,
    };
    SDL_BindGPUFragmentSamplers(pass, 0, &sampler_binding, 1);
    SDL_DrawGPUPrimitives(pass, 6, 1, (Uint32)(g_selection * 6), 0);
    (void)cmd;
}

static void destroy_overlay(void) {
    release_gpu_resources();
}

void dw3_pc_title_menu_overlay_register(void) {
    Psyz_OverlayInit_SDL3GPU(init_overlay);
    Psyz_OverlayFrameCB(set_three_x_presentation_size);
    Psyz_OverlayRender_SDL3GPU(render_overlay);
    Psyz_OverlayDestroyCB(destroy_overlay);
}
