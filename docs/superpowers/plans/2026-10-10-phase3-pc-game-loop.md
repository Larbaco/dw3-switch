# Phase 3 — PC Game Loop Bring-up Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Run the full Digimon World 3 game loop on PC (field, stages, battle, menus, card game, audio, movies) from decomp C over PSY-Z, until a start→first-boss segment is playable without crashes for US (and EU builds when linked).

**Architecture:** Keep Phase 2 layering: never edit `vendor/dw3_decomp` in place; extend `port/patches/` for match-neutral 64-bit fixes and `PORT_HOST` glue; grow `port/CMakeLists.txt` object libraries (field / battle / menus / cardgame / debug) into the same static image as engine+title; restore overlays via `port/overlays` as each mode is linked. PC host (`platforms/pc`) remains the bring-up target; Switch stays thin until Phase 4. Audio and movie paths go through PSY-Z (`libsnd` / `libpress`); comparison harness uses DuckStation screenshots as the visual oracle.

**Tech Stack:** CMake ≥ 3.21, Ninja, host GCC, PSY-Z (`vendor/psyz` @ pin), Python 3.12 + pytest, DuckStation (owner machine, optional for P3.9), GitHub Actions.

## Parallelization (read this first)

SDD forbids two implementers on the **same** worktree. Phase 3 uses **one integration branch** plus optional side branches that own disjoint paths.

| Track | Branch / worktree | Owns | ROADMAP items |
|---|---|---|---|
| **Core** | `phase3-pc-bringup` → `.worktrees/phase3-pc-bringup` | `port/patches/`, `port/CMakeLists.txt`, `port/overlays/`, `platforms/pc/`, `docs/ROADMAP.md`, game bring-up | P3.1, P3.2, P3.3, P3.4, P3.5, P3.7, P3.8, gate |
| **Audio-inv** | optional `phase3-audio` (merge into Core before P3.6 done) | `docs/` audio notes, PSY-Z libsnd probes, `tools/tests/` audio timing fixtures only — **no** `port/CMakeLists.txt` / patches that Core owns | P3.6 investigation + timing notes |
| **Harness** | optional `phase3-harness` (merge into Core before gate) | `tools/compare_harness/`, `docs/P3.9_REFERENCE.md`, synthetic fixture scripts — **no** game C patches | P3.9 scaffold |

**Wave 0 (coordinator, this session):** branch + worktree from `main` @ Phase 2 merge; land this plan + ROADMAP “Phase 3 in progress”; docs-only sync to `main`; start Task A1 (P3.1).

**Wave 1 (parallel — safe after Wave 0):**

1. Core Task A1 = P3.1 64-bit audit overlays+stages (patches + compile-clean warnings).
2. Audio-inv Task S1 = P3.6 investigation doc (PSY-Z `SsSeqCalledTbyT` / VAB / SEP surface) — no game link required.
3. Harness Task H1 = P3.9 scaffold (script stubs + doc page skeleton).

**Wave 2 (Core sequential after A1):** A2=P3.2 field+first stage → A3=P3.3 battle → A4=P3.4 menus/shops → A5=P3.5 card game.

**Wave 3 (after field music path exists):** merge Audio-inv → Core Task A6=P3.6 audio done-when (title + field switch + battle SFX).

**Wave 4:** A7=P3.7 movies → A8=P3.8 stage sweep → merge Harness → A9=P3.9 comparison captures → gate + PR.

**Do not parallelize:** two agents editing `port/CMakeLists.txt`, `port/patches/`, `platforms/pc/`, or `docs/ROADMAP.md` at once. ROADMAP ticks ride with the Core task that finishes each P3.x item.

## Global Constraints

- Branch: `phase3-pc-bringup` (integration + PR). Never land Phase 3 gate work straight on `main` except the docs-only “Phase 3 in progress” tracking commit.
- Version: **US** first (`-DVERSION_US=1`); EU link is a follow-up once US field/battle are stable.
- Own-disc only; never commit `disks/`, `runtime/`, `*.bin` disc images, or extracted trees. Synthetic fixtures under `tools/testdata/` are fine.
- Patches apply to a **build copy** only; `vendor/dw3_decomp` stays clean.
- Match-neutral fixes: `Upstream-bound: yes` in patch headers + `docs/PATCHES.md`; open upstream PRs when a MIPS `make compare` environment is available (document blocker if not).
- Warning gates for audit tasks: `-Werror=pointer-to-int-cast -Werror=int-to-pointer-cast` (already in `port/CMakeLists.txt` `_DW3_GAME_OPTS`).
- Portuguese in chat with owner; English in code/commits.
- Stage paths explicitly; no `git add -A`.
- Commit + push after each task (project default per `AGENTS.md`).
- Follow `AGENTS.md`, `docs/ROADMAP.md` Phase 3, `docs/SURVEY.md` §5, `docs/ARCHITECTURE.md`, `docs/PATCHES.md`, `docs/PSYZ.md`.

## File map (create / modify)

| Path | Role | Track |
|---|---|---|
| `port/patches/0018-*.patch` … | P3.1 createTask sizeof, stage_select mode arg, pointer casts | Core |
| `docs/PATCHES.md` | Series table + upstream-compare blocker notes | Core |
| `port/CMakeLists.txt` | Object libs for field/battle/menus/cardgame/debug (+ warning-clean compile) | Core |
| `port/overlays/*` | Register FIELDSTG / FIGHTSTG / menu overlays as linked | Core |
| `platforms/pc/*` | Link new objects; runtime flags; skip-movie toggles | Core |
| `docs/P3.1_64BIT_AUDIT.md` | Remaining warning inventory with evidence (if any) | Core |
| `docs/P3.6_AUDIO.md` | PSY-Z libsnd investigation + timing notes | Audio-inv → Core |
| `tools/compare_harness/*` | DuckStation + dw3-pc screenshot driver stubs | Harness |
| `docs/P3.9_REFERENCE.md` | Side-by-side comparison page (title/field/battle) | Harness → Core |
| `docs/ROADMAP.md` | Tick P3.x; Phase 3 status | Core |
| `docs/ARCHITECTURE.md` | Reflect full overlay link set when gate nears | Core (gate) |

---

### Task W0: Branch + worktree + plan on remote

**Files:** this plan; `docs/ROADMAP.md` (Phase 3 status + tracking).

- [x] **Step 1: Sync `main`, create `phase3-pc-bringup` + `.worktrees/phase3-pc-bringup`**

```bash
MAIN=/mnt/windows/Jogos/dw3-switch
git -C "$MAIN" fetch origin
git -C "$MAIN" checkout main && git -C "$MAIN" pull --ff-only origin main
git -C "$MAIN" submodule update --init --recursive
mkdir -p "$MAIN/.worktrees"
git -C "$MAIN" worktree add "$MAIN/.worktrees/phase3-pc-bringup" -b phase3-pc-bringup
```

- [x] **Step 2: Commit plan + “Phase 3 in progress” on `phase3-pc-bringup`; push `-u`**
- [x] **Step 3: Docs-only sync to `main`:** copy plan + ROADMAP Phase 3 status; commit `docs: start Phase 3 ROADMAP tracking`; push `main` (no force).

---

### Task A1 (Wave 1): P3.1 64-bit audit — overlays + stages

**Status:** done — `0018` + `0019`, CMake `dw3_p31_audit` (432 objs); see `docs/P3.1_64BIT_AUDIT.md`.

**Files:**
- Create: `port/patches/0018-match-neutral-overlay-createTask-sizeof.patch` (and further `0019+` if split helps review)
- Create: `port/patches/00xx-match-neutral-stage-select-mode-arg.patch` (may fold into 0018 if small)
- Create: `port/patches/00xx-match-neutral-overlay-pointer-casts.patch`
- Create (if needed): `docs/P3.1_64BIT_AUDIT.md` for any remaining warnings with compiler evidence
- Modify: `docs/PATCHES.md`, `port/CMakeLists.txt` (compile units under audit dirs), `docs/ROADMAP.md` (tick P3.1)

**Interfaces:**
- Consumes: Phase 2 `_DW3_GAME_OPTS` (`-Werror=pointer-to-int-cast -Werror=int-to-pointer-cast`); `createTask` / `createTaskWithId` semantics from patch `0014` (`childrenSize / sizeof(void *)`)
- Produces: patched `build/decomp-src` sources under `src/field`, `src/battle`, `src/cardgame`, `src/menus`, `src/debug` that compile under those warnings; CMake object target(s) that exercise the dirs (full link of every stage may wait for P3.2 — compile-to-object is enough for “warnings clean”)

**Inventory (US tree @ decomp pin — re-grep when starting):**

Literal `createTask` / `createTaskWithId` **size** args (2nd parameter not `sizeof`):

| Site | Current | Intended fix |
|---|---|---|
| `src/debug/stage_select/stage_select.c` | `createTask(..., 0, 0)` | `sizeof(StageSelect)` (or `sizeof(Task)` if header-only — verify `StageSelect`) |
| `src/field/stages/amaterasu_city/wstag934.c` | `0x58, 8` | `sizeof(StageTask)`, `2 * sizeof(Task *)` (or typed children) |
| `src/field/stages/kusanagi_city/wstag780.c` | `0x60, 0` / `0x50, 0x1C` | `sizeof` of local effect / effect-set structs |
| `src/field/stages/spy_satellite/wstag810.c` | `0x68` (+ `createTaskWithId`) | `sizeof(StageTilePairN)` |
| `src/field/stages/spy_satellite/wstag815.c` | `0x60` | `sizeof` of background task struct |
| `src/field/stages/common/create_tile_anims_{54,58,5c}.inc.c` | `0x54` / `0x58` / `0x5C` | Prefer `sizeof(StageTileAnims)` **or** document why sizes differ (anims[N] packing) — if structs match PS1 layout comments, use `sizeof` |
| `src/field/stages/common/create_far_sprites.inc.c` | `0x50` | `sizeof` of far-sprites task |

Also fix **childrenSize** literals that mean pointer slots (same class as engine `0014`): e.g. menus `createTask(..., sizeof(T), 4|8)` → `N * sizeof(Task *)` / `sizeof(ChildrenStruct)` after confirming child layout.

Mode arg:

| Site | Issue | Fix |
|---|---|---|
| `src/debug/stage_select/stage_select.c` (US + EU tables) | `(s32)0x80000000` save flag (bit 31; consumed as `(u32)getModeArg() >> 31` in `STGMCARD_createScreen`) | Named constant e.g. `#define MODE_ARG_SAVE_FLAG ((s32)(1u << 31))` in `debug/stage_select.h` (or `engine/game.h` if shared); keep `s32` width — do **not** widen `StageSelectEntry.arg` to pointer |

Pointer ↔ integer / unsafe casts (grep + fix under warning flags):

| Pattern | Example sites | Fix direction |
|---|---|---|
| `(s32)TASK_REGISTRY.funcs.find(...)` | `src/field/field_mode/event.c` | Compare/store as `Task *` / null check |
| `(s32)task` into `GFX.vsyncArg` | `src/battle/battle_mode/cursor.c` | Host-safe handle (PORT_HOST intptr or opaque id) — prefer match-neutral if API allows |
| `(u_long)state.prim.ptr & 0xFFFFFF` | `src/battle/battle_mode/model_mesh.c` | Follow engine OT tagging pattern from patches `0015`/`0017` |
| `(Task *)createX(...)` upcasts | menus/cardgame shops | Usually fine (object pointer); only change if a warning fires |
| Function pointer stored as `Task *` without call | (re-grep) | Cast via intermediate typed pointer or change field type |

- [ ] **Step 1: Refresh inventory**

```bash
WT=/mnt/windows/Jogos/dw3-switch/.worktrees/phase3-pc-bringup
cd "$WT"
rg -n 'createTask(WithId)?\([^,]+,\s*(0x[0-9A-Fa-f]+|[0-9]+)\s*,' \
  vendor/dw3_decomp/src/{field,battle,cardgame,menus,debug} -g '*.c' -g '*.inc.c'
rg -n '\(s32\)0x80000000|\(s32\)|\(u32\).*funcs|\(Task \*\)' \
  vendor/dw3_decomp/src/{field,battle,cardgame,menus,debug} -g '*.c' | head -80
```

Expected: ~10 literal size sites in field/debug (SURVEY “11” counted engine `createInn` already fixed in P2.4); mode-arg hits in `stage_select.c`; cast list as above.

- [ ] **Step 2: Add a CMake compile target for audit dirs (objects only)**

In `port/CMakeLists.txt`, add something equivalent to:

```cmake
file(GLOB_RECURSE DW3_P31_SOURCES CONFIGURE_DEPENDS
  "${DECOMP_SRC}/src/field/*.c"
  "${DECOMP_SRC}/src/battle/*.c"
  "${DECOMP_SRC}/src/cardgame/*.c"
  "${DECOMP_SRC}/src/menus/*.c"
  "${DECOMP_SRC}/src/debug/*.c")
list(FILTER DW3_P31_SOURCES EXCLUDE REGEX "\\.inc\\.c$")
# Exclude title sources already in dw3_title to avoid duplicate symbols later:
list(FILTER DW3_P31_SOURCES EXCLUDE REGEX "/menus/title/")

add_library(dw3_p31_audit OBJECT ${DW3_P31_SOURCES})
add_dependencies(dw3_p31_audit dw3_decomp_patched)
target_include_directories(dw3_p31_audit PRIVATE ${_DW3_GAME_INCLUDES})
target_compile_definitions(dw3_p31_audit PRIVATE ${_DW3_GAME_DEFS})
target_compile_options(dw3_p31_audit PRIVATE ${_DW3_GAME_OPTS})
# Do NOT add dw3_p31_audit to dw3_engine_objs until P3.2+ links overlays for real.
```

If the full GLOB fails to compile for missing symbols / ASM, split into per-directory OBJECT libs and compile the files that contain the inventory sites first; expand until the five dirs are covered or document blockers with exact `gcc` errors in `docs/P3.1_64BIT_AUDIT.md`.

- [ ] **Step 3: Write failing build evidence**

```bash
cmake -S platforms/pc -B build/pc -G Ninja
ninja -C build/pc dw3_p31_audit 2>&1 | tee /tmp/p31-before.txt
```

Expected: FAIL or warn on pointer casts / undersized tasks once those TUs are compiled (or link-irrelevant compile errors — fix includes next).

- [ ] **Step 4: Author match-neutral patches**

Example fragment for tile-pair N (repeat pattern per site):

```diff
--- a/src/field/stages/spy_satellite/wstag810.c
+++ b/src/field/stages/spy_satellite/wstag810.c
@@
-    StageTilePairN *task = createTaskWithId(updateTilePairN, 0x68, 0, arg);
+    StageTilePairN *task = createTaskWithId(updateTilePairN, sizeof(StageTilePairN), 0, arg);
@@
-    StageTilePairN *task = createTask(updateTilePairN, 0x68, 0);
+    StageTilePairN *task = createTask(updateTilePairN, sizeof(StageTilePairN), 0);
```

Stage select mode flag:

```c
/* bit 31: memcard screen treats as "save" vs "load" via (u32)arg >> 31 */
#define STAGE_SELECT_MODE_ARG_SAVE ((s32)(1u << 31))
/* ... */
{ "...", "...", MODE_CONTINUE, STAGE_SELECT_MODE_ARG_SAVE },
```

Header every new patch with `Upstream-bound: yes` or `no` per `docs/PATCHES.md`.

- [ ] **Step 5: Apply + rebuild until warning-clean**

```bash
tools/apply_patches.sh --check
tools/apply_patches.sh
ninja -C build/pc dw3_p31_audit 2>&1 | tee /tmp/p31-after.txt
# Must not contain: warning: cast from pointer to integer|cast to pointer from integer
# Prefer zero warnings of those classes under -Werror (build succeeds).
git -C vendor/dw3_decomp status --porcelain   # must be empty
```

If a TU cannot be made clean without a larger PORT_HOST redesign, record the exact diagnostic, file:line, and rationale in `docs/P3.1_64BIT_AUDIT.md` and leave the ROADMAP item open or partially noted — prefer fixing.

- [ ] **Step 6: Docs + ROADMAP + commit + push**

```bash
# update docs/PATCHES.md series table
# tick P3.1 in docs/ROADMAP.md when clean (or link P3.1_64BIT_AUDIT.md)
git add port/patches/*.patch docs/PATCHES.md port/CMakeLists.txt docs/ROADMAP.md docs/P3.1_64BIT_AUDIT.md
git commit -m "$(cat <<'EOF'
fix: P3.1 64-bit audit for overlays and stages

Replace literal createTask sizes and unsafe pointer casts under
field/battle/cardgame/menus/debug so LP64 host builds stay warning-clean.
EOF
)"
git push -u origin HEAD
```

**Done when:** `-Wpointer-to-int-cast` / `-Wint-to-pointer-cast` clean for those dirs (or remaining cases documented with compiler evidence); submodule clean; patches listed in `docs/PATCHES.md`.

---

### Task S1 (Wave 1, parallel): P3.6 audio investigation

**Files:**
- Create: `docs/P3.6_AUDIO.md`
- Optional: `tools/tests/test_audio_timing_notes.md` or a tiny pytest that documents expected vsync callback ordering (no disc assets)

**Interfaces:**
- Consumes: PSY-Z libsnd API surface; game calls `SsSeqCalledTbyT`, VAB load, SEP play from engine/overlays
- Produces: written investigation — which PSY-Z symbols exist at pin `fac9bd58`, what the host must pump each frame, gaps vs game usage

- [ ] **Step 1: Grep game + PSY-Z**

```bash
rg -n 'SsSeqCalledTbyT|SsVab|SsSep|SsUt|VSyncCallback' \
  vendor/dw3_decomp/src vendor/psyz --glob '!**/SDL/**' | head -100
```

- [ ] **Step 2: Write `docs/P3.6_AUDIO.md`** with: call graph from title/field/battle, host pump requirements, known gaps, proposed validation (title music, stage switch, battle SFX).
- [ ] **Step 3: Commit on `phase3-audio` or Core if no branch split; push.**

**Done when:** investigation doc exists; implementation work deferred to Task A6.

---

### Task H1 (Wave 1, parallel): P3.9 harness scaffold

**Files:**
- Create: `tools/compare_harness/README.md`
- Create: `tools/compare_harness/capture_pair.py` (stub: argparse for `--runtime`, `--save`, `--label`; exits 2 with “not implemented” until DuckStation + dw3-pc paths exist)
- Create: `docs/P3.9_REFERENCE.md` (skeleton sections: Title, Field, Battle)

**Interfaces:**
- Consumes: owner DuckStation + local `build/pc/dw3-pc` + `runtime/us` (never committed)
- Produces: script entry points and doc skeleton; filled screenshots land only on owner machines / gitignored paths

- [ ] **Step 1: Scaffold script**

```python
#!/usr/bin/env python3
"""Capture DuckStation vs dw3-pc frames for the same save/label (P3.9)."""
import argparse
import sys

def main() -> int:
    p = argparse.ArgumentParser()
    p.add_argument("--runtime", default="runtime/us")
    p.add_argument("--save", required=False, help="path to .mcr or slot id")
    p.add_argument("--label", required=True, choices=("title", "field", "battle"))
    p.add_argument("--out", default="build/compare")
    args = p.parse_args()
    print(f"compare_harness: stub label={args.label} runtime={args.runtime}", file=sys.stderr)
    return 2

if __name__ == "__main__":
    raise SystemExit(main())
```

- [ ] **Step 2: Doc skeleton + README (no binary screenshots in git).**
- [ ] **Step 3: Commit + push on harness branch or Core.**

**Done when:** stub runs and exits 2; docs explain how owner will fill comparisons later.

---

### Task A2: P3.2 Field + first stage

**Status:** link scaffold done — `dw3_p32_field` + US table
(`FIELDSTG`/`STPLNMET`/`WSTAG260`/`780`/`785`); `dw3-pc` links. Runtime
walk/door still open.

**Files:** `port/CMakeLists.txt` (link `FIELDSTG` + starting town stage e.g. `WSTAG260`), `port/overlays/*`, patches as needed, `platforms/pc`, `docs/ROADMAP.md`.

**Interfaces:**
- Consumes: P3.1 clean overlays; cdfs file ids; static overlay restore
- Produces: new game → name entry → walkable first field; door changes stage

- [x] **Step 1: Identify US starting stage file ids / overlay names from `docs/SURVEY.md` + decomp `stages` tables.**
- [x] **Step 2: Link FIELDSTG + required shared field objects; register overlay restore entries.**
- [ ] **Step 3: Boot path: title → new game → `STPLNMET` if required → field.**
- [ ] **Step 4: Manual verify walk + door; tick P3.2; commit + push.**

**Done when:** first field area walkable; a door changes stage.

---

### Task A3: P3.3 Battle

**Status:** Done-when met on `phase3-pc-bringup` — see `docs/P3.3_BATTLE.md`
(`DW3_FORCE_ENCOUNTER` + `DW3_FORCE_WIN` + `STFGTREP` → field).

**Files:** link `FIGHTSTG` + `WFIGHTMN` + `STFGTREP` (+ `WFIGHTTS` optional); battle patches; ROADMAP.

- [x] **Step 1: Link battle overlays; fix remaining 64-bit / MIPS issues that surface at link/run.**
- [x] **Step 2: Trigger encounter (forced for bring-up); complete fight; return with stats.**
- [x] **Step 3: Tick P3.3; commit + push.**

**Done when:** encounter completes and returns to field with stats applied.

---

### Task A4: P3.4 Menus and shops

**Files:** link `STSTATUS`, `STITSHOP`, `STGTRAIN`, `STGDGLAB`, `STGMCARD`, `STDGNAME`, `STPLNMET`; memcard uses P2.7.

- [x] **Step 1: Link each overlay; open from field.** (`dw3_p34_menus`, `DW3_FORCE_MENU`)
- [ ] **Step 2: End-to-end smoke per menu; save via `STGMCARD` writes `.mcr`.**
  (open/return + Triangle cancel + STGMCARD save/load — `docs/P3.4_MENUS.md`)
- [x] **Step 3: Tick P3.4; commit + push.** (`READY_FOR_MAIN_ROADMAP_SYNC: P3.4`)

**Done when:** each listed menu works end-to-end; save uses P2.7.

---

### Task A5: P3.5 Card game

**Files:** `CARDGAME`, `STCRDDEK`, `STCRDSHP`, `STCRDABM`.
Branch: `phase3-cardgame` (separate worktree from P3.4 memcard work).

- [x] **Step 1: Link card overlays.** (`dw3_p35_cardgame`, patch `0029`)
- [x] **Step 2: Win a card battle; album updates.**
  (force-win → `addCards`/`cardsSeen` → field → `STCRDABM` → field;
  `docs/P3.5_CARDGAME.md`)
- [x] **Step 3: Tick P3.5; commit + push.** (Done-when met on `phase3-cardgame`)

**Done when:** card battle winnable; album updates.

---

### Task A6: P3.6 Audio (implementation)

**Files:** host vsync → `SsSeqCalledTbyT` pump; any `PORT_HOST` libsnd gaps in `port/psyz_missing/`; `docs/P3.6_AUDIO.md` update; ROADMAP.

**Interfaces:**
- Consumes: Task S1 investigation
- Produces: audible title music, field music change on stage switch, battle SFX

- [ ] **Step 1: Implement host pump per S1 notes.**
- [ ] **Step 2: Verify three cases (title / field switch / battle SFX).**
- [ ] **Step 3: Tick P3.6; commit + push.**

**Done when:** those three audio cases work on PC.

---

### Task A7: P3.7 Movies

**Files:** PSY-Z `libpress` + streaming (`CdRead2`, `StSetRing`); raw-sector `.STR` from P1.6 layout; skip path retained for bring-up.

- [ ] **Step 1: Wire STR streaming from runtime pack.**
- [ ] **Step 2: Opening movies play with audio; Start skips.**
- [ ] **Step 3: Tick P3.7; commit + push.**

**Done when:** opening movies play with audio and can be skipped.

---

### Task A8: P3.8 Stage sweep

**Files:** link `STAGSLCT`; logging harness; crash/assert log.

- [ ] **Step 1: Boot debug stage select; script or checklist over all US stages.**
- [ ] **Step 2: Log failures; fix blockers or document.**
- [ ] **Step 3: Tick P3.8; commit + push.**

**Done when:** all stages load and return to the select screen.

---

### Task A9: P3.9 Reference comparison (fill harness)

**Files:** `tools/compare_harness/capture_pair.py` (real capture), `docs/P3.9_REFERENCE.md` (embedded or linked owner screenshots — prefer gitignored `build/compare/` + markdown descriptions).

- [ ] **Step 1: Implement capture for title, field, battle against DuckStation.**
- [ ] **Step 2: Fill doc page with comparison notes.**
- [ ] **Step 3: Tick P3.9; commit + push (no disc assets).**

**Done when:** doc page exists with comparison for title, field, battle.

---

### Task G1: Phase 3 gate + PR

- [ ] Start → first boss segment on PC without crashes; US (and EU if claimed) build.
- [ ] ROADMAP Phase 3 checkboxes honest; `docs/ARCHITECTURE.md` updated.
- [ ] CI green on `phase3-pc-bringup`.
- [ ] Open PR into `main`.

**Done when:** gate criteria met; PR ready for review.

---

## Self-review (plan author)

1. **Spec coverage:** P3.1–P3.9 mapped to A1–A9 + S1 + H1 + W0 + G1.
2. **Placeholder scan:** no TBD steps; audio/movie details live in dedicated tasks with concrete commands.
3. **Type consistency:** `createTask` children sizing follows patch `0014`; mode arg stays `s32` bit 31; CMake audit target name `dw3_p31_audit` reused only until P3.2 splits real overlay libs.
4. **Parallelism:** Wave 1 allows A1 ∥ S1 ∥ H1 on disjoint paths; Core owns patches/CMake/ROADMAP.
