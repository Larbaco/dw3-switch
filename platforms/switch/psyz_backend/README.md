# PSY-Z native libnx backend

P4.1 chose this layout over SDL3-dkp
([docs/SWITCH_BACKEND.md](../../../docs/SWITCH_BACKEND.md)).
P4.2 links `nx_gpu.c` (EGL + GLES2 present); P4.3 links `nx_audio.c`
(audren @ 44.1 kHz). Combined sample: `../source/main.c` (`host_hello`).

| File | Role | Linked? |
|---|---|---|
| `nx.h` | Shared Switch helpers / GPU + audio API | header |
| `nx_gpu.c` | EGL + GLES2 present, docked/handheld | **yes (P4.2)** |
| `nx_audio.c` | `audren` PCM from software SPU | **yes (P4.3)** |
| `nx_input.c` | `hid` / `pad` → PSY-Z pad frames | **yes (P4.4)** |
| `nx_rumble.c` | `PadSetAct` → HD rumble (`hidSendVibrationValue`), P10.4 | **yes (P10.4, unverified on hardware)** |
| `nx_os.c` | Threads, locks, time | empty TU until P4.5 |
| `nx_file.c` | `romfs` / `sdmc` file I/O | empty TU until P4.5 |
| `nx_log.c` | Logging to stdout / nxlink | empty TU until P4.5 |
| `nx_module.c` | Static `Psyz_ModuleOpen` table | empty TU until P4.6 |

Build: `./tools/dk.sh make -C platforms/switch` (see `../README.md` for the
NTFS `DW3_DK_TMPBUILD` workaround). Details:
[docs/P4.2_VIDEO.md](../../../docs/P4.2_VIDEO.md),
[docs/P4.3_AUDIO.md](../../../docs/P4.3_AUDIO.md).
