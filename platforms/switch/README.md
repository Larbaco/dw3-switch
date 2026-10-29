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

### Region launcher (P6.24 slice 2)

`launcher/` is a separate, game-free libnx project: a picker `.nro` that
chain-loads `dw3-us.nro` or `dw3-eu.nro`. Build it with
`tools/build_switch_launcher.sh` (output `build/switch-launcher/out/dw3.nro`);
see [docs/SINGLE_BUILD.md](../../docs/SINGLE_BUILD.md#as-built-slice-2-the-switch-launcher).
The Makefile above and the CMake game image do not build it.

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
(`0x18` in the header) should be 16440 with the stock libnx icon; a custom
JPEG icon makes that section larger by the icon file size.

### Homebrew icon (game image)

The CMake game `.nro` passes `ICON` to `nx_create_nro` so `elf2nro` embeds
[`icon.jpg`](icon.jpg) (256×256 JPEG). The owner-approved 4C master is
[`docs/assets/dw3nx-4c-master.png`](../../docs/assets/dw3nx-4c-master.png).
The launcher-sized export is
[`docs/assets/icon-256.png`](../../docs/assets/icon-256.png); regenerate it
with nearest-neighbor sampling to preserve pixel edges, then export the JPEG:

```bash
convert docs/assets/dw3nx-4c-master.png -filter point -resize 256x256! \
  docs/assets/icon-256.png
convert docs/assets/icon-256.png -colorspace sRGB -type TrueColor -quality 90 \
  platforms/switch/icon.jpg
```

hbmenu shows this icon for SD installs (and most homebrew launchers). The
Makefile `host_hello` sample keeps the libnx default icon.

### hbmenu version (game image)

The CMake game `.nro` sets NACP `display_version` from git / the release tag
via [`tools/dw3_nro_version.py`](../../tools/dw3_nro_version.py) and
`-DDW3_NACP_VERSION` in [`CMakeLists.txt`](CMakeLists.txt). libnx stores that
field in 16 bytes (15 printable chars + NUL); the helper compresses
`0.6.5-preview.18` to `0.6.5-p.18` so nacptool does not truncate it to
`0.6.5-preview.1`.

| Build | Full label (`DW3_NRO_VERSION`) | hbmenu version |
|---|---|---|
| Tagged release (`RELEASE_TAG=v0.6.5-preview.18`) | `0.6.5-preview.18` | `0.6.5-p.18` |
| Untagged CI / PR (`GITHUB_SHA`) | short SHA | same short SHA |
| Local `build_switch_game.sh` | `git describe --tags --dirty --always` | compressed ≤15 |

hbmenu shows the NACP version for copies installed on the SD card. An
`nxlink` load may not refresh that metadata; the same full label is logged
once at boot as `dw3: version=…` (stderr on nxlink, and `sdmc:/switch/dw3/dw3.log`).
Each boot rotates the log: `dw3.log` is the current run, `dw3.1.log` the one
before, up to `dw3.4.log` (5 runs kept). The `dw3-run:` line dates the run.
`tools/build_switch_game.sh` checks the generated `.nacp` after each build.
Manual check: in hbmenu, highlight `Digimon World 3 NX` / `Digimon World 3 NX (nxlink)` and read
the version under the title.

### Displayed app name

The title shown in hbmenu and Sphaira comes from the NACP name, not the file name.
Release builds (US and EU) show **Digimon World 3 NX**; the nxlink build shows
**Digimon World 3 NX (nxlink)**, and the replay test build appends ` (replay)`.
The `.nro` file names (`dw3-switch.nro`, `dw3-switch-eu.nro`, `-nxlink`, `-replay`)
are unchanged. A forwarder created from an earlier build keeps the old name, so
recreate the forwarder in Sphaira (or re-add the `.nro` in hbmenu) to pick up the
new name. Set in `platforms/switch/CMakeLists.txt` (`DW3_NACP_NAME`).

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

Since 2026-10-29 every game build, including the released `dw3-switch.nro`,
checks at run time whether nxlink started it (libnx `__nxlink_host`, set from
the final argv token) and only then calls `nxlinkStdio()`. Started from hbmenu
or Sphaira it needs no network and writes `dw3.log` only. The log says
`dw3: nxlink host=... connected` or `dw3: no nxlink host (launched from ...)`.
The standard CMake build:

```bash
./tools/build_switch_game.sh
# build/switch/dw3-switch.nro
```

The separate `--nxlink` build below is now a developer variant: it adds the HUD
and the sampled debug lines (compile-time, they cost per-frame work), and is not
published:

```bash
./tools/build_switch_game.sh --nxlink
# build/switch-nxlink/dw3-switch-nxlink.nro
```

This option defines `DW3_NXLINK_STDIO` only for the CMake game image (the
debug defaults, the file name and the "(nxlink)" title). The Makefile
`host_hello` sample is unaffected. When nxlink started the NRO, it initializes
libnx's default socket service before calling `nxlinkStdio()`, before input,
SD mount and game logging. The host and stdio fd are recorded in `dw3.log`;
connection failure is nonfatal.
Logs continue going to `dw3.log` on SD and `svcOutputDebugString` as before.
`--nxlink` also defaults `DW3_NX_DEBUG_MESH=ON`, `DW3_NX_DEBUG_MEM=ON`, and
`DW3_NX_DEBUG_OVERLAY=ON`: sampled `mesh dbg:` / `mem dbg:` lines alongside
each `frame dt us=` (~1 Hz), plus an on-screen HUD (FPS / mode / mesh /
heap; toggle **Minus + L + R**). Details:
[docs/NXLINK_DEBUG.md](../../docs/NXLINK_DEBUG.md),
[docs/DEBUG_OVERLAY.md](../../docs/DEBUG_OVERLAY.md), and
[docs/P5.1_MEMORY.md](../../docs/P5.1_MEMORY.md). Battle defaults to
`DW3_NX_MESH_SOFT_BATTLE=ON` (software `RasterTriangle` under the FIGHTSTG
gate) until mesh FBO dumps are non-black; pass
`-DDW3_NX_MESH_SOFT_BATTLE=OFF` to exercise GLES mesh.

### Build without the Cheats screen (P6.20)

Every build has the Cheats screen of the Options screen, off until its master
switch is turned on. `./tools/build_switch_game.sh --no-cheats` (with `--eu`,
`--nxlink` as usual) builds without it (`-DDW3_CHEATS=OFF`, build directory
suffix `-nocheats`, for example `build/switch-eu-nocheats/dw3-switch.nro`). The
code is the game as before the feature. When `build/` is a symlink to another
directory (an ext4 build tree under an NTFS checkout), the script also mounts
that directory into the container. See [docs/RELEASES.md](../../docs/RELEASES.md#building-without-cheats).

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
   The host firewall must allow the nxlink connection. The released
   `dw3-switch.nro` works the same way (`nxlink -a <ip> -s dw3-switch.nro`);
   without an nxlink launch it never touches the network.

Latest tagged preview Release:
[v0.6.5-preview.10](https://github.com/Larbaco/dw3-switch/releases/tag/v0.6.5-preview.10)
(gated FIGHTSTG mesh + nxlink `mesh dbg:`). `main` may be ahead (flat
saves, soft battle default, debug HUD, STGMCARD bar). Picture baseline:
[v0.6.5-preview.2](https://github.com/Larbaco/dw3-switch/releases/tag/v0.6.5-preview.2)
(`8273a91`, 8935621 bytes).
[v0.6.5-preview.1](https://github.com/Larbaco/dw3-switch/releases/tag/v0.6.5-preview.1)
is the earlier 8927429-byte file. It still crops the title and freezes
the movie.

The default US image reads the owner's files from `sdmc:/switch/dw3/us`
(`DW3_DEFAULT_RUNTIME` from CMake `DW3_VERSION`). If that directory is
missing or incomplete, US boot extracts it from the matching Mode2 BIN/CUE in
the parent folder. EU: `./tools/build_switch_game.sh --eu` →
`sdmc:/switch/dw3/eu`; the EU image can extract its own Mode2 BIN/CUE placed
inside that directory (or its parent). It accepts `SLES_039.36`, uses the EU
file-sector table and boot-TIM address, and writes `files_eu.json` last as the
completion marker. A US disc is rejected by the EU build. Existing complete
trees are checked without re-extracting. Packing:
[docs/DISC_SETUP.md](../../docs/DISC_SETUP.md),
[docs/RUNTIME_LAYOUT.md](../../docs/RUNTIME_LAYOUT.md),
[docs/P3_EU_BUILD.md](../../docs/P3_EU_BUILD.md).

The title blit width follows the display mode (320, not the 256-wide
horizontal range). That landed with the movie present and the 4:3
full-height fit in pull request #53.

## PSY-Z backend (Phase 4)

P4.1 chose a **native libnx** backend (not SDL3-dkp). `nx_gpu.c` presents
VRAM. `nx_audio.c` feeds audren from the SPU in the CMake image.
`nx_input.c` and the SD mount in `boot_main.c` are the P4.4 and P4.5 host
paths. `nx_module.c` stays a static dispatch, with no `dlopen`. Hardware
checks for P4.3 through P4.6 are still open.
Decision: [docs/SWITCH_BACKEND.md](../../docs/SWITCH_BACKEND.md).

## Test targets

| Target | Role |
|---|---|
| **OLED Switch (Atmosphere + hbmenu)** | **Primary gate** for Phase 4 host work — copy `dw3-switch.nro` to SD, launch from hbmenu; use **nxlink** for stderr/FPS when debugging. |
| **Local Switch emulators (owner machine)** | **Active** smoke loop since 2026-10-10: Ryubing 1.3.339-canary loads and renders the `.nro` (EGL/GLES2 present animates, `audren` starts). Paths live outside this repo (not vendored). Eden (Yuzu-line AppImages) untried. Emulator runs do not satisfy P4.3 (underruns on hardware) or the field and battle gate. |

Owner-local install hints (not in git): Ryubing at `/mnt/windows/Jogos/emuladores/Ryubing` (needs `DOTNET_ROOT=$HOME/.dotnet`); Eden AppImages at `/mnt/windows/Jogos/emuladores/Eden-Linux-*.AppImage`. Guest `stderr` is invisible in Ryubing (it expects nxlink), and `--no-gui` headless fails at `Image initialization failed` — use the GUI. Details: [docs/SWITCH_BACKEND.md](../../docs/SWITCH_BACKEND.md#ryubing-smoke-loop-active-since-2026-10-10).
