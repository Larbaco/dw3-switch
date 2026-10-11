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

### CMake build — the game image (P4.7, in progress)

`platforms/switch/CMakeLists.txt` is the entry point that links the Phase 3 game
image against PSY-Z for aarch64. It mirrors `platforms/pc/CMakeLists.txt` and
reuses `port/CMakeLists.txt` verbatim (same `--whole-archive` overlay libs and
`__start_dw3data_*` section keeps). The devkitPro Makefile above stays for the
standalone bring-up sample. It globs `source/*.c` and `psyz_backend/*.c`, then
filters out `boot_main.c`, `nx_input.c`, and `nx_present.c` — those three need
PSY-Z, which this sample does not link. CI builds this Makefile `.nro` only.

Cross-compile with devkitA64's toolchain file — the Makefile path needs
`DEVKITPRO`; this one needs CMake inside the devkitPro image, which is what
`tools/dk.sh` provides:

```bash
DW3=/mnt/windows/Jogos/dw3-switch/.worktrees/<worktree>   # or any checkout
docker run --rm \
  -v "$DW3:$DW3:ro" -v /tmp/dw3-nx-build:"$DW3/build" \
  -v /path/to/main/.git:/path/to/main/.git:ro -w "$DW3" \
  -e DEVKITPRO=/opt/devkitpro \
  -e GIT_CONFIG_COUNT=1 -e GIT_CONFIG_KEY_0=safe.directory -e GIT_CONFIG_VALUE_0='*' \
  -e PATH="/opt/devkitpro/tools/bin:/opt/devkitpro/devkitA64/bin:/usr/bin:/bin" \
  devkitpro/devkita64:latest bash -c \
  'cmake -S platforms/switch -B build/switch -G Ninja \
     -DCMAKE_TOOLCHAIN_FILE=/opt/devkitpro/cmake/Switch.cmake &&
   cmake --build build/switch -j"$(nproc)"'
```

Four things there are load-bearing and easy to trip over:

- **Source mounted read-only, `build/` redirected to ext4.** Nothing the build
  needs to write lives outside `build/` ([tools/apply_patches.sh](../../tools/apply_patches.sh)
  writes `build/decomp-src`), and drvfs cannot be written by the container user.
- **Mount at the same absolute path as the host.** `vendor/dw3_decomp` is a
  submodule, and in a git worktree its `.git` pointer resolves into the *main*
  repo — mounting at a different prefix (or not mounting the main `.git`) makes
  `apply_patches.sh` fail with `fatal: not a git repository`.
- **`safe.directory`**, because the container runs as a different uid than the
  checkout owner.
- **Do not use a login shell** (`bash -l`): `/etc/profile` rewrites `PATH` and
  drops `devkitA64/bin`, which is where `aarch64-none-elf-nm` lives.

**Current CMake status (2026-10-10):** the full game image links and produces a
`.nro`; the temporary combined build used current `main` plus the P4.4/P4.5
feature commits. It compiled 1144 targets and emitted an 8.9 MB NRO. The
Makefile build above remains the separate `host_hello` bring-up sample and is
still the `.nro` CI builds; it does not contain the game.

**OLED smoke (2026-10-10/11):** the owner installed the US runtime and launched
the combined game NRO from hbmenu. The first attempt stopped because the runtime
lacked `us/memcard/`; creating that empty directory allowed launch. The owner
then confirmed the game reached the main menu and loaded a new game correctly.
The P4.7 title-screen done-when is met on hardware; the wider Phase 4 gate and
P4.4/P4.5/P4.6's separate hardware checks remain open. The Switch runtime layout
and first-boot memory-card requirement are documented in
[docs/RUNTIME_LAYOUT.md](../../docs/RUNTIME_LAYOUT.md).

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
