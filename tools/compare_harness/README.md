# P3.9 — DuckStation vs dw3-pc comparison harness

Side track: branch `phase3-harness` (worktree `.worktrees/phase3-harness`).
Owns only this directory, `docs/P3.9_REFERENCE.md`, and optional synthetic
fixtures under `tools/testdata/compare_harness/`. Does **not** touch game
C, `port/patches/`, or PC bring-up.

PS1 (DuckStation) is the **visual oracle**. `dw3-pc` is the subject.

## Status

| Label | Automation | Notes |
|---|---|---|
| `title` | OK | One Start skips the FMV. Shot is MODE_TITLE (Press Start) @ 200% |
| `field` | Partial | DuckStation: Asuka Inn interior from the owner card. `dw3-pc` hits STGMCARD "Failed to access" (checksum) |
| `battle` | Blocked | Needs mid-fight `--statefile`. Do not invent a PNG |

ROADMAP P3.9 stays open until three reviewed pairs exist. PNGs + `.mcr`
stay under gitignored `build/compare/`.

| Mode | Exit | Meaning |
|---|---|---|
| `--dry-run` | 0 | Print plan; no assets required |
| `--label title` | 0 | Boot + F10 |
| `--label field` + `--save` | 0 | Stage card + auto-load + F10 |
| `--label field\|battle` without `--save`/`--statefile` | 3 | Blocked |

Owner card on this machine:

`~/.local/share/duckstation/memcards/Digimon World 3 (USA)_1.mcd`

→ staged to `build/compare/memcard0.mcr` (never commit).

## Emulation speed (required: 200%)

DuckStation has **no CLI flag** for speed (checked AppImage `-help`,
0.1-12074). The harness always rewrites
`~/.local/share/duckstation/settings.ini` before launch:

| Key | Value | Meaning |
|---|---|---|
| `Main/EmulationSpeed` | `2` | 200%. `1` is 100%. **`0` = uncapped** — do not use |
| `Main/FastForwardSpeed` | `2` | Cap FF at 200% if somehow triggered |
| `Main/TurboSpeed` | `2` | Cap turbo at 200% |
| Hotkey `FastForward` | *(unbound)* | A stray toggle cannot jump to uncapped |

Also: `SetupWizardIncomplete=false`, `ConfirmPowerOff=false`,
`BIOS/PathNTSCU=scph1001.bin`, `Folders/Screenshots` → harness shots dir.

## One-liners (this machine)

```bash
DS=/mnt/windows/Jogos/emuladores/duckstation
CUE="/mnt/windows/Jogos/PS1/Digimon World 3/Digimon World 3.cue"
BIOS="$HOME/.local/share/duckstation/bios/scph1001.bin"
SRC="$HOME/.local/share/duckstation/memcards/Digimon World 3 (USA)_1.mcd"

# Title: one Start, then PRESS START. Sequenced waits ignore --settle-seconds.
python3 tools/compare_harness/capture_pair.py \
  --label title \
  --duckstation "$DS" --disc "$CUE" --bios "$BIOS" --out build/compare

# Field: same card, Continue, leave "Loaded."
python3 tools/compare_harness/capture_pair.py \
  --label field --save "$SRC" \
  --duckstation "$DS" --disc "$CUE" --bios "$BIOS" --out build/compare

# Battle, after the owner drops a savestate (gitignored):
python3 tools/compare_harness/capture_pair.py \
  --label battle --duck-only --no-auto-load \
  --statefile build/compare/battle/owner.state \
  --settle-seconds 3 \
  --duckstation "$DS" --disc "$CUE" --bios "$BIOS" --out build/compare
```

Needs: `DISPLAY`, `xdotool`, `wmctrl`. Pad1 is temporarily rebound to
keyboard (`Enter`/`K`/arrows) during auto-load, then restored.

Dry-run:

```bash
python3 tools/compare_harness/capture_pair.py --dry-run --label field --save "$SRC"
```

## DuckStation CLI (what exists / what does not)

From `duckstation -help`:

| Flag | Use |
|---|---|
| `-batch` | Exit after power-off |
| `-fastboot` | Skip BIOS intro (not game movies) |
| `-nofullscreen` | Windowed |
| `-earlyconsole` | Early log |
| `-- <disc>` | Boot cue/bin (spaces OK after `--`) |
| `-state` / `-statefile` | Load save state (field/battle later) |
| screenshot / quit-after-N-frames | **Not available** — harness uses F10 hotkey |

## Owner prerequisites (not in git)

| Item | Role |
|---|---|
| DuckStation AppImage | Oracle host |
| BIOS `scph1001.bin` under data `bios/` | Own BIOS only — never downloaded by harness |
| US BIN/CUE | Boot path |
| Own disc → `runtime/us/` | PC subject data |
| Shared `memcard0.mcr` | Field/battle same-save |
| `build/pc/dw3-pc` | Subject binary (Core) |

## Same-save contract (field/battle)

1. One `memcard0.mcr` both sides accept.
2. Copy into both run roots before capture.
3. Settled frame per label (title idle / field after load / battle fade-in).

## Output layout (local only)

```
build/compare/<label>/
  duck.png
  pc.png          # optional
  pair.md
  duckstation.log
```

## Blockers (honest)

- **Title:** one Start reaches PRESS START. A second Start opens New Game / Continue. At 200% the 600-frame timeout is about 5s of wall clock once that prompt is up.
- **Field:** DuckStation Continue from the owner card reaches the Asuka Inn interior. `dw3-pc` loads the same `memcard0.mcr` and stops on "Failed to access" (`STGMCARD_loadSave` checksum `got=67 calc=129`).
- **Battle:** blocked until `build/compare/battle/owner.state` exists.

## Related

- ROADMAP **P3.9** (checkbox open until three reviewed captures)
- `docs/P3.9_REFERENCE.md`
- Plan Task H1 / A9
