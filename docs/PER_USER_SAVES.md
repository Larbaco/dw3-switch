# P6.26 Per-user saves (design)

Owner decision (2026-10-24, recorded in [SINGLE_BUILD.md](SINGLE_BUILD.md)):
**saves are bound to the Switch user profile that is playing.** This note is the
design behind the [ROADMAP P6.26 entry](ROADMAP.md#phase-6-goal-notes).
**Nothing is implemented.** It builds on [P6.25](CROSS_REGION_SAVES.md) (the
canonical save container) and P6.24 (one build) depends on it.

## Owner decision: the Switch profile selector (2026-10-24)

> "Quero usar o seletor de perfil do Switch."
> (I want to use the Switch's profile selector.)

So the user choice is the system profile selector, `pselShowUserSelector`
(libnx `applets/psel.h`), not an in-game picker. The game stays an NRO (P10.7
NSP packaging was dropped, see PR #227), so per-user saves are SD-card folders
per account uid, not native account save data.

## Owner decision: cancelling the profile selector (2026-10-24)

> "Se o jogador cancelar a escolha, O jogo não deve ser iniciado."
> (If the player cancels the choice, the game must not be started.)

So when `pselShowUserSelector` returns no user, the game does not start. It exits
cleanly back to the menu it was launched from. Nothing is written: no
`saves/<uid>/` or `shared/` folder is created and no `last_user` is stored. The
`shared/` fallback applies only when the account service fails (step 1 below),
not on a cancel.

## Where saves and options live today

- Saves: one flat file per memory-card port, `save0.bin` / `save1.bin` (32 KiB),
  under `sdmc:/switch/dw3/<us|eu>/memcard/` on the Switch and
  `runtime/<ver>/memcard/` on the PC. Set up in `platforms/switch/source/boot_main.c`
  (`memdir`, ~line 486 and 531) and `platforms/pc/src/main.c` (~231 and 282);
  `DW3_MEMCARD_DIR` overrides both. Code: `port/memcard/memcard_host.c`
  (`memcard_host_set_dir`), format in [P5.3](P5.3_FLAT_SAVE.md).
- Options: `port_options.cfg` in the runtime root, i.e. per region, via
  `dw3_port_options_persist_init(cfgdir ? cfgdir : root)` (`boot_main.c:550`),
  `DW3_CONFIG_DIR` overrides. Store: `port/options/options_cfg.c`; rules in
  [PORT_OPTIONS.md](PORT_OPTIONS.md#persistence).
- The Switch host has **no account or applet-type code**: `platforms/switch`
  has no `account*`, `appletGetAppletType` or `psel*` call. Everyone who uses
  the console shares one save folder.

## What libnx offers (checked in the devkitPro `libnx` headers)

Read in the `devkitpro/devkita64` image (`services/acc.h`, `applets/psel.h`,
`services/applet.h`). Names below exist there; **behaviour on the console is
not tested** and each point marked "check" is an OLED task of the first slice.

| Need | API | Note |
|---|---|---|
| Open the account service | `accountInitialize(AccountServiceType_Application)` (acc:u0) | Needs `accountExit()` at shutdown |
| The user the system preselected | `accountGetPreselectedUser(AccountUid*)` | "The cached PreselectedUser loaded during `accountInitialize`, when available": a launch parameter (`AppletLaunchParameterKind_PreselectedUser`, `applet.h:86`) that the system sends when it starts an **Application**. Expected to be present in **title-override mode** (hbmenu started over a game, after the profile selector) and absent in **applet mode** (started from the Album) -- check |
| Applet type | `appletGetAppletType()` (`AppletType_Application` vs `AppletType_LibraryApplet`) | `boot_main.c:118` already notes the NRO runs as a library applet under hbloader in applet mode |
| Only one user exists | `accountTrySelectUserWithoutInteraction(&uid, false)` | Succeeds without UI when the choice is unambiguous -- check what it returns with several users |
| List users | `accountGetUserCount`, `accountListAllUsers(uids, max, &n)` | Works through acc:u0 in both modes |
| Name and icon | `accountGetProfile`, `accountProfileGet` (nickname, UTF-8, 0x20 bytes), `accountProfileLoadImage` (JPEG) | Nickname for a `profile.txt` (and for the fallback picker, if one is needed; the system selector shows its own names) |
| Profile selector (the user choice) | `pselShowUserSelector(&uid, &settings)` (`psel.h`, `PselUiMode_UserSelector`) | Chosen by the owner (2026-10-24). Launches the system profile-selection applet; whether an NRO in applet mode may launch it is **unknown -- check** |
| Valid uid | `accountUidIsValid` | All-zero means not set |

`AccountUid` is 128 bits (`u64 uid[2]`); the profile can be deleted and created
again with a new uid, and uids are local to a console.

## Account probe (diagnostic, in the tree)

`platforms/switch/source/account_probe.c`, called once from `boot_main.c` after
the log is open. It only logs; it changes no save path and no game behaviour,
and every failed call is logged as an `rc` and skipped. Grep `account probe:` in
`sdmc:/switch/dw3/dw3.log` (or the nxlink output). Result codes are hex.

| Line | Answers |
|---|---|
| `appletGetAppletType=<name> (<n>)` | Which applet type the NRO runs as in this launch path (`Application` 0, `LibraryApplet` 2, ...) |
| `accountInitialize(Application) rc` | Whether acc:u0 opens |
| `accountGetPreselectedUser rc uid nickname` | Whether the system handed a preselected user (rc 0 and a valid uid), and who |
| `accountGetUserCount`, `accountListAllUsers` + `user[i]` | How many profiles, their uids and nicknames |
| `accountGetLastOpenedUser rc uid nickname` | The last profile opened on the console |
| `accountTrySelectUserWithoutInteraction(false) rc uid` | Whether libnx picks a user with no UI (single profile) |

The optional psel probe is the CMake option `DW3_ACCOUNT_PROBE_PSEL` (default
OFF), `tools/build_switch_game.sh --account-probe-psel` (build dir
`build/switch[-eu][-nxlink]-pselprobe`). When ON it calls `pselShowUserSelector`
once, after the block above and before the GPU starts, logs `psel chosen ...` or
`psel rc=...` (a cancel or an error), and then boots normally; the choice is
ignored. The selector appearing at boot is the test. Test builds only, never
release/preview. The owner runs it in three launch paths (Sphaira forwarder NSP
on HOME, hbmenu/Sphaira from the Album, hold R over a game) to settle the
unknowns marked "check" above.

### OLED results, 2026-10-24

Probe v1, EU nxlink build with the psel option ON, OLED, three profiles
(Larbaco, Glaupe, Isa), last opened = Isa. Logs stay out of git.

| Launch path | Applet type | Account service | Preselected user | `pselShowUserSelector` |
|---|---|---|---|---|
| Sphaira forwarder NSP (HOME) | Application (0) | ok | none (`0x1159`) | opened; chose Larbaco, rc=0 |
| Hold R over a game | Application (0) | ok | Larbaco (the system asked at launch) | opened (forced by the probe); chose Larbaco |
| Forwarder, B in the selector | Application (0) | ok | none | rc=`0x27c` (module 124, desc 1) = cancel, no user |
| Album (hbmenu/Sphaira) | LibraryApplet (2) | `accountInitialize(Application)` = `0x2c7c` | not asked | **untested** (probe stopped before it) |

`accountTrySelectUserWithoutInteraction` returned an invalid uid with three
profiles.

Conclusions:

- Forwarder = Application mode without a preselected user: the selector is
  needed, and it works.
- Hold R = a preselected user exists: no second prompt is needed. In the final
  code, hold-R must **not** show a second selector, because the preselected user
  exists (step 2 of the resolution order already does this; the probe opens psel
  only to test it).
- Cancel is detectable (`0x27c`, module 124 desc 1): the game exits, per the
  owner decision.
- Album: the Application account service is refused (`0x2c7c`) and psel is
  untested there. Probe v2 retries with `AccountServiceType_System`, runs the
  same queries if that works, and calls psel regardless of the account result;
  it also names `0x27c` as "cancelled". v2 results are pending (Album, once
  choosing a profile, once pressing B).

## Design

### Resolving the user (boot order: region first, then user)

1. `accountInitialize`. If it fails, use the **`shared` profile** (below) and log
   it.
2. `accountGetPreselectedUser`: valid, use it. No picker.
3. Else if `accountListAllUsers` returns exactly one user, use it (or
   `accountTrySelectUserWithoutInteraction`). No picker.
4. Else (several profiles, no preselect) call **`pselShowUserSelector`**, the
   Switch profile selector. It returns the uid the player picks. The screen,
   the names and the icons are the system's, so the game draws nothing here.
   A cancel is "no user" and the game does not start (see the
   [owner decision](#owner-decision-cancelling-the-profile-selector-2026-10-24)).
   An error is also "no user" (proposed: start nothing, log it, the player
   relaunches). Whether this
   works from applet mode (started from the Album) is **unverified: check on the
   OLED**. If the check shows that `psel` cannot launch in applet mode, the
   fallback is an **open question for the owner** (see
   [Open questions](#open-questions-for-the-owner)): an in-game picker over
   `accountListAllUsers` in the style of the region picker, or require title
   takeover so the preselect is present. Nothing is decided for that case.
5. The choice is remembered as `last_user=<uid>` in the global options.
   `user_default=last` skips the selector and uses it (below).

The region picker and the profile selector run before `dw3_game_main`; a forced
region and `DW3_MEMCARD_DIR` / `DW3_SAVE_PROFILE` (replays, tests) skip them.

### Directory layout

```
sdmc:/switch/dw3/
  us/ eu/                      game data, unchanged; memcard/ = legacy saves, kept
  port_options.cfg             global options (see below)
  saves/
    <uid>/                     32 hex digits: uid[0] then uid[1], %016llx each
      profile.txt              nickname and first-seen date, informational
      card0.dw3s card1.dw3s    the P6.25 canonical cards
      card0.dw3s.bak1..3       rotating backups (P6.25)
      imported.txt             legacy import record (P6.25)
    shared/                    no account service, or an unnamed fallback
```

The user folder is opened by a **soft binding**: nothing stops another user from
choosing someone else's profile in the selector, and the saves are plain files on
the SD card. The goal is separation, not security. Real Nintendo save data
(`fsdevMountSaveData`) is not used: an NRO has no title of its own to bind it
to, and the SD file model keeps saves editable and backed up by hand.

The crash report and the log carry the uid hash (first 8 hex digits), never the
nickname.

### PC

No Switch account. Profile = `default`, i.e. `runtime/saves/default/`
(`runtime` is the parent of `runtime/<ver>`), selectable with
`DW3_SAVE_PROFILE=<name>` or `--profile <name>`. The OS user name is **not** the
default: it changes between a container, a CI box and a desktop and would hide
the saves silently. `DW3_MEMCARD_DIR` still means "this directory is the store",
no profile.

### Migration of the existing saves

The legacy `memcard/saveN.bin` files belong to nobody, and on a family console
guessing the owner is wrong half the time. Proposal:

1. The P6.25 import ([CROSS_REGION_SAVES.md](CROSS_REGION_SAVES.md#migrating-existing-saves))
   runs **per user**, on first launch of a profile with no `imported.txt`, as a
   prompt right after the profile is chosen: "Found old saves (US: 2 slots, EU: 1).
   Import them for <nickname>?  [Import]  [Not now]".
2. Import is a **copy** (the originals are never touched), so a second user can
   import the same legacy saves later. Option row (a P6.20-style Options entry,
   or a hidden key) "Import old saves" re-runs it; "Not now" asks again next
   launch, "Never" writes `imported.txt` with that result.
3. The `shared` profile is where P6.25 puts saves before P6.26 exists
   (separate builds sharing one folder); at the first P6.26 launch, `shared/`
   is offered the same way and is copied into the user's folder, not moved.

### Options scope

**Recommendation: one global `port_options.cfg`**, not per user and not per
region. Reasons: most rows are console or display settings (Widescreen,
Fast-forward, volume), the file is tiny, the options screen is reached before
the player's saves are, and a per-user file makes "my widescreen setting
changed" depend on which profile was chosen. The region key and the user key must be global
by definition (they decide what loads).

- Location: `sdmc:/switch/dw3/port_options.cfg` (PC `runtime/port_options.cfg`);
  `DW3_CONFIG_DIR` still overrides it, so the replay runner is unchanged.
- Migration: if it does not exist, copy the file of the region that has one
  (last played region first) and write `imported_options=<region>` as a key so
  it is not repeated; never delete the old per-region files.
- New global keys: `region_default=ask|us|eu`, `last_region=us|eu`,
  `last_user=<uid>`, `user_default=ask|last`.
- If gameplay options (text speed, encounter rate, battle cursor) turn out to
  be personal, a later step can add `saves/<uid>/port_options.cfg` as an
  override of the same keys, using the same loader (unknown keys are already
  kept). Not needed now.

## Interactions

- **P6.25**: the container is the same; this item only decides the folder. Land
  P6.25 first with `shared/` as its only profile.
- **P6.24**: the region picker runs first; the profile selector second; the save
  folder is `saves/<uid>/` for whichever region boots.
- **P6.15 replays**: `DW3_MEMCARD_DIR` / `DW3_REPLAY` (the Switch replay build:
  `DW3_NX_REPLAY`) skip the region picker, the profile selector and the import;
  the recordings do not change. `tools/replay_ryubing.sh` runs in title-override
  or applet mode depending on the emulator; the single-user default makes it pick
  the user without UI -- check.
- **Crash / log (P5.4)**: add `profile: <hash>` next to the `disc:` line; no
  nickname.
- **Docs**: [RUNTIME_LAYOUT.md](RUNTIME_LAYOUT.md) and
  [PORT_OPTIONS.md](PORT_OPTIONS.md) change when this lands.

## Risks

- Applet mode with several users and no preselect is the common homebrew case;
  the profile selector covers it (if the OLED check passes, else see the open
  question), but a player who picks the wrong profile
  creates a second, empty save set (no data loss; they can re-import).
- A deleted and recreated profile has a new uid: its saves are orphaned in the
  old folder. `profile.txt` (nickname) makes the folder recognizable; a manual
  rename fixes it. Not automated.
- libnx behaviour differs by firmware; the first slice must log the applet type,
  the preselect result and the user count on the OLED.
- The account service can fail in unusual launch environments (Ryubing, some
  forwarders); the fallback is `shared/`, not an abort.

## Test plan

1. Unit tests on the PC for the profile path builder, the uid formatter, the
   options-scope resolver and the import first-fit, with a fake account
   provider (none, one user, three users, preselected).
2. A host seam (`dw3_switch_user_resolve()` returning a uid + reason) so the
   Switch code is a thin wrapper over the libnx calls.
3. OLED, each of: title-override mode with a preselected user; applet mode with
   one user and with several; a second profile created; the user deleted and
   recreated. Log lines: applet type, preselect ok/err, user count, chosen uid
   hash. Save and Continue in each profile, and check the other profile does not
   see the slot.
4. Replays unchanged (forced directory).

## Order

1. Seam + unit tests + PC `default` profile (no console).
2. Switch: account init, resolve order, `shared` fallback, log the OLED facts.
3. Profile selector call (or the fallback picker, if the OLED check fails) and
   the import prompt.
4. Global options file and its migration.

## Open questions for the owner

1. **Applet-mode fallback.** If the OLED check shows that `pselShowUserSelector`
   cannot launch from applet mode (Album): an in-game picker over
   `accountListAllUsers`, or require title takeover (the NRO started as a title
   override from hbmenu, where the preselect is expected)? Not decided.
