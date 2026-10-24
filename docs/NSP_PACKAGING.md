# NSP packaging research (P10.7)

Status: **dropped 2026-10-24 by the owner ("vamos ignorar o NSP então").
The game stays an NRO.** Research only; nothing is implemented and there is
no code. Kept for reference in case the item is reopened.
Dated 2026-10-24. Roadmap entry: [ROADMAP.md](ROADMAP.md), P10.7.

Owner request (Portuguese): "Vamos investigar a possibilidade de empacotar o
jogo como NSP para instalar no Switch". In short: could the game be packaged
as an NSP that installs on the Switch and shows on the HOME menu as its own
title?

This note keeps to the requirements and risks. It does not link to or
describe keys, signature patches (sigpatches) or copyrighted Nintendo files,
and it gives no step-by-step build or install instructions.

## Scope lines that apply

- [AGENTS.md](../AGENTS.md), "Out of scope": "Official Nintendo SDK / eShop
  packaging", and "bypassing console security beyond normal homebrew
  (Atmosphere + homebrew `.nro`) assumptions".
- [ARCHITECTURE.md](ARCHITECTURE.md), Non-goals: "Official eShop packaging".
- [SWITCH_FEATURES.md](SWITCH_FEATURES.md), Homebrew caveats: "Prefer
  **applet mode** heap; do not require title takeover for the gate", and
  "Official cloud / play activity / eShop packaging stay out of scope".

Reading these lines against the routes below:

- A **full NSP** (route b) is the "official packaging" shape. It needs keys
  to build and signature patches to run, so it sits on the out-of-scope line.
- A **forwarder NSP** (route a) needs signature patches on the console to
  install and launch. This note treats that as beyond "normal homebrew". The
  owner decides whether it is.
- **NRO plus title takeover** (route c) stays inside "Atmosphere + homebrew
  `.nro`".

## Routes compared

| | (a) Forwarder NSP | (b) Full NSP (our NSO) | (c) NRO, title takeover |
|---|---|---|---|
| What it is | A small title whose code launches our `.nro` from SD through hbloader | A title whose ExeFS holds our game as an NSO, with npdm, NACP, icon and optional RomFS | Our `.nro` run from hbmenu. A user-side Atmosphere override makes it run as the game's title |
| Own HOME icon | Yes | Yes | No |
| Keys needed to build | Yes: the user's own, or derived on the console | Yes, to sign the NCAs | No |
| Signature patches needed | Yes, to install and run | Yes, to run | No |
| Launch mode | Application | Application | Application (takeover) |
| Project ships | Nothing | Nothing | The `.nro`, as today |
| Inside AGENTS.md scope | Grey: user-built, needs patches | No: official packaging shape, needs keys | Yes |

## Requirements per route

### (a) Forwarder NSP

- **Keys.** The community builders take the user's own key set. NTON and
  hacBrewPack use the `prod.keys` dumped from the user's own console. Sphaira
  derives the NCA header key on the console through SPL, so that part needs no
  keys file; it can also read a `prod.keys` file. switch-nsp-forwarder also
  derives on the console. The project never holds keys.
- **Firmware.** NTON needs firmware 12.0.0 or later. switch-nsp-forwarder
  v0.0.7 lists Switch 21.x support (libnx 4.12.0-1).
- **Install and launch.** Needs signature patches (sigpatches) on the console.
  The sigpatch project is not linked here.
- **Metadata.** A forwarder carries its own NACP and icon. hacBrewPack expects
  `control/control.nacp` and `icon_AmericanEnglish.dat` (256x256 JPEG, EXIF
  stripped). It must be built with `--nologo`, because the Nintendo logo and
  startup movie are Nintendo files and must not be used.
- **Storage.** NTON sets all save and storage sizes to 0, since a forwarder
  needs no save data. The install is about 1 MB.
- **Path.** Other people's forwarders often fail because the `.nro` path
  differs (NTON notes this). The generated title depends on the path (see
  [Title IDs](#title-ids)).

### (b) Full NSP (our NSO)

- **Main NSO.** Built from our ELF with elf2nso. The NSO is LZ4-compressed,
  and its header carries the ModuleId. It is a separate format from the NRO
  (see [P6.5_NRO_FORMAT.md](P6.5_NRO_FORMAT.md)).
- **Other parts.** npdm, NACP, icon, and optionally RomFS.
- **Keys.** Signing the NCAs needs the user's keys (hacBrewPack takes the
  header key and the application key-area keys). The project cannot hold them,
  and CI cannot build the NSP.
- **Install and launch.** Needs signature patches to run.
- **Crash symbolization.** Changes; see [crash reporting](#crash-reporting-p54).

### (c) NRO, title takeover

- The user sets this up in their own Atmosphere config. The `[hbl_config]`
  section supports `override_any_app=true` with `override_any_app_key=R`, and
  `override_key_N` per program ID. On a game, holding R starts the `.nro`
  as that title.
- No NSP, no keys, no signature patches.
- Gives application mode and full RAM. There is no own HOME icon.

## Legal, ToS and ban context

- NTON's README carries this caution, quoted: "Installing these kinds of NSP
  files (NRO to NSP forwarders) can result in a console ban. This applies not
  just to this software but to all forms of NRO to NSP forwarder generators,
  including homebrew and web-app versions. To run these NSP files, you need to
  make further modifications to your system. I do not support Piracy on any
  Nintendo device therefore support on that is not provided."
- Forwarders need sigpatches on the console. Only third-party pages say that
  official Atmosphere does not ship or load sigpatches. That is unverified.
- Nintendo's terms of service and copyright position were **not** researched
  for this note. Any public route (a) or (b) needs legal review first.
- The project must not distribute NSPs, keys, sigpatches or any copyrighted
  Nintendo file, and must not ship a forwarder. Route (a) would be the user's
  own build on the user's own console.
- The own-disc rule in AGENTS.md is unchanged.

## Title IDs

- hacBrewPack's stated valid range is `0x0100000000000000` to
  `0x0fffffffffffffff`, and it does not suggest going above
  `0x01ffffffffffffff`.
- NTON picks a random value "within generally conformed bounds" and checks it
  against a list of existing system and software title IDs.
- Sphaira (and switch-nsp-forwarder, which matches it) derives the ID from the
  NRO path and arguments: `tid = 0x0500000000000000 | (sha256(nro_path + args)[0] & 0x00FFFFFFFFFFF000)`.
  Sphaira moved from a `0x01` prefix to `0x05` and calls
  `nsDeleteApplicationCompletely` on the old ID. So `0x05` is the current
  community convention for forwarders, and it is clear of the `0x01`
  application range.
- **Unresolved:** that mask keeps bit 12. The DBI release note says bit 11 is
  the update flag and bit 12 the DLC flag, and that both are cleared for a
  base application. The two sources do not agree. Check this before any ID is
  chosen.
- A changed ID leaves the old title installed. The user removes it in HOME,
  under Data Management.

## Impact on dw3-switch

### Memory and applet mode

- Today the `.nro` runs under hbloader as `AppletType_LibraryApplet`.
  `appletSetFocusHandlingMode` is not available, and `appletNotifyRunning` is
  skipped because it is Application-only
  ([P5.3_HOME_QUIT.md](P5.3_HOME_QUIT.md), `boot_main.c` ~l.112-153).
- As an Application (route a or c takeover), hbloader sets
  `AppletType_SystemApplication` and `EnvAppletFlags_ApplicationOverride`.
  That changes the focus and suspend behaviour that P10.2 is testing, so the
  P10.2 smokes need a re-run in application mode.
- Heap: an 8 MiB game heap arena in BSS, plus fixed VRAM buffers
  ([P5.1_MEMORY.md](P5.1_MEMORY.md)). On the rc7 OLED sample, the newlib
  high-water was 144435K (about 141 MB), and the game heap was 1685K of
  8192K.
- Exact heap sizes for applet and application mode were **not** found in a
  reliable source. hbloader reads the `applet_heap_size` and
  `applet_heap_reservation_size` settings. No MB figure is given here.
- Community sources say applet mode limits RAM, and that full mode comes from
  hold-R takeover or a forwarder. These are secondary and have not been
  measured on our build.

### SD data paths

- Runtime paths are absolute and hardcoded: `sdmc:/switch/dw3` in
  `platforms/switch/psyz_backend/nx_file.c`, `nx_crash.c` and
  `nx_vram_dump.c`. `DW3_DEFAULT_RUNTIME` is `sdmc:/switch/dw3/us` in
  `platforms/switch/source/boot_main.c` (~l.62).
- `main(argc, argv)` ignores argv (`boot_main.c` ~l.607-611).
- So a forwarder or takeover finds its data and saves on SD through
  `fsdevMountSdmc`, whatever the launch path. Routes (a) and (c) need no code
  change for data.

### User account and saves

- An Application launch with NACP `startup_user_account=1` gives a preselected
  user through `accountGetPreselectedUser`. That matters for per-user saves.
  The owner's P6.24 decision on per-user saves is **not on main** at the time
  of writing, so this note does not depend on its wording.
- Library-applet mode has no preselected user. It would need the player
  select applet.
- Saves today are flat files, `save0.bin` and `save1.bin` (32 KiB each), under
  the SD memcard directory, plus `port_options`
  ([P5.3_FLAT_SAVE.md](P5.3_FLAT_SAVE.md)). No account or fs save-data code
  exists (a grep found none).
- Native fs account save data needs an installed title ID and save sizes, so
  it works only with an installed NSP. Keep the SD saves.

### Crash reporting (P5.4)

- The module base comes from `svcQueryMemory` on our text. The GNU build ID is
  read from `.note.gnu.build-id` in the loaded image.
  `tools/symbolize_crash.py` matches that build ID to the `.elf`
  ([P5.4_CRASH.md](P5.4_CRASH.md)).
- Atmosphere writes crash reports to
  `sdmc:/atmosphere/crash_reports/<n>_<program id>.log`. Under routes (a) and
  (c), the only change is the program ID in the file name, which becomes the
  forwarder's title ID.
- Under route (b), the NSO module ID and build-ID reading need re-checking.
  This is **unverified**.

### nxlink and debug builds

- Route (c): no change.
- Route (a): the nxlink `.nro` would need its own forwarder, with its own path
  and so its own title ID.

### Release and CI

- `switch-release.yml` publishes `dw3-switch.nro`, `dw3-switch-nxlink.nro`,
  `dw3-switch-elf.zip` (built by `tools/pack_release_elfs.py`, with
  `BUILD_IDS.txt`) and `README.txt`. See [RELEASES.md](RELEASES.md).
- The workflow checks the NACP version (`tools/dw3_nro_version.py`), the
  `sdmc:/switch/dw3/us` string in the `.nro`, `dw3: version=`, and that
  `nxlink stdio connected` is present only in the nxlink `.nro`.
- The NRO is built in `platforms/switch/CMakeLists.txt` (~l.434-448) with
  `nx_generate_nacp` and `nx_create_nro` (icon `platforms/switch/icon.jpg`).
  It has no RomFS.
- **Any NSP needs keys to build.** So no NSP can be built in CI or in the
  release workflow. The repo holds no secrets, and the project must never hold
  keys.

### RomFS and game data

- An NSP could carry RomFS. Any RomFS would hold only port files (icon, NACP,
  control).
- Game data stays on SD. The rule "never ship game data" is unchanged.

## Recommendation

1. **Stay NRO, and document title takeover (route c)** as the supported path
   for full RAM and application mode. It needs no keys and no signature
   patches, and it is inside AGENTS.md scope. The cost is that there is no
   own HOME icon.
2. **Route (a) at most a user-side compatibility note.** It is the user's own
   choice and risk. The project ships no NSP, keys, sigpatches or patch
   instructions, and it does not pursue (a) further.
3. **Route (b) is not recommended.** It needs keys that the project cannot
   hold, so CI cannot build it, and it needs sigpatches to run. It sits on the
   out-of-scope line, and it changes crash symbolization.

## First slice

1. This PR: docs only.
2. Then an OLED check of the game launched via title takeover (application
   mode), on US and EU, covering the P10.2 focus and sleep lines, the mem dbg
   log, and saves. The test report must state the method, as AGENTS.md
   requires.
3. Later, optionally, a code PR that logs `appletGetAppletType` at boot so
   reports show the launch mode. Not part of this PR.

## Proposed AGENTS.md wording (not applied)

> a forwarder a user builds for our .nro is the user's choice and risk; the
> project documents compatibility only and ships no NSP, keys or patches

The out-of-scope line stays unchanged until the owner decides.

## Open questions for the owner

1. Is "a user-built forwarder is the user's business; we only document
   compatibility" acceptable, or should the docs not mention forwarders at all?
2. Should the release README recommend title takeover (hold R) for full RAM?
3. Should P6.24's per-user saves depend on application mode (a preselected
   user), or use an in-game user picker?
4. Should a fixed `0x05` title ID ever be suggested, or never?
5. Is route (b), a full NSP, ever wanted? It would need an AGENTS.md scope
   change.

## Unverified

- Exact heap sizes for applet and application mode. No reliable source found.
- Whether official Atmosphere ships or loads sigpatches. Only third-party pages
  say so.
- The title ID mask and bit 12 (see [Title IDs](#title-ids)).
- Crash symbolization for an NSO (route b).
- The single forum report that "Full Memory Override" failed on a
  Sphaira-made forwarder while a PC-built NSP worked.
- Nintendo's terms of service and copyright position.
- The owner's per-user saves decision for P6.24 (not on main yet).

## Sources

All fetched 2026-10-24.

- NTON (PC NRO to NSP forwarder builder), GitHub: https://github.com/rlaphoenix/NTON
  (repo updated 2026-10-22). PyPI: https://pypi.org/project/nton/
- hacBrewPack (fork; the original The-4n/hacBrewPack now returns 404):
  https://github.com/rlaphoenix/hacBrewPack (GPL-2.0)
- switch-nsp-forwarder (on-console generator): https://github.com/TooTallNate/switch-nsp-forwarder
  (v0.0.7 of 2026-02-27; v0.0.8 of 2026-05-03)
- Sphaira (hbmenu replacement, GPL-3.0): https://github.com/ITotalJustice/sphaira
  (source `sphaira/source/owo.cpp`). Forum thread on forwarders from the
  homebrew tab: https://gbatemp.net/threads/sphaira-hbmenu-replacement.664523/post-10637426
- Forum report on "Full Memory Override" with a Sphaira-made forwarder (single
  source): https://gbatemp.net/posts/10893051/
- DBI release note on title ID bits: https://newreleases.io/project/github/rashevskyv/dbi/release/187
- nx-hbloader: https://github.com/switchbrew/nx-hbloader/blob/master/source/main.c
- Homebrew ABI (NextLoadPath, argv): https://switchbrew.org/wiki/Homebrew_ABI
- Atmosphere override config template: https://github.com/Atmosphere-NX/Atmosphere/blob/master/config_templates/override_config.ini
- Atmosphere configurations: https://github.com/Atmosphere-NX/Atmosphere/blob/master/docs/features/configurations.md
- Applet vs full RAM, secondary (NH switch guide mirror):
  https://git.hitsave.org/code-sync/switch-guide/commit/5dc05a4d5259c4bf95c77c6ed638f919d8b6e016
- Applet vs full RAM, secondary (GBAtemp): https://gbatemp.net/threads/melonds-for-switch.518300/post-8827203

Not linked on purpose: the sysmodule project that patches system services at
boot (the sigpatch role), and the key-dumping tooling. The NTON README names
one of them.

## History moved from ROADMAP.md (2026-10-24)

Moved from the ROADMAP.md entries on 2026-10-24, word for word (two leading
spaces removed). "Above" and "below" refer to the old entry. The roadmap
keeps the current state and links here.

**Findings:** three routes. (a) a user-built forwarder NSP needs the user's
own keys and signature patches, and carries ban risk (a community caution
that applies to all forwarder generators); (b) a full NSP with our NSO needs
keys the project cannot hold, so CI cannot build it, and it sits on the
out-of-scope "official packaging" line; (c) staying NRO with hbmenu title
takeover needs no keys and no patches.
**Recommended approach (not decided):** stay NRO and document title takeover
as the supported path (application mode, full RAM, no own HOME icon). At
most, a user-side forwarder compatibility note; the project ships no NSP,
keys or patches. Route (b) is not recommended.
**Impact to check:** application mode changes the focus and suspend
behaviour that P10.2 tests; saves stay on SD unless the owner wants native
account save data (which needs an installed NSP); crash symbolization for
an NSO is unverified. Title ID for a forwarder and the bit-12 question are
open in the doc.
**First slice:** docs only (this note). Then an OLED check of the game under
title takeover on US and EU (P10.2 focus and sleep lines, mem dbg log,
saves). A boot log of the applet type is a later code PR.
**Open questions (owner):** is a user-built forwarder "the user's business"
in the docs, or should forwarders go unmentioned; should the release README
recommend title takeover (hold R) for full RAM; should P6.24's per-user saves
depend on application mode; should a fixed `0x05` title ID ever be
suggested; is route (b) ever wanted (it needs an AGENTS.md scope change).
