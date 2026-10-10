# Switch host

devkitPro / libnx target for Digimon World 3.

## Build

```bash
# from repo root (Linux/ext4 clone preferred — see tools/docker/README.md)
./tools/dk.sh make -C platforms/switch
# artifact: platforms/switch/dw3-switch.nro

# NTFS / drvfs (Windows mount): overlay build dir on ext4
DW3_DK_TMPBUILD=/tmp/dw3-dk-build ./tools/dk.sh make -C platforms/switch
```

Current binary is the **combined host bring-up sample** (P4.2 EGL
`gpu_hello` present + P4.3 `audio_hello` audren). No game data required.
See [docs/P4.2_VIDEO.md](../../docs/P4.2_VIDEO.md) and
[docs/P4.3_AUDIO.md](../../docs/P4.3_AUDIO.md).

## PSY-Z backend (Phase 4)

P4.1 chose a **native libnx** backend (not SDL3-dkp). `psyz_backend/nx_gpu.c`
and `nx_audio.c` are linked; other units are empty placeholders for P4.4–P4.6.
Decision: [docs/SWITCH_BACKEND.md](../../docs/SWITCH_BACKEND.md).

## Test targets

| Target | Role |
|---|---|
| **OLED Switch (Atmosphere + hbmenu)** | **Primary gate** for Phase 4 host work — copy `dw3-switch.nro` to SD, launch from hbmenu; use **nxlink** for stderr/FPS when debugging. |
| **Local Switch emulators (owner machine)** | Optional **deferred** smoke before hardware deploy; paths live outside this repo (not vendored). Prefer **Ryubing** (Ryujinx fork) for homebrew `.nro` when we run smoke tests. Eden (Yuzu-line AppImages) is a secondary option. Emulator runs do **not** satisfy P4.2/P4.3 done-when (hardware fps / underruns). |

Owner-local install hints (not in git): Ryubing at `/mnt/windows/Jogos/emuladores/Ryubing` (+ Ryubing-Canary tarball); Eden AppImages at `/mnt/windows/Jogos/emuladores/Eden-Linux-*.AppImage`. Details: [docs/P4.2_VIDEO.md](../../docs/P4.2_VIDEO.md#optional-local-emulator-smoke-deferred).
