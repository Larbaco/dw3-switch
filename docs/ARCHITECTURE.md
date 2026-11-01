# Architecture

## Sources of truth

| Layer | Location | Role |
|---|---|---|
| Game logic (matching C) | `vendor/dw3_decomp/src` | PS1 game code; keep as upstream, patch via port glue |
| PsyQ headers (reference) | `vendor/dw3_decomp/external/psyq_headers` | API surface the game expects |
| Psy-Q shim | `vendor/psyz` (pinned submodule) | PSY-Z: libgpu/libgte/libgs/libcd/libsnd/libpress/libmcrd in portable C; see `docs/PSYZ.md` |
| Host (PC) | `platforms/pc` | PSY-Z SDL3 host: links engine + `STDWTITL` + cdfs/memcard; debugging target |
| Host (Switch) | `platforms/switch` | Native libnx PSY-Z backend (`psyz_backend/`); P4.2 EGL present (`nx_gpu.c`); P5.2 prefers GLES3 so **FIGHTSTG** OT triangle mesh can hit a mesh FBO (`nx_mesh.c`) while field/menus/sprites/UI stay on the software RGB555 blit (`switch_gpu.c`); letterbox uses the PC display-range aspect (4:3 at full framebuffer height, side bars on 16:9) |
| Port glue | `port` | Overlays, cdfs, memcard, PSY-Z gap stubs, decomp patches (`build/decomp-src`) |
| Text pack (P6.7) | `port/l10n/` + `tools/l10n/build_pack.py` | a local pack of the owner's own translation replaces the English text files through a `port/cdfs` override and the file-table arrays (no engine patch); [LOCALIZATION](LOCALIZATION.md) |
| Disc → runtime | `tools/` + `port/cdfs/runtime_extract.c` + `docs/DISC_SETUP.md` / `docs/RUNTIME_LAYOUT.md` | `build_file_map.py` / `pack_runtime.py` (PC) and version-matched Switch BIN/CUE extraction → `runtime/<ver>/` or SD layout |

## Runtime model (target)

```
+------------------------------+   +------------------------------+
|  PC host (SDL3, PSY-Z's own) |   |  Switch host (libnx backend) |
|  present / audio / pad / fs  |   |  present / audio / pad / fs  |
+--------------+---------------+   +--------------+---------------+
               |                                  |
+--------------v----------------------------------v---------------+
|  PSY-Z (Psy-Q compatible shim)                                  |
|  libgpu / libgte / libgs / libcd+St / libsnd / libpress / libmcrd |
+--------------------------------+--------------------------------+
                                 |
+--------------------------------v--------------------------------+
|  port/ glue: overlay table, cdfs (file id -> path), memcard file |
+--------------------------------+--------------------------------+
                                 |
+--------------------------------v--------------------------------+
|  Digimon World 3 game C (vendor/dw3_decomp, patched at build)   |
|  engine modules + 21 overlays + stages, one linked image        |
+-----------------------------------------------------------------+
```

## Hard constraints from the decomp

See [SURVEY.md](SURVEY.md) for the evidence behind each point.

- PsyQ libraries in the decomp are MIPS disassembly; they are **not**
  linked into the port. The shim replaces them. The surface the game uses
  is small (~120 functions) but includes libsnd (SEQ/SEP/VAB) and MDEC.
- Overlays and stages load at fixed PS1 addresses by `memcpy`, only when
  the id changes. The port links everything into **one image** with
  per-binary symbol prefixes and emulates the load by restoring the
  overlay's initial `.data`. libnx has no `dlopen`, so this is also the
  Switch-compatible choice.
- The game is 32-bit code: literal struct sizes, `u32` holding pointers and
  heap-address comparisons must be fixed for AArch64 (`port/patches/`,
  upstream when match-neutral).
- Disc I/O is by **numeric file id** over a sector table, not by path.
  The port serves files from an unpacked tree through an id → path map
  generated from the owner's disc.
- The saved part of `GameState` has no pointers. The running port stores
  that payload in a flat per-port file (`port/memcard`); 128 KiB `.mcr`
  interchange is an external tool (P9.6), not in-game.
- MIPS-only constructs (P2.3 done for engine host): scratchpad stack
  no-ops, `allocMemHigh` returns the pointer, GTE macros → PSY-Z software
  GTE, overlay address symbols + static restore in `port/overlays/`
  (P2.6: restore `.data` / zero `.bss` on id change).
- P2.8: PC host calls `dw3_game_main` after cdfs/memcard/boot TIM init;
  `CD_READER` via cdfs; US `STDWTITL` linked; title mode draws. Later
  picture work is in `docs/P3_GPU_GLITCHES.md`. See
  `docs/P2.8_TITLE_BOOT.md`.
- P3.2+ (landed with Phase 3): field/stage bring-up uses **objcopy /
  `ld -r`** to prefix stage globals and rename `.data`/`.bss` into
  per-overlay sections (`dw3data_*` / `dw3bss_*`); `overlay_sections.c`
  snapshots them for restore. Weak stubs remain only for intentionally
  unlinked overlays (the debug ones: `WFIGHTTS`, `SOUNDTST`, `SHOCKTST`; every field stage is linked and
  registered, `tools/tests/test_overlay_table_stages.py`).

The P9.2 title entry is game-layer `PORT_HOST` patch glue in
`port/patches/0044*` / `0045*`, with the shared shell and resident-font
title label in `port/options/`. It uses the original title cursor and
retains New Game/Continue choices. Hosts supply input/present only;
neither the Switch diagnostic HUD nor a host-specific menu owns this UI.

P6.9 restart policy lives in `port/options/restart.c`; it checks the shared
PS1 logical pad state, flushes P5.3 saves, and only then returns a restart
request. The game loop patch `0046*` resets the game save-state RAM, clears
the current mode task and overlay cache, and lets the existing `dw3_game_main`
loop create the title mode again. It does not rerun the host or PSY-Z
initialization path. The Switch host stays limited to input and presentation.

P6.18 pacing (encounter rate, text speed) is the same shape: two one-call
`PORT_HOST` hooks in the decomp (patches `0062*` in
`FIELDSTG_countBattleSteps`, `0063*` in `textWindowSetTypeDelay` /
`updateTextWindow`) that ask `port/pacing/` what to do, and the answer is the
game's own value while the Port options rows are at their defaults. The policy
(`pacing_policy.c`) is pure and unit tested; the Switch tree has no part in it.

The Options screen's menu sounds (cursor, choice, cancel) are played from the
shared `port/options/port_options.c` through the game's own `SOUND.playSound`
(no patch, no host code); they are key-ons, so they land on P6.17's SFX bus.

P7.7 fixes of the original game are plain decomp patches, not port hooks: `0065` in
`FIGHTSTG_showFighterNames` (the HUD names the Digimon a partner fights as) and `0066` in
`FIGHTSTG_updateTechAction` (a Claw's steal on the killing blow). Patch `0064` adds PC-only
environment hooks for the replay scenarios. See [P7.7_BUG_FIXES.md](P7.7_BUG_FIXES.md).

P7.9 battle menu memory has the P6.18 shape: patch `0067` has a few `PORT_HOST` calls in
`player_turn.c` / `tech_menu.c` that ask `port/battle_menu/` where the cursor starts; the
answer is the original start unless the Battle cursor row of the Options screen (key `battle_menu_memory`) is set. See
[P7.9_BATTLE_MENU_MEMORY.md](P7.9_BATTLE_MENU_MEMORY.md).

## Non-goals (for now)

- Byte-identical matching builds on Switch
- Shipping any original assets, BIOS, or third-party patches in git
- Official eShop packaging
- Running PS1 EXE/overlay binary patches inside the `.nro` (community QoL
  parity is a later native/`port/` effort; see [COMMUNITY_QOL.md](COMMUNITY_QOL.md))
- Shipping texture packs or original art (owner overlays only; see
  [TEXTURE_PACKS.md](TEXTURE_PACKS.md))
- Shipping a forced “modded” default: port-added features are opt-in via
  the Phase 9 options menu ([PORT_OPTIONS.md](PORT_OPTIONS.md))
- Duplicating Switch host work in Phase 10 that already belongs to P4–P6
  (present, pad/rumble, saves, scale/widescreen, turbo); see
  [SWITCH_FEATURES.md](SWITCH_FEATURES.md)

### Switch movie empty-scan invalidation

`psyz_backend/stgetnext_invalidate.c` owns the `StGetNext` empty-ring verdict.
The Switch linker wraps `CdReady`, `StSetRing`, `StClearRing`, and
`data_ready_callback`. At pinned PSY-Z `eb0393b`, stream production in
`psyz/src/psyz/libcd.c` calls `CdReady` in the separate `decomp/.../sys.c`
translation unit before every ring write, including rewind and sentinel
publication; therefore the hook invalidates even if cursors return to the same
values. Set/clear and callback wrappers also cover direct API calls and stored
callback pointers. None of these wrappers replaces the vendor implementation.
An idle pump with no sector budget does not call `CdReady`, preserving the
clean verdict and removing redundant scans. This cache assumes the current
single host/game thread for stream APIs; audio mixing does not produce STR
ring data. It is not synchronization for a future threaded stream producer.
