# Port glue

Platform-independent pieces between the host (PC / Switch) and game C from
`vendor/dw3_decomp` (via the patched copy in `build/decomp-src/`).

- CD / file table → host filesystem (`port/cdfs/` — file-id reads, and US BIN extract at boot)
- Overlay and stage loading (`port/overlays/` — static restore, P2.6)
- Local text pack (`port/l10n/` — P6.7, the `Text language` row: a pack built on the owner's PC replaces the English text files; docs/LOCALIZATION.md)
- Memory card virtualization (`port/memcard/` — flat `saveN.bin` per port;
  legacy `.mcr` is migration / fixtures / P9.6 only)
- Build flags that drop matching-only PS1 constraints

See `port/cdfs/README.md` for the file-id API and fixture tests.
See `port/overlays/README.md` for the static overlay restore model.
See `port/memcard/README.md` for the flat-save host and P5.3 notes.

## Layout

| Path | Role |
|---|---|
| `CMakeLists.txt` | `dw3_engine` OBJECT library (`src/engine/**/*.c`, US flags) plus host overlay/cdfs/memcard objects |
| `include/` | Host headers first (`common.h`, `libpad.h`, `libgte.h`/`libgs.h` fill-ins) |
| `patches/` | Build-copy patches; see [`docs/PATCHES.md`](../docs/PATCHES.md) |
| `overlays/` | Host VRAM symbols + static restore (`overlay_host`, P2.6) |
| `cdfs/` | File-id → packed runtime reads (P2.5) |
| `memcard/` | Flat `saveN.bin` host (P5.3); P2.7 was the earlier `.mcr` path |
| `psyz_missing/` | PSY-Z gap stubs linked ahead of vendored libs (e.g. MDEC, movie path) |
| `options/` | P9.2 port options shell (`dw3_port_options_*`; title Select entry); P6.20 Cheats screen and save marks (`cheats.c`, `cheat_saves.c`, `port_cheats.h`; only with `DW3_CHEATS`) |
| `audio/` | P6.17 music / SFX volume: step -> gain curve, SPU mixer bus glue, optional bus statistics (`port_audio.h`) |
| `pacing/` | P6.18 encounter rate and text speed: pure policy (`pacing_policy.c`) and the hooks of patches 0062 / 0063 (`pacing.c`, `port_pacing.h`) |
| `l10n/` | P6.7 text pack loader: `l10n_core.c` (pack.json, sha256 source check, cdfs override, `FILE_SECTORS` / `COUNTS` patch and restore, logs) and `l10n.c` (bound to the game's arrays and file cache, `port_l10n.h`) |
| `items_menu/` | P6.33 Items menu usability: the partner cycle policy of the equip panel (`items_menu.c`, `port_items_menu.h`, patch 0128) |
| `ui/` | Shared 8×8 VRAM text blit for port UI (not Switch debug overlay) |

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
