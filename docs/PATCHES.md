# Decomp patches

Patches against `vendor/dw3_decomp` live in `port/patches/` and are applied
only to a **build copy** under `build/decomp-src/` by
`tools/apply_patches.sh`. Never edit or commit a dirty tree inside
`vendor/dw3_decomp`.

## Policy

1. **Build-copy only.** `apply_patches.sh` copies the submodule (excluding
   `.git`) into `build/decomp-src/` and applies `port/patches/*.patch` in
   sorted order. The submodule itself must stay clean after every task
   (`git -C vendor/dw3_decomp status --porcelain` empty).
2. **Declare Upstream-bound.** Every patch file must start with a header
   that includes `Upstream-bound: yes` or `Upstream-bound: no`:
   - `yes` — match-neutral fix intended for an upstream PR later
   - `no` — port-only change (host/`PORT_HOST`, AArch64, etc.)
3. **Pin the submodule.** `port/patches/DECOMP_PIN` holds the expected
   `vendor/dw3_decomp` commit SHA. `tools/apply_patches.sh --check` fails
   if `git -C vendor/dw3_decomp rev-parse HEAD` differs, if the submodule
   working tree is dirty, or if any patch fails `git apply --check`.
4. **Empty series is valid.** With no `*.patch` files, copy + apply must
   succeed and `build/decomp-src/` matches the pinned submodule contents.

## Commands

```bash
tools/apply_patches.sh --check   # pin, clean tree, dry-run apply
tools/apply_patches.sh           # refresh build/decomp-src + apply
```

## Series

| Patch | Upstream-bound | Notes |
|---|---|---|
| `0001-PORT_HOST-scratchpad-stack-noops.patch` | no | `SetSpadStack` / `ResetSpadStack` no-ops under `PORT_HOST` |
| `0002-PORT_HOST-game_bss-complete-types.patch` | no | Include complete types before `game_bss.c` object defs on host |
| `0003-PORT_HOST-allocMemHigh-return-pointer.patch` | no | `allocMemHigh` returns the pointer under `PORT_HOST` (not v0) |
| `0004-PORT_HOST-gte-software-macros.patch` | no | `include/engine/gte.h` → PSY-Z software GTE under `PORT_HOST` |
| `0005-PORT_HOST-overlay-addrs-boot-tim-stub.patch` | no | Host overlay addrs from `port/overlays`; boot TIM upload stubbed |
| `0006-match-neutral-engine-sizeof-sizes.patch` | yes | `sizeof`/`offsetof` for `PAD_DATA_SIZE`, `GFX.prim` clear span, `MemBlock` split, `createInn` |
| `0007-match-neutral-engine-pointer-casts.patch` | yes | `freeFilesFrom(void *)`; archive/text pointer+offset (no pointer↔`u32`/`s32`) |
| `0008-PORT_HOST-static-overlay-restore.patch` | no | `loadModeOverlay` / `loadSubOverlay` → `dw3_overlay_restore` (P2.6) |
| `0009-PORT_HOST-cdfs-cd-reader.patch` | no | `CD_READER.read` → `cdfs_read` (sync; offset 0) |
| `0010-PORT_HOST-game-entry-boot-tim-title.patch` | no | `dw3_game_main`, boot TIM upload, `MODE_TITLE`, quit, mode log |
| `0011-PORT_HOST-stcdinterrupt-decl.patch` | no | Declare `StCdInterrupt` for title `movie.c` |
| `0012-PORT_HOST-host-heap.patch` | no | `HEAP_BASE`/`HEAP_END` → `port/overlays/host_heap.c` |
| `0013-PORT_HOST-timcursor-u32.patch` | no | `TimCursor.word` is `u32 *` on host (not 64-bit `u_long *`) |
| `0014-match-neutral-createTask-child-sizeof.patch` | yes | `childrenSize / sizeof(void *)`; mode/cursor `sizeof(Task *)` |
| `0015-match-neutral-layer-ot-sizeof-OT_TYPE.patch` | yes | OT alloc uses `sizeof(OT_TYPE)` (PSY-Z `{tag,len}`) |
| `0016-PORT_HOST-flip-pending-vsync-pump.patch` | no | `VSync(0)` while spinning on `FLIP_PENDING` |
| `0017-PORT_HOST-layer-ot-type-indexing.patch` | no | Host OT_TYPE indexing; skip 24-bit empty-OT walk |
| `0018-match-neutral-overlay-64bit-audit.patch` | yes | P3.1: overlay/stage `createTask` sizeof, mode-arg save flag, `void*` script/vsync pointers, pointer+offset casts |
| `0019-PORT_HOST-field-file-cache-watermark.patch` | no | P3.1: `FILE_CACHE.freeFrom` uses `HEAP_BASE+0xB1E74` on host |
| `0020-PORT_HOST-p32-boot-mode-and-stage-log.patch` | no | P3.2: `DW3_START_MODE` / `DW3_AUTO_DOOR_*`, stage pick log |
| `0021-PORT_HOST-field-null-slots-and-state-zero.patch` | no | P3.2: NULL `slots` guard + `FieldState` zero stops before `defaultStart` (door spawn) |
| `0022-match-neutral-drawcallback-void-arg.patch` | yes | P3.2: `DrawCallback.arg` / layer add-callback APIs as `void*` (LP64; fixes `FIELDSTG_drawActor` SEGV) |
| `0023-PORT_HOST-p32-script-leave-walk-evidence.patch` | no | P3.2: `SCRIPT_LEAVE`/`leaveField`/`moveByPad` logs; `DW3_FAST_SCRIPT`; `DW3_PROGRESS` |
| `0024-PORT_HOST-p33-battle-boot-evidence.patch` | no | P3.3: `FIGHTSTG` boot logs; empty-party `setParty(0)` for all battles; `DW3_FORCE_ENCOUNTER` seeds `BATTLE_SETUP` |
| `0025-PORT_HOST-p33-battle-complete-evidence.patch` | no | P3.3: `DW3_FORCE_WIN` → report; `fieldMode` seed; party HP/XP logs; STFGTREP return |
| `0026-PORT_HOST-p34-menu-evidence.patch` | no | P3.4: `DW3_FORCE_MENU` / `DW3_FORCE_FIELD_MENU` (host latch); menu `setParty`/`fieldMode`/`modeArg`; `startFade` guard; menu return logs; `DW3_FORCE_MCARD_SAVE` |
| `0027-PORT_HOST-p34-menu-return-and-save.patch` | no | P3.4: `DW3_AUTO_MENU_RETURN` requests `fieldMode` after N menu frames |
| `0028-PORT_HOST-memcard-system-bundle.patch` | no | P3.4: `g_dw3_memcard` contiguous `MemCard`+`MemCardFuncs` (LP64 `MEMCARD_SYSTEM` cast) |
| `0029-PORT_HOST-p35-cardgame-evidence.patch` | no | P3.5: card-mode seed/`modeArg`; `DW3_FORCE_CARD_WIN` prize; return log before `TASK_KILL`; `DW3_AFTER_CARD_ALBUM`; `AUTO_MENU_RETURN` for deck/album/shop; host `CARDGAME_markedCounts` |
| `0030-PORT_HOST-p38-stage-sweep.patch` | no | P3.8: `DW3_FORCE_STAGE` / `DW3_AUTO_STAGE_RETURN` / `STAGE_SWEEP_*`; STAGSLCT `StageSelectWindows` children; host BIOS ROM stub; skip TASK_KILL overwrite when FORCE_STAGE |
| `0031-PORT_HOST-p34-npc-and-cd-partial.patch` | no | P3.4: `CD_READER.read` offset/partial via `cdfs_read_at`; `DW3_FORCE_NPC_ACTION` → `setParty` if empty + `FLAGS_00.applyAction` |
| `0032-PORT_HOST-p35-pack-album-evidence.patch` | no | P3.5: gate win `addCards` behind `DW3_FORCE_CARD_WIN_ADDCARDS`; `DW3_FORCE_PACK_OPEN` → real `STCRDSHP_openPack` `addCards`; AFTER album waits for field |
| `0033-PORT_HOST-p34-triangle-load-npc.patch` | no | P3.4: `DW3_FORCE_MCARD_LOAD`; Triangle cancel logs; `DW3_FORCE_FIELD_POS` / `SHOP_FLAG` / `TALK_AHEAD`; `talkToActorAt` log; `AUTO_MENU_RETURN` skips load |
| `0034-PORT_HOST-p37-movie-start-skip-held.patch` | no | P3.7: movie Start-skip also accepts `getHeld(Start)` + logs `dw3-movie: Start-skip` under `PORT_HOST` |
| `0035-PORT_HOST-p36-playsound-keyon-log.patch` | no | P3.6: capped `dw3: SsUtKeyOn` / `SsSepPlay` logs from `playSound` (`phase3-pc-bringup` series; listed on `main` for evidence traceability) |
| `0036-PORT_HOST-p3-force-npc-mode-gate.patch` | no | P3 gate: `DW3_FORCE_NPC_ACTION=code[@mode][:value]`; mode mismatch keeps `force_once` (Lobby `0x9004@0x203` → event 9) |
| `0037-PORT_HOST-p3-movie-loadimage-before-dout.patch` | no | P3 GPU: under `PORT_HOST`, `LoadImage` completed MDEC slice before next `DecDCTout` (sync soft-MDEC double-buf) |
| `0038-PORT_HOST-timblock-pixels-u32.patch` | no | P3 GPU R1: host `TimBlock.pixels` is `u32[]` so `LoadImage` starts at file offset 12 (LP64 `u_long` padded to 16 and skipped 2 halfwords) |
| `0039-PORT_HOST-battle-mesh-ot-stride.patch` | no | P3 GPU R4: `FIGHTSTG_drawMesh` OT index steps by `sizeof(OT_TYPE)` so host `{addr,len}` entries are not split |
| `0040-PORT_HOST-p39-force-party.patch` | no | P3.9: optional `DW3_FORCE_PARTY=<0\|1\|2>` selects `STARTER_PARTIES` before a forced encounter (Monmon lead = 1) |
| `0041-PORT_HOST-field-tile-one-per-frame.patch` | no | Preview.7: host field-tile read stays busy until `cdfs_poll` (once per VSync) so `FIELDSTG_requestTiles` issues one tile per frame. Superseded by `0042` |
| `0042-PORT_HOST-field-map-preload.patch` | no | Drop the one-tile hold. A field tile (`done != NULL`) copies from one resident map file; `isCdReading` is idle when `readFile` returns |
| `0043-PORT_HOST-stgcard-panel-frames.patch` | no | P5.3: host `MEMCARD_LOAD_FRAMES` / `MEMCARD_SAVE_FRAMES` → 100 (flat sector pace; PS1 kept ~1239/1266) |
| `0044-PORT_HOST-p92-title-port-options.patch` | no | P9.2: title `runOptions` Select → `dw3_port_options_*` shell (no `choice` / `leaveTitle`) |
| `0055-PORT_HOST-p62-eu-country-select-boot.patch` | no | P6.2: EU host `newGame` → `MODE_COUNTRY_SELECT` (CNTY_SEL sets `LANGUAGE`); US still `MODE_TITLE` |
| `0058-PORT_HOST-balloon-null-actor-draw.patch` | no | `FIELDSTG_drawBalloon` draws nothing when `Balloon.actor` is NULL (a close command for a balloon that never opened; WSTAG210 script30 on the new-game bridge). The PS1 reads NULL+0x58/0x5C/0x64 (kernel RAM); a 64-bit host faults. See [BALLOON_NULL_ACTOR.md](BALLOON_NULL_ACTOR.md) |
| `0059-PORT_HOST-null-guards-mark-camera-diag.patch` | no | Preventive follow-up to 0058 (NULL reads the PS1 tolerated): `wstag785.c` `drawActorMark` draws nothing for a NULL `actor`; `FIELDSTG_updateCamera` holds the camera when `FIELDSTG_followWithCamera` stored a NULL target; the 0058 balloon guard and both new ones report through `DW3_NULL_GUARD_HIT` (`port/include/port_null_guard.h`, logs `dw3: null-guard hit: <site>` once per site, only with `DW3_NULL_GUARD_LOG`: PC Debug, `--nxlink`, or `-DDW3_NULL_GUARD_LOG=ON`). Audit and the sites left alone: [BALLOON_NULL_ACTOR.md](BALLOON_NULL_ACTOR.md) |
| `0060-PORT_HOST-p617-music-sfx-bus.patch` | no | P6.17: `playSound` tells the port which SEQ access number and track (SEP) it starts and whether it is music (the exclusive flag, bit 30 of the sound id), `dw3_port_audio_note_sequence` in `port/audio/audio_bus.c`; the PSY-Z mixer then sends music tracks to one bus and everything else (`SsUtKeyOn`, non-exclusive tracks) to another. `main.c` also calls `dw3_port_audio_poll` once per frame (the `DW3_AUDIO_STATS` log only). Design: [P6.17_VOLUME.md](P6.17_VOLUME.md) |
| `0061-PORT_HOST-overlay-restore-loud-failure.patch` | no | Follow-up to 0008: the loaders go through `dw3_overlay_load_mode` / `dw3_overlay_load_sub` (`port/overlays/overlay_host.c`). A failed restore (id not in the host table) logs `dw3: ERROR overlay restore failed`, is not recorded in `OVERLAY_LOADER` (latch `-1`), and aborts in strict mode (`DW3_REPLAY` runs, the Switch replay build, `DW3_OVERLAY_STRICT=1`). See `port/overlays/README.md` |
| `0062-PORT_HOST-p618-encounter-rate.patch` | no | P6.18: `FIELDSTG_countBattleSteps` (`field/field_mode/battle.c`, the per-footstep countdown of the random battles) asks `dw3_port_pacing_encounter_step` what the footstep costs: the rate unchanged at 1x (default, bit-identical), half of it at 0.5x (odd half step carried), or the footstep is skipped (Off: no countdown, no roll, so no random battle). A battle area whose rate is 2000 (`BattleList.count` 0, a map-forced first-step trigger) and every scripted battle (`FIELDSTG_startEventBattle`, events 9000, boss / gauge / hidden-spot battles) are never touched. `dw3_port_pacing_encounter_started` logs each random battle with its footstep count. Design and verification: [P6.18_PACING.md](P6.18_PACING.md) |
| `0063-PORT_HOST-p618-text-speed.patch` | no | P6.18: `textWindowSetTypeDelay` (`engine/text/text_window.c`) maps the dialogue type delay 6 to 2 for Fast / Instant (other delays, such as the area banner's 5, and 0 are left alone); `updateTextWindow` shows a page whole as soon as it starts for Instant (`textWindowShowPage`, what Cross does; waits, pauses and page breaks still apply) and reports each window's typing and done state so the port can log `text page typed in N frame(s)`. Normal changes nothing the game reads. Design and verification: [P6.18_PACING.md](P6.18_PACING.md) |
| `0064-PORT_HOST-p77-test-hooks.patch` | no | P7.7: environment hooks for the replay scenarios, nothing happens when they are unset (PC only): `DW3_LANGUAGE` (EU, in `newGame` before the names are copied), `DW3_FORCE_FORM` (the first partner starts as that Digimon id), `DW3_FORCE_NICKNAME`, `DW3_FORCE_TECH` (`FIGHTSTG_startTechAction` uses that technique), `DW3_FORCE_HIT` (`FIGHTSTG_rollHit`), `DW3_FORCE_STEAL` (`FIGHTSTG_rollSteal`), `DW3_FORCE_ENEMY_ITEM` / `DW3_FORCE_ENEMY_HP` (`WFIGHTMN_initFighters`). Design: [P7.7_BUG_FIXES.md](P7.7_BUG_FIXES.md) |
| `0065-PORT_HOST-p77-battle-hud-form-name.patch` | no | P7.7: `FIGHTSTG_showFighterNames` (`battle_mode/hud.c`) shows the name of the Digimon the partner fights as (`GET_DIGIMON(BattleFighter.id)`, `TEXT_DIGIMON_NAMES`) instead of the Rookie name `info.name` keeps from the new game, unless the player gave it a nickname; `shown[0]` keys slot and Digimon id so a digivolution in battle refreshes it; a name wider than 60 px is squeezed with the text window's scale so it stays clear of the HP number. Same on US and EU, always on (a display bug). Design and verification: [P7.7_BUG_FIXES.md](P7.7_BUG_FIXES.md) |
| `0066-PORT_HOST-p77-claw-steal-on-knockout.patch` | no | P7.7: Picking / Snapping Claw (techniques 127 / 128, `TECH_EFFECT_STEAL`) on the hit that knocks the enemy out. `FIGHTSTG_updateTechAction` skipped the action events when `step` was 1 (knock-out), dropping a steal the roll had already won. With the steal flag set the other effects are cleared, `FIGHTSTG_updateActionEvents` runs, and the action ends (new substate 10) without the wake-up / counter motion; "Stole X!" comes before "Defeated!". No extra random number; a claw that misses, does not kill or did not roll the steal is unchanged. Same on US and EU, always on. Design and verification: [P7.7_BUG_FIXES.md](P7.7_BUG_FIXES.md) |

`tools/apply_patches.sh --check` applies the series in order (stacked
patches on the same file are valid).

### Upstream `make compare` (P2.4 / P3.1)

Match-neutral patches 0006/0007/0018/0022 are intended to stay byte-identical
on PS1, but this host has no MIPS/Psy-Q toolchain image for
`vendor/dw3_decomp` `make compare`. Blocker: run compare in the decomp’s
Docker/toolchain when available before opening the upstream PR.
Evidence for the host warning gate: `docs/P3.1_64BIT_AUDIT.md`.
