# Switch PSY-Z backend (P4.1)

Decision record for Phase 4 host work. PC remains the game bring-up target;
this doc only picks how PSY-Z talks to libnx.

## Recommendation

**Native libnx backend** in `platforms/switch/psyz_backend/`, modelled on
PSY-Z's PSP layout (`gpu`, `input`, `audio`, `os`, `file`, `log`, `module`).

Do **not** base the Switch host on PSY-Z's `sdl3_*` / `sdl3_gpu` path.

### Why native libnx

1. **Matches the rest of Phase 4.** P4.2–P4.5 already name EGL/OpenGL or
   deko3d, `audren`, `hid`, `romfs`/`sdmc`, and static modules — the same
   surfaces a PSP-style native backend would own.
2. **PSY-Z console precedent.** The PSP target does not use SDL; it replaces
   the platform files with `psyz/src/psp/psp_*.c`. Switch should follow that
   pattern under `platforms/switch/` (port ownership), not fork PSY-Z's SDL
   stack.
3. **PC default is `sdl3_gpu`, which Switch SDL3 does not provide.** A
   successful build of `devkitPro/SDL` `switch-sdl-3.4` (see experiment A)
   enables video `switch` + render `ogl` / `ogl_es2`, but **GPU drivers are
   empty** and `SDL_RENDER_GPU` configures **OFF**. Reusing PSY-Z's PC default
   renderer is a non-starter without a large SDL_GPU port or forcing
   `PSYZ_RENDERER=sdl3_gl` plus GLES wiring and maintenance of a from-source
   SDL3 tree.
4. **No pacman package.** SDL3 Switch is not in `devkitpro/devkita64`
   portlibs (only SDL2). Every CI/local build would clone and compile the
   dkp fork until packaging lands
   ([pacman-packages#432](https://github.com/devkitPro/pacman-packages/issues/432)).
5. **Thin host.** AGENTS.md keeps Switch-specific code under
   `platforms/switch/`. A native sketch stays reviewable and avoids pulling
   ~17 MiB of static SDL3 plus EGL into the critical path before we need it.

SDL3-dkp remains a **fallback** if a future packaged SDL3 + mature GLES path
makes `sdl3_gl` cheaper than finishing native present — revisit only if P4.2
stalls hard on raw EGL.

## Experiment A — dkp SDL3 from source

| Item | Result |
|---|---|
| Source | `https://github.com/devkitPro/SDL` branch `switch-sdl-3.4` (also `switch-sdl-3.2`) |
| Host image | `devkitpro/devkita64:latest` via Docker |
| Configure | `cmake -DCMAKE_TOOLCHAIN_FILE=/opt/devkitpro/cmake/Switch.cmake -DSDL_SHARED=OFF -DSDL_STATIC=ON -DSDL_TESTS=OFF -DSDL_EXAMPLES=OFF` |
| Build | **Succeeded** — `libSDL3.a` (~17 MiB), 209 objects |
| Enabled backends | Video: `dummy` `offscreen` `switch`; Render: `ogl` `ogl_es2`; Audio: `disk` `dummy` `switch`; Joystick: `switch`; **GPU: (none)** |
| Portlibs | Image has SDL2 + EGL/GLESv2/`glapi`/`drm_nouveau`; **no** `libSDL3*` |
| PSY-Z `sdl3_*` | **Not linked / not run on hardware.** Blocked for product use: no SDL_GPU (PC default), no pacman SDL3, would need cmake/toolchain glue outside PSY-Z's `PSP` branch, and community reports of incomplete Switch SDL3 |

Reproduce (build dir on a writable Linux path; do not write into the NTFS
checkout — see [tools/docker/README.md](../tools/docker/README.md)):

```bash
git clone --depth 1 --branch switch-sdl-3.4 \
  https://github.com/devkitPro/SDL.git /tmp/dkp-sdl3-switch
docker run --rm \
  -v /tmp/dkp-sdl3-switch:/sdl:ro \
  -v /tmp/sdl3-nx-build:/build \
  -e DEVKITPRO=/opt/devkitpro \
  -e PATH="/opt/devkitpro/tools/bin:/opt/devkitpro/devkitA64/bin:/usr/bin:/bin" \
  devkitpro/devkita64:latest \
  bash -lc 'cmake -S /sdl -B /build -G Ninja \
    -DCMAKE_TOOLCHAIN_FILE=/opt/devkitpro/cmake/Switch.cmake \
    -DCMAKE_BUILD_TYPE=Release -DSDL_SHARED=OFF -DSDL_STATIC=ON \
    -DSDL_TEST_LIBRARY=OFF -DSDL_TESTS=OFF -DSDL_EXAMPLES=OFF \
    && cmake --build /build -j"$(nproc)"'
```

`./tools/dk.sh` mounts the repo and uses `-u $(id -u):$(id -g)`; on this
machine that user often cannot create `platforms/switch/build/` on the
checkout mount. The SDL3 experiment used an isolated `/tmp` volume as root
inside the container. Skeleton `.nro` still builds that way on ext4.

## Experiment B — native libnx sketch (PSP layout)

| PSY-Z PSP file | Switch file | Intended libnx / stack | Status |
|---|---|---|---|
| `psp_gpu.c` | `nx_gpu.c` | EGL + OpenGL ES 2 (mesa/nouveau); PSY-Z VRAM blit later | **P4.2 linked** (`gpu_hello`) |
| `psp_input.c` | `nx_input.c` | `pad` / `hid` → PS1 pad bits (P4.4) | empty TU |
| `psp_audio.c` (+ SAS on PSP) | `nx_audio.c` | `audren` thread; software SPU like PC (`psyz_spu.c`) (P4.3) | empty TU |
| `psp_os.c` | `nx_os.c` | libnx threads / mutex / time | empty TU |
| `psp_file.c` | `nx_file.c` | `romfs:` / `sdmc:` paths for P1.6 layout (P4.5) | empty TU |
| `psp_log.c` | `nx_log.c` | stdout / nxlink | empty TU |
| `psp_module_*.c` | `nx_module.c` | static dispatch table — no `dlopen` (P4.6) | empty TU |

See [platforms/switch/psyz_backend/README.md](../platforms/switch/psyz_backend/README.md)
and [docs/P4.2_VIDEO.md](P4.2_VIDEO.md).

Wiring into PSY-Z (P4.7, partial): the submodule gained a
`CMAKE_SYSTEM_NAME STREQUAL "NintendoSwitch"` branch that skips SDL3 and appends
only the portable `psyz_spu.c` plus the SDL-free `sdl3_software.c` rasterizer,
while the platform symbols keep coming from `platforms/switch/psyz_backend/`.
The split is deliberate: the submodule only had to stop pulling SDL, so the
backend stays in this repo per the P4.1 decision. `vendor/psyz` is pinned to
`dw3-switch-backend` @ `15afac1` on the fork. The game image still does not
finish linking (see below).

## P4.7 (2026-10-10)

`platforms/switch/CMakeLists.txt` compiles the Phase 3 image for aarch64. It
mirrors `platforms/pc/CMakeLists.txt` and reuses `port/CMakeLists.txt` — same
`--whole-archive` overlay libs, same ~20 `-Wl,--undefined=__start_dw3data_*`
flags. Overlay packing (`ld -r` / `objcopy` / `nm`) is pointed at devkitA64
binutils (`DW3_LD` / `DW3_OBJCOPY` / `DW3_NM`), because the bare names are the
host tools.

Three toolchain facts that CMakeLists handles: EGL/GLES2 headers come from the
`switch-*` portlibs (no CMake config packages, so the dirs are explicit);
`libEGL.a` carries nouveau's C++ `nv50_ir` codegen, so the link goes through
g++ (the Makefile's `LD := $(CXX)`); and those binutils paths are absolute,
because a login shell's `/etc/profile` drops `devkitA64/bin` from `PATH`.

Two entry points, on purpose:

| Build | `main()` | What it produces |
|---|---|---|
| devkitPro Makefile (`make -C platforms/switch`, CI) | `source/main.c` `host_hello` | bring-up `.nro` (EGL + audren sample). No game. |
| CMake (`platforms/switch/CMakeLists.txt`) | `source/boot_main.c` | calls `dw3_game_main`. Does **not** link yet. |

`boot_main.c` is the PC host's boot order (cdfs, STR bridge, audio, memcard,
boot TIM, overlay table, vsync pump) plus the SD mount, pad source, and EGL
surface. Referencing `dw3_game_main` pulls `libgpu.o` into the link. Rebuilt
on 2026-10-10 after rebase onto current `main`: compile succeeds, then the
link stops on these **20** undefined symbols, all `Draw_*`:

`Draw_ClearImage`, `Draw_DisplayArea`, `Draw_DisplayEnable`,
`Draw_DisplayHorizontalRange`, `Draw_DisplayVerticalRange`, `Draw_ExequeSync`,
`Draw_FlushBuffer`, `Draw_LoadImage`, `Draw_MoveImage`, `Draw_PushPrim`,
`Draw_Reset`, `Draw_ResetBuffer`, `Draw_SetAreaEnd`, `Draw_SetAreaStart`,
`Draw_SetDisplayMode`, `Draw_SetMask`, `Draw_SetOffset`, `Draw_SetTexpageMode`,
`Draw_SetTextureWindow`, `Draw_StoreImage`.

`nm -u` on `libgpu.o` also lists `Draw_SetHorizontalGrid` (21 undefined in the
object). The link did not demand it. No `dw3-switch.elf` and no game `.nro`
from this configure. The earlier “links, 8.5 MiB `.nro` / 16.7 MB `.elf`”
note was the image *before* `boot_main.c` pulled `libgpu.o` in — those
platform symbols (log, `Psyz_Os*`, pad, vsync, `__wrap_*`) do resolve now.
P4.7 stays open: nothing renders, so there is no title on hardware.

## P4.2 video (2026-10-10)

- **Stack:** EGL + OpenGL ES 2 (`-lEGL -lGLESv2 -lglapi -ldrm_nouveau`).
- **Sample:** `platforms/switch/source/main.c` clear-color `gpu_hello` with
  `eglSwapInterval(1)` and docked 1920×1080 / handheld 1280×720 via
  recreate-on-`appletGetOperationMode`.
- **Hardware:** not run in this session — ROADMAP P4.2 stays open until 60 fps
  is measured on a real Switch. Details: [P4.2_VIDEO.md](P4.2_VIDEO.md).

## P4.3 audio (2026-10-10)

- **Stack:** libnx `audren` / `audrv` (renderer bus 48 kHz; voice 44.1 kHz).
- **API:** `Psyz_AudioInit` thread pulls `Psyz_SpuPullSamples` (PSP-shaped).
- **Sample:** `platforms/switch/source/main.c` `audio_hello` + weak 440 Hz
  SPU stub until PSY-Z is linked.
- **Hardware:** not run in this session — ROADMAP P4.3 stays open until a
  libsnd sample plays without underruns. Details: [P4.3_AUDIO.md](P4.3_AUDIO.md).

## Blockers / environment notes

| Blocker | Impact |
|---|---|
| NTFS / drvfs checkout (`/mnt/windows/...`) | Default `./tools/dk.sh make -C platforms/switch` cannot `mkdir build` as non-root. Use `DW3_DK_TMPBUILD=/tmp/dw3-dk-build ./tools/dk.sh make -C platforms/switch`, a Linux/ext4 clone, or CI. |
| No host `/opt/devkitpro` | Local Switch builds go through Docker (`tools/dk.sh`). |
| No SDL3 pacman package | SDL3-dkp path needs a from-source recipe every time (fallback only). |
| No Switch hardware here | P4.2/P4.3 code can build; 60 fps and audren underrun checks not validated. |
| PSY-Z not linked on Switch yet | P4.3 uses weak SPU stubs; libsnd done-when needs P4.7 link. |
| Phase 3 PC bring-up | Switch host remains a side track; do not block PC movies/audio. |

## Validation targets (hardware vs emulator)

**Gate for ticking P4.x done-when items:** owner OLED Switch (Atmosphere,
**hbmenu**), with **nxlink** when useful for stderr/FPS.

### Ryubing smoke loop (active since 2026-10-10)

The owner dev machine runs the `.nro` in **Ryubing** 1.3.339-canary
(`/mnt/windows/Jogos/emuladores/Ryubing`, source checkout with a built
`build/Ryujinx`). This is an **active pre-hardware loop**, not the deferred
path this doc used to describe — it exercises the host without a console.

```bash
export DOTNET_ROOT="$HOME/.dotnet"   # framework-dependent build; not /usr/share/dotnet
R=/mnt/windows/Jogos/emuladores/Ryubing
$R/build/Ryujinx --use-main-config <path>/dw3-switch.nro
```

Proven on 2026-10-10 with the combined `gpu_hello` + `audio_hello` sample:
EGL/GLES2 present renders and animates; `audren` acquires a session,
initializes (REV4) and starts; `SurfaceFlinger` creates a layer and nvdrv is
reached; the window title shows the app's own title/version read from the
embedded NACP.

Caveats — do not over-read a green smoke:

| Caveat | Detail |
|---|---|
| **`--no-gui` headless does not work** | Loads the NRO, then `GUI.RenderLoop` dies with `Image initialization failed` (reproduces on unrelated homebrew too). Use the GUI path. |
| **Guest stderr is invisible** | `platforms/switch` logs with `fprintf(stderr)` expecting **nxlink**; Ryubing loads the `.nro` directly, so `host_hello:` lines and the fps counter never surface. P4.2's frame-rate number is **not** obtainable this way — and emulator fps would not be meaningful anyway. |
| **The `.nro` needs an embedded NACP** | Without it LibHac-based loaders reject the file outright. Fix and rationale: [platforms/switch/README.md](../platforms/switch/README.md). |
| **Updating Ryubing is blocked on this LAN** | `git.ryujinx.app` and `update.ryujinx.app` resolve to `91.239.78.255`, unreachable here (80/443/22 blocked); the old GitHub mirrors 404. The installed canary is what we have. |

Screenshot shape: launch the GUI, `xdotool windowsize <wid> 640 360` (the owner
config opens fullscreen on an ultrawide), then `import -window <wid> out.png`.

| Resource | Path (owner machine) | Notes |
|---|---|---|
| Ryubing (Ryujinx fork) | `/mnt/windows/Jogos/emuladores/Ryubing` | **Active** smoke; canary 1.3.339; needs `DOTNET_ROOT=$HOME/.dotnet` |
| Ryubing Flatpak (`io.github.ryubing.Ryujinx`) | system install | 1.3.3 stable, **no `prod.keys` configured** — not usable as-is |
| Eden (Yuzu-line) | `/mnt/windows/Jogos/emuladores/Eden-Linux-*.AppImage` | Untried secondary |

Emulator smoke **does not** satisfy P4.2 done-when (**60 fps on hardware** —
see [P4.2_VIDEO.md](P4.2_VIDEO.md)). DuckStation is PS1-only and irrelevant
here.

## Next steps

1. ~~P4.2: implement `nx_gpu.c` present path~~ — code on PR #21; **hardware fps check** open.
2. ~~P4.3: `nx_audio.c` / `audren`~~ — host wiring on PR #23; **hardware + libsnd** open.
3. **P4.7 boot path:** `source/boot_main.c` is in the CMake target and calls
   `dw3_game_main`. The Makefile `source/main.c` sample stays. The game image
   does not link until the renderer symbols below exist.
4. **P4.7 renderer:** the 20 `Draw_*` symbols the link reports (plus
   `Draw_SetHorizontalGrid` in `libgpu.o`). The thin route is an adapter over
   the in-tree `software_rasterizer`, presenting `SoftwareRasterizer_GetVram()`
   through `nx_gpu.c`'s EGL surface.
5. Do not vendor SDL3 into git; if revisited, document a `tools/` fetch+build
   helper only.
