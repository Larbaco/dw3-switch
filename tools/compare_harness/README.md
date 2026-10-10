# P3.9 — DuckStation vs dw3-pc comparison harness

Side track: branch `phase3-harness` (worktree `.worktrees/phase3-harness`).
Owns only this directory, `docs/P3.9_REFERENCE.md`, and optional synthetic
fixtures under `tools/testdata/compare_harness/`. Does **not** touch game
C, `port/patches/`, or PC bring-up.

PS1 (DuckStation) is the **visual oracle**. `dw3-pc` is the subject.

## Status

Automated **boot/oracle capture** works for `title` (DuckStation + own disc
+ BIOS). Field/battle still need a shared `.mcr` or `-statefile` + pad;
ROADMAP P3.9 stays open until all three labels have reviewed pairs.

| Mode | Exit | Meaning |
|---|---|---|
| `--dry-run` | 0 | Print plan; no assets required |
| `--label title` (live) | 0 | BIOS check + boot + F10 → `build/compare/title/duck.png` |
| `--label field\|battle` without `--save` | 3 | Blocked (no settled scene) |

Screenshots stay under gitignored `build/compare/` (never commit game PNGs).

## Emulation speed (required: 100%)

DuckStation has **no CLI flag** for speed (checked AppImage `-help`,
0.1-12074). The harness always rewrites
`~/.local/share/duckstation/settings.ini` before launch:

| Key | Value | Meaning |
|---|---|---|
| `Main/EmulationSpeed` | `1` | Normal realtime (100%). **`0` = uncapped** (thousands %) — do not use |
| `Main/FastForwardSpeed` | `1` | Cap FF at 100% if somehow triggered |
| `Main/TurboSpeed` | `1` | Cap turbo at 100% |
| Hotkey `FastForward` | *(unbound)* | Tab cannot race the settle window |

Also: `SetupWizardIncomplete=false`, `ConfirmPowerOff=false`,
`BIOS/PathNTSCU=scph1001.bin`, `Folders/Screenshots` → harness shots dir.

## One-command owner/agent recipe (unattended title/boot)

From this worktree (defaults match the owner machine paths):

```bash
python3 tools/compare_harness/capture_pair.py \
  --label title \
  --duck-only \
  --settle-seconds 20 \
  --out build/compare
```

Explicit paths:

```bash
python3 tools/compare_harness/capture_pair.py \
  --label title \
  --duck-only \
  --duckstation /mnt/windows/Jogos/emuladores/duckstation \
  --disc "/mnt/windows/Jogos/PS1/Digimon World 3/Digimon World 3.cue" \
  --bios "$HOME/.local/share/duckstation/bios/scph1001.bin" \
  --settle-seconds 20 \
  --out build/compare
```

Needs: `DISPLAY` + `xdotool` (F10 → native screenshot). Optional PC pair
omits `--duck-only` and passes `--pc-bin` / `--runtime` when present.

Dry-run (no GUI):

```bash
python3 tools/compare_harness/capture_pair.py --dry-run --label title
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

- **Title settle:** opening movies may still be playing after N seconds at
  100%; pad skip or save state needed for true `MODE_TITLE`.
- **Field / battle:** need shared `.mcr` or DuckStation `-statefile`.
- **PC pair:** best-effort window grab when `dw3-pc` + runtime exist.

## Related

- ROADMAP **P3.9** (checkbox open until three reviewed captures)
- `docs/P3.9_REFERENCE.md`
- Plan Task H1 / A9
