# Build and test speed

How long builds and tests take, where the time goes, and what was done about
it. For agents: see [Agent guidance](#agent-guidance); for the numbers, the
[Measurements](#measurements).

## Quick use

```bash
# Build (ccache is used when it exists; nothing to enable)
tools/get_ccache.sh                 # once, no root: a static ccache in ~/.cache/dw3-switch/bin
tools/build_pc.sh [--eu|--both]     # build/pc, build/pc-eu, build/pc-both (what the replay runner expects)
tools/build_switch_game.sh [--eu|--both] [--nxlink]   # as before; ccache is mounted into the container
DW3_NO_CCACHE=1 tools/build_switch_game.sh            # bypass the cache (byte-exact release build)

# Test
tools/replay_tests.sh --changed --jobs 2 --fail-fast  # pre-PR: only what the changed files can affect
tools/replay_tests.sh --dry-run --changed             # show the selection and stop
tools/replay_tests.sh --jobs auto                     # CI or a dedicated box: half the cores, RAM-guarded

# Housekeeping
tools/cleanup_tmp.py                # dry run: stale scratch dirs in /mnt/cabal/tmp (deletes nothing)
```

## Measurements

Machine: 16 cores, 31 GB, shared with the owner's desktop (load average 3 to
10 during these runs, so wall times move by tens of percent; CPU seconds are the
steadier number). Builds in a worktree on ext4 under `/mnt/cabal/tmp`,
`nice -n 10`, `DW3_BUILD_JOBS=8`, 2026-10-28. "Before" is `origin/main` at
88696d1; "after" is this change.

### Builds

Clean build = fresh build dir and fresh `build/decomp-src`. PC rows are
configure plus build. "Cold" = empty ccache. "Warm" = a different, fresh
worktree that hits objects another worktree cached.

| Step | Before | After, cold cache | After, warm cache |
|---|---|---|---|
| PC US, clean | 24.7 s (92 cpu-s) | 30.0 s (133 cpu-s) | 22.1 s (66 cpu-s) |
| PC US via `tools/build_pc.sh` (only the `dw3-pc` target), clean | | | 19.8 s (60 cpu-s) |
| PC both, clean | 27.1 s (108 cpu-s) | 27.4 s (102 cpu-s) | 24.2 s (91 cpu-s) |
| Switch US, clean | 18.9 s | 21.2 s | 8.3 s |
| Switch both, clean | 19.0 s | 14.7 s (shares the US run's objects) | 13.1 s |
| Switch US, nothing changed | 13.0 s | | 1.0 s |
| Switch US, touch one `port/*.c` | 12.1 s | | 1.8 s |
| Switch US, touch one decomp patch | 12.2 s | | 1.8 s |

The Switch rows are wall time of the whole script (docker start, configure,
compile, link, NRO and NACP checks); the container's CPU is not counted by
`time`. A first run in the NTFS checkout, with `build/` a symlink to ext4 and
a busier machine, measured Switch US at 30 s and Switch both at 27 s.

Replay runner (2 jobs, `--bin-both`): quick tier 32.6 s for 68 checks (6
scenarios); a 12-scenario slice of the full tier 119.8 s for 128 checks. The
runner spends 98% of that in the game processes (sum of process seconds 236 s,
2 jobs, 120 s wall); PNG compare and Xvfb start are noise. Scenario length
varies a lot: that slice averaged 20 s of game time per scenario, the 14 cheat
scenarios of the `--changed` run below averaged 66 s. The full tier has about
250 scenarios (116 US, 128 EU); the ~3 h at 2 jobs the owner sees is about 85 s
per scenario, i.e. the work itself, not overhead. Only parallelism (more jobs)
or running fewer scenarios (`--changed`) shortens it.

`--changed` on a `port/options/cheats.c` edit selects 43 of 273 scenarios (about
17% of the full tier, so roughly 25 to 30 min at 2 jobs instead of ~3 h). Measured
with `--fail-fast`: 14 scenarios ran in 487 s wall (2 jobs), then the run
stopped on `us-field-options-cheats` and skipped 32. That failure is not from
this change: the capture of an unchanged `origin/main` binary differs 2% from
the reference, and that reference (`~/.cache/dw3-switch/replay-refs`, shared by
every agent) was rewritten at 18:00 the same day by someone else's `--update`,
probably against another branch's options UI. Worth knowing: that directory is
shared and mutable, so a failing UI scenario can mean "the refs moved".

### Where the time goes

* **Compile** is not the bottleneck on this machine. A clean Switch build is 747
  game-side TUs, 10 to 20 s with 8 jobs. A clean PC build is 1044 TUs, about 10
  s of compile. Both are -O0/-O2 C on fast cores.
* **PC configure** is 14 to 18 s of the 22 to 30 s: SDL3's CMake feature checks.
  ccache cannot help there, and neither does anything else here without reusing
  a configured build dir (not changed).
* **Decomp patch apply** is 1 s by itself (118 patches, 16 MB). What it cost was
  the side effect: `apply_patches.sh` deleted `build/decomp-src` and re-copied
  it on every configure and again for the build stamp, so all 501 files the
  patches touch got a new mtime and recompiled every time: a no-op Switch
  rebuild took 13 s, and touching a patch or a `port/*.c` took 12 s.
* **Docker** start is 0.5 s; pulling `devkitpro/devkita64` on a GitHub runner is
  inside the 20 to 30 s of the small Switch jobs. A cache of the 2 GB image
  would not beat the pull.
* **Submodules** from a fresh worktree: 21 s for decomp and psyz, 48 s for
  nugget and SDL (network, history included). On CI it is 15 to 87 s per job.
* **Replay** time is game time. Each scenario replays at the game's own pace
  (about 40 fps; only some scenarios set `DW3_TURBO`), so the levers are
  running fewer scenarios, or more at once.

### CI (before this change)

Recent PR runs, `ci.yml`, GitHub-hosted `ubuntu-24.04`: 266 to 298 s wall. Jobs
run in parallel; the critical path is `pc`.

| Job | Duration | Biggest steps |
|---|---|---|
| pc | 254 to 282 s | submodules 22 to 43, deps 14, PC build 45 to 62, both build 52 to 64, unit tests 90 to 116 |
| switch-game-both | 146 to 210 s | submodules 16 to 87, two builds 105 to 122 |
| switch-game | 96 to 168 s | submodules 15 to 85, build 73 to 75 |
| switch-launcher | 28 to 38 s | |
| switch | 28 to 33 s | |
| changes | 8 to 11 s | |

See [CI after](#ci-after) for what it is now.

## What changed

### ccache

`tools/cmake/dw3_ccache.cmake` makes ccache the compiler launcher for the PC
host and the Switch image when ccache is on `PATH`
(`-DDW3_CCACHE=OFF` or `DW3_NO_CCACHE=1` turns it off, `-DDW3_CCACHE=ON` fails
if it is missing). `tools/dw3_ccache_env.sh` sets the cache up for the build
scripts:

* One cache per user, shared by every worktree: `~/.cache/dw3-switch/ccache`
  (`DW3_CCACHE_DIR`), 10 GB cap, compressed. A full PC plus Switch set is about
  40 MB (PC US and both, Switch US and both).
* `CCACHE_BASEDIR` is the worktree and `CCACHE_NOHASHDIR=1`, so paths in the
  command line stop naming the worktree. **Two worktrees hit each other's
  objects when `build/` is a real directory inside the worktree**, which is
  the `/mnt/cabal/tmp/dw3-<topic>` layout of AGENTS.md. The NTFS checkout with
  a `build -> /mnt/cabal/tmp/...` symlink still gets hits on its own rebuilds,
  but not from other worktrees (the relative path from the build dir to the
  sources names the worktree).
* SDL3 compiles with a precompiled header, so `CCACHE_SLOPPINESS` includes
  `pch_defines,time_macros`; without them 310 of the 1044 PC calls were not
  cacheable.
* `tools/build_switch_game.sh` mounts the ccache binary and the cache dir into
  the devkitPro container. The binary must be static: `tools/get_ccache.sh`
  downloads the pinned ccache 4.14.2 musl build (sha256 checked) to
  `~/.cache/dw3-switch/bin`, no root. `DW3_CCACHE_FETCH=1` lets the build
  script call it by itself (CI does).

What a cache hit changes in the output: the compiler is run on the same
preprocessed input, so the code is the same, but ccache's base-dir rewriting
makes `__FILE__` the build-dir relative path (`../../platforms/switch/...`)
instead of the absolute one. That string only reaches log and crash-notice
text, and it makes the NRO/ELF the same bytes in every worktree (checked: two
worktrees, one hit, one miss, cmp-identical NRO and ELF). A build with
`DW3_NO_CCACHE=1` is byte-identical to a build before this change. Release
builds (`switch-release.yml`) do not use ccache.

### Incremental decomp patches

`tools/apply_patches.sh` stamps `build/decomp-src/.dw3-apply-hash` with a hash
of the pin, the patches and the script. If the hash matches and no file in the
copy was edited since, it does nothing. If the inputs changed, it builds the new
tree beside the old one and syncs by content (`rsync -c`), so only files whose
bytes changed get a new mtime. `--force` redoes everything. An edited
`build/decomp-src` file is restored on the next run (as before), but its mtime
is new, so it rebuilds. Tests: `tools/tests/test_build_speed.py`.

### Replay runner

* `--jobs N|auto`: `auto` is half the cores, at most 8, and at most one job per
  700 MiB of available RAM. The default stays 3. **On the owner's desktop use
  `--jobs 2`** (AGENTS.md: at most 2 dw3-pc instances).
* `--min-avail-mib N` (default 1536, `DW3_REPLAY_MIN_AVAIL_MIB`): a scenario
  waits, up to 15 min, while `MemAvailable` is below N. 0 turns it off.
* `--fail-fast`: after the first failing scenario nothing new starts; the rest
  are reported as `SKIP`. Scenarios already running finish.
* `--changed [BASE]` (or `--changed-files PATH...`): runs the scenarios the
  changed files can affect (on EU, or on both where the file is region-sensitive),
  plus the quick tier, an EU smoke set (field walk, two battles, a save round
  trip) and the cross-region canary. BASE defaults to `origin/main`;
  uncommitted and untracked files count. `--dry-run` prints the selection.

`tools/replay/areas.py` is the map from files to scenarios: first matching rule
wins, rules are regexes on the path with scenario-name globs.

* Docs, unit tests, `tools/*.sh|py`, `platforms/switch/`: nothing to replay.
* Option screens, cheats, autosave, walkthrough, items, map, pacing, sweep,
  file cache: the scenarios named for that area. Decomp patches map by topic
  (`p77` to `*p77*`, `p620-cheats` to `*cheat*`, and so on).
* **A CMake file** (`CMakeLists.txt` or `*.cmake` under `port/` or
  `platforms/pc/`) is read through its diff (`git diff -U0`, branch plus working
  tree). If every changed line is a source-file entry (`"${CMAKE_CURRENT_SOURCE_DIR}/x/y.c"`,
  `${CMAKE_SOURCE_DIR}/../../port/x.c`, `src/y.c`, `${DW3_ROOT}/...`), a comment or
  blank, the selection is the union of those sources through the rules below (a
  source with no rule still selects the full tier). Anything else in the file
  (flags, definitions, targets, `if()` blocks, a variable the rule does not know,
  a new untracked build file) or `--changed-files` (no git, so no diff) selects
  the full tier. A PR that only adds one `.c` to a target (like #363) therefore
  no longer pays for all 336 scenarios.
* **A file under `port/`, `platforms/pc/`, `vendor/` or the runner with no
  rule is engine-wide** (the safe default), as is a new decomp pin; under the
  region policy that runs the area sweep (one scenario per area), not every
  scenario (`--escalate full` restores the full tier).
  So does any patch without a topic rule (`match-neutral-*`, the early
  `PORT_HOST-*` engine fixes). `port/options/cheats.c` alone selects 43 of 273 scenarios; a
  `port/ui/` change 128.
* **`platforms/pc/src/main.c` is engine-wide, on purpose.** It is the PC
  host entry: `main()`, the boot and region pick, the frame hooks (pad, turbo
  pace, vsync) and the options flush. Those run in every scenario, and no area
  rule can say which scenarios a boot-path edit reaches, so a narrower map could
  miss a regression. The `--changed` output names the reason on that file's line
  (`PC host entry (main(), boot, frame loop, options wiring): full tier`), so it
  is not mistaken for the generic "no rule" fallback. The `#if` / `DW3_*` blocks
  in it (replay shot, XSHOT, map precompute) are not split out: a sweep on a
  main.c change is slow, but it never misses one.

`--changed` is a heuristic for the pre-PR loop. It is not a substitute for the
full tier, which previews and releases still run on both regions.

**Region policy (docs/REPLAY_POLICY.md).** `--changed` runs one region (EU) unless
a changed file is region-sensitive, always adds the cross-region canary, and runs
the area sweep for an engine-wide file. `--region us|eu|both`, `--canary`,
`--sweep`, `--escalate full` and `--paced` change that. The replay runs unpaced on
both regions (the EU build used to be held to 50 frames per second).

### Cleanup helper

`tools/cleanup_tmp.py` is a dry run unless given `--apply --yes`. It lists
directories directly under `/mnt/cabal/tmp` that are older than `--days`
(default 14), not a registered worktree of this repo, not a worktree's `build`
symlink target, with no process inside, and named like our scratch trees
(`dw3-*`, `build*`, `pr<N>*`, `p<N>*`, `*-build`, `*-src`, `*-out`). It never
lists `dw3-preview<NN>*-out`, `evidence`, `assets`, anything with `backup`,
`keep`, `refs`, `runtime`, `disks`, `memcard` or `ccache` in its name, or loose
files (the owner's downloads).

Dry run on 2026-10-28 (nothing deleted): 693 entries; the repo is two weeks old,
so with the default 14 days it lists 0. With `--days 3` it lists 49 directories,
about 26.7 GiB (largest: `dw3-preview29-ryu` 4.3 GiB, `dw3-p622` 3.9 GiB,
`dw3-p97` 2.0 GiB, `dw3-ws-title-intro` 1.9 GiB, `dw3-p620` and `dw3-p620b` 1.4
GiB each). Check that none of them is another agent's live work before using
`--apply`.

## CI after

Workflow changes in `.github/workflows/ci.yml`:

* `concurrency`: a new push to a PR cancels the older run of that PR.
* ccache for `pc`, `pc-both`, `switch-game` and `switch-game-both`:
  `actions/cache` restore and save per job (newest cache wins,
  `CCACHE_MAXSIZE=400M`; about 20 to 40 MB each). The release workflow is
  unchanged and uncached.
* Submodules are cloned with `--depth 1`.
* The Python/C unit tests moved out of `pc` into their own job `tools-tests`
  (decomp and psyz only, no SDL or nugget), which runs beside the builds, with
  `pytest -n auto --dist worksteal` (xdist; the suite passed with 4 workers,
  91 s serial to 49 s locally; one test, `test_options_cache[cheats]`, takes
  32 s alone).
* The both-regions PC build is its own job `pc-both`, beside `pc`.

Job durations in seconds, GitHub-hosted runners. "Before" is the range of three
recent PR runs on `main` code. Run 1 is the first push of this PR (empty cache,
no `pc-both` job yet), run 5 a later push with every cache restored (the final
job layout).

| | Before | Run 1 (cold) | Run 5 (warm) |
|---|---|---|---|
| Whole run (wall) | 266 to 298 | 142 | 120 |
| pc | 254 to 282 (builds and tests) | 122 (US and both builds) | 78 |
| pc-both (new) | | | 98 |
| tools-tests (new) | 90 to 116 (inside pc) | 89 | 107 |
| switch-game | 96 to 168 | 111 | 74 |
| switch-game-both | 146 to 210 | 127 | 108 |
| switch, switch-launcher, changes | 28 to 38, 28 to 38, 8 to 11 | 30, 29, 6 | 28, 27, 6 |

Six runs of this PR after the change took 142, 130, 136, 113, 120 and 138 s
wall (mean 130 s against a mean of 280 s before: -54%). Single jobs move 20 to 40%
between runs on the same code (`tools-tests` 60 to 107 s, `switch-game-both` 69
to 108 s), so read the table as a range, not a point.

What moved the whole run: unit tests no longer sit behind the builds (the old
critical path was `pc` = submodules + deps + 2 builds + tests), shallow
submodule clones (15 to 87 s per job down to 8 to 19 s), the two PC builds
running side by side, and the cache for the Switch jobs (`switch-game` 111 s cold, 55 to 74 s
once warm). With a warm cache the PC jobs report 100% hits, and what is
left is SDL3's CMake configure (15 s on a 16-core desktop, 36 s on the runner),
apt (12 to 18 s) and the link. `switch-game-both` keeps about 30 s per build
(docker, configure, link) even at 99% hits.

The cache only pays for objects that did not change: a PR that touches a
header used everywhere recompiles most of the game side, as before.

### Why `switch-game-both` still runs on every code change

Skipping it unless region or build files change is not safe. The combined image
compiles `port/` twice with a suffix per region (`dual_region.cmake`), so an
ordinary game-side change (a new global, a duplicate symbol, a header that
differs between US and EU) can break it while the US-only job stays green. The
cheaper answer is ccache: the second region and the `--nxlink` variant mostly
hit. If the owner wants it skipped anyway, the condition would be "files under
`port/`, `platforms/`, `tools/*.sh` or CMake changed", which is nearly every
code PR, so the saving is small.

## Agent guidance

1. Work in `/mnt/cabal/tmp/dw3-<topic>` (a real `build/` dir), not in an NTFS
   checkout with a `build` symlink, if you want cache hits from other agents.
2. `tools/get_ccache.sh` once. Build with `tools/build_pc.sh` and
   `tools/build_switch_game.sh`. `ccache -s` (with `CCACHE_DIR` set as the
   script prints it) shows hits.
3. Pre-PR: `tools/replay_tests.sh --changed --jobs 2 --fail-fast`. Read the
   printed selection; if it says "full tier", that is the area map telling you the
   change is engine-wide.
4. Run the full tier (`tools/replay_tests.sh --jobs 2`, about 2 h) before a
   preview, or when asked; `--changed --escalate full` for an engine-wide change
   you want proven on both regions (`--changed` itself runs the area sweep). `--soak` only for
   memory work.
5. `DW3_NO_CCACHE=1` for a build whose bytes matter (preview/release candidates
   built by hand).
6. Do not delete from `/mnt/cabal/tmp` yourself; run `tools/cleanup_tmp.py` and
   give the owner the list.

## Not done

* PC configure (SDL3 checks, 14 to 18 s) is now the largest part of a warm PC
  build.
* No global `DW3_TURBO` for replay: it can change what a checkpoint shows.
* `--twice` doubles the time; keep it for determinism work.
* The Ryubing runner (`tools/replay_ryubing.sh`) was not measured.
