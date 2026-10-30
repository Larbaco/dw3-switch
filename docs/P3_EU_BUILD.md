# Phase 3 follow-up — EU build and Switch own-disc extraction

Status: **link + owner bring-up validated** (configure/link with
`-DDW3_VERSION=eu`; owner EU disc packed to `runtime/eu`; PC smoke reached
opening movie / PAL 50 Hz; OLED EU extraction and early gameplay on
2026-10-14). Full EU playthrough and save/relaunch validation remain open.
Gate stance unchanged: US continuous path only
(`docs/P3_GATE.md`).

Owner validation note (2026-10-12, own disc only, not in git): Redump-style
`Digimon World 2003 (Europe)` MODE2/2352 → `SLES_039.36` /
`BESLES-03936`; `files_eu.json` `--check` clean; `boot_tim.bin` from EXE;
`tools/build_switch_game.sh --eu` emits NRO defaulting to
`sdmc:/switch/dw3/eu`.

**Switch auto-extraction (2026-10-13):** `VERSION_EU` selects the EU
`FILE_SECTORS`/sector-count arrays, `SLES_039.36`, `files_eu.json`, and EU
`STAGE_VRAM` (`0x800A5DE0`) for `boot_tim.bin`. Place the owner's matching
Mode2/2352 BIN/CUE in `sdmc:/switch/dw3/eu/` (or its parent). The NRO rejects
an image containing only the US `SLUS_014.36` executable. It writes the
versioned JSON map after extraction and validates the map, executable, boot
TIM, and every mapped file before CDFS starts. FAT/exFAT rename cannot replace
an existing file, so runtime output swaps use a backup-and-restore sequence;
the host test harness simulates `EEXIST`. A failed extraction leaves no
accepted map, and completed files can be reused on retry. No disc image or
extracted runtime is included in the repository. Synthetic host tests cover
EU output, wrong-region rejection, retry after a partial copy, replacement of
partial outputs, and second-boot no-reextract behavior. Independent Ryubing validation extracted 2383 files from only the matching
BIN/CUE, then reused the runtime on a second boot. The movie reached frame
2100 at about 15 fps with zero skipped frames; pressing Plus for one second
returned to the title. This proves playback and skip, not natural completion
or audible A/V synchronization. That emulator build preceded the FAT swap
hardening; the final FAT path has synthetic host coverage. OLED extraction
and early gameplay were subsequently observed as recorded below; full EU
movie acceptance and the EU Switch playthrough remain open. Evidence stays
outside Git under `/mnt/cabal/tmp/dw3-eu-extraction-test/` for that emulator run.

## OLED EU own-disc extraction (2026-10-14)

Before launch, the SD EU input contained only `eu/disc.bin` and `eu/disc.cue`,
with their copy hashes verified; no pre-extracted EU runtime was supplied.
The EU nxlink NRO was **9,062,945 bytes**, SHA-256
`69fb04aa8f4d5f0631a8f73508894d75fa815da83152a27d5ae778b63a478cd9`.
The live nxlink stdout (session 91889) reports `runtime extract wrote=2383 skipped=0`,
then runtime `version=eu`, root `sdmc:/switch/dw3/eu`, executable
`SLES_039.36`, and EU CDFS initialization. This establishes extraction by
this NRO and EU runtime selection on the OLED, rather than a prepacked-tree
boot or an inference from the movie's appearance. A manually transcribed,
labelled excerpt is preserved outside Git at
`/mnt/cabal/tmp/dw3-eu-auto-extraction/build/eu-nxlink-session-excerpts.txt`;
it is not a complete raw log.

Movie diagnostics reached frame 1020 with `fc_skipped=0 restarts=0`.
The owner then pressed Start; `ended=0` records the skip, not natural movie
completion. The owner reported playing toward the first save. This confirms
early EU gameplay only: first-save success, relaunch/Continue reload, the
complete movie's image/audio quality, and the EU first-boss/playthrough gate
remain unverified. The existing US runtime and saves were preserved; this
evidence update does not require reading or modifying save files.

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

## Switch EU NRO

```bash
# separate build tree from US (Docker / tools/dk.sh hosts)
./tools/build_switch_game.sh --eu
# → build/switch-eu/dw3-switch.nro (DW3_DEFAULT_RUNTIME=sdmc:/switch/dw3/eu)
```

Either copy a packed `runtime/eu` (including an empty `memcard/`) to
`sdmc:/switch/dw3/eu/`, or place the owner's matching EU Mode2 BIN/CUE in
that directory and let the EU NRO extract it. The default EU NRO checks for
`SLES_039.36`, writes `files_eu.json`, and rejects a US image. This path has
synthetic host coverage and the OLED extraction/early-gameplay evidence above;
full EU playthrough and save/relaunch validation remain open.

## Remaining open items

- Owner `disks/eu` + `runtime/eu` stay gitignored (bring-up pack proven on
  owner disc; not a committed tree).
- ~~`CNTY_SEL` weak-stubbed~~ — linked (P6.2, `dw3_p62_cnty_sel`). The EU
  host boot now starts at the country select (patch `0055`), as on the PS1:
  Up/Down picks a language, Start confirms, `LANGUAGE =
  CNTY_SEL_languages[choice]` (`{2,3,5,4,6,1,0}`; options 5/6 are
  unreachable on the disc too), then `MODE_OPENING_2` (or `MODE_OPENING`
  for `LANGUAGE` 0) and the title. `DW3_START_MODE=0xE00` skips it. Owner
  Ryubing/OLED check of the screen and of each language is still open.
- EU-only stages WSTAG920–974 (the extra chapter): linked and registered 2026-10-30 (patch `0197`,
  `port/overlays/overlay_sections_eu_extra.c`; [P5.5](P5.5_STAGE_SWEEP.md#the-wstag460-gap-2026-10-30)).
- EU playthrough / “both versions play” not claimed (Phase 6 / P7.2 class).
- OLED EU extraction and early gameplay are evidenced above; natural movie
  completion, EU audio/image acceptance, and first-save/relaunch remain open.
- Keep US as the default CI / day-to-day bring-up target.

## Regenerating `overlay_table_eu.c`

If US table rows change, regenerate EU ids from decomp `FIELDSTG_stages`
(same approach as the generator used on this branch: parse US table + remapping
mode ids from `overlay.h` and stage ids from EU `fieldstg.c`). Do not invent
file ids; prefer decomp tables over `files_eu.json` guesses.
