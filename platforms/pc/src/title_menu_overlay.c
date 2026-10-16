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
static SDL_GPUGraphicsPipeline *g_gpu_options_composite_pipeline;
static SDL_GPUBuffer *g_vertices;
static SDL_GPUTexture *g_texture;
static SDL_GPUTexture *g_gpu_options_atlas;
static SDL_GPUTexture *g_gpu_options_target;
static SDL_GPUBuffer *g_gpu_options_vertices;
static SDL_GPUTransferBuffer *g_cpu_options_transfer;
static unsigned g_gpu_options_art_generation;
static int g_gpu_options_cursor = -1;
static int g_gpu_options_values[DW3_TITLE_OPTION_ROWS];
static int g_gpu_options_ready;
static int g_gpu_options_valid;
static int g_gpu_options_failed;
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
static unsigned g_options_generation;
enum { MENU_COMPOSITION_REFERENCE, MENU_COMPOSITION_CPU, MENU_COMPOSITION_GPU };
static int g_menu_composition = -1;
static uint8_t *g_cpu_options_static;
static uint8_t *g_cpu_options_canvas;
static unsigned g_cpu_options_art_generation;
static int g_cpu_options_cursor = -1;
static int g_cpu_options_values[DW3_TITLE_OPTION_ROWS];
static int g_cpu_options_ready;
static int g_cpu_options_valid;
static int g_cpu_options_upload_pending = 1;
static int g_cpu_fallback_logged;
static FILE *g_profile_csv;
static unsigned long long g_profile_frame;
static unsigned long long g_profile_present_frame;
static Uint64 g_profile_present_counter;
static Uint64 g_profile_present_interval_ns;
static Uint64 g_profile_frame_start;
static Uint64 g_profile_raster_ns, g_profile_staging_ns;
static Uint64 g_profile_submit_ns, g_profile_wait_ns;
static size_t g_profile_upload_bytes;
static unsigned g_profile_allocations, g_profile_rebuilds;
static size_t g_profile_dirty_pixels;
static unsigned g_profile_dirty_copies;
static const char *g_profile_event = "idle";
static int g_profile_enabled = -1;
static int g_profile_was_options;
static int g_profile_previous_cursor = -1;
static int g_profile_previous_values[DW3_TITLE_OPTION_ROWS];
static unsigned g_profile_previous_art_generation;
static unsigned g_profile_previous_options_generation;
static int g_profile_pending;
static unsigned long long g_profile_pending_frame;
static Uint64 g_profile_pending_change_counter;
static Uint64 g_profile_pending_overlay_cpu_ns;
static Uint64 g_profile_pending_raster_ns, g_profile_pending_staging_ns;
static Uint64 g_profile_pending_submit_ns, g_profile_pending_wait_ns;
static size_t g_profile_pending_upload_bytes;
static unsigned g_profile_pending_allocations, g_profile_pending_rebuilds;
static size_t g_profile_pending_dirty_pixels;
static unsigned g_profile_pending_dirty_copies;
static int g_profile_pending_cursor;

static int menu_composition_mode(void) {
    if (g_menu_composition < 0) {
        const char *mode = getenv("DW3_MENU_COMPOSITION");
        const char *force_fallback = getenv("DW3_MENU_FORCE_FALLBACK");
        g_menu_composition = MENU_COMPOSITION_REFERENCE;
        if (mode && strcmp(mode, "cpu") == 0) g_menu_composition = MENU_COMPOSITION_CPU;
        else if (mode && strcmp(mode, "gpu") == 0) g_menu_composition = MENU_COMPOSITION_GPU;
        else if (mode && strcmp(mode, "reference") != 0)
            fprintf(stderr, "dw3-title-overlay: unknown DW3_MENU_COMPOSITION '%s'; using reference\n", mode);
        if (force_fallback && *force_fallback) {
            g_menu_composition = MENU_COMPOSITION_REFERENCE;
            fprintf(stderr, "dw3-title-overlay: forced reference fallback enabled\n");
        }
    }
    return g_menu_composition;
}
static const char *menu_composition_name(void) {
    switch (menu_composition_mode()) {
    case MENU_COMPOSITION_CPU: return "cpu";
    case MENU_COMPOSITION_GPU: return "gpu";
    default: return "reference";
    }
}
static void profile_close(void) {
    if (g_profile_csv) fclose(g_profile_csv);
    g_profile_csv = NULL;
}

static int profile_is_enabled(void) {
    if (g_profile_enabled < 0) {
        const char *path = getenv("DW3_MENU_PROFILE_CSV");
        g_profile_enabled = path && *path;
    }
    return g_profile_enabled;
}
static Uint64 profile_counter(void) {
    return profile_is_enabled() ? SDL_GetPerformanceCounter() : 0;
}
static Uint64 profile_elapsed_ns(Uint64 start) {
    if (!profile_is_enabled()) return 0;
    const Uint64 elapsed = profile_counter() - start;
    const Uint64 frequency = SDL_GetPerformanceFrequency();
    return frequency ? elapsed * 1000000000ULL / frequency : 0;
}
static void profile_add(Uint64 *total, Uint64 start) {
    *total += profile_elapsed_ns(start);
}
static void profile_begin(void) {
    if (!profile_is_enabled()) return;
    const int options_open = dw3_port_options_is_open();
    const int cursor = dw3_pc_title_overlay_options_cursor();
    const unsigned art_generation = dw3_pc_title_overlay_generation();
    const unsigned options_generation = dw3_pc_title_overlay_options_generation();
    int value_changed = 0;
    g_profile_frame++;
    g_profile_frame_start = profile_counter();
    g_profile_raster_ns = g_profile_staging_ns = 0;
    g_profile_submit_ns = g_profile_wait_ns = 0;
    g_profile_upload_bytes = 0;
    g_profile_allocations = g_profile_rebuilds = 0;
    g_profile_dirty_pixels = 0;
    g_profile_dirty_copies = 0;
    for (int i = 0; i < DW3_TITLE_OPTION_ROWS; ++i) {
        const int value = dw3_port_options_level(i);
        if (value != g_profile_previous_values[i]) value_changed = 1;
    }
    if (options_open && !g_profile_was_options) g_profile_event = "options-open";
    else if (!options_open && g_profile_was_options) g_profile_event = "options-close";
    else if (options_open && cursor != g_profile_previous_cursor)
        g_profile_event = "options-cursor";
    else if (options_open && value_changed) g_profile_event = "options-value";
    else if (art_generation != g_profile_previous_art_generation)
        g_profile_event = "language-rebuild";
    else if (options_generation != g_profile_previous_options_generation)
        g_profile_event = "options-change";
    else g_profile_event = options_open ? "options-idle" : "title-idle";
    g_profile_pending_change_counter = g_profile_frame_start;
    g_profile_was_options = options_open;
    g_profile_previous_cursor = cursor;
    g_profile_previous_art_generation = art_generation;
    g_profile_previous_options_generation = options_generation;
    for (int i = 0; i < DW3_TITLE_OPTION_ROWS; ++i)
        g_profile_previous_values[i] = dw3_port_options_level(i);
}
static void profile_end(void) {
    if (!profile_is_enabled()) return;
    g_profile_pending = 1;
    g_profile_pending_frame = g_profile_frame;
    g_profile_pending_overlay_cpu_ns = profile_elapsed_ns(g_profile_frame_start);
    g_profile_pending_raster_ns = g_profile_raster_ns;
    g_profile_pending_staging_ns = g_profile_staging_ns;
    g_profile_pending_submit_ns = g_profile_submit_ns;
    g_profile_pending_wait_ns = g_profile_wait_ns;
    g_profile_pending_upload_bytes = g_profile_upload_bytes;
    g_profile_pending_allocations = g_profile_allocations;
    g_profile_pending_rebuilds = g_profile_rebuilds;
    g_profile_pending_dirty_pixels = g_profile_dirty_pixels;
    g_profile_pending_dirty_copies = g_profile_dirty_copies;
    g_profile_pending_cursor = dw3_pc_title_overlay_options_cursor();
}
static void profile_overlay_draw_complete(void) {
    const char *path = getenv("DW3_MENU_PROFILE_CSV");
    if (!profile_is_enabled()) return;
    g_profile_present_frame++;
    g_profile_present_counter = profile_counter();
    if (!g_profile_pending || !path || !*path) return;
    if (!g_profile_csv) {
        g_profile_csv = fopen(path, "w");
        if (!g_profile_csv) return;
        setvbuf(g_profile_csv, NULL, _IOFBF, 65536);
        fprintf(g_profile_csv,
                "mode,state_call,overlay_draw_frame,overlay_draw_counter,counter_frequency_hz,state_update_to_draw_us,event,cursor,raster_us,staging_us,submit_us,wait_us,overlay_cpu_us,upload_bytes,allocations,rebuilds,dirty_pixels,dirty_copies,gpu_elapsed_us\n");
        atexit(profile_close);
    }
    const Uint64 frequency = SDL_GetPerformanceFrequency();
    const Uint64 draw_counter = g_profile_present_counter;
    const Uint64 latency = draw_counter >= g_profile_pending_change_counter
        ? draw_counter - g_profile_pending_change_counter : 0;
    fprintf(g_profile_csv,
            "%s,%llu,%llu,%llu,%llu,%llu,%s,%d,%llu,%llu,%llu,%llu,%llu,%zu,%u,%u,%zu,%u,-1\n",
            menu_composition_name(), g_profile_pending_frame, g_profile_present_frame,
            (unsigned long long)draw_counter, (unsigned long long)frequency,
            (unsigned long long)(frequency ? latency * 1000000ULL / frequency : 0),
            g_profile_event, g_profile_pending_cursor,
            (unsigned long long)(g_profile_pending_raster_ns / 1000ULL),
            (unsigned long long)(g_profile_pending_staging_ns / 1000ULL),
            (unsigned long long)(g_profile_pending_submit_ns / 1000ULL),
            (unsigned long long)(g_profile_pending_wait_ns / 1000ULL),
            (unsigned long long)(g_profile_pending_overlay_cpu_ns / 1000ULL),
            g_profile_pending_upload_bytes, g_profile_pending_allocations,
            g_profile_pending_rebuilds, g_profile_pending_dirty_pixels,
            g_profile_pending_dirty_copies);
    g_profile_pending = 0;
}

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
    if (g_gpu_options_composite_pipeline)
        SDL_ReleaseGPUGraphicsPipeline(g_device, g_gpu_options_composite_pipeline);
    if (g_vertices) SDL_ReleaseGPUBuffer(g_device, g_vertices);
    if (g_gpu_options_vertices) SDL_ReleaseGPUBuffer(g_device, g_gpu_options_vertices);
    if (g_cpu_options_transfer) SDL_ReleaseGPUTransferBuffer(g_device, g_cpu_options_transfer);
    if (g_texture) SDL_ReleaseGPUTexture(g_device, g_texture);
    if (g_gpu_options_atlas) SDL_ReleaseGPUTexture(g_device, g_gpu_options_atlas);
    if (g_gpu_options_target) SDL_ReleaseGPUTexture(g_device, g_gpu_options_target);
    if (g_logo_texture) SDL_ReleaseGPUTexture(g_device, g_logo_texture);
    if (g_background_texture) SDL_ReleaseGPUTexture(g_device, g_background_texture);
    if (g_background_sampler) SDL_ReleaseGPUSampler(g_device, g_background_sampler);
    if (g_sampler) SDL_ReleaseGPUSampler(g_device, g_sampler);
    if (g_logo_sampler) SDL_ReleaseGPUSampler(g_device, g_logo_sampler);
    g_pipeline = NULL;
    g_gpu_options_composite_pipeline = NULL;
    g_vertices = NULL;
    g_gpu_options_vertices = NULL;
    g_cpu_options_transfer = NULL;
    g_texture = NULL;
    g_gpu_options_atlas = NULL;
    g_gpu_options_target = NULL;
    g_gpu_options_ready = 0;
    g_gpu_options_valid = 0;
    g_gpu_options_failed = 0;
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
    g_cpu_options_upload_pending = 1;
}

static int upload_art_and_quad(void) {
    const size_t canvas_bytes = (size_t)CANVAS_W * CANVAS_H * 4;
    const size_t texture_bytes = canvas_bytes * STATE_COUNT;
    uint8_t *pixels = (uint8_t *)malloc(texture_bytes);
    if (!pixels) return -1;
    g_art_generation = dw3_pc_title_overlay_generation();
    g_options_generation = dw3_pc_title_overlay_options_generation();
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

    OverlayVertex quads[STATE_COUNT * 6 + 30];
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
    {
        const OverlayVertex a = {{0,0},{0,0}};
        const OverlayVertex b = {{1,0},{1,0}};
        const OverlayVertex c = {{1,1},{1,1}};
        const OverlayVertex d = {{0,1},{0,1}};
        OverlayVertex *q = &quads[(STATE_COUNT + 4) * 6];
        q[0]=a; q[1]=b; q[2]=c; q[3]=a; q[4]=c; q[5]=d;
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
    SDL_GPUColorTargetBlendState premultiplied_blend = blend;
    premultiplied_blend.src_color_blendfactor = SDL_GPU_BLENDFACTOR_ONE;
    SDL_GPUColorTargetDescription premultiplied_target = target;
    premultiplied_target.blend_state = premultiplied_blend;
    SDL_GPUGraphicsPipelineCreateInfo premultiplied_info = pipeline_info;
    premultiplied_info.target_info.color_target_descriptions = &premultiplied_target;
    g_gpu_options_composite_pipeline = SDL_CreateGPUGraphicsPipeline(g_device,
        &premultiplied_info);
    SDL_ReleaseGPUShader(g_device, vs);
    SDL_ReleaseGPUShader(g_device, fs);
    if (!g_pipeline || !g_gpu_options_composite_pipeline) {
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
    Uint64 profile_start;

    if (!g_device || !g_texture ||
        (g_art_generation == dw3_pc_title_overlay_generation() &&
         g_options_generation == dw3_pc_title_overlay_options_generation())) return;
    if (g_art_generation == dw3_pc_title_overlay_generation()) {
        /* Only the options state changed: upload that one canvas. */
        const SDL_GPUTransferBufferCreateInfo one_info = {
            .usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD,
            .size = (Uint32)canvas_bytes,
        };
        profile_start = profile_counter();
        SDL_GPUTransferBuffer *one = SDL_CreateGPUTransferBuffer(g_device, &one_info);
        if (!one) return;
        g_profile_allocations++;
        uint8_t *one_mapped = (uint8_t *)SDL_MapGPUTransferBuffer(g_device, one, false);
        if (!one_mapped) {
            SDL_ReleaseGPUTransferBuffer(g_device, one);
            return;
        }
        profile_add(&g_profile_staging_ns, profile_start);
        const unsigned options_generation = dw3_pc_title_overlay_options_generation();
        profile_start = profile_counter();
        dw3_pc_title_overlay_art(one_mapped, CANVAS_W * 4, DW3_TITLE_ART_OPTIONS);
        profile_add(&g_profile_raster_ns, profile_start);
        profile_start = profile_counter();
        SDL_UnmapGPUTransferBuffer(g_device, one);
        profile_add(&g_profile_staging_ns, profile_start);
        SDL_GPUCommandBuffer *one_cmd = SDL_AcquireGPUCommandBuffer(g_device);
        if (!one_cmd) {
            SDL_ReleaseGPUTransferBuffer(g_device, one);
            return;
        }
        SDL_GPUCopyPass *one_copy = SDL_BeginGPUCopyPass(one_cmd);
        const SDL_GPUTextureTransferInfo one_src = {
            .transfer_buffer = one,
            .pixels_per_row = CANVAS_W,
            .rows_per_layer = CANVAS_H,
        };
        const SDL_GPUTextureRegion one_dst = {
            .texture = g_texture,
            .x = 0,
            .y = STATE_OPTIONS * CANVAS_H,
            .w = CANVAS_W,
            .h = CANVAS_H,
            .d = 1,
        };
        SDL_UploadToGPUTexture(one_copy, &one_src, &one_dst, false);
        SDL_EndGPUCopyPass(one_copy);
        g_profile_upload_bytes += canvas_bytes;
        g_profile_rebuilds++;
        /* Mark the canvas current only once it reached the GPU, so a failed
         * submit retries on the next frame. */
        profile_start = profile_counter();
        if (SDL_SubmitGPUCommandBuffer(one_cmd)) {
            profile_add(&g_profile_submit_ns, profile_start);
            profile_start = profile_counter();
            SDL_WaitForGPUIdle(g_device);
            profile_add(&g_profile_wait_ns, profile_start);
            g_options_generation = options_generation;
        }
        profile_start = profile_counter();
        SDL_ReleaseGPUTransferBuffer(g_device, one);
        profile_add(&g_profile_staging_ns, profile_start);
        return;
    }
    const SDL_GPUTransferBufferCreateInfo transfer_info = {
        .usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD,
        .size = (Uint32)texture_bytes,
    };
    profile_start = profile_counter();
    transfer = SDL_CreateGPUTransferBuffer(g_device, &transfer_info);
    if (!transfer) return;
    g_profile_allocations++;
    mapped = (uint8_t *)SDL_MapGPUTransferBuffer(g_device, transfer, false);
    if (!mapped) {
        SDL_ReleaseGPUTransferBuffer(g_device, transfer);
        return;
    }
    profile_add(&g_profile_staging_ns, profile_start);
    g_art_generation = dw3_pc_title_overlay_generation();
    g_options_generation = dw3_pc_title_overlay_options_generation();
    profile_start = profile_counter();
    for (int state = 0; state < STATE_COUNT; ++state) {
        const int selected = state == 0 ? -1 :
                             state == STATE_OPTIONS ? DW3_TITLE_ART_OPTIONS :
                             state - 1;
        dw3_pc_title_overlay_art(mapped + (size_t)state * canvas_bytes,
                                 CANVAS_W * 4, selected);
    }
    profile_add(&g_profile_raster_ns, profile_start);
    profile_start = profile_counter();
    SDL_UnmapGPUTransferBuffer(g_device, transfer);
    profile_add(&g_profile_staging_ns, profile_start);
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
    g_profile_upload_bytes += texture_bytes;
    g_profile_rebuilds++;
    profile_start = profile_counter();
    SDL_SubmitGPUCommandBuffer(cmd);
    profile_add(&g_profile_submit_ns, profile_start);
    profile_start = profile_counter();
    SDL_WaitForGPUIdle(g_device);
    profile_add(&g_profile_wait_ns, profile_start);
    profile_start = profile_counter();
    SDL_ReleaseGPUTransferBuffer(g_device, transfer);
    profile_add(&g_profile_staging_ns, profile_start);
}

typedef struct MenuDirtyRect { int x, y, w, h; } MenuDirtyRect;
enum { OPTIONS_ROW_X = 132, OPTIONS_ROW_Y = 212, OPTIONS_ROW_W = 696,
       OPTIONS_ROW_H = 52, OPTIONS_ROW_STEP = 68, OPTIONS_VALUE_X = 684,
       OPTIONS_VALUE_W = 144, OPTIONS_DESC_Y = 500 };

static MenuDirtyRect options_row_rect(int row) {
    return (MenuDirtyRect){OPTIONS_ROW_X,
        OPTIONS_ROW_Y + OPTIONS_ROW_STEP * row, OPTIONS_ROW_W, OPTIONS_ROW_H};
}
static MenuDirtyRect options_value_rect(int row) {
    return (MenuDirtyRect){OPTIONS_VALUE_X,
        OPTIONS_ROW_Y + OPTIONS_ROW_STEP * row, OPTIONS_VALUE_W, OPTIONS_ROW_H};
}
static MenuDirtyRect options_description_rect(void) {
    return (MenuDirtyRect){OPTIONS_ROW_X, OPTIONS_DESC_Y, OPTIONS_ROW_W, OPTIONS_ROW_H};
}
static int rect_intersection(MenuDirtyRect a, MenuDirtyRect b, MenuDirtyRect *out) {
    const int x0 = a.x > b.x ? a.x : b.x;
    const int y0 = a.y > b.y ? a.y : b.y;
    const int x1 = a.x + a.w < b.x + b.w ? a.x + a.w : b.x + b.w;
    const int y1 = a.y + a.h < b.y + b.h ? a.y + a.h : b.y + b.h;
    if (x0 >= x1 || y0 >= y1) return 0;
    if (out) *out = (MenuDirtyRect){x0, y0, x1 - x0, y1 - y0};
    return 1;
}
static void copy_dirty_rect(const uint8_t *src, uint8_t *dst, MenuDirtyRect rect) {
    const size_t row_bytes = (size_t)rect.w * 4;
    for (int y = 0; y < rect.h; ++y)
        memcpy(dst + (size_t)(rect.y + y) * CANVAS_W * 4 + rect.x * 4,
               src + (size_t)(rect.y + y) * CANVAS_W * 4 + rect.x * 4,
               row_bytes);
}
static void draw_cpu_component(MenuDirtyRect rect, int layer, int row, int value) {
    dw3_pc_title_overlay_options_component(g_cpu_options_canvas, CANVAS_W * 4,
        layer, row, value, rect.x, rect.y, rect.w, rect.h, 0);
}
static void draw_cpu_state_in_rect(MenuDirtyRect rect, int cursor,
                                   const int *values) {
    for (int row = 0; row < DW3_TITLE_OPTION_ROWS; ++row) {
        MenuDirtyRect intersection;
        if (cursor == row && rect_intersection(rect, options_row_rect(row), &intersection))
            draw_cpu_component(intersection, DW3_TITLE_OPTIONS_FOCUS, row, 0);
        if (rect_intersection(rect, options_value_rect(row), &intersection))
            draw_cpu_component(intersection, DW3_TITLE_OPTIONS_VALUE, row, values[row]);
    }
    MenuDirtyRect intersection;
    if (cursor >= 0 && cursor < DW3_TITLE_OPTION_ROWS &&
        rect_intersection(rect, options_description_rect(), &intersection))
        draw_cpu_component(intersection, DW3_TITLE_OPTIONS_DESCRIPTION, cursor, 0);
}
static int upload_cpu_dirty_rects(const MenuDirtyRect *rects, int count) {
    size_t total_bytes = 0, offset = 0;
    SDL_GPUTransferBuffer *transfer = g_cpu_options_transfer;
    SDL_GPUCommandBuffer *cmd;
    uint8_t *mapped;
    Uint64 start;
    for (int i = 0; i < count; ++i)
        total_bytes += (size_t)rects[i].w * rects[i].h * 4;
    if (!total_bytes) return 0;
    start = profile_counter();
    if (!transfer) {
        const SDL_GPUTransferBufferCreateInfo transfer_info = {
            .usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD,
            .size = (Uint32)((size_t)CANVAS_W * CANVAS_H * 4),
        };
        transfer = SDL_CreateGPUTransferBuffer(g_device, &transfer_info);
        if (!transfer) return -1;
        g_cpu_options_transfer = transfer;
        g_profile_allocations++;
    }
    mapped = (uint8_t *)SDL_MapGPUTransferBuffer(g_device, transfer, false);
    if (!mapped) return -1;
    for (int i = 0; i < count; ++i) {
        const MenuDirtyRect rect = rects[i];
        const size_t row_bytes = (size_t)rect.w * 4;
        for (int y = 0; y < rect.h; ++y)
            memcpy(mapped + offset + (size_t)y * row_bytes,
                   g_cpu_options_canvas + (size_t)(rect.y + y) * CANVAS_W * 4 + rect.x * 4,
                   row_bytes);
        offset += row_bytes * rect.h;
    }
    SDL_UnmapGPUTransferBuffer(g_device, transfer);
    profile_add(&g_profile_staging_ns, start);
    cmd = SDL_AcquireGPUCommandBuffer(g_device);
    if (!cmd) return -1;
    SDL_GPUCopyPass *copy = SDL_BeginGPUCopyPass(cmd);
    offset = 0;
    for (int i = 0; i < count; ++i) {
        const MenuDirtyRect rect = rects[i];
        const SDL_GPUTextureTransferInfo src = {
            .transfer_buffer = transfer,
            .offset = (Uint32)offset,
            .pixels_per_row = (Uint32)rect.w,
            .rows_per_layer = (Uint32)rect.h,
        };
        const SDL_GPUTextureRegion dst = {
            .texture = g_texture, .x = (Uint32)rect.x,
            .y = STATE_OPTIONS * CANVAS_H + (Uint32)rect.y,
            .w = (Uint32)rect.w, .h = (Uint32)rect.h, .d = 1,
        };
        SDL_UploadToGPUTexture(copy, &src, &dst, false);
        offset += (size_t)rect.w * rect.h * 4;
    }
    SDL_EndGPUCopyPass(copy);
    g_profile_upload_bytes += total_bytes;
    for (int i = 0; i < count; ++i)
        g_profile_dirty_pixels += (size_t)rects[i].w * rects[i].h;
    g_profile_dirty_copies += (unsigned)count;
    start = profile_counter();
    const bool submitted = SDL_SubmitGPUCommandBuffer(cmd);
    profile_add(&g_profile_submit_ns, start);
    if (submitted) {
        start = profile_counter();
        SDL_WaitForGPUIdle(g_device);
        profile_add(&g_profile_wait_ns, start);
    }
    return submitted ? 0 : -1;
}

static int refresh_art_cpu(void) {
    const size_t canvas_bytes = (size_t)CANVAS_W * CANVAS_H * 4;
    const unsigned art_generation = dw3_pc_title_overlay_generation();
    const unsigned options_generation = dw3_pc_title_overlay_options_generation();
    const int cursor = dw3_pc_title_overlay_options_cursor();
    int values[DW3_TITLE_OPTION_ROWS];
    MenuDirtyRect dirty[DW3_TITLE_OPTION_ROWS + 3];
    int dirty_count = 0;
    Uint64 start;
    for (int i = 0; i < DW3_TITLE_OPTION_ROWS; ++i)
        values[i] = dw3_port_options_level(i);
    if (!g_cpu_options_ready) {
        if (!g_cpu_options_static) {
            g_cpu_options_static = (uint8_t *)malloc(canvas_bytes);
            if (g_cpu_options_static) g_profile_allocations++;
        }
        if (!g_cpu_options_canvas) {
            g_cpu_options_canvas = (uint8_t *)malloc(canvas_bytes);
            if (g_cpu_options_canvas) g_profile_allocations++;
        }
        if (!g_cpu_options_static || !g_cpu_options_canvas) return -1;
        g_cpu_options_ready = 1;
    }
    const int full_rebuild = !g_cpu_options_valid ||
        g_cpu_options_art_generation != art_generation;
    if (full_rebuild) {
        start = profile_counter();
        dw3_pc_title_overlay_options_component(g_cpu_options_static, CANVAS_W * 4,
            DW3_TITLE_OPTIONS_STATIC, -1, 0, 0, 0, CANVAS_W, CANVAS_H, 1);
        memcpy(g_cpu_options_canvas, g_cpu_options_static, canvas_bytes);
        g_cpu_options_art_generation = art_generation;
        g_cpu_options_valid = 1;
        profile_add(&g_profile_raster_ns, start);
        dirty[dirty_count++] = (MenuDirtyRect){0, 0, CANVAS_W, CANVAS_H};
    } else if (g_cpu_options_upload_pending) {
        dirty[dirty_count++] = (MenuDirtyRect){0, 0, CANVAS_W, CANVAS_H};
    } else {
        if (cursor != g_cpu_options_cursor) {
            if (g_cpu_options_cursor >= 0 && g_cpu_options_cursor < DW3_TITLE_OPTION_ROWS)
                dirty[dirty_count++] = options_row_rect(g_cpu_options_cursor);
            if (cursor >= 0 && cursor < DW3_TITLE_OPTION_ROWS)
                dirty[dirty_count++] = options_row_rect(cursor);
            dirty[dirty_count++] = options_description_rect();
        }
        for (int i = 0; i < DW3_TITLE_OPTION_ROWS; ++i) {
            if (values[i] == g_cpu_options_values[i]) continue;
            const MenuDirtyRect value_rect = options_value_rect(i);
            int contained = 0;
            for (int j = 0; j < dirty_count; ++j)
                if (rect_intersection(dirty[j], value_rect, NULL)) contained = 1;
            if (!contained) dirty[dirty_count++] = value_rect;
        }
    }
    if (!dirty_count) return 0;
    start = profile_counter();
    for (int i = 0; i < dirty_count; ++i) {
        copy_dirty_rect(g_cpu_options_static, g_cpu_options_canvas, dirty[i]);
        draw_cpu_state_in_rect(dirty[i], cursor, values);
    }
    profile_add(&g_profile_raster_ns, start);
    g_cpu_options_cursor = cursor;
    memcpy(g_cpu_options_values, values, sizeof(values));
    if (full_rebuild || (dirty_count == 1 && dirty[0].w == CANVAS_W &&
        dirty[0].h == CANVAS_H)) g_profile_rebuilds++;
    const int result = upload_cpu_dirty_rects(dirty, dirty_count);
    if (!result) {
        g_cpu_options_upload_pending = 0;
        g_options_generation = options_generation;
    }
    return result;
}

enum { GPU_OPTIONS_ATLAS_H = 1812, GPU_OPTIONS_VALUES_Y = 876,
       GPU_OPTIONS_DESCRIPTION_Y = 1656, GPU_OPTIONS_VARIANTS = 375 };
static void atlas_copy_tile(uint8_t *atlas, const uint8_t *scratch,
                            int atlas_y, MenuDirtyRect source) {
    for (int y = 0; y < source.h; ++y)
        memcpy(atlas + (size_t)(atlas_y + y) * CANVAS_W * 4,
               scratch + (size_t)(source.y + y) * CANVAS_W * 4 + source.x * 4,
               (size_t)source.w * 4);
}
static void atlas_make_component(uint8_t *scratch, uint8_t *atlas, int atlas_y,
                                 int layer, int row, int value,
                                 MenuDirtyRect rect) {
    dw3_pc_title_overlay_options_component(scratch, CANVAS_W * 4,
        layer, row, value, rect.x, rect.y, rect.w, rect.h, 1);
    atlas_copy_tile(atlas, scratch, atlas_y, rect);
}
static void gpu_options_vertex(OverlayVertex *vertices, int *count,
                               MenuDirtyRect dst, MenuDirtyRect src) {
    const float x0 = (float)dst.x / CANVAS_W;
    const float y0 = (float)dst.y / CANVAS_H;
    const float x1 = (float)(dst.x + dst.w) / CANVAS_W;
    const float y1 = (float)(dst.y + dst.h) / CANVAS_H;
    const float u0 = (float)src.x / CANVAS_W;
    const float v0 = (float)src.y / GPU_OPTIONS_ATLAS_H;
    const float u1 = (float)(src.x + src.w) / CANVAS_W;
    const float v1 = (float)(src.y + src.h) / GPU_OPTIONS_ATLAS_H;
    const OverlayVertex a = {{x0,y0},{u0,v0}}, b = {{x1,y0},{u1,v0}};
    const OverlayVertex c = {{x1,y1},{u1,v1}}, d = {{x0,y1},{u0,v1}};
    vertices[(*count)++] = a; vertices[(*count)++] = b; vertices[(*count)++] = c;
    vertices[(*count)++] = a; vertices[(*count)++] = c; vertices[(*count)++] = d;
}
static int gpu_options_combo(int cursor, const int *values) {
    const int c = cursor < 0 ? 0 : cursor > 2 ? 2 : cursor;
    const int a = values[0] < 0 ? 0 : values[0] > 4 ? 4 : values[0];
    const int b = values[1] < 0 ? 0 : values[1] > 4 ? 4 : values[1];
    const int d = values[2] < 0 ? 0 : values[2] > 4 ? 4 : values[2];
    return c * 125 + a * 25 + b * 5 + d;
}
static int build_gpu_options(void) {
    Uint64 profile_start = profile_counter();
    const size_t atlas_bytes = (size_t)CANVAS_W * GPU_OPTIONS_ATLAS_H * 4;
    uint8_t *atlas = (uint8_t *)calloc(1, atlas_bytes);
    uint8_t *scratch = (uint8_t *)malloc((size_t)CANVAS_W * CANVAS_H * 4);
    OverlayVertex *vertices = (OverlayVertex *)malloc(
        (size_t)GPU_OPTIONS_VARIANTS * 36 * sizeof(*vertices));
    if (!atlas || !scratch || !vertices) {
        free(atlas); free(scratch); free(vertices); return -1;
    }
    dw3_pc_title_overlay_options_component(scratch, CANVAS_W * 4,
        DW3_TITLE_OPTIONS_STATIC, -1, 0, 0, 0, CANVAS_W, CANVAS_H, 1);
    memcpy(atlas, scratch, (size_t)CANVAS_W * CANVAS_H * 4);
    for (int row = 0; row < DW3_TITLE_OPTION_ROWS; ++row) {
        const MenuDirtyRect focus = options_row_rect(row);
        atlas_make_component(scratch, atlas, CANVAS_H + row * OPTIONS_ROW_H,
            DW3_TITLE_OPTIONS_FOCUS, row, 0, focus);
        for (int value = 0; value < 5; ++value) {
            const MenuDirtyRect value_rect = options_value_rect(row);
            const int index = row * 5 + value;
            atlas_make_component(scratch, atlas,
                GPU_OPTIONS_VALUES_Y + index * OPTIONS_ROW_H,
                DW3_TITLE_OPTIONS_VALUE, row, value, value_rect);
        }
        const MenuDirtyRect desc = options_description_rect();
        atlas_make_component(scratch, atlas,
            GPU_OPTIONS_DESCRIPTION_Y + row * OPTIONS_ROW_H,
            DW3_TITLE_OPTIONS_DESCRIPTION, row, 0, desc);
    }
    free(scratch);
    profile_add(&g_profile_raster_ns, profile_start);
    for (int cursor = 0; cursor < 3; ++cursor)
        for (int v0 = 0; v0 < 5; ++v0)
            for (int v1 = 0; v1 < 5; ++v1)
                for (int v2 = 0; v2 < 5; ++v2) {
                    int count = 0;
                    const int values[] = {v0,v1,v2};
                    const int combo = gpu_options_combo(cursor, values);
                    OverlayVertex *q = vertices + (size_t)combo * 36;
                    gpu_options_vertex(q, &count,
                        (MenuDirtyRect){0,0,CANVAS_W,CANVAS_H},
                        (MenuDirtyRect){0,0,CANVAS_W,CANVAS_H});
                    for (int row = 0; row < 3; ++row) {
                        const MenuDirtyRect rect = options_row_rect(row);
                        if (row == cursor)
                            gpu_options_vertex(q,&count,rect,
                                (MenuDirtyRect){0,CANVAS_H+row*OPTIONS_ROW_H,
                                                rect.w,rect.h});
                        const MenuDirtyRect val = options_value_rect(row);
                        gpu_options_vertex(q,&count,val,
                            (MenuDirtyRect){0,GPU_OPTIONS_VALUES_Y+
                                (row*5+values[row])*OPTIONS_ROW_H,val.w,val.h});
                    }
                    const MenuDirtyRect desc = options_description_rect();
                    gpu_options_vertex(q,&count,desc,
                        (MenuDirtyRect){0,GPU_OPTIONS_DESCRIPTION_Y+
                            cursor*OPTIONS_ROW_H,desc.w,desc.h});
                }
    const SDL_GPUTextureCreateInfo atlas_info = {
        .type = SDL_GPU_TEXTURETYPE_2D,
        .format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM,
        .usage = SDL_GPU_TEXTUREUSAGE_SAMPLER,
        .width = CANVAS_W, .height = GPU_OPTIONS_ATLAS_H,
        .layer_count_or_depth = 1, .num_levels = 1,
        .sample_count = SDL_GPU_SAMPLECOUNT_1,
    };
    const SDL_GPUTextureCreateInfo target_info = {
        .type = SDL_GPU_TEXTURETYPE_2D,
        .format = SDL_GetGPUSwapchainTextureFormat(g_device, g_window),
        .usage = SDL_GPU_TEXTUREUSAGE_SAMPLER | SDL_GPU_TEXTUREUSAGE_COLOR_TARGET,
        .width = CANVAS_W, .height = CANVAS_H,
        .layer_count_or_depth = 1, .num_levels = 1,
        .sample_count = SDL_GPU_SAMPLECOUNT_1,
    };
    const SDL_GPUBufferCreateInfo buffer_info = {
        .usage = SDL_GPU_BUFFERUSAGE_VERTEX,
        .size = (Uint32)((size_t)GPU_OPTIONS_VARIANTS * 36 * sizeof(*vertices)),
    };
    g_gpu_options_atlas = SDL_CreateGPUTexture(g_device, &atlas_info);
    g_gpu_options_target = SDL_CreateGPUTexture(g_device, &target_info);
    g_gpu_options_vertices = SDL_CreateGPUBuffer(g_device, &buffer_info);
    const size_t vertex_bytes = (size_t)GPU_OPTIONS_VARIANTS * 36 * sizeof(*vertices);
    const SDL_GPUTransferBufferCreateInfo transfer_info = {
        .usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD,
        .size = (Uint32)(atlas_bytes + vertex_bytes),
    };
    SDL_GPUTransferBuffer *transfer = SDL_CreateGPUTransferBuffer(g_device,&transfer_info);
    if (!g_gpu_options_atlas || !g_gpu_options_target ||
        !g_gpu_options_vertices || !transfer) {
        if (transfer) SDL_ReleaseGPUTransferBuffer(g_device,transfer);
        if (g_gpu_options_atlas) SDL_ReleaseGPUTexture(g_device,g_gpu_options_atlas);
        if (g_gpu_options_target) SDL_ReleaseGPUTexture(g_device,g_gpu_options_target);
        if (g_gpu_options_vertices) SDL_ReleaseGPUBuffer(g_device,g_gpu_options_vertices);
        g_gpu_options_atlas=NULL; g_gpu_options_target=NULL;
        g_gpu_options_vertices=NULL;
        free(atlas); free(vertices); return -1;
    }
    uint8_t *mapped = (uint8_t *)SDL_MapGPUTransferBuffer(g_device,transfer,false);
    if (!mapped) {
        SDL_ReleaseGPUTransferBuffer(g_device,transfer);
        SDL_ReleaseGPUTexture(g_device,g_gpu_options_atlas);
        SDL_ReleaseGPUTexture(g_device,g_gpu_options_target);
        SDL_ReleaseGPUBuffer(g_device,g_gpu_options_vertices);
        g_gpu_options_atlas=NULL; g_gpu_options_target=NULL;
        g_gpu_options_vertices=NULL;
        free(atlas); free(vertices); return -1;
    }
    memcpy(mapped,atlas,atlas_bytes);
    memcpy(mapped+atlas_bytes,vertices,vertex_bytes);
    g_profile_allocations += 7;
    free(atlas); free(vertices);
    SDL_UnmapGPUTransferBuffer(g_device,transfer);
    profile_add(&g_profile_staging_ns, profile_start);
    SDL_GPUCommandBuffer *cmd = SDL_AcquireGPUCommandBuffer(g_device);
    if (!cmd) {
        SDL_ReleaseGPUTransferBuffer(g_device,transfer);
        SDL_ReleaseGPUTexture(g_device,g_gpu_options_atlas);
        SDL_ReleaseGPUTexture(g_device,g_gpu_options_target);
        SDL_ReleaseGPUBuffer(g_device,g_gpu_options_vertices);
        g_gpu_options_atlas=NULL; g_gpu_options_target=NULL;
        g_gpu_options_vertices=NULL;
        return -1;
    }
    SDL_GPUCopyPass *copy = SDL_BeginGPUCopyPass(cmd);
    const SDL_GPUTextureTransferInfo texture_src = {
        .transfer_buffer=transfer,.pixels_per_row=CANVAS_W,
        .rows_per_layer=GPU_OPTIONS_ATLAS_H,
    };
    const SDL_GPUTextureRegion texture_dst = {
        .texture=g_gpu_options_atlas,.w=CANVAS_W,.h=GPU_OPTIONS_ATLAS_H,.d=1,
    };
    SDL_UploadToGPUTexture(copy,&texture_src,&texture_dst,false);
    const SDL_GPUTransferBufferLocation buffer_src = {
        .transfer_buffer=transfer,.offset=(Uint32)atlas_bytes,
    };
    const SDL_GPUBufferRegion buffer_dst = {
        .buffer=g_gpu_options_vertices,.size=(Uint32)vertex_bytes,
    };
    SDL_UploadToGPUBuffer(copy,&buffer_src,&buffer_dst,true);
    SDL_EndGPUCopyPass(copy);
    profile_start = profile_counter();
    const bool submitted = SDL_SubmitGPUCommandBuffer(cmd);
    profile_add(&g_profile_submit_ns, profile_start);
    SDL_ReleaseGPUTransferBuffer(g_device,transfer);
    if (!submitted) {
        SDL_ReleaseGPUTexture(g_device,g_gpu_options_atlas);
        SDL_ReleaseGPUTexture(g_device,g_gpu_options_target);
        SDL_ReleaseGPUBuffer(g_device,g_gpu_options_vertices);
        g_gpu_options_atlas=NULL; g_gpu_options_target=NULL;
        g_gpu_options_vertices=NULL;
        return -1;
    }
    profile_start = profile_counter();
    SDL_WaitForGPUIdle(g_device);
    profile_add(&g_profile_wait_ns, profile_start);
    g_gpu_options_ready = 1;
    g_profile_upload_bytes += atlas_bytes + vertex_bytes;
    g_profile_rebuilds++;
    return 0;
}
static int refresh_art_gpu(void) {
    const unsigned generation = dw3_pc_title_overlay_generation();
    const int cursor = dw3_pc_title_overlay_options_cursor();
    int values[DW3_TITLE_OPTION_ROWS];
    for (int i=0;i<DW3_TITLE_OPTION_ROWS;++i) values[i]=dw3_port_options_level(i);
    if (g_gpu_options_failed) return -1;
    if (!g_gpu_options_ready && build_gpu_options()!=0) {
        g_gpu_options_failed = 1;
        return -1;
    }
    if (g_gpu_options_valid && generation==g_gpu_options_art_generation &&
        cursor==g_gpu_options_cursor &&
        memcmp(values,g_gpu_options_values,sizeof(values))==0) return 0;
    const int combo = gpu_options_combo(cursor,values);
    SDL_GPUCommandBuffer *cmd=SDL_AcquireGPUCommandBuffer(g_device);
    if (!cmd) return -1;
    const SDL_GPUColorTargetInfo target = {
        .texture=g_gpu_options_target,.clear_color={0,0,0,0},
        .load_op=SDL_GPU_LOADOP_CLEAR,.store_op=SDL_GPU_STOREOP_STORE,
        .cycle=true,
    };
    SDL_GPURenderPass *pass=SDL_BeginGPURenderPass(cmd,&target,1,NULL);
    if (!pass) return -1;
    SDL_BindGPUGraphicsPipeline(pass,g_pipeline);
    const SDL_GPUBufferBinding binding={
        .buffer=g_gpu_options_vertices,
        .offset=(Uint32)((size_t)combo*36*sizeof(OverlayVertex)),
    };
    SDL_BindGPUVertexBuffers(pass,0,&binding,1);
    const SDL_GPUTextureSamplerBinding sampler={
        .texture=g_gpu_options_atlas,.sampler=g_sampler,
    };
    SDL_BindGPUFragmentSamplers(pass,0,&sampler,1);
    SDL_DrawGPUPrimitives(pass,36,1,0,0);
    SDL_EndGPURenderPass(pass);
    Uint64 profile_start=profile_counter();
    const bool submitted=SDL_SubmitGPUCommandBuffer(cmd);
    profile_add(&g_profile_submit_ns,profile_start);
    if (!submitted) return -1;
    profile_start=profile_counter();
    SDL_WaitForGPUIdle(g_device);
    profile_add(&g_profile_wait_ns,profile_start);
    g_gpu_options_art_generation=generation;
    g_gpu_options_cursor=cursor;
    memcpy(g_gpu_options_values,values,sizeof(values));
    g_gpu_options_valid=1;
    g_profile_rebuilds++;
    return 0;
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
    const int composition = menu_composition_mode();
    profile_begin();
    /* The options screen replaces the menu and the logo while it is open. */
    if (dw3_port_options_is_open() && g_pipeline && g_texture && g_vertices &&
        g_sampler) {
        if (composition == MENU_COMPOSITION_CPU) {
            if (refresh_art_cpu() != 0) {
                if (!g_cpu_fallback_logged) {
                    fprintf(stderr, "dw3-title-overlay: CPU dirty-region path unavailable; restoring reference artwork\n");
                    g_cpu_fallback_logged = 1;
                }
                g_options_generation = ~dw3_pc_title_overlay_options_generation();
                refresh_art();
            }
        } else if (composition == MENU_COMPOSITION_GPU) {
            if (refresh_art_gpu() != 0) {
                if (!g_cpu_fallback_logged) {
                    fprintf(stderr, "dw3-title-overlay: GPU atlas path unavailable; restoring reference artwork\n");
                    g_cpu_fallback_logged = 1;
                }
                g_options_generation = ~dw3_pc_title_overlay_options_generation();
                refresh_art();
            }
        } else {
            refresh_art();
        }
        g_selection = STATE_OPTIONS;
        g_overlay_active = 1;
        profile_end();
        return;
    }
    if (!active || !g_pipeline || !g_texture || !g_vertices || !g_sampler) {
        g_overlay_active = 0;
        profile_end();
        return;
    }
    if (selected == -2) {
        g_overlay_active = 0;
        profile_end();
        return;
    }
    if (selected < 0) g_selection = 0;
    else if (selected >= 2) g_selection = 3;
    else g_selection = selected + 1;
    refresh_art();
    g_overlay_active = 1;
    profile_end();
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
    SDL_BindGPUGraphicsPipeline(pass, g_gpu_options_composite_pipeline);
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
    const int gpu_options = g_selection == STATE_OPTIONS &&
        menu_composition_mode() == MENU_COMPOSITION_GPU &&
        g_gpu_options_valid && g_gpu_options_target;
    const SDL_GPUTextureSamplerBinding sampler_binding = {
        .texture = gpu_options ? g_gpu_options_target : g_texture,
        .sampler = g_sampler,
    };
    SDL_BindGPUFragmentSamplers(pass, 0, &sampler_binding, 1);
    SDL_DrawGPUPrimitives(pass, 6, 1,
        gpu_options ? (STATE_COUNT + 4) * 6 : (Uint32)(g_selection * 6), 0);
    profile_overlay_draw_complete();
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
