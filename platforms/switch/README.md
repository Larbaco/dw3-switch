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
filters out `boot_main.c`, `nx_input.c`, and `nx_present.c`. Those three need
PSY-Z, which this sample does not link. CI job `switch` builds this Makefile
`.nro` only. Job `switch-game` builds the CMake game image and does not
publish a Release.

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

Status (P4.7): the CMake target links. `source/boot_main.c` calls
`dw3_game_main`. `psyz_backend/switch_gpu.c` supplies the `Draw_*` entry
points an earlier link stop was missing, and `nx_gpu.c` presents the
software rasterizer. A clean build is about 1144 objects and an 8.9 MB
`.nro` at `build/switch/dw3-switch.nro`. The title draws in Ryubing, and
the owner has launched the game from hbmenu on an OLED Switch, reaching the
main menu and loading a new game. Current `main` also contains the title-width,
4:3 aspect and movie-picture sampling fixes. The first boot needs an empty
`sdmc:/switch/dw3/us/memcard/` or it stops in `memcard_host_ensure()`.
See [docs/SWITCH_BACKEND.md](../../docs/SWITCH_BACKEND.md) and
[docs/RUNTIME_LAYOUT.md](../../docs/RUNTIME_LAYOUT.md).

The Makefile path above is unchanged. It still links `source/main.c`
(`host_hello`). CI job `switch` builds that sample. Release packaging
does not publish it.

The game `.nro` is this CMake target. `tools/build_switch_game.sh` is the
command CI runs (ext4 checkout, `devkitpro/devkita64`, no login shell).
Pull requests upload `build/switch/dw3-switch.nro` as the Actions artifact
`dw3-switch-game-nro`. A GitHub Release is
[`.github/workflows/switch-release.yml`](../../.github/workflows/switch-release.yml),
on `workflow_dispatch` or a `v*` tag, marked a prerelease. `workflow_dispatch`
shows up in the Actions UI only after that file is on `main`. A tag push
runs it from the tagged commit. Assets are that `.nro` and
[`release/README.txt`](release/README.txt). No disc data.

### Development build with live nxlink logs

The standard CMake build remains unchanged and does not require a network
connection:

```bash
./tools/build_switch_game.sh
# build/switch/dw3-switch.nro
```

For live stdout/stderr on the development host, opt in to a separate NRO and
build directory:

```bash
./tools/build_switch_game.sh --nxlink
# build/switch-nxlink/dw3-switch-nxlink.nro
```

This option defines `DW3_NXLINK_STDIO` only for the CMake game image. The
Makefile `host_hello` sample and standard/release NRO are unaffected. The
nxlink NRO initializes libnx's default socket service before calling
`nxlinkStdio()` and does so before input, SD mount and game logging. The socket
result and stdio fd are recorded in `dw3.log`; connection failure is nonfatal.
Logs continue going to `dw3.log` on SD and `svcOutputDebugString` as before.

libnx obtains the nxlink host address from a special final argv token supplied
by the NetLoader. Start with hbmenu's NetLoader (press **Y**) if a third-party
launcher does not establish the callback stream; successful NRO transfer alone
does not prove the callback connection is configured.

To launch and receive the log stream:

1. Put the OLED and development host on the same reachable network.
2. Open hbmenu on the Switch, press **Y** for NetLoader and note its IP; leave
   NetLoader waiting for a connection.
3. On the host, start the upload and listener (replace the address as needed):

   ```bash
   docker run --rm --network host \
     -v "$PWD/build/switch-nxlink:/nro:ro" \
     devkitpro/devkita64:latest \
     nxlink -a 192.168.1.42 -s /nro/dw3-switch-nxlink.nro
   ```

   If `nxlink` is installed on the host, the equivalent is
   `nxlink -a <Switch-IP> -s build/switch-nxlink/dw3-switch-nxlink.nro`.
   The host firewall must allow the nxlink connection. The standard NRO does
   not call `nxlinkStdio()` and remains the no-network fallback.

First preview: [v0.6.5-preview.1](https://github.com/Larbaco/dw3-switch/releases/tag/v0.6.5-preview.1)
(`dw3-switch.nro`, 8927429 bytes, plus `README.txt`).

The running game reads the owner's US files from `sdmc:/switch/dw3/us`
(`DW3_DEFAULT_RUNTIME` in `source/boot_main.c`). Packing steps:
[docs/DISC_SETUP.md](../../docs/DISC_SETUP.md) and
[docs/RUNTIME_LAYOUT.md](../../docs/RUNTIME_LAYOUT.md).

The title blit width follows the display mode (320, not the 256-wide
horizontal range). That landed with the movie present and the 4:3
full-height fit in pull request #53.

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
