# P3.9 — DuckStation vs dw3-pc comparison harness

Side track: branch `phase3-harness` (worktree `.worktrees/phase3-harness`).
Owns only this directory, `docs/P3.9_REFERENCE.md`, and optional synthetic
fixtures under `tools/testdata/compare_harness/`. Does **not** touch game
C, `port/patches/`, or PC bring-up.

PS1 (DuckStation) is the **visual oracle**. `dw3-pc` is the subject.

## Status

Scaffold only. `capture_pair.py` accepts the planned CLI and exits:

| Mode | Exit | Meaning |
|---|---|---|
| `--dry-run` | 0 | Print capture plan; no disc / BIOS / binary needed |
| default | 2 | Capture not implemented yet |

Real captures and ticking ROADMAP P3.9 wait on owner DuckStation + saves.
Do not claim done-when without PNGs under a local (gitignored) out dir.

## Owner prerequisites (not in git)

| Item | Role |
|---|---|
| Own US disc → `runtime/us/` | PC data root (`docs/RUNTIME_LAYOUT.md`, `tools/pack_runtime.py`) |
| DuckStation + US BIN/CUE | Oracle boot; BIOS as DuckStation requires |
| Shared `memcard0.mcr` (128 KiB) | Same slot-0 card for both sides (`port/memcard/README.md`) |
| `build/pc/dw3-pc` | Subject binary from Core (`phase3-pc-bringup`) |
| Optional env | `DW3_START_MODE`, `DW3_PROGRESS`, `DW3_MEMCARD_DIR`, … |

Synthetic empty card for layout checks only:

`tools/testdata/memcard/empty_slot.mcr` (not a gameplay save).

## Same-save contract

1. Export or produce one `memcard0.mcr` both sides accept.
2. Copy it into **both** run roots before capture (do not let one side
   rewrite the card mid-harness unless the label is a save test).
3. Boot to a **settled frame** per label (title idle, field after load,
   battle after fade-in or report — pick one and stick to it).

## How to run (today)

From the repo root (or this worktree):

```bash
# Validate args / print plan — no assets required
python3 tools/compare_harness/capture_pair.py --dry-run \
  --label title \
  --runtime runtime/us \
  --save path/to/memcard0.mcr \
  --duckstation /path/to/duckstation \
  --pc-bin build/pc/dw3-pc

# Future capture (exits 2 until implemented)
python3 tools/compare_harness/capture_pair.py \
  --label field \
  --runtime runtime/us \
  --save runtime/us/memcard/memcard0.mcr \
  --duckstation "$HOME/Applications/duckstation" \
  --pc-bin build/pc/dw3-pc \
  --out build/compare
```

## Output layout (local only)

Never commit screenshots or disc images. Prefer:

```
build/compare/<label>/
  duck.png    # DuckStation oracle
  pc.png      # dw3-pc subject
  pair.md     # settle criterion + human verdict
```

Fill the review tables in `docs/P3.9_REFERENCE.md` with paths/notes; keep
PNGs out of git.

## Capture flow (target)

```
                    shared memcard0.mcr
                            |
            +---------------+---------------+
            v                               v
   DuckStation (oracle)              dw3-pc (subject)
   boot → settle → PNG               boot → settle → PNG
            |                               |
            +---------> build/compare/<label>/
```

1. Pack `runtime/us`, build `dw3-pc`, place shared `.mcr`.
2. Oracle: launch DuckStation, load card / boot game, settle, save `duck.png`.
3. Subject: run `dw3-pc --runtime …` with the same card dir; grab `pc.png`
   (SDL hotkey or future host capture hook).
4. Write `pair.md`; update `docs/P3.9_REFERENCE.md` verdict rows.

## Merge note

This track merges into Core (`phase3-pc-bringup`) before the Phase 3 gate,
then Task A9 fills real captures. Draft PR may target `main` for review
isolation; land via Core if ROADMAP/game trees conflict.

## Related

- ROADMAP **P3.9** (checkbox open until captures)
- `docs/P3.9_REFERENCE.md` — title / field / battle review skeleton
- Plan Task H1: `docs/superpowers/plans/2026-10-10-phase3-pc-game-loop.md`
