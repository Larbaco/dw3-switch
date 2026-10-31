# AGENTS.md — dw3-switch

Read this file before changing anything in `/mnt/windows/Jogos/dw3-switch`.
It is the AI source of truth for this folder. Prefer it over chat history when
they conflict.

Owner context: Thiago (Backend / DevOps). Prefer concise Portuguese in chat;
keep code, commits, and technical identifiers in English.

## Mission

Build a **Nintendo Switch homebrew native port** of Digimon World 3 / Digimon
World 2003, using the matching PS1 decompilation as the game-logic source.

This is **not** an emulator wrapper project and **not** a remake from scratch.

## Source of truth

| Doc | Use for |
|---|---|
| `README.md` | Project overview and layout |
| `docs/ROADMAP.md` | Phased goals; do not skip ahead without reason |
| `docs/ARCHITECTURE.md` | Host / shim / game layering |
| `vendor/dw3_decomp/README.md` | Upstream decomp build and status |
| `vendor/dw3_decomp/docs/` | Binaries, overlays, toolchain facts |

If docs disagree with code, trust code and update the docs in the same change.

### Documentation must track roadmap progress

Do not leave `docs/` (or the layout bits in `README.md` / this file) stale
after a real advance. In the **same commit** (or the immediate follow-up
commit before push) as a roadmap step:

- Tick or rewrite the matching task in `docs/ROADMAP.md` (checkbox, status,
  “done when”, blockers). Mark a phase/gate done only when it actually is.
- Update `docs/ARCHITECTURE.md` when layering, hosts, shims, or loaders
  change.
- Update `README.md` (and the layout block in this file) when directories,
  build entry points, or how-to-run change.
- Add or refresh a short note under `docs/` for non-obvious decisions;
  chat history is not documentation.
- Prefer one small doc touch per advance over a big catch-up later.

## Repository layout (boundaries)

```
platforms/pc/       PC host (PSY-Z / SDL3 bring-up)
platforms/switch/   Switch host only (libnx, present, audio, input, FS)
port/               Platform-independent glue (CD→FS, overlays, memcard)
tools/              Extract/pack helpers for Switch/PC runtime layout
docs/               Design notes for THIS port
vendor/dw3_decomp/  Upstream decomp (git submodule)
vendor/psyz/        PSY-Z Psy-Q shim (git submodule, Phase 1+)
```

Rules:

- Put Switch-specific code under `platforms/switch/`.
- Put reusable glue under `port/`, not inside the Switch tree.
- Do **not** vend game assets into git. `disks/`, `romfs/game/`, `*.bin`,
  `*.cue`, `*.iso` are gitignored on purpose.
  Exception (owner-approved 2026-10-27): original artwork created for this port
  (fan art) may be committed under `assets/` if it has a LICENSE and a
  PROVENANCE.md stating that no extracted or derived game asset (no tracing,
  img2img or upscaling of game images) was used. Anything derived from the
  owner's disc, including upscales, stays out of git.
- Treat `vendor/dw3_decomp/` as upstream. Prefer patches/glue in `port/` or
  `platforms/`. Only edit the submodule when there is a clear upstream-bound
  fix, and say so explicitly.
- Do not rewrite the decomp into a new engine “for cleanliness” unless the
  owner asks for that scope change.

## Technical directives

1. **Shim, don’t reimplement the console.** Prefer **PSY-Z** (Psy-Q compatible)
   over a from-scratch GPU/GTE/SPU stack; PsyCross-style layers only when PSY-Z
   does not cover a needed API.
2. **PC bring-up before Switch polish** when a change is easier to validate on
   desktop; keep the Switch host thin.
3. **No matching-build cargo on Switch.** Byte-identical PS1 output is the
   decomp’s job. The port may use a modern compiler and relocatable modules.
4. **Overlays/stages need an explicit loader story.** Do not assume fixed PS1
   VRAM addresses work as-is on AArch64.
5. **Own-disc only.** Never add, commit, or upload original game files, BIOS,
   or redistributable ROMs. Exception (owner-approved 2026-10-27): original
   fan art for this port may be committed under `assets/` as described in the
   "Do not vend game assets" rule above; anything derived from the disc stays
   out of git.
6. **Keep Phase 0/1 honest.** A minimal `.nro` (boot, framebuffer, pad, log)
   comes before linking game code.
7. **Do not overclock.** Leave the CPU and GPU clocks where the console
   set them. Do not call `clkrstSetClockRate` or `appletSetCpuBoostMode`.
   Performance work stays on the present path, the field-tile pace, and
   the raster.
8. **Every change versus the original game is switchable from the Options
   menu** (owner rule, 2026-10-31). An improvement, quality-of-life change or
   other behaviour difference from the original PS1 game gets an Options row
   (registry row in `port/options/`, `port_options.cfg` key, default, label and
   message line in the five languages), not a file-only key; a feature without
   its row is not done. This includes test, dev and experimental keys: the
   owner decided on 2026-10-31 that `show_version`, `save_store`
   (experimental, with a warning), `stage_sweep`, `map_mod` and the `map_view_*`
   bindings all get a row. **Exempt, with no row (owner, 2026-10-31):**
   (A) crash and 64-bit fixes; (B) host plumbing (pause on focus loss, save
   write-through and backups, per-profile saves, the cross-region import,
   display pacing); (C) the port's own screens (the Options entry, Return to
   title, Quit game, Help, Walkthrough, the version footer, the menu
   background); (D) the original bugs fixed and always on (P7.7 patches `0065`
   and `0066`, the area-name banner, `0187`, `0129`). `DW3_*` environment test
   aids are not options. The exact list is in `docs/PORT_OPTIONS.md` ("Port-wide
   rule") and `docs/OPTIONS_REORG.md` section 1.3; anything not on it needs a
   row, and a change you are unsure about is flagged to the owner in the PR.

## Git and safety

This repo is followed from more than one machine. Keep the **remote** as the
shared timeline — local-only progress is not enough.

- **Commit frequently** after each small, reviewable unit of work (scaffold,
  fix, doc note, stub). Prefer several small commits over one large dump.
- **Push frequently** after those commits: `git push -u origin HEAD` on the
  first push of a branch, then `git push`. Do not leave finished work only on
  the local disk between sessions.
- Work on a named feature/topic branch when the change is more than a tiny
  doc tweak; keep that branch on the remote so another PC can `git fetch` /
  `git pull` and continue.
- At session start on any machine: `git fetch` / `git pull` (or rebase onto
  the tracking branch) before editing, so you do not diverge from what was
  pushed elsewhere.
- Do not force-push, or rewrite history unless asked.
- Do not change git config.
- Stage files explicitly; do not `git add .` / `git add -A`.
- Do not commit secrets, keys, disc images, or extracted game trees.
- Submodule updates (`vendor/dw3_decomp`) must be intentional and called out
  in the commit message.
- For this project, frequent commit + push is the default (no need to wait for
  an explicit “commit/push” prompt each time), unless the owner says to hold.

## Owner's machine rules (every agent, every harness)

These hold for any agent working on the owner's desktop, whatever the tool
(Claude Code, Cursor, Codex, Gemini, Copilot, OpenCode, ...):

1. **Git through the terminal.** The desktop's own `git` has push access to
   the GitHub remotes. When a harness's GitHub integration cannot push or
   open a PR, use the terminal `git` instead of stopping.
2. **Builds and temporary files go in `/mnt/cabal/tmp`** (worktrees as
   `/mnt/cabal/tmp/dw3-<topic>`), not `/tmp` or the NTFS checkout.
3. **Evidence goes in `/mnt/windows/Jogos/dw3-switch/tmp/evidence`**:
   screenshots, logs and captures from test runs (git-ignored; never commit
   game frames that contain copyrighted art to the repo). The only exception
   is the small, owner-approved set of README screenshots in
   `docs/assets/screenshots/`; do not add to it without the owner's OK.
4. **GUI runs use a virtual display** (Xvfb or similar, e.g.
   `xvfb-run -s "-screen 0 1920x1080x24"`), never the owner's desktop
   session, so tests do not get in the way of the owner's work. This
   includes the PC build and Ryubing.
5. **Tests run on one region by default and on both at the gate**
   (`docs/REPLAY_POLICY.md`). The EU build (five languages) comes from
   `tools/build_switch_game.sh --eu`. A PR runs `tools/replay_tests.sh
   --changed`: the affected scenarios on **EU** (the superset: languages, the
   extra chapter, 50 Hz), plus a small US+EU canary (boots, stage registries,
   cross-region saves), and **both regions** where the changed files are
   region-sensitive (saves, boot, region, overlays, text, fonts, anything that
   mentions `VERSION_EU/US` or `dw3_region`; the runner decides and prints why).
   Both ISOs still apply to the build (US and EU must compile), to the
   release/preview gate (the full tier on both, `tools/replay_tests.sh --jobs 2`)
   and to anything the owner asks. A change that only works on US is not done.
6. **Merge with squash by default, then delete the branch.** If the cloud
   harness cannot delete it, do it from the owner's terminal. Keep `release/*`
   branches as history; never delete them.
7. **Releases say whether the EU build ships.** Attach the EU `.nro` too, or
   say why it is not attached. The release README lists only files that were
   actually published.
8. **Test reports state the method**: emulator or OLED, display used, how the
   game was driven, and how long it was played; the paths of screenshots and
   logs; and what was not covered (for example, untested EU languages).
9. **Crashes**: before running again, copy `crash_report.txt` and the logs into
   the evidence folder with date and time, so one crash does not overwrite the
   next. Note what was being done at the time.
10. **Repositories**: the project uses `Larbaco/dw3-switch` and the renderer
    fork `Larbaco/psyz` (PSY-Z).
11. **Codex PRs**: when a PR is opened or merged, record in `docs/ROADMAP.md`
    or in the PR what changed and what is still open, so any agent can see
    where the work stopped.
12. **Logs answer the test plan.** Every OLED test-plan item names the
    `dw3.log` line(s) that prove its result, and new features log their
    user-visible decisions and timings (once per event, not per frame), so the
    owner only has to report feel, looks and preferences. Keep the last runs in
    the log folder (the Switch build rotates `dw3.log` to `dw3.1.log` ...
    `dw3.4.log`, 5 runs); never rely on the owner copying a log between runs.
13. **Build and test speed** (`docs/BUILD_SPEED.md`, `docs/REPLAY_POLICY.md`). Build with
    `tools/build_pc.sh` and `tools/build_switch_game.sh`: both use the shared
    ccache when there is one (`tools/get_ccache.sh` installs it; worktrees hit
    each other's objects when `build/` is a real directory in
    `/mnt/cabal/tmp/dw3-<topic>`; `DW3_NO_CCACHE=1` for a byte-exact release
    build). Before a PR run `tools/replay_tests.sh --changed --jobs 2
    --fail-fast` (one region, the canary, both regions only for
    region-sensitive files; an engine-wide file such as `main.c` runs the area
    sweep). The full tier on both regions (`--changed --escalate full` or
    `tools/replay_tests.sh --jobs 2`) is for a preview or release, when asked, and
    for the occasional engine-wide change you want proven. CI has no game data and
    runs no replay. `tools/cleanup_tmp.py` lists stale
    scratch dirs (dry run; the owner decides what to delete).

## Working style

- Small, reviewable changes aligned to the current roadmap phase.
- Prefer extending existing scaffold paths over inventing parallel trees.
- When blocked on missing hardware/SDK (devkitPro, real Switch), say what is
  missing and leave the tree in a buildable or clearly stubbed state.
  Reflect that blocker in `docs/ROADMAP.md` (or a short `docs/` note) so the
  next session/PC does not rediscover it only from chat.
- Treat doc updates as part of “done”, not a later chore. A feature commit
  that advances the roadmap without touching the relevant docs is incomplete.
- In multi-agent sessions, follow **Orchestration** below: subagents own
  investigation, coding, builds/tests, commits, and PRs; the parent agent
  orchestrates and merges.

## Orchestration (Cursor / multi-agent)

The parent chat agent is the **orchestrator**. It plans, reviews, merges PRs,
and updates the owner. It does **not** run investigations, tests, or coding
itself when a subagent can.

**Subagents** own: investigation, measurement, implementation, local
builds/tests, commits, and opening PRs.

Each coding or investigation workstream uses its **own git branch** and its
**own git worktree** under `/mnt/cabal/tmp/dw3-<topic>` (see **Owner's
machine rules**). Avoid writing build trees on the NTFS checkout at
`/mnt/windows/Jogos/dw3-switch` when Docker/devkitPro builds need a local
filesystem.

Subagents **commit frequently** (small reviewable units) and **push** the
topic branch. When the workstream is done they **open a PR** against `main`.
The orchestrator **reviews and merges** (comment + squash merge is fine when
GitHub blocks self-approve). Do not leave finished work only local.

Reference notes that are not for git may go under
`~/.cursor/projects/mnt-windows-Jogos-dw3-switch/notes/` (cursorignored).

Keep the existing **Git and safety** rules; this section clarifies multi-agent
roles. Frequent commit + push remains the project default unless the owner
says to hold.

Switch game builds on this desktop: use the local Docker image
`devkitpro/devkita64:latest` via `tools/build_switch_game.sh` / `tools/dk.sh`.
Every preview or beta must ship an nxlink `.nro` too.

Do **not** probe TCP 28280 before `nxlink -s` (an empty connect breaks hbmenu
NetLoader with `err=11`). Prefer hbmenu **Y** NetLoader over Sphaira grid for
nxlink upload.

Do not overclock (see technical directive 7).

## Out of scope (unless explicitly requested)

- Shipping or hosting game assets
- Matching decompilation work inside `vendor/dw3_decomp` as the main task
- Official Nintendo SDK / eShop packaging
- Cheats (except the off-by-default test menu the owner asked for,
  ROADMAP P6.20), piracy tooling, or bypassing console security beyond normal
  homebrew (Atmosphere + homebrew `.nro`) assumptions

## Cursor Cloud specific instructions

PC host is the loop to verify here. Switch `.nro` builds stay on GitHub Actions
(`devkitpro/devkita64`); this VM has no Docker daemon.

CI job `switch` is `make -C platforms/switch`, the `host_hello` sample.
The game image is `tools/build_switch_game.sh` (CMake, `boot_main` →
`dw3_game_main`). Pull requests build it as job `switch-game` and upload
an Actions artifact. A GitHub Release is only
`.github/workflows/switch-release.yml` (`workflow_dispatch` or a `v*` tag),
and it is a prerelease. `workflow_dispatch` is listed only once that file
is on `main`. Tag `v0.6.5-preview.1` is the first preview
(8927429-byte game `.nro` plus `README.txt`). It still crops the title
and freezes the movie. The upcoming tag is `v0.6.5-preview.31`; the tags
already published, with their notes, are listed in `docs/RELEASES.md`.
Earlier checkpoints:
`docs/RELEASE_PREVIEW21.md` (STR read-race fix, OLED movie validation) and
`docs/RELEASE_PREVIEW20.md`. The picture baseline remains
`v0.6.5-preview.2` at `8273a91` (display-mode title width and full-height
movie). Do not publish the sample `.nro` as the game.

The game image needs `vendor/psyz/nugget` as well as `external/SDL`, without
`--recursive` on nugget. Missing `nugget/bsdec/bsdec.c` shows up as
"No SOURCES given to target: dw3_movies".

- Default `cc` is Clang 18. It links with GCC 14's libstdc++, so
  `libstdc++-14-dev` must be installed or CMake's C++ compiler check fails.
- There is no `/dev/dri`. `sdl3_gpu` needs `mesa-vulkan-drivers` (lavapipe).
  A good run logs `SDL_GPU with driver vulkan`.
- `git submodule update --init --recursive` fails while codeberg.org returns
  HTTP 429 for `vendor/psyz/nugget/third_party/xmake-psx` (PS1 matching
  toolchain). The PC host only needs `vendor/dw3_decomp` and
  `vendor/psyz/external/SDL`.
- Build and test from the repo root:

```bash
cmake -S platforms/pc -B build/pc -G Ninja -DPSYZ_RENDERER=sdl3_gpu
cmake --build build/pc -j"$(nproc)"
python3 -m pytest tools/tests -v
```

- Run `./build/pc/dw3-pc` with `DISPLAY` set (this desktop uses `:1`) and a
  writable `XDG_RUNTIME_DIR`. The window title is `dw3-pc Phase 2`. Close it
  to quit.

## First actions for a new session

1. Read this file, `README.md`, and `docs/ROADMAP.md`.
2. `git fetch` / sync with the remote tracking branch; check `git status` and
   submodule state. For a coding/investigation workstream, use a fresh branch
   and worktree (see **Orchestration**); do not assume the primary checkout
   is free.
3. Continue the current roadmap phase; do not restart scaffolding.
4. Before ending a workstream (or when leaving a coherent checkpoint), ensure
   roadmap/docs match what landed, then commit, push, and open a PR so the
   orchestrator can review/merge and another PC can pick up from the remote.
