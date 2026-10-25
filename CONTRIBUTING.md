# Contributing

Short and practical, for people and for AI agents. [AGENTS.md](AGENTS.md) is
the source of truth for this repo; if this page and AGENTS.md ever differ,
AGENTS.md wins. Read it first. This page only points at the rules and lists the
commands. The docs index is [docs/README.md](docs/README.md).

## Never commit game data

Own-disc only. Do not add, commit or upload disc images (`*.bin`, `*.cue`,
`*.iso`), BIOS files, extracted game trees (`romfs/game/`, runtime trees) or
captures of the game's output. Replay references are copyrighted frames and
stay outside git (see Replay tests). Do not commit secrets or keys. Stage
files by name; do not use `git add .` or `git add -A`.

## Repository layout

| Path | What it holds |
|---|---|
| `platforms/pc/` | PC host (PSY-Z with SDL3), builds `dw3-pc` |
| `platforms/switch/` | Switch host only (libnx, present, audio, input, FS) |
| `port/` | Platform-independent glue: CD to file system, overlays, memcard, movies, `patches/` |
| `tools/` | Extract/pack helpers, build scripts, replay runner, `tests/` |
| `docs/` | Design notes for this port ([ROADMAP](docs/ROADMAP.md), [ARCHITECTURE](docs/ARCHITECTURE.md)) |
| `vendor/dw3_decomp/` | Upstream decomp (git submodule). Treat as upstream, never edit it |
| `vendor/psyz/` | PSY-Z, the Psy-Q shim (git submodule, our fork) |

Put Switch-specific code under `platforms/switch/` and reusable glue under
`port/`. Docs change in the same commit as the roadmap step they describe
(AGENTS.md, "Documentation must track roadmap progress").

## Build

Clone with only the submodules the hosts need. Do not initialize nugget
recursively ([docs/CI_SUBMODULES.md](docs/CI_SUBMODULES.md)).

```bash
git submodule update --init vendor/dw3_decomp vendor/psyz
git -C vendor/psyz submodule update --init nugget external/SDL
```

You need your own disc, packed into a runtime tree:
[docs/DISC_SETUP.md](docs/DISC_SETUP.md).

PC build (the loop to verify changes; run it on a virtual display such as
`xvfb-run`, never on your desktop session):

```bash
cmake -S platforms/pc -B build/pc -G Ninja -DPSYZ_RENDERER=sdl3_gpu
cmake --build build/pc -j"$(nproc)"
```

Switch build (devkitPro, the `devkitpro/devkita64` image; CI builds the same):

```bash
bash tools/build_switch_game.sh           # build/switch/dw3-switch.nro
bash tools/build_switch_game.sh --eu      # EU build
```

Every change is tested on both discs, US and EU. The EU PC build uses its own
tree, see [docs/P3_EU_BUILD.md](docs/P3_EU_BUILD.md). Build trees and
temporary files go under `/mnt/cabal/tmp` on the owner's machine. Do not
overclock the console (AGENTS.md, technical directive 7).

## Tests without a disc

```bash
python3 -m pytest tools/tests -q
python3 tools/check_doc_links.py            # docs index and links
```

`tools/tests/test_doc_links.py` runs the link checker over every markdown file,
so a broken relative link or anchor fails CI.

## Decomp patches (PORT_HOST)

The decomp is never edited in place. Changes to it are numbered patches in
`port/patches/`, applied to a build copy by `tools/apply_patches.sh`
(full policy: [docs/PATCHES.md](docs/PATCHES.md)).

1. Take the next free number. Do not reuse one and do not add a duplicate
   (`0051` already has two for historical reasons).
2. Name it `NNNN-PORT_HOST-<topic>.patch` (port-only) or
   `NNNN-match-neutral-<topic>.patch` (intended for upstream). Start the
   header with `Upstream-bound: yes` or `Upstream-bound: no`.
3. Guard port-only code with `PORT_HOST`. Test hooks must do nothing when
   their environment variable is unset.
4. Add a row to the Series table in [docs/PATCHES.md](docs/PATCHES.md) in the
   same change. `tools/tests/test_patch_series.py` fails on a missing row or a
   new duplicate number.
5. Run `tools/apply_patches.sh --check` (pin, clean submodule, dry-run apply).
   The submodule must stay clean; `port/patches/DECOMP_PIN` holds its expected
   commit. A submodule update is intentional and called out in the commit
   message.

## Replay tests

`tools/replay_tests.sh` replays recorded pad input on the US and EU builds and
diffs checkpoint screenshots against references, with separate crash, hang and
memory verdicts. It needs `Xvfb`, ImageMagick `import` and `python3-pil`, and
your own runtimes. Details: [docs/P6.15_REPLAY_TESTS.md](docs/P6.15_REPLAY_TESTS.md).

```bash
tools/replay_tests.sh --quick                 # title menus only, about 1 minute
tools/replay_tests.sh --only us-field-walk    # one scenario
tools/replay_tests.sh --list
```

References are captures of copyrighted output. They live outside git, by
default in `~/.cache/dw3-switch/replay-refs`, and `--update` refuses a
reference directory inside the repository. Never commit them; only `.rec`
recordings and scripts are committed.

Work with your own private copy of the references while you iterate: point
`DW3_REPLAY_REFS` (or `--refs DIR`) at a directory of your own, and write new
references there with `--update`. Copy your references into the shared cache
(`~/.cache/dw3-switch/replay-refs`) only just before you open the PR, so other
runs on the machine do not compare against references you are still changing.

## PSY-Z (`vendor/psyz`)

The submodule URL is the fork `Larbaco/psyz`; `Xeeynamo/psyz` is the read-only
upstream. Details and the current pin are in [docs/PSYZ.md](docs/PSYZ.md).

- Put each change on a `dw3-<topic>` branch of the fork, one small commit on
  top of the previous pin, and say what it is based on.
- Move the pin intentionally: bump the gitlink in this repo, update the
  Commit row in `docs/PSYZ.md`, and call out the submodule SHA change in the
  commit message.
- A change to `sdl3_software.c` is gated on zero pixel differences against the
  previous revision (see "CPU rasterizer speed" in `docs/PSYZ.md`).

## Pull requests

- Small, reviewable changes on a named topic branch, one per workstream.
  Branch from an up-to-date `main` (`git fetch` first).
- Commit often and push the branch; open a PR against `main`.
- Do not force-push and do not rewrite history unless asked.
- Merging is a squash merge, then the branch is deleted. Keep `release/*`
  branches.
- Say in the PR what was tested (US and EU, method, what was not covered) and
  what is still open. Update `docs/ROADMAP.md` in the same change.
- Releases and their notes: [docs/RELEASES.md](docs/RELEASES.md).
