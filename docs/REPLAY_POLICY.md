# Replay policy: one region by default, both at the gate

Owner request, 2026-10-31: "almost everything that diverged between US and EU
is unified, so one test is enough; investigate whether that makes sense and
readjust the tests so they are not so slow." The owner allowed AGENTS.md rule 5
("every change is tested on both ISOs") to change if the evidence supports it.

This note is the evidence, the policy that follows from it, and how to run it.
Measured numbers come from the p56 health run (`962462d`, 2026-10-30) and from
the git and PR history; where a number was measured for this change it says so.

## Verdict

**Partly.** The claim holds for the *port*: 200 of the 218 `.c`/`.h` files in
`port/` and `platforms/` know nothing about the region, and in the last health
run every replay drift (25 scenarios) was a stale reference that showed on both
regions, or on one region only because that reference had been written earlier.
No replay in that run, and no fix PR in the last 100 merges, found a bug that
only the second region would have shown *for a change that did not mention the
region*. It does **not** hold for the *game underneath*: the decomp is still two
builds, and 317 of its 627 source files carry `VERSION_EU` conditionals. The
bugs that were region-only (EU language select, the EU extra chapter's 55
stages) were all in code that mentions the region.

So: **one region (EU) is enough for a change that is region-agnostic**, and
that is most PRs. A change that touches region code, saves, boot, overlays,
text or fonts, and every release, still needs both. EU is the better single
region: it is the superset (five languages, the 0x2D chapter, 55 extra stages,
the 50 Hz path), and nothing runs on US that cannot also run on EU except the
US overlay table and the US text.

Time: the full run (both regions) stays at about 2 h 18 min on `--jobs 2` and
becomes the release gate. The PR default drops to **TIMING_PLACEHOLDER**.

## 1. What still differs between US and EU

| Divergence | Where | How much port code depends on it |
|---|---|---|
| Two decomp builds (`config/us`, `config/eu`: different overlay ids, statics, text, `FILE_SECTORS`) | `vendor/dw3_decomp`; 317 of 627 `.c` files have `VERSION_EU`/`VERSION_US` blocks (631 occurrences, most in `field/stages/*_sector`, `*_city`, `battle_mode`, `card_battle`) | The port patches the decomp: 21 of 129 patches mention `VERSION_EU`/`VERSION_US`/`TITLE_EU`; the rest are region-neutral by construction (`match-neutral-*`, engine fixes). |
| Overlay tables and sections | `port/overlays/overlay_table_us.c` (257 rows) / `overlay_table_eu.c` (312), `overlay_sections_eu_extra.c` | Hand-written; 3 files. #378 was a missing row in both tables. |
| Five languages, text, fonts, help pages | EU only: language select (`CNTY_SEL`), `help-*`, `items-*`, `map-names-*`, `p77-hud-form-*`, `title-quit-*` copies | 47 scenario copies (`LANG_COPY`) of 41 measured minutes (about 34 min of process time) exist only to look at another language. Port code: `port/help`, `port/pages`, `port/walkthrough`, `port/options/lang_select.c`, patches `p61-language-select`, `p62-eu-country`, `p92-*`. |
| EU extra chapter (progress 0x2D, 55 stages, a 12th walkthrough chapter) | `overlay_sections_eu_extra.c`, walkthrough rules, `eu-extra-stage-*`, `xr-eu-extra-us` | EU only; 3 files in `port/`. |
| PAL: 50 Hz, 20% more movement per frame | PSY-Z frame limiter (EU targets 50 Hz; the PC host's turbo base rate follows it), recordings of the bridge walk | `port/pacing`, `port/audio/turbo_music.c`, `platforms/pc/src/main.c` read the rate from PSY-Z; 3 files. 42 of 134 paired recordings differ in length, none is byte-identical. |
| Save layout (US slot 0x26BC bytes, EU 0x26C4) and cross-region import | `port/saves`, `port/memcard` (6 `VERSION_*` blocks), `tools/replay` `mutate_save` | Isolated in 2 directories; covered by the 12 `xr-*` scenarios. |
| Region boot, picker, relaunch, combined binary | `port/region/*` (8 `VERSION_*` references), `platforms/pc/src/main.c`, Switch launcher | 7 files call `dw3_region()` outside `port/region` (19 call sites); tiers `both` (10 scenarios) and `single` (7). |
| Everything else in `port/` | options, UI, map view, cheats, autosave, items, battle menu | 18 of 218 `.c`/`.h` files in `port/` and `platforms/` mention `VERSION_*`; `port/options` has 6, `port/memcard` 3, `port/region` 3, `platforms/pc` 2, `port/map_view`, `port/audio`, `port/walkthrough` 1 each. |

Reading: the port is region-agnostic by design (one source, one behaviour), and
the region enters at four places: the data (overlay tables, text), the saves,
the boot/relaunch path and the PAL rate. Each has a path rule or a `VERSION_*`
mark, so the selector can see it (section 4).

## 2. History: would one region have caught it?

p56 health run (full tier, `tmp/evidence/2026-10-30-health-p56`): 442 scenarios,
5370 checks, **0 failures**, no flake (`--twice` on the 25 drifted scenarios:
0 non-identical captures). 25 scenarios had a non-zero pixel diff under the
threshold:

| Kind | Scenarios | Region pattern |
|---|---|---|
| By-design reference change (#343, battle Items/technique list bar) | 12 | all on **both** regions (`us|eu-two-battles`, `-restart-in-battle`, `-battle-options`, ...) |
| Stale shared reference (help, controls, map view, title) | 13 | 10 appear on both regions; 5 (`us-map-view`, `us-map-view-mod`, `us-nintendo-map-view`, `us-restart-save-continue`, `us-return-title`) on US only, and their EU twins passed with 0 px. A reference written on an older state, not a bug. |
| Real regression | 0 | |
| Region-only drift (EU shows it, US does not) | 0 | |

Last 100 merged PRs (`gh pr list`, #286 to #386, 2026-10-27 to 2026-10-31), fix PRs
whose cause is region-related:

| PR | Bug | Region | Would a single EU run have caught it? |
|---|---|---|---|
| #378 | `WSTAG460` and the extra chapter's 55 stages missing from the overlay tables | US and EU (the 55 stages EU only) | Yes (EU has the superset of stages) |
| #330 | EU language select did not confirm with the confirm button | EU only (US has no language select) | Yes; a US-only run could not |
| #308 | EU language select not 16:9 | EU only | Yes |
| #343 | Battle Items list had no selection bar (`ContinueDraw` stub) | both | Yes |
| #323 | Item shop 16:9 panels | both | Yes |
| #325 | Combined build finds the disc in the parent folder | region boot | Needs the canary and the `both` tier |
| #385, #316 | Region relaunch / combined `.nro` picker | combined binary | Needs the canary and the `both` tier |

Count: of 7 region-related fixes, **5 are visible to an EU run, 2 only to the
combined-binary scenarios**; none was visible to US alone and not to EU. No fix
in the last 100 PRs was a US-only failure. (Bodies and comments of #286 to #386
record the failing region for these seven; the others are region-neutral:
the Ryubing memory checks #264/#266, the log rotation #296/#301, GPU paths
#229/#253, and so on.)

What this does *not* show: the health run is one point in time; before #343 the
suite had stale references that nobody noticed on either region, because the
full run is expensive and rarely done. A faster default that runs more often is
more likely to catch drift than a slow one nobody runs.

## 3. Timing: where the 2 h 18 min go

Sum of process seconds in the p56 full tier, `--jobs 2`, `nice -n 10`, 16 cores:
4.58 h of process time for 442 scenarios, 2.29 h wall (the measured 2 h 18 min
is the same number: the runner adds almost nothing).

| Part | Scenarios | Process time | Share |
|---|---|---|---|
| US | 193 | 1.34 h | 29% |
| EU | 237 | **3.09 h** | **67%** |
| Cross-region `xr-*` | 12 | 0.16 h | 4% |

**EU takes 2.1 times as long as US for the same scenario** (191 paired
scenarios: US 78 min, EU 151 min; the recordings have the same number of frames,
499 136 vs 502 736). The cause is pacing, not the game: under Xvfb the driver
VSync never blocks, so the US build runs flat out, while the EU build targets
50 Hz, cannot use the 60 Hz driver VSync, and PSY-Z's frame limiter holds it to
about 50 frames per second (`items-bag-view`: 9900 frames in 60 s on US =
165 fps, 183 s on EU = 54 fps). A replay is keyed by the game frame, so its speed
changes nothing in the captures. The runner now lets a replay run unpaced on
both regions (`DW3_REPLAY_PACED=1` or `--paced` restores the old pacing).

Where the rest goes (p56, process minutes): `items-bag-view` family 16 (6
scenarios, 3 minutes each, because they stay in a menu), the 28 `cheat*`
13, `encounter*` 8, `map-names` 7.5, `items-use-stays` 6.6, `banner-instant` 6,
`field-walk`/`bridge-partner-gate`/`newgame-field` 5.5 each (the opening is
about 150 s of game time and is replayed in 5 scenarios per region), `p79`/`p77`
5 each, the 62 `walkthrough-p*` only 4 (4 s each: they start from
`DW3_START_MODE`). The median scenario is 24 s; the 259 scenarios under 30 s
add up to 56 min, and the 40 slowest (above 100 s) to 2.3 h of the 4.58 h.

Duplicates and near duplicates found (**candidates only: nothing was deleted;
the owner decides**):

* `newgame-field` is a strict prefix of `field-walk` (same opening, then four map
  transitions). `field-walk` already checks the first control frame. Dropping
  `us|eu-newgame-field` saves 5.3 min of process time and loses the
  `newgame-field` checkpoint pictures only.
* `items-bag-view`, `-43`, `-off`, `items-equip-stays`, `-off`, `items-equip-switch`,
  `-43`, `glyphs-on-items`, `glyphs-off-items`, `menu-screens` repeat the same
  Start-menu walk to the Items screen with one option changed. They are about 25
  min of process time on EU (before the unpaced replay).
* The language copies (`LANG_COPY`: 47 scenarios, 34 min) repeat an English path
  in French, German, Italian and Spanish. They stay; the policy only runs them
  when text, fonts or language code changes.
* `us|eu-cheat-*` (28 scenarios, 13 min) are an `-off` twin per cheat; the twin
  checks that the option off is the old game.

## 4. The policy

Tiers:

| Tier | When | What | Target |
|---|---|---|---|
| **PR default** | before every PR (`tools/replay_tests.sh --changed --jobs 2 --fail-fast`) | the scenarios the changed files can affect, on **EU only**; both regions where the file is region-sensitive; the language copies only for a text/font/language change; an engine-wide file (main.c, the decomp pin, a build file) runs the **area sweep**, not the full tier; always the quick tier, three EU smoke scenarios and the **canary** | 15 to 40 min |
| **Region-sensitive** | automatic inside `--changed` | for a changed file of class `both`, `langs`, `us` or `eu` (below), the affected areas run on the regions it needs | the same run |
| **Release gate** | before a preview/release, nightly, or when asked | the full tier on both regions (`tools/replay_tests.sh --jobs 2`, about 2 h) and the both-binary subset | about 2 h |

The canary (`areas.CANARY`, about 6 min) is what the single-region default would
otherwise stop checking: US boot (`us-title`, `us-newgame-field`), the stage
registries (`us|eu-stage-0x236`), a localized EU menu (`eu-lang-fr`), the EU extra
chapter (`eu-extra-stage-920`, `-974`), cross-region saves (`xr-us-to-eu`,
`xr-eu-to-us`, `xr-eu-extra-us`, `xr-store-us-to-eu`, `xr-store-eu-to-us`). With
`--bin-both` it adds the `both` tier (Region at start row, relaunch); with
`--launcher` the `single` tier. A skip counts as a failure, so those tiers join
only when their binary is given.

Region class of a changed file (`areas.region_class`):

| Class | Files | Runs |
|---|---|---|
| default | everything region-agnostic (options, UI, cheats, map, items, pacing, ...) | EU only |
| `eu` / `us` | `overlay_table_eu.c`, `overlay_sections_eu_extra.c` / `overlay_table_us.c` | that region |
| `both` | `port/saves`, `port/memcard`, `port/region`, `port/autosave`, `port/cdfs`, `overlay_sections.c`, `wstag` patches, `DECOMP_PIN`, `vendor/`, and **any** `.c`/`.h`/`.patch` under `port/` or `platforms/pc/` whose text mentions `VERSION_EU/US`, `TITLE_EU/US`, `DW3_BOTH`, `dw3_region` | the affected scenarios on US and EU |
| `langs` | `port/help`, `port/pages`, `port/walkthrough`, `lang_select`, `region*`, text/font patches (`language`, `eu-country`, `p635`, `p92`), the title lettering | `both` plus the language copies |
| `exact` | a changed `.rec` | that recording's scenarios, whatever the region |

The path rules are the area map's `RULES` (which scenarios) plus
`REGION_CLASS_RULES` (on which regions). The content mark is the safety net: a
file that starts mentioning the region becomes `both` without anyone editing a
list.

Flags (`tools/replay_tests.sh`):

```bash
tools/replay_tests.sh --changed --jobs 2 --fail-fast      # PR default (--region auto)
tools/replay_tests.sh --changed --region both             # the pre-policy behaviour
tools/replay_tests.sh --changed --escalate full           # an engine-wide file runs the full tier
tools/replay_tests.sh --region eu --jobs 2                # the full tier of one region
tools/replay_tests.sh --canary                            # the cross-region canary alone
tools/replay_tests.sh --sweep [--region eu|us|both]       # one scenario per area, plus the canary
tools/replay_tests.sh --dry-run --changed                 # selection, by region, and an estimate
tools/replay_tests.sh --paced                             # keep the real frame pacing
tools/replay_tests.sh --jobs 2                            # release gate: both regions, full tier
```

`--dry-run` prints the estimate from `tools/replay/timings.json` (process
seconds per scenario from p56; no game data), and the runner starts the longest
measured scenario first.

## 5. Measurement

MEASUREMENT_PLACEHOLDER

## 6. What moved to the release gate

* Every region-agnostic scenario on **US** (about 190 scenarios, 1.3 h): a PR no
  longer runs them. The canary keeps US boot, new game to control, the stage
  registry and the cross-region saves.
* The language copies for a change that does not touch text, fonts or language.
* The full tier for an engine-wide file (`main.c`, a build file, an unmapped
  file): replaced by the area sweep (one scenario per area, EU; both regions when the
  file mentions the region).
* Nothing is removed from the full tier: CI has no game data and runs no replay
  (it builds both regions and runs `tools/tests`), so the gate is the owner's
  machine. The `--twice` determinism check, soaks and the Ryubing runner are
  unchanged.

## 7. Open points (owner)

* The duplicate candidates in section 3 (`newgame-field` under `field-walk`, the
  items-menu family). Deleting a scenario means deleting its `.rec`, its
  manifest entry and its reference directory; none of that was done.
* Orphan references from the p56 run (16 directories under
  `~/.cache/dw3-switch/replay-refs` that no scenario names) are still listed in
  `tmp/evidence/2026-10-30-health-p56/README.md`; not deleted.
* A nightly full run on the owner's machine would catch the drift that a
  single-region PR default cannot see for one release cycle. It is a scheduled
  job, not a code change; say if you want it.
