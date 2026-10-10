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

Sketch tree (stubs only; not linked into the Phase 1 `.nro` yet):

| PSY-Z PSP file | Switch sketch | Intended libnx / stack |
|---|---|---|
| `psp_gpu.c` | `nx_gpu.c` | EGL + OpenGL ES (mesa/nouveau) or deko3d; present PSY-Z VRAM (P4.2) |
| `psp_input.c` | `nx_input.c` | `pad` / `hid` → PS1 pad bits (P4.4) |
| `psp_audio.c` (+ SAS on PSP) | `nx_audio.c` | `audren` thread; software SPU like PC (`psyz_spu.c`) (P4.3) |
| `psp_os.c` | `nx_os.c` | libnx threads / mutex / time |
| `psp_file.c` | `nx_file.c` | `romfs:` / `sdmc:` paths for P1.6 layout (P4.5) |
| `psp_log.c` | `nx_log.c` | stdout / nxlink |
| `psp_module_*.c` | `nx_module.c` | static dispatch table — no `dlopen` (P4.6) |

See [platforms/switch/psyz_backend/README.md](../platforms/switch/psyz_backend/README.md).

Wiring into PSY-Z (later, P4.2+): either a `CMAKE_SYSTEM_NAME` / `__SWITCH__`
branch in the submodule (upstream-bound) or an object library in
`platforms/switch/` that supplies the same platform symbols the PSP files
provide. Prefer keeping Switch files in this repo until the surface is stable.

## Blockers / environment notes

| Blocker | Impact |
|---|---|
| NTFS / drvfs checkout (`/mnt/windows/...`) | `./tools/dk.sh make -C platforms/switch` cannot `mkdir build` as non-root. Use Linux/ext4 clone, CI, or an isolated `/tmp` build volume. |
| No host `/opt/devkitpro` | Local Switch builds go through Docker (`tools/dk.sh`). |
| No SDL3 pacman package | SDL3-dkp path needs a from-source recipe every time. |
| No hardware validation in this spike | Decision is from compile + API fit, not docked/handheld playtest. |
| Phase 3 gate still open | Switch host work is a side track; do not block PC movies/audio. |

## Next steps (after P4.1)

1. P4.2: implement `nx_gpu.c` present path (EGL/GLES first; deko3d if needed).
2. Keep Phase 1 `source/main.c` skeleton until the backend can own `main` /
   applet loop.
3. Do not vendor SDL3 into git; if revisited, document a `tools/` fetch+build
   helper only.
