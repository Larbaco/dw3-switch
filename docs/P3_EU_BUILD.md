# Phase 3 follow-up — EU `dw3-pc` build scaffold

Status: **link scaffold landed** (configure + link with `-DDW3_VERSION=eu`).
EU playthrough and owner `runtime/eu` remain open. Gate stance unchanged:
US continuous path only (`docs/P3_GATE.md`).

## Root causes (pre-scaffold blockers)

Evidence from `origin/main` at gate close (`0eec250` era; still true before
this branch):

| Blocker | Evidence |
|---|---|
| CMake forced US | `port/CMakeLists.txt` hardcoded `VERSION_US=1` in `_DW3_GAME_DEFS` (was ~L138) |
| Overlay table US-only | `port/CMakeLists.txt` always listed `overlays/overlay_table_us.c` (was ~L80); US `FILE_FIELDSTG` `0x158` vs EU `0x166` in `vendor/dw3_decomp/include/engine/overlay.h` L23 / L43 |
| No version option | `platforms/pc/CMakeLists.txt` had no `DW3_VERSION` / no EU compile defs on `dw3-pc` |
| Host default runtime US | `platforms/pc/src/main.c` defaulted `DW3_RUNTIME` → `runtime/us` and always called `dw3_overlay_register_us_title()` |
| Engine EU already OK | Gate probe: all 43 `src/engine/**/*.c` under `-DVERSION_EU` (see `docs/P3_GATE.md` “EU build attempt”) |
| `LANGUAGE` / PAL knobs | `extern s32 LANGUAGE` only for EU (`include/engine/file.h`); definition in `src/engine/data/eu_settings.c` L12–16 (`LANGUAGE`, `NTSC_MODE`, `SHIFT_PAL_SCREEN`) under `#if VERSION_EU` |
| Runtime pack story | `tools/pack_runtime.py` already accepts `--version eu` → `SLES_039.36` / `files_eu.json`; owner must supply `disks/eu` (gitignored). No committed `runtime/eu` |

`version.h` requires exactly one of `-DVERSION_US` / `-DVERSION_EU` (not both).

## What this scaffold adds

1. **CMake** `-DDW3_VERSION=us|eu` (default `us`) in `platforms/pc` + `port`:
   sets `VERSION_US=1` or `VERSION_EU=1`, picks `overlay_table_{us,eu}.c`.
2. **`port/overlays/overlay_table_eu.c`** — same registration shape as US;
   mode ids from `overlay.h` / `battle_mode.h` EU branch; stage file ids from
   EU `FIELDSTG_stages` in `fieldstg.c`.
3. **PC host** — `DW3_DEFAULT_RUNTIME` (`runtime/eu` when EU); register
   `dw3_overlay_register_eu_title()` under `VERSION_EU`.
4. **P3.1 audit** — under EU, do not strip WSTAG920–974 (they need `LANGUAGE`).

## Build (configure + link)

Use a **separate** build tree so US and EU objects do not collide:

```bash
# from repo root (separate build tree from US)
cmake -S platforms/pc -B build/pc-eu -G Ninja \
  -DDW3_VERSION=eu -DPSYZ_RENDERER=sdl3_gpu \
  -DSDL_X11_XSCRNSAVER=OFF
ninja -C build/pc-eu dw3-pc
# binary: build/pc-eu/dw3-pc
```

**Proven on this branch:** configure prints `DW3_VERSION=eu (VERSION_EU=1)`;
`ninja -C build/pc-eu dw3-pc` links; `nm` shows `dw3_overlay_register_eu_title`
and `LANGUAGE`; without `runtime/eu`, run exits with
`cdfs_load_map under runtime/eu failed` (rc=1).

US (unchanged default):

```bash
cmake -S platforms/pc -B build/pc -G Ninja   # or -DDW3_VERSION=us
ninja -C build/pc dw3-pc
```

## Run-time (still owner-blocked)

Without a packed tree, the binary fails honestly at cdfs/boot:

```bash
./build/pc-eu/dw3-pc --runtime runtime/eu
# → cdfs_set_root / cdfs_load_map / boot_tim.bin failure if runtime/eu missing
```

Owner pack (own disc only, never commit):

```bash
python3 tools/pack_runtime.py --version eu --disks disks/eu --out runtime/eu --force
python3 tools/build_file_map.py --check runtime/eu/files_eu.json \
  --file-table vendor/dw3_decomp/src/engine/file/file_table.c
DW3_RUNTIME=runtime/eu ./build/pc-eu/dw3-pc --runtime runtime/eu
```

## Remaining open items

- Owner `disks/eu` + `runtime/eu` (not in git).
- `CNTY_SEL` still weak-stubbed (`port/overlays/mode_stubs.c`) — EU boots
  via country select; real overlay link is a later task.
- EU-only stages WSTAG920–974 still weak-stubbed for link (not in P3.8 batches).
- EU playthrough / “both versions play” not claimed (Phase 6 / P7.2 class).
- Keep US as the default CI / day-to-day bring-up target.

## Regenerating `overlay_table_eu.c`

If US table rows change, regenerate EU ids from decomp `FIELDSTG_stages`
(same approach as the generator used on this branch: parse US table + remapping
mode ids from `overlay.h` and stage ids from EU `fieldstg.c`). Do not invent
file ids; prefer decomp tables over `files_eu.json` guesses.
