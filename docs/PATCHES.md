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
| `0021-PORT_HOST-field-null-slots-and-state-zero.patch` | no | P3.2: NULL `slots` guard + `FieldState` zero through `init` |
| `0022-match-neutral-drawcallback-void-arg.patch` | yes | P3.2: `DrawCallback.arg` / layer add-callback APIs as `void*` (LP64; fixes `FIELDSTG_drawActor` SEGV) |
| `0023-PORT_HOST-p32-script-leave-walk-evidence.patch` | no | P3.2: `SCRIPT_LEAVE`/`leaveField`/`moveByPad` logs; `DW3_FAST_SCRIPT`; `DW3_PROGRESS` |
| `0024-PORT_HOST-p33-battle-boot-evidence.patch` | no | P3.3: `FIGHTSTG` boot logs; `DW3_FORCE_ENCOUNTER` seeds `BATTLE_SETUP` + `setParty(0)` |
| `0025-PORT_HOST-p33-battle-complete-evidence.patch` | no | P3.3: `DW3_FORCE_WIN` → report; `fieldMode` seed; party HP/XP logs; STFGTREP return |
| `0026-PORT_HOST-p34-menu-evidence.patch` | no | P3.4: `DW3_FORCE_MENU` / `DW3_FORCE_FIELD_MENU` (host latch); menu `setParty`/`fieldMode`/`modeArg`; `startFade` guard; menu return logs; `DW3_FORCE_MCARD_SAVE` |
| `0027-PORT_HOST-p34-menu-return-and-save.patch` | no | P3.4: `DW3_AUTO_MENU_RETURN` requests `fieldMode` after N menu frames |
| `0028-PORT_HOST-memcard-system-bundle.patch` | no | P3.4: `g_dw3_memcard` contiguous `MemCard`+`MemCardFuncs` (LP64 `MEMCARD_SYSTEM` cast) |
| `0029-PORT_HOST-p35-cardgame-evidence.patch` | no | P3.5: card-mode seed/`modeArg`; `DW3_FORCE_CARD_WIN` prize; return log before `TASK_KILL`; `DW3_AFTER_CARD_ALBUM`; `AUTO_MENU_RETURN` for deck/album/shop; host `CARDGAME_markedCounts` |
| `0030-PORT_HOST-p38-stage-sweep.patch` | no | P3.8: `DW3_FORCE_STAGE` / `DW3_AUTO_STAGE_RETURN` / `STAGE_SWEEP_*`; STAGSLCT `StageSelectWindows` children; host BIOS ROM stub; skip TASK_KILL overwrite when FORCE_STAGE |
| `0031-PORT_HOST-p34-npc-and-cd-partial.patch` | no | P3.4: `CD_READER.read` offset/partial via `cdfs_read_at`; `DW3_FORCE_NPC_ACTION` → `setParty` if empty + `FLAGS_00.applyAction` |
| `0032-PORT_HOST-p35-pack-album-evidence.patch` | no | P3.5: gate win `addCards` behind `DW3_FORCE_CARD_WIN_ADDCARDS`; `DW3_FORCE_PACK_OPEN` → real `STCRDSHP_openPack` `addCards`; AFTER album waits for field |
| `0033-PORT_HOST-p34-triangle-load-npc.patch` | no | P3.4: `DW3_FORCE_MCARD_LOAD`; Triangle cancel logs; `DW3_FORCE_FIELD_POS` / `DW3_FORCE_SHOP_FLAG`; `AUTO_MENU_RETURN` skips load |

`tools/apply_patches.sh --check` applies the series in order (stacked
patches on the same file are valid).

### Upstream `make compare` (P2.4 / P3.1)

Match-neutral patches 0006/0007/0018/0022 are intended to stay byte-identical
on PS1, but this host has no MIPS/Psy-Q toolchain image for
`vendor/dw3_decomp` `make compare`. Blocker: run compare in the decomp’s
Docker/toolchain when available before opening the upstream PR.
Evidence for the host warning gate: `docs/P3.1_64BIT_AUDIT.md`.
