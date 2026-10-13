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

enum { CANVAS_W = 960, CANVAS_H = 720, ROW_COUNT = 3 };
#define ART_TEXTURE_H (CANVAS_H * ROW_COUNT)

typedef struct OverlayVertex {
    float position[2];
    float uv[2];
} OverlayVertex;

static SDL_Window *g_window;
static SDL_GPUDevice *g_device;
static SDL_GPUGraphicsPipeline *g_pipeline;
static SDL_GPUBuffer *g_vertices;
static SDL_GPUTexture *g_texture;
static SDL_GPUSampler *g_sampler;
static int g_selection;
static unsigned g_active_frames;

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
    if (!g_device) return;
    if (g_pipeline) SDL_ReleaseGPUGraphicsPipeline(g_device, g_pipeline);
    if (g_vertices) SDL_ReleaseGPUBuffer(g_device, g_vertices);
    if (g_texture) SDL_ReleaseGPUTexture(g_device, g_texture);
    if (g_sampler) SDL_ReleaseGPUSampler(g_device, g_sampler);
    g_pipeline = NULL;
    g_vertices = NULL;
    g_texture = NULL;
    g_sampler = NULL;
    g_window = NULL;
    g_device = NULL;
    g_active_frames = 0;
}

static int upload_art_and_quad(void) {
    const size_t canvas_bytes = (size_t)CANVAS_W * CANVAS_H * 4;
    const size_t texture_bytes = canvas_bytes * ROW_COUNT;
    uint8_t *pixels = (uint8_t *)malloc(texture_bytes);
    if (!pixels) return -1;
    for (int row = 0; row < ROW_COUNT; ++row) {
        dw3_pc_title_overlay_art(pixels + (size_t)row * canvas_bytes,
                                 CANVAS_W * 4, row);
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
        .min_filter = SDL_GPU_FILTER_NEAREST,
        .mag_filter = SDL_GPU_FILTER_NEAREST,
        .mipmap_mode = SDL_GPU_SAMPLERMIPMAPMODE_NEAREST,
        .address_mode_u = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE,
        .address_mode_v = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE,
        .address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE,
    };
    g_sampler = SDL_CreateGPUSampler(g_device, &sampler_info);
    if (!g_texture || !g_sampler) {
        free(pixels);
        return -1;
    }

    OverlayVertex quads[ROW_COUNT * 6];
    for (int row = 0; row < ROW_COUNT; ++row) {
        const float u0 = 0.5f / (float)CANVAS_W;
        const float u1 = ((float)CANVAS_W - 0.5f) / (float)CANVAS_W;
        const float v0 = ((float)(row * CANVAS_H) + 0.5f) /
                         (float)ART_TEXTURE_H;
        const float v1 = ((float)((row + 1) * CANVAS_H) - 0.5f) /
                         (float)ART_TEXTURE_H;
        const OverlayVertex a = {{0.0f, 0.0f}, {u0, v0}};
        const OverlayVertex b = {{1.0f, 0.0f}, {u1, v0}};
        const OverlayVertex c = {{1.0f, 1.0f}, {u1, v1}};
        const OverlayVertex d = {{0.0f, 1.0f}, {u0, v1}};
        OverlayVertex *q = &quads[row * 6];
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
            "3 states, 6 vertices and 1 draw/frame\n",
            CANVAS_W, ART_TEXTURE_H, texture_bytes);
    return 0;
}

static void init_overlay(SDL_Window *window, SDL_GPUDevice *device) {
    g_window = window;
    g_device = device;
    if (upload_art_and_quad() != 0) {
        fprintf(stderr, "dw3-title-overlay: GPU setup failed: %s\n",
                SDL_GetError());
        release_gpu_resources();
    }
}

void dw3_pc_title_menu_set_state(int active, int selected) {
    if (dw3_port_options_is_open()) active = 0;
    if (!active || !g_pipeline || !g_texture || !g_vertices || !g_sampler) {
        g_active_frames = 0;
        return;
    }
    if (selected < 0) selected = 0;
    if (selected >= ROW_COUNT) selected = ROW_COUNT - 1;
    g_selection = selected;
    g_active_frames = 2;
}

static void render_overlay(SDL_GPUCommandBuffer *cmd, SDL_GPURenderPass *pass) {
    if (!g_active_frames || !g_pipeline || !g_texture || !g_vertices ||
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
    SDL_SetGPUViewport(pass, &viewport);
    SDL_BindGPUGraphicsPipeline(pass, g_pipeline);
    const SDL_GPUBufferBinding binding = {.buffer = g_vertices, .offset = 0};
    SDL_BindGPUVertexBuffers(pass, 0, &binding, 1);
    const SDL_GPUTextureSamplerBinding sampler_binding = {
        .texture = g_texture,
        .sampler = g_sampler,
    };
    SDL_BindGPUFragmentSamplers(pass, 0, &sampler_binding, 1);
    SDL_DrawGPUPrimitives(pass, 6, 1, (Uint32)(g_selection * 6), 0);
    --g_active_frames;
    (void)cmd;
}

static void destroy_overlay(void) {
    release_gpu_resources();
}

void dw3_pc_title_menu_overlay_register(void) {
    Psyz_OverlayInit_SDL3GPU(init_overlay);
    Psyz_OverlayRender_SDL3GPU(render_overlay);
    Psyz_OverlayDestroyCB(destroy_overlay);
}
