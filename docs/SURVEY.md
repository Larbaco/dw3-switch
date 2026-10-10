# Repository survey (port-oriented)

Findings from reading `vendor/dw3_decomp` with the Switch port in mind.
Facts here drive `ROADMAP.md`. Counts are from submodule commit `b9caa6c`;
re-check them after a submodule bump.

## 1. Upstream state

- Matching decompilation is **complete** for both versions: executable,
  21 overlays and every stage are C (EU 3,606 functions, US 3,380), 0 fake
  matches, data 100 %. PsyQ libraries stay as splat disassembly (not game
  code).
- Source size: 627 `.c` files (~218k lines), 55 headers under `include/`.
- `make TOOLCHAIN=gcc` already builds the game C with GCC 12 for MIPS and
  boots it in DuckStation (`make smoke`). `mk/toolchain/gcc.mk` lists the
  flags that reproduce GCC 2.8.1 semantics; the port must keep the
  behavioural ones:
  `-std=gnu89 -fsigned-char -fno-builtin -fno-strict-aliasing -fwrapv
  -fno-delete-null-pointer-checks -fno-aggressive-loop-optimizations
  -fno-zero-initialized-in-bss -fno-common -fno-toplevel-reorder`.
  `include/common.h` already has `FLEXIBLE` and `INTERRUPT_SHARED` for
  modern compilers.
- The build is **shiftable**: `make shiftcheck` guarantees every address in
  code/data is a symbol; `config/*/shiftcheck.txt` is empty. The few raw
  `0x80...` literals left in C are sound ids (`SOUND_*`, not addresses) and
  two deliberate values (`FILE_CACHE.freeFrom(0x8015C674)` in
  `field.c`, `HEAP_END` in `heap.h`).
- Upstream TODO is readability only (names, long functions, duplicated
  stage data). Nothing blocks a port on the decomp side.

## 2. Binaries, memory model, control flow

| Piece | Where | Notes |
|---|---|---|
| Executable | `SLUS_014.36` / `SLES_039.36` | engine modules + PsyQ, `.text` `0x80010EBC`.., `.bss` ends at `OVERLAY_VRAM` |
| Mode overlays (21) | `AAA/PRO/*.PRO` → `OVERLAY_VRAM` (`0x80082448` US) | one per game mode, `mode >> 8` indexes `MODE_ENTRY_POINTS` / `MODE_OVERLAY_FILES` |
| Sub-overlays | `WSTAG###`, `WFIGHTMN`, `WFIGHTTS` → `STAGE_VRAM` (`0x800A4CA4` US) | stages load on top of `FIELDSTG`; `WFIGHTMN/TS` on top of `FIGHTSTG` |
| Heap | `HEAP_BASE` (linker symbol after the overlays) .. `HEAP_END = 0x801FF000` | first-fit list of `MemBlock {prev,next,tag}`; `allocHigh` from the top; file cache evicts on pressure |
| Boot image | exe tail at `OVERLAY_VRAM`, read via `SUB_OVERLAY_ADDRESS` | 320x480 TIM, `main()` uploads it before anything else |

Control flow (`src/engine/system/main.c`): PsyQ init → `HEAP.init`,
`SOUND.init`, `MEMCARD_FUNCS.init`, `PAD.init`, `GAME.newGame`,
`FONT.load` → infinite loop: run mode task, `GFX.drawFrame`, `PAD.update`,
`RANDOM.next`, `FILE_CACHE.update`, `SOUND.updateLoading`. Mode changes kill
the root task; `main` frees `MEM_MODE`, resets GFX and recreates it.

Overlay loading (`src/engine/system/overlay.c`) is a `memcpy` of the whole
file over the fixed region, **only when the id changes**
(`OVERLAY_LOADER.mode != id`). Consequences for a single linked image:

- Overlay globals must be re-initialised on load (fresh `.data` from the
  file) but **persist** when the same overlay is re-entered.
- Overlay `.bss` is neither loaded nor cleared on PS1 (hence
  `-fno-zero-initialized-in-bss`).
- Stage entry is already symbolic: `FIELDSTG` links `WSTAGnnn_startStage`
  (`include/field/stages.h`), and `FIGHTSTG` links `WFIGHTMN`/`WFIGHTTS`.
- Stage files define the **same global names**: `setupStage` ×293,
  `updateStage` ×90, `stageObjects`, `stageSlots`, `stageFuncs`, … They are
  separate binaries on PS1; in one image they collide and need prefixing.

Every engine module is a global struct with a method table (`HEAP`, `GFX`,
`FILE_CACHE`, `PAD`, `SOUND`, `GAME`, `TASK_REGISTRY`, `MEMCARD_FUNCS`,
`OVERLAY_LOADER`). Overlays only reach the engine through those tables, which
makes the engine the natural place for port hooks.

## 3. PsyQ surface actually used by the C

Call sites counted in `src/` (names, not unique lines):

| Library | Functions |
|---|---|
| libgpu | `ResetGraph SetGraphDebug SetDispMask SetDefDispEnv SetDefDrawEnv PutDispEnv DrawSync DrawOTag ClearOTag ClearOTagR LoadImage MoveImage ClearImage ClearImage2 GetTPage GetClut SetDrawTPage SetSprt SetSemiTrans IsIdleGPU BreakDraw ContinueDraw SetVideoMode` + prim macros (`setXY*`, `setRGB*`, `addPrim`, …) |
| libgte (C API) | `InitGeom SetGeomOffset SetBackColor rsin rcos ScaleMatrix ApplyMatrixSV` |
| libgte (inline) | 30 `__asm__` macros in `include/engine/gte.h` (`gte_ldv0`, `gte_rtps`, `gte_stsxy`, `gte_ncs`, `gte_nclip`, `gte_SetRotMatrix`, …) plus `gtemac.h` composites (`gte_CompMatrix`, `gte_AverageZ3/4`); ~90 uses |
| libgs | `GsInitGraph GsInit3D GsGetTimInfo GsSetRefView2 GsSetProjection GsSetLightMode GsSetFlatLight` |
| libetc | `VSync VSyncCallback ResetCallback PadInit`-family via libpad |
| libpad | `PadInitDirect PadInitMtap PadStartCom PadStopCom PadInfoMode PadInfoAct PadSetAct PadSetActAlign PadSetMainMode PadGetState PadChkVsync` |
| libcd | `CdInit CdSetDebug CdControl CdControlB CdControlF CdSyncCallback CdReadyCallback CdGetSector CdIntToPos CdPosToInt CdRead2` + `StSetRing StSetStream StGetNext StFreeRing StUnSetRing StCdInterrupt` (STR streaming) |
| libsnd | `SsInit SsStart2 SsSetTableSize SsSetTickMode SsSeqCalledTbyT SsSetMVol SsSetSerialAttr SsSetSerialVol SsVabOpenHeadSticky SsVabTransBody SsVabTransCompleted SsVabClose SsSepOpen SsSepPlay SsSepStop SsSepClose SsSepSetVol SsSepSetDecrescendo SsUtKeyOn SsUtKeyOff SsUtAllKeyOff SsUtReverbOn SsUtSetReverbType SsUtSetReverbDepth` |
| libpress (MDEC) | `DecDCTReset DecDCTin DecDCTout DecDCToutCallback DecDCTvlc2 DecDCTvlcBuild` (title movies only, `src/menus/title/movie.c`) |
| libmcrd | `MemCardInit MemCardStart MemCardExist MemCardAccept MemCardReadFile MemCardWriteFile MemCardCreateFile MemCardGetDirentry MemCardFormat MemCardUnformat MemCardSync` |
| libc | `memcpy strcpy strncpy strlen atoi` (the game has its own heap; no `malloc`) |

No libspu direct calls: all audio goes through libsnd (SEP sequences + VAB
banks loaded from numbered files) and XA/STR for the opening movies.

## 4. MIPS / PS1 specifics that the port must replace

| Item | Where | Port action |
|---|---|---|
| Scratchpad stack `SetSpadStack(0x1F8003FC)` | `src/engine/task/registry.c` | compile out; run `update()` on the normal stack |
| `allocMemHigh` returns "whatever is left in v0" | `src/engine/system/heap.c`, `Heap.allocHigh` | make it return the pointer (`tryAllocMemHigh`) |
| GTE inline asm | `include/engine/gte.h` | provide the same macro names over the shim's software GTE |
| CD callbacks with `int status` cast to `CdlCB` | `src/engine/file/cd_reader.c` | the shim's callback type decides; keep the cast or wrap |
| `FLIP_PENDING` busy-wait on vsync callback | `src/engine/gfx/display.c` | the shim must deliver `VSyncCallback` from its frame loop |
| `-G8`/`$gp` small data | several engine units | irrelevant off MIPS |
| `GsGetTimInfo((u_long *)SUB_OVERLAY_ADDRESS + 1, …)` | `main.c` | load the boot TIM from the exe tail file instead |

## 5. 64-bit hazards (Switch is AArch64 only)

Pointers grow to 8 bytes, so structs that hold pointers change size and
offsets. The documented `/* 0xNN */` offsets in headers are fine as
comments, but these patterns break:

- **Literal sizes instead of `sizeof`**: 11 `createTask(…, <number>, …)`
  (213 use `sizeof`), 37 `HEAP.alloc/zero/allocZeroed/allocHigh` calls
  without `sizeof` (27 with), e.g. `HEAP.zero(&GFX.prim, 0x18)`,
  `HEAP.zero(&PAD, PAD_DATA_SIZE)`, `HEAP.allocHigh(0x9615C, 2)`.
- **Pointer ↔ integer casts**: ~19 lines (`(u32)slot->data`, `(s32)table`,
  `(u32)GAME.funcs`, `(s32)TASK_REGISTRY.funcs`, `(u_long)state.prim`,
  `(s32)0x80000000` as a mode argument in `stage_select.c`).
- **Address arithmetic**: `freeFilesFrom(u32 addr)` compares heap
  addresses with a literal (`0x8015C674`) — must become an offset from
  the heap base or a symbol.
- `MemBlock` header is 12 bytes on PS1, 24 on AArch64; `tryAllocMem` uses
  `splitSize = size + 20` (header + minimum payload) — recompute from
  `sizeof`.
- `CardDirEntry` mirrors the kernel `DIRENTRY` with a `next` pointer.
- Task structs are allocated by size and accessed by field, so growth is
  fine as long as sizes come from `sizeof`.
- Good news: the saved part of `GameState` (`GAME_SAVE_SIZE` bytes) holds
  **no pointers**, so PS1 save compatibility is achievable.

A fork-free way to apply these is a `port/patches/` series over the
submodule plus upstream PRs where the change is neutral for the match
(`sizeof` instead of literals is byte-identical).

## 6. Disc data

- Files are addressed by **numeric id**: `FILE_SECTORS[2199]` (US) in
  `src/engine/file/file_table.c` gives each id's first sector and size.
  Ids differ per version (`FILE_FIELDSTG` is `0x158` US / `0x166` EU).
- `AAA/DAT`, `AAA/PRO`, `AAA/STR` are reachable only through the ISO path
  table; upstream `tools/extract_disc.py` handles that and keeps paths.
- The port therefore needs an **id → path map per version**, built by
  matching each id's LBA against the ISO directory records. That is a new
  tool (`tools/`), run once by the owner over their own disc.
- Sound banks (`SOUND_FILES_*`) and the title movies are also numeric ids;
  movies use `CdRead2` streaming + MDEC and need the raw `.STR` sectors
  (Mode 2 Form 2) rather than 2048-byte user data.
- Only `disks/<version>/<exe>` and `AAA/PRO/` are needed to *build* the
  decomp; the port needs the whole `AAA/` tree at runtime.

## 7. Shim layer options

| | PSY-Z (`Xeeynamo/psyz`) | PsyCross (`OpenDriver2/PsyCross`) | Own thin shim |
|---|---|---|---|
| 64-bit | yes (32/64 "out of the box") | no (per PSY-Z author and SotN wiki) | yes |
| libsnd | yes (`libsnd.c`, decomp-backed) | **no** | would have to write a SEQ/SEP/VAB sequencer |
| libpress/MDEC | yes (`libpress.c`, `mdec.c`) | TODO | would have to write |
| libcd + streaming | yes | yes (BIN/CUE) | file-backed |
| libgs subset | yes | partial | small |
| Backends | SDL3 (GL / SDL_GPU / software), PSP (`sceGu`), iOS, web | SDL2 + OpenGL + OpenAL | n/a |
| Overlays | `Psyz_ModuleOpen` (.so/.dll/.prx) | n/a | n/a |
| Track record | SotN, FF7, MMX4, Rayman, Rage Racer | OpenDriver2 | — |

Recommendation: **PSY-Z**. Two gaps for Switch, both solvable in this repo:

1. **No SDL3 for libnx in public.** The SDL3 Switch port is NDA-only; the
   devkitPro `switch-sdl3` package is still an open request. Options:
   build devkitPro's SDL3 branch from source (spike), or write a libnx
   backend for PSY-Z following its PSP backend layout (`gpu`, `input`,
   `audio`, `os`, `file`, `log`, `module`). The latter is what
   `platforms/switch/` is for.
2. **No `dlopen` on libnx.** PSY-Z modules are dynamic objects. The port
   needs a *static overlay model*: all overlays and stages linked into one
   image with per-binary symbol prefixing and a table that emulates
   `loadModeOverlay`/`loadSubOverlay` (restore the overlay's initial
   `.data` when the id changes). This also removes a runtime dependency on
   the SD card for code.

## 8. Local environment (this machine)

- No devkitPro / `DEVKITPRO`; no `aarch64-none-elf-gcc`. Docker 29 is
  available → use `devkitpro/devkita64` for Switch builds.
- Host has `gcc`, `make`, `python3`, OpenGL/GLES/EGL dev libs; no SDL3
  dev package detected yet (needed for the PC host).
- No `disks/`, no `romfs/`, decomp `.venv`/`bin/` not set up. The owner
  must supply the disc; nothing in the repo may contain it.
- `AGENTS.md` is present but untracked.
