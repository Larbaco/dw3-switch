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

### The `.nro` must embed its NACP

`switch_rules`' `%.nro` rule is `elf2nro $< $@ $(NROFLAGS)` — it never passes
`--nacp` itself, so the Makefile sets `NROFLAGS := --nacp=$(OUTPUT).nacp`.
Without it `elf2nro` writes its stock "HOMEBREW" NACP and **no asset
section**: hbmenu shows no title/author/version (even though `APP_TITLE`,
`APP_AUTHOR` and `APP_VERSION` are built into `dw3-switch.nacp`), and
LibHac-based loaders reject the file — Ryujinx dies with
`NullReferenceException` in `Nro.OpenNroAssetSection`. A correct build has a
16 KiB asset section past the executable: `file size - NRO size field`
(`0x18` in the header) should be 16440.

## PSY-Z backend (Phase 4)

P4.1 chose a **native libnx** backend (not SDL3-dkp). `psyz_backend/nx_gpu.c`
and `nx_audio.c` are linked; other units are empty placeholders for P4.4–P4.6.
Decision: [docs/SWITCH_BACKEND.md](../../docs/SWITCH_BACKEND.md).

## Test targets

| Target | Role |
|---|---|
| **OLED Switch (Atmosphere + hbmenu)** | **Primary gate** for Phase 4 host work — copy `dw3-switch.nro` to SD, launch from hbmenu; use **nxlink** for stderr/FPS when debugging. |
| **Local Switch emulators (owner machine)** | **Active** smoke loop since 2026-10-10: Ryubing 1.3.339-canary loads and renders the `.nro` (EGL/GLES2 present animates, `audren` starts). Paths live outside this repo (not vendored). Eden (Yuzu-line AppImages) untried. Emulator runs do **not** satisfy P4.2/P4.3 done-when (hardware fps / underruns). |

Owner-local install hints (not in git): Ryubing at `/mnt/windows/Jogos/emuladores/Ryubing` (needs `DOTNET_ROOT=$HOME/.dotnet`); Eden AppImages at `/mnt/windows/Jogos/emuladores/Eden-Linux-*.AppImage`. Guest `stderr` is invisible in Ryubing (it expects nxlink), and `--no-gui` headless fails at `Image initialization failed` — use the GUI. Details: [docs/SWITCH_BACKEND.md](../../docs/SWITCH_BACKEND.md#ryubing-smoke-loop-active-since-2026-10-10).
