# Phase 2 — PC Engine Bring-up Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Compile the decomp's US `src/engine` (+ title overlays needed for boot) on x86_64 under PSY-Z, with a patch-copy build, cdfs by file id, memcard `.mcr` files, and a static overlay restore model — until the title screen runs on PC.

**Architecture:** Never edit `vendor/dw3_decomp` in place. `tools/apply_patches.sh` copies the submodule into `build/decomp-src/` and applies `port/patches/*.patch`. `port/CMakeLists.txt` compiles that tree with GCC 2.8.1-semantics flags + `VERSION_US` + `PORT_HOST`. Host I/O lives in `port/cdfs` and `port/memcard`; overlay load becomes restore-initial-`.data` / zero-`.bss` in `port/overlays`. PC host links engine + glue + PSY-Z.

**Tech Stack:** CMake ≥ 3.21, Ninja, PSY-Z (`vendor/psyz` @ current pin), GCC (host), Python 3.12 + pytest, GitHub Actions.

## Parallelization (read this first)

SDD forbids two implementers on the **same** worktree (merge conflicts). Phase 2 therefore uses **two worktrees / branches** that own disjoint paths, then a merge wave.

| Track | Branch / worktree | Owns | ROADMAP items |
|---|---|---|---|
| **Engine** | `phase2-engine` → `.worktrees/phase2-engine` | `port/CMakeLists.txt`, `port/patches/`, `port/overlays/`, `tools/apply_patches.sh`, `docs/PATCHES.md`, `platforms/pc/` (link + flags), CI patch-check | P2.1, P2.2, P2.3, P2.4, P2.6, then P2.8 after merge |
| **I/O** | `phase2-io` → `.worktrees/phase2-io` | `port/cdfs/`, `port/memcard/`, their unit tests under `tools/tests/`, notes in `port/README.md` for those dirs only | P2.5, P2.7 |

**Wave 0 (coordinator, once):** create both branches from `main`, land this plan + ROADMAP “Phase 2 in progress” on `phase2-engine`, merge that commit into `phase2-io`.

**Wave 1 (parallel — two implementers):**

1. Engine Task E1 = P2.2 patch mechanism (empty series + CI `--check`).
2. I/O Task I1 = P2.5 cdfs (file-id read + unit test with synthetic fixture; real-disc test skipped in CI).

**Wave 2 (parallel after Wave 1 reviews):**

1. Engine Task E2 = P2.1 source list + flags; compile `src/engine/**/*.c` objects (expect MIPS failures → stubs via next task).
2. I/O Task I2 = P2.7 memcard 128 KiB `.mcr` backend + round-trip test (synthetic card image in testdata; DuckStation check is manual/owner).

**Wave 3 (Engine only, sequential):** E3=P2.3 MIPS replacements → E4=P2.4 64-bit engine audit → E5=P2.6 static overlays.

**Wave 4 (integration on `phase2-engine`):** merge `phase2-io` → wire cdfs/memcard into CMake → E6=P2.8 boot to title → Phase 2 gate + ROADMAP checkboxes → PR.

**Do not parallelize:** two agents editing `platforms/pc/CMakeLists.txt`, `port/CMakeLists.txt`, or `docs/ROADMAP.md` at once. ROADMAP updates ride with the task that completes the matching P2.x item (engine track owns ROADMAP; I/O agent mentions which boxes to tick in the commit message; coordinator ticks P2.5/P2.7 on merge if needed).

## Global Constraints

- Branches: `phase2-engine` (integration + PR) and `phase2-io` (merged into engine before P2.8). Never commit Phase 2 gate work straight to `main`.
- Version: **US** only this cycle (`-DVERSION_US=1`, `VERSION=us`).
- Own-disc only; never commit `disks/`, `runtime/`, `*.bin` disc images, or extracted trees. Synthetic fixtures under `tools/testdata/` are fine.
- Patches apply to a **build copy** only; `vendor/dw3_decomp` stays clean (`git -C vendor/dw3_decomp status` clean after every task).
- Match-neutral engine fixes: prefer `port/patches/` first; open upstream PR later (note in `docs/PATCHES.md`). Port-only changes stay port-only.
- Portuguese in chat with owner; English in code/commits.
- Stage paths explicitly; no `git add -A`.
- Commit + push after each task (project default).
- Follow `AGENTS.md`, `docs/ROADMAP.md` Phase 2, `docs/SURVEY.md`, `docs/ARCHITECTURE.md`, `docs/PSYZ.md`.

## File map (create / modify)

| Path | Role | Track |
|---|---|---|
| `docs/PATCHES.md` | Patch policy (upstream-bound vs port-only) | Engine |
| `port/patches/*.patch` | Ordered quilt-style series | Engine |
| `tools/apply_patches.sh` | Copy submodule → `build/decomp-src`, apply, `--check` | Engine |
| `port/CMakeLists.txt` | Engine (+ later overlays) object library | Engine |
| `port/host_compat.h` (or similar) | `PORT_HOST` helpers if needed outside patches | Engine |
| `port/overlays/*` | Prefix table, loadModeOverlay/loadSubOverlay restore | Engine |
| `port/cdfs/*` | file-id → host path read path | I/O |
| `port/memcard/*` | 128 KiB PS1 card image per slot | I/O |
| `platforms/pc/CMakeLists.txt` | Link port + PSY-Z; define `PORT_HOST` | Engine |
| `platforms/pc/src/main.c` | Eventually call game `main()` / boot | Engine |
| `tools/tests/test_apply_patches.py` or shell CI step | `--check` round-trip | Engine |
| `tools/tests/test_cdfs.py` | Synthetic id map + bytes | I/O |
| `tools/tests/test_memcard.py` | Read/write `.mcr` slots | I/O |
| `.github/workflows/ci.yml` | Add `phase2-*` branches; run patch `--check` | Engine |
| `docs/ROADMAP.md` | Tick P2.x as done | Engine (+ merge) |
| `docs/ARCHITECTURE.md` | Reflect patch-copy + cdfs/memcard/overlays | Engine (gate) |

---

### Task E0: Branch + worktrees + plan on remote

**Files:** this plan; `docs/ROADMAP.md` (Phase 2 status line only).

- [ ] **Step 1: Create branches and worktrees from `main`**

```bash
MAIN=/mnt/windows/Jogos/dw3-switch
git -C "$MAIN" checkout main && git -C "$MAIN" pull --ff-only origin main
mkdir -p "$MAIN/.worktrees"
git -C "$MAIN" branch phase2-engine
git -C "$MAIN" branch phase2-io
git -C "$MAIN" worktree add "$MAIN/.worktrees/phase2-engine" phase2-engine
git -C "$MAIN" worktree add "$MAIN/.worktrees/phase2-io" phase2-io
```

- [ ] **Step 2: Commit plan + “Phase 2 in progress” on `phase2-engine`, merge into `phase2-io`, push both**

Expected: both remotes track; worktrees clean.

---

### Task E1 (Wave 1): P2.2 Patch mechanism

**Files:**
- Create: `docs/PATCHES.md`, `port/patches/.gitkeep` (or `series` file), `tools/apply_patches.sh`
- Modify: `.github/workflows/ci.yml` (branches + check step)
- Modify: `docs/ROADMAP.md` (tick P2.2 when done)

**Interfaces:**
- Consumes: `vendor/dw3_decomp` at pinned submodule SHA
- Produces: `build/decomp-src/` (gitignored) identical to submodule when series empty; `--check` exits 0

- [ ] **Step 1: Write `docs/PATCHES.md`**

Policy must state: apply only to build copy; every patch header declares `Upstream-bound:` yes/no; never commit dirty submodule; `--check` fails if submodule SHA ≠ recorded pin file `port/patches/DECOMP_PIN` (or read from `.gitmodules` / `git rev-parse`).

- [ ] **Step 2: Implement `tools/apply_patches.sh`**

```bash
# Usage:
#   tools/apply_patches.sh          # copy + apply into build/decomp-src
#   tools/apply_patches.sh --check  # verify pin + patches apply cleanly (no leave tree required)
```

Behavior:
1. Resolve repo root; `SRC=vendor/dw3_decomp`; `DST=build/decomp-src`.
2. `--check`: verify `git -C "$SRC" rev-parse HEAD` matches `port/patches/DECOMP_PIN`; `git -C "$SRC" status --porcelain` empty; dry-run apply onto a temp copy or `git apply --check`.
3. Default: `rm -rf "$DST"`; copy (rsync or `cp -a`) excluding `.git`; apply patches in sorted order from `port/patches/*.patch`.
4. Empty series must succeed.

- [ ] **Step 3: Record pin + gitignore `build/`**

Write `port/patches/DECOMP_PIN` with current submodule commit. Ensure root `.gitignore` has `/build/`.

- [ ] **Step 4: CI runs `--check`**

Add `phase2-engine`, `phase2-io` to `on.push.branches`. After checkout+submodules, run `tools/apply_patches.sh --check`.

- [ ] **Step 5: Commit + push**

```bash
git commit -m "$(cat <<'EOF'
feat: add decomp patch-copy apply script and policy

EOF
)"
git push -u origin HEAD
```

- [ ] **Step 6: Tick P2.2 in ROADMAP** (same or follow-up commit).

**Done when:** empty series round-trips; CI runs `--check`; submodule stays clean.

---

### Task I1 (Wave 1, parallel with E1): P2.5 CD → filesystem (core)

**Files:**
- Create: `port/cdfs/cdfs.h`, `port/cdfs/cdfs.c`, `port/cdfs/README.md`
- Create: `tools/testdata/cdfs_mini/` (tiny files + `files_mini.json`)
- Create: `tools/tests/test_cdfs.py` (or a small C test driven by CMake later — Python wrapping a CLI is OK for Wave 1)
- Optional helper: `tools/cdfs_read_id.py` for tests

**Interfaces:**
- Consumes: JSON map shaped like `files_us.json` (`id` → relative path + size); root dir of packed runtime
- Produces: `cdfs_read(id, buf, buflen)` / `cdfs_file_size(id)` returning host file bytes; async completion can be sync-complete for PC Wave 1 (`cdReadyCallback` invoked before return or on `cdfs_poll`)

Semantics (from SURVEY / engine):
- Engine uses numeric file ids and sector counts; packed runtime already materializes one file per id.
- Do **not** reimplement PSY-Z `libcd` STR path here; only the file-id bulk read used by `FILE_CACHE` / `CD_READER.read`.
- Keep API host-agnostic (no SDL); PC will call `cdfs_set_root("runtime/us")` at startup.

- [ ] **Step 1: Synthetic fixture + failing test**

Mini map with 2–3 ids; files with known bytes; test expects byte-identical read.

- [ ] **Step 2: Implement minimal cdfs**

- [ ] **Step 3: Document owner test** (skip in CI): compare `FILE_CACHE.load(id)` / tool read vs disc for a few ids when `runtime/us` exists.

- [ ] **Step 4: Commit + push on `phase2-io`**

**Done when:** unit test passes without real disc; README states CI skip for owner data.

---

### Task E2 (Wave 2): P2.1 Source selection and flags

**Files:**
- Create: `port/CMakeLists.txt`, maybe `port/sources_engine.cmake` generated or GLOB under `build/decomp-src/src/engine`
- Modify: `platforms/pc/CMakeLists.txt` to `add_subdirectory` port as objects (or OBJECT lib) **without** requiring link of full game yet
- Modify: `docs/ROADMAP.md` P2.1

**Flags** (from `vendor/dw3_decomp/mk/toolchain/gcc.mk`, host-relevant subset):

```
-std=gnu89 -fsigned-char -fno-builtin -fno-strict-aliasing -fwrapv
-fno-delete-null-pointer-checks -fno-aggressive-loop-optimizations
-fno-zero-initialized-in-bss -fno-common -fno-toplevel-reorder
-DVERSION_US=1 -DPORT_HOST=1
-Werror=implicit-function-declaration -Werror=int-conversion
```

Includes (order matters):
1. PSY-Z / port host headers that satisfy Psy-Q names
2. `build/decomp-src/include`
3. decomp psyq_headers as needed

- [ ] **Step 1: `apply_patches.sh` then CMake object library `dw3_engine`** listing `build/decomp-src/src/engine/**/*.c` (exclude asm-only units).

- [ ] **Step 2: Custom command depends on patches; build dir uses copied tree.**

- [ ] **Step 3: Build until the failure list is only MIPS/asm/`mtc2`/pointer issues (document in commit message). If clean compile achieved only after E3/E4, leave P2.1 checked when objects compile with the two `-Werror=` flags.

- [ ] **Step 4: Commit + push**

**Done when:** `src/engine/**/*.c` compiles to objects on x86_64 with those `-Werror=` flags (MIPS patched out via E3 as needed — if E2 is blocked on asm, finish E3 stubs first then return to tick P2.1).

---

### Task I2 (Wave 2, parallel with E2): P2.7 Memory card → file

**Files:**
- Create: `port/memcard/memcard_host.c`, `port/memcard/memcard_host.h`, `port/memcard/README.md`
- Create: `tools/testdata/memcard/empty_slot.mcr` (128 KiB zero or formatted header)
- Create: `tools/tests/test_memcard.py`

**Interfaces:**
- Back PSY-Q `MemCard*` surface used by `src/engine/memcard/memcard.c` **or** provide a thin host backend that PSY-Z `libmcrd` can be pointed at — prefer whatever PSY-Z already exposes for host card images (see `vendor/psyz/samples/etc_card`). Inspect before inventing a second API.
- On-disk: `memcard0.mcr` / `memcard1.mcr`, 128 KiB each, PS1 interleaved format so DuckStation can import.

- [ ] **Step 1: Inspect PSY-Z libmcrd host hooks; implement file backend.**
- [ ] **Step 2: Unit test read/write known bytes at a sector offset.**
- [ ] **Step 3: README notes owner DuckStation round-trip (manual).**
- [ ] **Step 4: Commit + push on `phase2-io`**

**Done when:** automated test passes; manual DuckStation check documented as owner follow-up if not available in CI.

---

### Task E3: P2.3 Replace MIPS-only code

**Files:** patches under `port/patches/` (preferred), not in-place vendor edits.

Targets (from SURVEY):
- `SetSpadStack` / `ResetSpadStack` → no-ops under `PORT_HOST` (`task/registry.c`)
- `allocMemHigh` returns the pointer (`heap.c`)
- `OVERLAY_ADDRESS` / `SUB_OVERLAY_ADDRESS` → host symbols (buffers or externs provided by `port/overlays`)
- Boot TIM read from packed exe tail / host file instead of `SUB_OVERLAY_ADDRESS` TIM hack
- `include/engine/gte.h` GTE macros → PSY-Z software GTE under `PORT_HOST`

- [ ] **Step 1: Grep `__asm__` / `mtc2` in engine; one patch per concern where possible.**
- [ ] **Step 2: Rebuild engine objects; `grep -r mtc2` on preprocessed engine output empty.**
- [ ] **Step 3: Update `docs/PATCHES.md` series table; tick P2.3; commit + push.**

**Done when:** no `__asm__` in host build of engine; preprocessed `mtc2` empty.

---

### Task E4: P2.4 64-bit audit (engine)

**Files:** patches for `src/engine` only.

- Literal sizes → `sizeof` (`HEAP.zero(&GFX.prim, 0x18)`, `PAD_DATA_SIZE`, `MemBlock` constants)
- `freeFilesFrom` takes offset/symbol not raw PS1 `u32` address compare
- Remove pointer↔`u32` casts in engine

- [x] **Step 1: Build with `-Wpointer-to-int-cast -Wint-to-pointer-cast` and fix until clean for engine.**
- [x] **Step 2: Note upstream-bound patches in `docs/PATCHES.md`.**
- [x] **Step 3: Tick P2.4; commit + push.**

**Done when:** those warnings clean for `src/engine`. (Upstream `make compare` is optional if no MIPS toolchain here — note blocker rather than blocking the port.)

---

### Task E5: P2.6 Static overlay model

**Files:** `port/overlays/` — table `{file id → entry, data start/end}`, `loadModeOverlay` / `loadSubOverlay` replacements, symbol-prefix strategy (objcopy `--prefix-symbols` **or** generated headers). Phase 2 minimum: engine + `STDWTITL` (+ `CNTY_SEL` only if required for US — US may skip country select).

- [x] **Step 1: Document overlay list needed for title boot in `port/overlays/README.md`.**
- [x] **Step 2: Implement restore-`.data` / zero-`.bss` when id changes; no-op reload when same id.**
- [x] **Step 3: Host test: two different “overlays” (can be fixtures) + same twice.**
- [x] **Step 4: Tick P2.6; commit + push.**

**Done when:** host test proves reset vs preserve semantics.

---

### Task M1 (Wave 4): Merge I/O → engine

```bash
cd "$MAIN/.worktrees/phase2-engine"
git fetch origin
git merge origin/phase2-io   # or local phase2-io
# fix conflicts; wire port/cdfs + port/memcard into port/CMakeLists.txt
```

- [ ] Tick P2.5 / P2.7 in ROADMAP if not already.
- [ ] Commit merge; push `phase2-engine`.

---

### Task E6: P2.8 Boot to title on PC

**Files:** `platforms/pc/src/main.c`, CMake link line, runtime path init, maybe skip movies via patch.

- [x] **Step 1: Link `dw3_engine` + title overlay objects + cdfs + memcard + PSY-Z.**
- [x] **Step 2: Init cdfs root to `runtime/us` (env or argv); init memcard paths.**
- [x] **Step 3: Call game entry; boot TIM then title (`STDWTITL_tickTitle`); pad path in menu.**
- [x] **Step 4: Update docs; tick P2.8 + Phase 2 gate; commit + push.**

**Done when:** title screen on PC from decomp C; no dirty files inside `vendor/`.
**Status:** done — see `docs/P2.8_TITLE_BOOT.md` (sprite glitches = follow-up).

---

### Task G1: Phase 2 gate + PR

- [ ] All P2.1–P2.8 checkboxes honest in `docs/ROADMAP.md`.
- [ ] CI green on `phase2-engine`.
- [ ] Open PR into `main` (same process as Phase 1).

---

## Self-review (plan author)

1. **Spec coverage:** P2.1–P2.8 mapped to E1–E6 + I1–I2 + M1 + G1.
2. **Placeholder scan:** none intentional; DuckStation round-trip is explicitly manual.
3. **Type consistency:** cdfs API named in I1; overlays consume host symbols from E3.
4. **Parallelism:** two worktrees; Waves 1–2 parallel; 3–4 sequential on engine after merge.
