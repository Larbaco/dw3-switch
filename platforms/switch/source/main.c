/* P4.2 gpu_hello + P4.3 audio_hello — combined host bring-up sample.
 * EGL clear/present at vsync plus audren @ 44.1 kHz (weak SPU tone stub).
 * Roadmap done-when still needs Switch hardware (60 fps, zero underruns).
 * Logging goes to stderr (nxlink). */
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
        fprintf(stderr, "host_hello: dw3_nx_gpu_init failed\n");
        return 1;
    }

    if (!dw3_nx_audio_init()) {
        fprintf(stderr, "host_hello: dw3_nx_audio_init failed\n");
        dw3_nx_gpu_shutdown();
        return 1;
    }

    Dw3NxGpuInfo info;
    dw3_nx_gpu_get_info(&info);
    Dw3NxAudioStats st;
    dw3_nx_audio_get_stats(&st);

    fprintf(stderr,
            "host_hello: backend v%d  gpu %ux%u (%s)  audio %u Hz x%u "
            "(%u-frame x %u bufs)\n",
            dw3_nx_backend_sketch_version(), info.width, info.height,
            info.docked ? "docked" : "handheld", st.sample_rate, st.channels,
            st.buf_frames, st.num_buffers);
    fprintf(stderr,
            "host_hello: A=pause  B=unpause  + =exit  (weak SPU 440 Hz stub)\n");

    u64 frames = 0;
    u64 fps_window_start = armGetSystemTick();
    u32 fps_frames = 0;
    float fps = 0.0f;
    u64 last_log = armGetSystemTick();

    while (appletMainLoop()) {
        padUpdate(&pad);
        u64 kDown = padGetButtonsDown(&pad);
        if (kDown & HidNpadButton_Plus) {
            break;
        }
        if (kDown & HidNpadButton_A) {
            Psyz_AudioPause();
            fprintf(stderr, "host_hello: audio paused\n");
        }
        if (kDown & HidNpadButton_B) {
            Psyz_AudioUnpause();
            fprintf(stderr, "host_hello: audio unpaused\n");
        }

        dw3_nx_gpu_update_mode();

        float t = (float)frames * (1.0f / 60.0f);
        float r = 0.25f + 0.25f * sinf(t);
        float g = 0.25f + 0.25f * sinf(t + 2.094f);
        float b = 0.35f + 0.25f * sinf(t + 4.189f);
        dw3_nx_gpu_clear(r, g, b, 1.0f);

        if (!dw3_nx_gpu_swap()) {
            fprintf(stderr, "host_hello: eglSwapBuffers failed\n");
            break;
        }

        frames++;
        fps_frames++;

        u64 now = armGetSystemTick();
        if (armTicksToNs(now - last_log) >= 1000000000ULL) {
            u64 delta = now - fps_window_start;
            fps = (float)fps_frames * 1000000000.0f / (float)armTicksToNs(delta);
            dw3_nx_gpu_get_info(&info);
            dw3_nx_audio_get_stats(&st);
            fprintf(stderr,
                    "host_hello: fps=%.1f  %ux%u (%s)  frames=%llu  |  "
                    "audio queued=%llu underruns=%llu %s\n",
                    fps, info.width, info.height,
                    info.docked ? "docked" : "handheld",
                    (unsigned long long)frames,
                    (unsigned long long)st.buffers_queued,
                    (unsigned long long)st.underruns,
                    st.paused ? "paused" : "playing");
            fps_window_start = now;
            fps_frames = 0;
            last_log = now;
        }
    }

    dw3_nx_audio_shutdown();
    dw3_nx_gpu_shutdown();
    return 0;
}
