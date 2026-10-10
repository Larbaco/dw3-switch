/* P4.2 gpu_hello — EGL + OpenGL ES clear/present at vsync.
 * Done-when for the roadmap requires 60 fps on hardware; this sample is the
 * artifact to run once a Switch is available. Logging goes to stderr (nxlink). */
#include <math.h>
#include <stdio.h>
#include <switch.h>

#include "../psyz_backend/nx.h"

int main(int argc, char **argv) {
    (void)argc;
    (void)argv;

    padConfigureInput(1, HidNpadStyleSet_NpadStandard);
    PadState pad;
    padInitializeDefault(&pad);

    if (!dw3_nx_gpu_init()) {
        fprintf(stderr, "gpu_hello: dw3_nx_gpu_init failed\n");
        return 1;
    }

    Dw3NxGpuInfo info;
    dw3_nx_gpu_get_info(&info);
    fprintf(stderr,
            "gpu_hello: backend v%d  %ux%u (%s)  + to exit\n",
            dw3_nx_backend_sketch_version(), info.width, info.height,
            info.docked ? "docked" : "handheld");

    u64 frames = 0;
    u64 fps_window_start = armGetSystemTick();
    u32 fps_frames = 0;
    float fps = 0.0f;

    while (appletMainLoop()) {
        padUpdate(&pad);
        if (padGetButtonsDown(&pad) & HidNpadButton_Plus) {
            break;
        }

        dw3_nx_gpu_update_mode();

        /* Slow hue cycle so a present path is obvious on-screen. */
        float t = (float)frames * (1.0f / 60.0f);
        float r = 0.25f + 0.25f * sinf(t);
        float g = 0.25f + 0.25f * sinf(t + 2.094f);
        float b = 0.35f + 0.25f * sinf(t + 4.189f);
        dw3_nx_gpu_clear(r, g, b, 1.0f);

        if (!dw3_nx_gpu_swap()) {
            fprintf(stderr, "gpu_hello: eglSwapBuffers failed\n");
            break;
        }

        frames++;
        fps_frames++;

        u64 now = armGetSystemTick();
        u64 delta = now - fps_window_start;
        /* ~1 second windows using the system tick counter. */
        if (armTicksToNs(delta) >= 1000000000ULL) {
            fps = (float)fps_frames * 1000000000.0f / (float)armTicksToNs(delta);
            dw3_nx_gpu_get_info(&info);
            fprintf(stderr, "gpu_hello: fps=%.1f  %ux%u (%s)  frames=%llu\n", fps,
                    info.width, info.height, info.docked ? "docked" : "handheld",
                    (unsigned long long)frames);
            fps_window_start = now;
            fps_frames = 0;
        }
    }

    dw3_nx_gpu_shutdown();
    return 0;
}
