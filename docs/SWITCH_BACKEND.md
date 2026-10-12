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

### Building the game image (CMake target)

The CMake build does **not** go through `tools/dk.sh`. It needs the worktree
mounted at its own absolute path (the `build.ninja` paths are absolute), the
`build/` directory shadowed by an ext4 volume (`docker run -v <ext4>:/…/build`
*after* the worktree mount — `build/` is on NTFS otherwise and the object
files, the packed overlays and `decomp-src` all land there), the main repo's
`.git` for the worktree's submodule pointers, and no login shell:

```bash
W=/mnt/windows/Jogos/dw3-switch/.worktrees/p4-renderer
docker run --rm \
  -v "$W:$W" -v /tmp/dw3-nx-renderer:"$W/build" \
  -v /mnt/windows/Jogos/dw3-switch/.git:/mnt/windows/Jogos/dw3-switch/.git:ro \
  -e DEVKITPRO=/opt/devkitpro \
  -e PATH=/opt/devkitpro/tools/bin:/opt/devkitpro/devkitA64/bin:/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin \
  -e GIT_CONFIG_COUNT=1 -e GIT_CONFIG_KEY_0=safe.directory -e GIT_CONFIG_VALUE_0='*' \
  -w "$W" devkitpro/devkita64:latest ninja -C "$W/build/switch"
```

An incremental build is ~10 s (three objects and a link); a clean one is 1144
targets. Changing `target_link_options` re-runs CMake and rebuilds the world,
so batch link-flag edits.

**A fresh worktree needs two more things before that build works**, and both
fail in ways that look unrelated to the cause:

1. `git submodule update --init vendor/dw3_decomp vendor/psyz` in the new
   worktree (worktrees do not carry submodule checkouts), **and then**
   `git -C vendor/psyz submodule update --init nugget` — a nested submodule.
   Without it the *configure* step dies with
   `Cannot find source file: …/vendor/psyz/nugget/bsdec/bsdec.c` and the
   misleading follow-up `No SOURCES given to target: dw3_movies`.
2. Configure before building. `ninja` alone in a worktree with no
   `build/switch` has nothing to do:

```bash
W=/mnt/windows/Jogos/dw3-switch/.worktrees/<name>
docker run --rm -v "$W:$W" -v /tmp/dw3-nxf-build:"$W/build" \
  -v /mnt/windows/Jogos/dw3-switch/.git:/mnt/windows/Jogos/dw3-switch/.git:ro \
  -e DEVKITPRO=/opt/devkitpro \
  -e PATH=/opt/devkitpro/tools/bin:/opt/devkitpro/devkitA64/bin:/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin \
  -e GIT_CONFIG_COUNT=1 -e GIT_CONFIG_KEY_0=safe.directory -e GIT_CONFIG_VALUE_0='*' \
  -w "$W" devkitpro/devkita64:latest \
  cmake -S "$W/platforms/switch" -B "$W/build/switch" -G Ninja \
    -DCMAKE_TOOLCHAIN_FILE=/opt/devkitpro/cmake/Switch.cmake \
    -DCMAKE_BUILD_TYPE=Release
```

The main repo's `.git` is mounted read-only because a worktree's submodules
keep their gitdirs there and `tools/apply_patches.sh` runs `git -C
vendor/dw3_decomp rev-parse` at configure time; the `safe.directory` env is
what lets that git call run as a different uid than the mount's owner.

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
backend stays in this repo per the P4.1 decision. That Switch CMake change is
`dw3-switch-backend` @ `8dbed91` on the fork. The submodule pin is
`dw3-str-pace` @ `eb0393b` (see [PSYZ.md](PSYZ.md)). The game image links and draws
the title. P4.7 is ticked: the owner reached the main menu on an OLED.
Field and battle on hardware are still the phase gate.

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
| devkitPro Makefile (`make -C platforms/switch`, CI job `switch`) | `source/main.c` `host_hello` | bring-up `.nro` (EGL + audren sample). Drops `boot_main.c`, `nx_input.c`, and `nx_present.c` (they need PSY-Z). No game. Not the release binary. |
| CMake (`platforms/switch/CMakeLists.txt`, CI job `switch-game`, workflow `switch-release.yml`) | `source/boot_main.c` | calls `dw3_game_main`. Links, and produces the game `.nro`. |

`boot_main.c` is the PC host's boot order (cdfs, STR bridge, audio, memcard,
boot TIM, overlay table, vsync pump) plus the SD mount, pad source, and EGL
surface. Referencing `dw3_game_main` pulls `libgpu.o` into the link, which is
what demanded the 20 `Draw_*` symbols below — for a while that link stop *was*
the state of P4.7, and it is kept here because the symbol list is what
`switch_gpu.c` had to satisfy:

`Draw_ClearImage`, `Draw_DisplayArea`, `Draw_DisplayEnable`,
`Draw_DisplayHorizontalRange`, `Draw_DisplayVerticalRange`, `Draw_ExequeSync`,
`Draw_FlushBuffer`, `Draw_LoadImage`, `Draw_MoveImage`, `Draw_PushPrim`,
`Draw_Reset`, `Draw_ResetBuffer`, `Draw_SetAreaEnd`, `Draw_SetAreaStart`,
`Draw_SetDisplayMode`, `Draw_SetMask`, `Draw_SetOffset`, `Draw_SetTexpageMode`,
`Draw_SetTextureWindow`, `Draw_StoreImage`.

`nm -u` on `libgpu.o` also lists `Draw_SetHorizontalGrid` (21 undefined in the
object). The linker does not demand it.

**All of that links now.** `psyz_backend/switch_gpu.c` implements the `Draw_*`
family: OT triangle polys go to `nx_mesh.c` (GLES3 FBO, P5.2 approach A)
**only while the mode overlay is FIGHTSTG**; field and other modes keep the
full software path. Sprites/tiles/lines always hit PSY-Z's in-tree CPU
rasterizer (`software_rasterizer`, `src/platform/sdl3_software.c`).
`nx_gpu.c` presents mesh then CPU VRAM (zero-color discard for UI) when the
mesh path drew that frame; otherwise the opaque blit. See
[P5.2_BATTLE_GPU.md](P5.2_BATTLE_GPU.md). Two traps worth keeping: the
rasterizer's own `rasterizer.h` does `#define RECT PS1_RECT` around its
`draw.h` include, so image-taking forwards must be signed `PS1_RECT*`; and
`nx_gpu.c` must not include PSY-Z headers next to the EGL ones (they redirect
libc names), which is why its probes call `psyz_log` directly.

### Boot stop (2026-10-10): the game runs, then blocks in `CdInit()`

In Ryubing the image gets through the whole host bring-up and well into the
game's own boot (`build/decomp-src/src/engine/system/main.c`):

```
[init_paths] cdfs root=sdmc:/switch/dw3/us
[init_paths] Psyz_AudioInit OK / memcard dir / boot TIM / overlay table
[main] entering dw3_game_main()
[Psyz_VideoVSync] first call (mode=0) → first present+swap done
[dw3_nx_vsync_pump] vsync pump #1  #2  #3  #4
trace: LoadImage(0,0 320x480) -> 0        <- pins #4 to the VSync after LoadImage
trace: SetDispMask(1) -> / <-
trace: CdInit ->                          <- entered, never returns
```

The pump prints once per `VSync()` from inside it, so the count is a boot
progress meter: the game's boot path makes five `VSync` calls before its main
loop (four `VSync(0)`, then a `VSync(3)` past the CD block), and #5 never
comes. The window between #4 and #5 is five calls long —
`SetDispMask(1)`, `CdInit()`, `CdSetDebug()`, `SetGraphDebug()`, and
`while (CdControl(CdlSetmode, param, 0) == 0) { }` — and the two that ran are
in the trace. Reading that window does not explain the stop, which is why the
answer came from a trace rather than from another reading: `D_800B5718[0x0E]`
is `0`, so `cd_cw` skips `Setloc`, and `CD_command`'s `CdlSetmode` arm can only
fail on a NULL param. The PC host links the same PSY-Z and boots to the title.

Also worth knowing: nothing logged after `CdInit` means the PSY-Z IRQ thread
never started and `Psyz_OsNowUs` was never called — both would have logged.
The block is upstream of both.

### Root cause: a self-deadlocked audio lock, not the CD

`CdInit()` was innocent. It is simply the **first `libcd` call in the boot
path**, and `CD_init()` ends in `CD_cw(CdlDemute, …)` → `psyz_demute()` →
`Psyz_AudioLock()` — the game thread's first ever audio lock. It blocked
because the audio thread had already wedged that lock against itself:

```
audio thread   fill_wavebuf(): mutexLock(s_mutex)
               Psyz_SpuPullSamples()
               psyz_spu.c spu_tick(): CD ring below low water
               Psyz_CdPullSamples()          (libcd.c)
               Psyz_AudioLock() -> mutexLock(s_mutex)   same thread, blocked
game thread    CD_init -> CD_cw(CdlDemute) -> psyz_demute()
               Psyz_AudioLock() -> the lock nobody will release
```

**PSY-Z's audio lock is re-entrant by contract**, and all three backends say
so except ours: SDL mutexes are documented recursive
(`SDL_mutex.h`, `SDL_THREAD_PTHREAD_RECURSIVE_MUTEX`), the PSP implements
recursion by hand (`psp_audio.c` `lock_owner`/`lock_depth`), and libnx's plain
`Mutex` is neither — `svcArbitrateLock` blocks on a non-zero word without
comparing the owner, while `RMutex` (`rmutexInit`/`rmutexLock`) is the
recursive one. Six lines in `nx_audio.c` fixed it.

`nx_os.c` deliberately keeps its plain `Mutex` for `Psyz_OsLockCreate`: the
PSP's is a plain non-recursive semaphore too, and PSY-Z's `KernelLock` carries
its own `lock_owner` guard.

Why this cost so much to find, and the lesson: the trace, not the reading.
Reading `CD_init` and `CD_command` says every call returns — `D_800B5718[0x0E]`
is `0`, so `cd_cw` skips `Setloc`, and the `CdlSetmode`/`CdlDemute` arms have
no failing path. The block was one call further down, in code the CD path only
touches because the SPU mixer shares the lock. A `--wrap` trace (all six
wrappers are temporary, remove with the rest) is what moved it from "somewhere
in this window" to "inside `CdInit`", and a subagent reading the PSY-Z CD and
kernel paths cold is what found the cycle.

### What it looks like now

The boot runs into the main loop, alternates `PutDispEnv` between `y=0` and
`y=256` (double buffer), and draws the title: logo, "Please Start Button",
copyright line, animating frame to frame. The speckle was not in the
rasterizer. `vram60.bin` / `vram120.bin` decoded as PS1 RGB555 (R in bits
0–4) are a clean title; the only motion between them is the blinking prompt.
`GL_UNSIGNED_SHORT_5_5_5_1` reads that halfword backwards (R in bits 15–11),
which is the speckled window. `nx_gpu.c` now expands to RGBA8888 the way
`ConvertRgb5551ToRgba8888` does, with alpha forced opaque, before
`glTexSubImage2D`. The owner confirmed that decode
(`/tmp/vram60-ps1-2x.png`, 2026-10-10). The temporary VRAM dump is gone.

### Picture (2026-10-11)

Current present rule, also the short form in `docs/ROADMAP.md` P4.7:

- Title blit width follows `Draw_SetDisplayMode` (320). The horizontal
  range is 256 dot-clock tenths and cropped the "3" when used as the width.
- Movie sample is the 416-line picture at VRAM y=36. The 480-line display
  has blank lines above and below that picture. An intermediate log that
  said "24-bit mode presents 480 lines" and "present flips 320x480" is the
  step before that crop. Do not treat it as the rule.
- Viewport aspect is the PC display-range formula,
  `(4/3) * (horiz/256) / (vert/240)`. On a docked 1920×1080 surface the
  picture is 4:3 at full height, with side bars. A Ryubing window set to
  21:9 letterboxes that framebuffer. That is the emulator aspect.

Opening movie, why it used to be audible and frozen: `DecDCTout` never
raised MDEC-out, so no slice reached `LoadImage`. The Switch present now
matches the PC path (DMA callback, RGB24 expand, quant/IDCT upload, slice
stash). After that, `StGetNext` could sit on a body sector while the frame
header was the previous slot. `__wrap_StGetNext` steps back onto that
header, or drops a leftover tail and scans for the next ready frame.
The owner confirmed the movie plays in Ryubing, then confirmed the y=36
crop fills the height.

Opening movie clock (2026-10-11): the picture ran fast and the sound ran
slow on hardware. The pump was delivering up to 8 sectors on every VSync
and again from `StGetNext`, so a ~15 fps STR (about 10 sectors per frame,
150 sectors/s at CD 2×) finished a frame every 60 Hz refresh. The packed
2048-byte STR also drops the last two XA ADPCM blocks; the decoder plays
that gap as silence, which stretches the soundtrack by 18/16. The pump is
now 150 sectors/s. Full-speed audio needs the STR files as raw 2352.
The `.nro` rewrites them on first launch when the Mode2 BIN/CUE sits in
`sdmc:/switch/dw3/` (or `DW3_DISC_BIN` / `DW3_DISC_CUE`). The screen keeps
the dark blue clear and draws a progress bar while that copy runs: bytes
written this run over the form-2 extents still to rewrite, with a sweep
before the first byte. The log still prints `restore`. This bar was not
run on an OLED.

### OLED (2026-10-10/11)

The owner launched a CMake game `.nro` from hbmenu. The first attempt
exited in `memcard_host_ensure()` because `sdmc:/switch/dw3/us/memcard/`
was missing. The log had already reached the SD runtime root and
`Psyz_AudioInit OK`. Creating the empty directory allowed launch. The
owner then reached the main menu and loaded a new game. That file was
`main` plus the P4.4/P4.5 commits, before the picture fix, so it still
cropped the title and froze the movie. `v0.6.5-preview.2` (`8273a91`) is
the picture fix. The owner started that file on hardware while this note
was written. The result is not recorded here.

P4.7 is ticked on that menu boot. P4.4 is ticked later the same day:
handheld Joy-Con and a Pro Controller both walk with the left stick.
P4.5 is ticked via the formal host CDFS suite against the packed P1.6
tree (`tools/cdfs_suite`, `docs/P4.5_STATUS.md`). P4.6 no-`dlopen` is
decided; the OLED overlay-restore checklist in
`docs/P4.6_MODULES_STATUS.md` still needs a hardware evidence row before
the ROADMAP box is ticked. The phase gate (title + field + battle) was
met on preview.4.

## P4.2 video (2026-10-11)

- **Stack:** EGL + OpenGL ES 2 (`-lEGL -lGLESv2 -lglapi -ldrm_nouveau`).
- **Sample:** `platforms/switch/source/main.c` clear-color `gpu_hello` with
  `eglSwapInterval(1)` and docked 1920×1080 / handheld 1280×720 via
  recreate-on-`appletGetOperationMode`.
- **Hardware:** the OLED showed the title framebuffer. That ticks P4.2.
  Frame time on handheld clocks is P5.2. Dock/undock while playing is
  P10.3. Details: [P4.2_VIDEO.md](P4.2_VIDEO.md).

## P4.3 audio (2026-10-11)

- **Stack:** libnx `audren` / `audrv` (renderer bus 48 kHz; voice 44.1 kHz).
- **API:** `Psyz_AudioInit` thread pulls `Psyz_SpuPullSamples` (PSP-shaped).
- **Game image:** the CMake `.nro` links PSY-Z's SPU. Opening-movie XA
  played in Ryubing.
- **Sample:** Makefile `host_hello` (`source/main.c`) still uses the weak
  440 Hz stub. That stub is not the game.
- **Hardware:** P4.3 stays open until a libsnd path is heard on device
  without underruns. Details: [P4.3_AUDIO.md](P4.3_AUDIO.md).

## Blockers / environment notes

| Blocker | Impact |
|---|---|
| NTFS / drvfs checkout (`/mnt/windows/...`) | Default `./tools/dk.sh make -C platforms/switch` cannot `mkdir build` as non-root. Use `DW3_DK_TMPBUILD=/tmp/dw3-dk-build ./tools/dk.sh make -C platforms/switch`, a Linux/ext4 clone, or CI. |
| No host `/opt/devkitpro` | Local Switch builds go through Docker (`tools/dk.sh`). |
| No SDL3 pacman package | SDL3-dkp path needs a from-source recipe every time (fallback only). |
| Frame time and underruns | P4.2 present is ticked. P5.2 still wants 16.7 ms on handheld. P4.3 still wants a hardware listen without underruns. |
| `host_hello` weak SPU stub | The Makefile sample still stubs `Psyz_SpuPullSamples`. The CMake game image links the real SPU. |

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
| **Read the guest log only through `svcOutputDebugString`** | `psyz_log` writes three sinks: stderr (invisible here — it expects **nxlink**), `sdmc:/switch/dw3/dw3.log`, and `svcOutputDebugString`, which Ryubing logs at warning level into `~/.config/Ryujinx/Logs/*.log`, live and timestamped. **Use the emulator log.** The `sdmc:` path is a *mirror* of the emulated card and Ryubing does not write guest file data through as the guest flushes: reading `dw3.log` while the emulator is up shows a log that has quietly stopped short, which reads exactly like a hang. It cost one wrong conclusion (a "the vsync callback never runs" contradiction that was pure flush lag) before the third sink existed. |
| **A stop in the log is not a crash** | The emulator ran 6 minutes with a black window and exited cleanly on request; guest threads 28/30 were parked in `ArbitrateLock` at teardown. Confirm with `~/.config/Ryujinx/Logs/` and `import -window <wid>` before calling anything hung. |
| **The `.nro` needs an embedded NACP** | Without it LibHac-based loaders reject the file outright. Fix and rationale: [platforms/switch/README.md](../platforms/switch/README.md). |
| **Updating Ryubing is blocked on this LAN** | `git.ryujinx.app` and `update.ryujinx.app` resolve to `91.239.78.255`, unreachable here (80/443/22 blocked); the old GitHub mirrors 404. The installed canary is what we have. |

Screenshot shape: launch the GUI, `xdotool windowsize <wid> 640 360` (the owner
config opens fullscreen on an ultrawide), then `import -window <wid> out.png`.

| Resource | Path (owner machine) | Notes |
|---|---|---|
| Ryubing (Ryujinx fork) | `/mnt/windows/Jogos/emuladores/Ryubing` | **Active** smoke; canary 1.3.339; needs `DOTNET_ROOT=$HOME/.dotnet` |
| Ryubing Flatpak (`io.github.ryubing.Ryujinx`) | system install | 1.3.3 stable, **no `prod.keys` configured** — not usable as-is |
| Eden (Yuzu-line) | `/mnt/windows/Jogos/emuladores/Eden-Linux-*.AppImage` | Untried secondary |

Emulator smoke does not satisfy P4.3 (underruns on hardware) or the Phase 4
gate (field and battle on the OLED). P4.2's present-on-hardware evidence is
the OLED title, not an emulator fps number. DuckStation is PS1-only and
irrelevant here.

## Next steps

1. P4.2 present and P4.7 title-on-hardware are ticked. `v0.6.5-preview.2`
   is the picture fix. Record the OLED result of that file when the owner
   reports it.
2. P4.3 stays open: hear libsnd on hardware without underruns.
3. P4.4 and P4.5 are ticked (stick on OLED; formal CDFS suite on packed
   runtime / SD layout — `docs/P4.5_STATUS.md`). P4.6 still needs an OLED
   restore-checklist evidence row (`docs/P4.6_MODULES_STATUS.md`). The
   phase gate (field and battle) was met on preview.4.
4. P5.2 owns the 16.7 ms frame-time check. Do not put it back on P4.2.
5. Do not vendor SDL3 into git; if revisited, document a `tools/` fetch+build
   helper only.
