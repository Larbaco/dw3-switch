# Port glue

Platform-independent pieces between the host (PC / Switch) and game C from
`vendor/dw3_decomp` (via the patched copy in `build/decomp-src/`).

- CD / file table → host filesystem (`port/cdfs/` — file-id reads, and US BIN extract at boot)
- Overlay and stage loading (`port/overlays/` — static restore, P2.6)
- Memory card virtualization (`port/memcard/` — 128 KiB `.mcr` per slot)
- Build flags that drop matching-only PS1 constraints

See `port/cdfs/README.md` for the file-id API and fixture tests.
See `port/overlays/README.md` for the static overlay restore model.
See `port/memcard/README.md` for the `.mcr` host backend and DuckStation note.

## Layout

| Path | Role |
|---|---|
| `CMakeLists.txt` | `dw3_engine` OBJECT library (`src/engine/**/*.c`, US flags) plus host overlay/cdfs/memcard objects |
| `include/` | Host headers first (`common.h`, `libpad.h`, `libgte.h`/`libgs.h` fill-ins) |
| `patches/` | Build-copy patches; see [`docs/PATCHES.md`](../docs/PATCHES.md) |
| `overlays/` | Host VRAM symbols + static restore (`overlay_host`, P2.6) |
| `cdfs/` | File-id → packed runtime reads (P2.5) |
| `memcard/` | 128 KiB `.mcr` slot images (P2.7) |
| `psyz_missing/` | PSY-Z gap stubs linked ahead of vendored libs (e.g. MDEC, movie path) |

## Build (engine + PC host)

From the repo root:

```bash
tools/apply_patches.sh
cmake -S platforms/pc -B build/pc -G Ninja -DPSYZ_RENDERER=sdl3_gpu
cmake --build build/pc -j"$(nproc)"
# → build/pc/dw3-pc (calls dw3_game_main after cdfs/memcard/boot TIM init)
```

To compile engine objects only:

```bash
cmake --build build/pc --target dw3_engine_objs -j"$(nproc)"
```

Runtime layout from your disc: [`docs/DISC_SETUP.md`](../docs/DISC_SETUP.md),
[`docs/RUNTIME_LAYOUT.md`](../docs/RUNTIME_LAYOUT.md). Run:
`./build/pc/dw3-pc --runtime runtime/us`.
