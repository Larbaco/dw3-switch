# Port glue

Platform-independent pieces between the host (PC / Switch) and game C from
`vendor/dw3_decomp` (via the patched copy in `build/decomp-src/`).

## Layout

| Path | Role |
|---|---|
| `CMakeLists.txt` | `dw3_engine` OBJECT library (`src/engine/**/*.c`, US flags) |
| `include/` | Host headers first (`common.h`, `libpad.h`, `libgte.h`/`libgs.h` fill-ins) |
| `patches/` | Build-copy patches; see [`docs/PATCHES.md`](../docs/PATCHES.md) |
| `overlays/` | Host `OVERLAY_ADDRESS` / VRAM buffers (P2.3); restore model in P2.6 |

## Engine objects (P2.1)

From the repo root (or `platforms/pc`):

```bash
tools/apply_patches.sh
cmake -S platforms/pc -B build/pc -G Ninja -DPSYZ_RENDERER=sdl3_gpu
cmake --build build/pc --target dw3_engine_objs -j"$(nproc)"
```

The PC stub (`dw3-pc`) still does not call game `main`; it only depends on
`dw3_engine_objs` so the engine compiles in the same configure.
