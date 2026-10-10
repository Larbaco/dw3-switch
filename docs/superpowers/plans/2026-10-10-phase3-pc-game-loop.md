# Phase 3 — PC Game Loop Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Run the full Digimon World 3 game loop on PC (field, stages, battle, menus, card game, audio, movies) over PSY-Z, with a reference comparison harness against DuckStation.

**Architecture:** Keep Phase 2 layering: never edit `vendor/dw3_decomp` in place; extend `port/patches/` for match-neutral 64-bit fixes and `PORT_HOST` glue; grow `port/CMakeLists.txt` / `port/overlays/` to link overlays as they are brought up; PC host stays thin. Parallel tracks own disjoint paths (audit patches vs audio investigation vs harness scaffold) and merge into `phase3-pc-bringup` before the gate.

**Tech Stack:** CMake ≥ 3.21, Ninja, PSY-Z (`vendor/psyz`), host GCC, Python 3.12 + pytest, DuckStation (oracle, owner machine), GitHub Actions.

## Parallelization (read this first)

SDD forbids two implementers on the **same** worktree. Phase 3 uses one integration branch plus optional side branches for safe parallel work.

| Track | Branch / worktree | Owns | ROADMAP items |
|---|---|---|---|
| **Core** | `phase3-pc-bringup` → `.worktrees/phase3-pc-bringup` | `port/patches/`, `port/CMakeLists.txt`, `port/overlays/`, `platforms/pc/`, `docs/ROADMAP.md`, `docs/PATCHES.md` | P3.1–P3.5, P3.7–P3.8, gate |
| **Audio** (optional side) | `phase3-audio` → `.worktrees/phase3-audio` | notes under `docs/` for libsnd/PSY-Z timing; **no** `port/CMakeLists.txt` / ROADMAP edits until merge | P3.6 investigation only until Core merges wiring |
| **Harness** (optional side) | `phase3-harness` → `.worktrees/phase3-harness` | `tools/compare_harness/` (or similar), `docs/` comparison page scaffold; synthetic fixtures only | P3.9 scaffold |

**Wave 0 (coordinator):** this plan + ROADMAP “Phase 3 in progress” on `phase3-pc-bringup` (+ docs sync on `main`).

**Wave 1 (parallel-safe):**

1. Core Task C1 = P3.1 64-bit audit (overlays + stages patches).
2. Audio Task A1 = P3.6 investigation notes (PSY-Z `SsSeqCalledTbyT`, VAB/SEP paths) — docs only.
3. Harness Task H1 = P3.9 scaffold (script stubs + doc outline; no disc assets).

**Wave 2 (Core sequential):** C2=P3.2 field/first stage → C3=P3.3 battle → C4=P3.4 menus/shops → C5=P3.5 card game.

**Wave 3 (after field music path exists):** merge Audio notes → C6=P3.6 audio wiring; C7=P3.7 movies; C8=P3.8 stage sweep; merge Harness → C9=P3.9 comparisons.

**Do not parallelize:** two agents editing `port/CMakeLists.txt`, `port/patches/`, `platforms/pc/`, or `docs/ROADMAP.md` at once.

## Global Constraints

- Branch: `phase3-pc-bringup` (integration + PR). Docs-only ROADMAP/plan sync may land on `main` so the owner can follow; game code stays on the feature branch until PR.
- Version: **US** first (`-DVERSION_US=1`); EU only when US segment is stable.
- Own-disc only; never commit `disks/`, `runtime/`, `*.bin` disc images, or extracted trees.
- Patches apply to a **build copy** only; `vendor/dw3_decomp` stays clean.
- Match-neutral fixes: `Upstream-bound: yes` in patch headers; note in `docs/PATCHES.md`. Upstream PRs when MIPS compare is available (same blocker pattern as P2.4).
- Portuguese in chat with owner; English in code/commits.
- Stage paths explicitly; no `git add -A`.
- Commit + push after each task (project default).
- Follow `AGENTS.md`, `docs/ROADMAP.md` Phase 3, `docs/SURVEY.md`, `docs/ARCHITECTURE.md`, `docs/PSYZ.md`, `docs/PATCHES.md`.

## File map (create / modify)

| Path | Role | Track |
|---|---|---|
| `port/patches/0018+` | P3.1 sizeof / pointer-safe overlay fixes | Core |
| `docs/PATCHES.md` | Series table + upstream notes | Core |
| `port/CMakeLists.txt` | Add field/battle/menus/… object libs as brought up | Core |
| `port/overlays/*` | Overlay table entries for FIELDSTG, stages, FIGHTSTG, menus | Core |
| `platforms/pc/` | Host boot path past title into new game / field | Core |
| `docs/ROADMAP.md` | Tick P3.x; Status/Tracking | Core |
| `docs/P3_AUDIO.md` (or similar) | P3.6 investigation | Audio |
| `tools/compare_harness/*` | DuckStation vs PC screenshot script scaffold | Harness |
| `docs/REFERENCE_COMPARE.md` | Side-by-side review page | Harness |

---

### Task C0: Branch + worktree + plan on remote

**Files:** this plan; `docs/ROADMAP.md` (Phase 3 Status / Tracking).

- [x] **Step 1:** `phase3-pc-bringup` from `main` @ Phase 2 merge; worktree `.worktrees/phase3-pc-bringup`.
- [x] **Step 2:** Commit plan + “Phase 3 in progress”; push `-u origin phase3-pc-bringup`.
- [x] **Step 3:** Docs-only sync on `main` (plan file + ROADMAP Status/Tracking).

---

### Task C1: P3.1 64-bit audit, part 2 (overlays + stages)

**Files:** `port/patches/0018-*.patch` (and follow-ons as needed); `docs/PATCHES.md`; tick P3.1 in ROADMAP.

**Inventory (from SURVEY / current tree; verify while patching):**

1. **Literal `createTask` / `createTaskWithId` sizes** in `src/field` (stages + common incs), plus any remaining in `src/battle`, `src/cardgame`, `src/menus`, `src/debug` — replace with `sizeof(Struct)` (and children with `sizeof` / `N * sizeof(Task *)` as appropriate). Known field hotspots include `0x50`/`0x54`/`0x58`/`0x5C`/`0x60`/`0x68` call sites and children literals `8` / `0x1C` / `0x24` / `0x30`.
2. **`stage_select.c`:** `(s32)0x80000000` mode arg (save flag for `MODE_CONTINUE`) → signed-safe constant (e.g. `-0x80000000`) so LP64 initializers stay clean; keep bit-31 meaning for `STGMCARD` (`getModeArg() >> 31`).
3. **Pointer ↔ integer:** e.g. `GFX.vsyncArg = (s32)task` (`battle_mode/cursor.c`), `target = (s32)TASK_REGISTRY.funcs.find(...)` (`field_mode/event.c`) — host-safe storage (widen host fields under `PORT_HOST` and/or pass pointers without truncating).
4. **`(Task *)` returns / assignments** where create helpers already return `Task *` — drop redundant casts only if they silence real warnings; do not churn style-only.

**Verify:**

```bash
tools/apply_patches.sh --check
# Compile the audited translation units (extend port/CMakeLists.txt object libs
# and/or a one-off host compile) with:
#   -Werror=pointer-to-int-cast -Werror=int-to-pointer-cast
```

- [ ] **Step 1:** Grep inventory → patch series for sizeof / stage_select arg / pointer casts.
- [ ] **Step 2:** Build with the cast warnings as errors for field/battle/cardgame/menus/debug TUs that are in the host build (or document remaining TUs not yet linked with evidence).
- [ ] **Step 3:** Update `docs/PATCHES.md`; tick P3.1; commit + push.

**Done when:** those warnings clean for linked overlay dirs, or remaining sites listed with evidence; match-neutral patches marked upstream-bound (upstream PR optional if no MIPS compare — same as P2.4).

---

### Task C2: P3.2 Field + first stage

**Files:** `port/CMakeLists.txt` (FIELDSTG + WSTAG260 + starting town), `port/overlays/` table, host boot → new game → name entry → field; patches as needed.

- [ ] **Step 1:** Link `FIELDSTG` + starting stage scripts; register overlay restore entries.
- [ ] **Step 2:** Drive new game → name entry → walkable first field; door changes stage.
- [ ] **Step 3:** Tick P3.2; commit + push.

**Done when:** new game → name entry → first field walkable; a door changes stage.

---

### Task C3: P3.3 Battle

**Files:** `FIGHTSTG`, `WFIGHTMN` (+ `WFIGHTTS` debug), battle overlays, report `STFGTREP`.

- [ ] **Step 1:** Link battle mode + models/camera/effects stubs as required.
- [ ] **Step 2:** Complete one random encounter and return to field with stats applied.
- [ ] **Step 3:** Tick P3.3; commit + push.

**Done when:** random encounter completes; return to field with stats applied.

---

### Task C4: P3.4 Menus and shops

**Overlays:** `STSTATUS`, `STITSHOP`, `STGTRAIN`, `STGDGLAB`, `STGMCARD`, `STDGNAME`, `STPLNMET`.

- [ ] **Step 1:** Link each overlay; open from field end-to-end.
- [ ] **Step 2:** Saving via `STGMCARD` uses P2.7 memcard backend.
- [ ] **Step 3:** Tick P3.4; commit + push.

**Done when:** each menu/shop works from field; save uses P2.7.

---

### Task C5: P3.5 Card game

**Overlays:** `CARDGAME`, `STCRDDEK`, `STCRDSHP`, `STCRDABM`.

- [ ] **Step 1:** Link card game + deck/shop/album.
- [ ] **Step 2:** Win a card battle; album updates.
- [ ] **Step 3:** Tick P3.5; commit + push.

**Done when:** card battle won; album updates.

---

### Task A1 / C6: P3.6 Audio

**Wave 1 (Audio track):** investigation doc only — VAB banks, SEP music, SFX, reverb, `SsSeqCalledTbyT` vsync timing in PSY-Z.

**Wave 3 (Core):** wire libsnd path; verify title music, field music on stage switch, battle SFX.

- [ ] **Step 1 (A1):** Write investigation notes under `docs/` (no CMake churn).
- [ ] **Step 2 (C6):** Implement/fix host path; tick P3.6; commit + push.

**Done when:** title music, field music change on stage switch, battle SFX.

---

### Task C7: P3.7 Movies

**Files:** PSY-Z `libpress` + streaming (`CdRead2`, `StSetRing`) over packed raw-sector `.STR` from P1.6 layout.

- [ ] **Step 1:** Title STR playback path on host.
- [ ] **Step 2:** Skip works; audio present.
- [ ] **Step 3:** Tick P3.7; commit + push.

**Done when:** opening movies play with audio and can be skipped.

---

### Task C8: P3.8 Stage sweep

**Files:** `STAGSLCT` debug stage select; crash/assert logging.

- [ ] **Step 1:** Boot stage select; script or manual sweep all US stages.
- [ ] **Step 2:** Log and fix load failures until all return to select.
- [ ] **Step 3:** Tick P3.8; commit + push.

**Done when:** all stages load and return to the select screen.

---

### Task H1 / C9: P3.9 Reference comparison harness

**Wave 1 (Harness):** scaffold script + doc outline (no copyrighted screenshots committed).

**Wave 3 (Core):** capture title/field/battle vs DuckStation; write review doc.

- [ ] **Step 1 (H1):** Script stubs + `docs/REFERENCE_COMPARE.md` outline.
- [ ] **Step 2 (C9):** Produce comparison page for title, field, battle (owner disc/oracle).
- [ ] **Step 3:** Tick P3.9; commit + push.

**Done when:** doc page with comparison for title, field, battle.

---

### Task CGATE: Phase 3 gate

- [ ] Full playthrough segment start → first boss on PC without crashes.
- [ ] Both versions build (US required; EU when ready) or document EU blocker.
- [ ] ROADMAP Phase 3 Status → complete; PR to `main`.

**Done when:** gate criteria in `docs/ROADMAP.md` met.
