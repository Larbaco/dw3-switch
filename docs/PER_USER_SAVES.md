# P6.26 Per-user saves (design)

Owner decision (2026-10-24, recorded in [SINGLE_BUILD.md](SINGLE_BUILD.md)):
**saves are bound to the Switch user profile that is playing.** This note is the
design behind the [ROADMAP P6.26 entry](ROADMAP.md#phase-6-goal-notes).
Slice 1 is built, see [As built](#as-built-slice-1-2026-10-27); the rest below is the
design. It builds on [P6.25](CROSS_REGION_SAVES.md) (the canonical save container,
built as an opt-in store in slice 2) and P6.24 (one build) depends on it.

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
- Options: **one global** `port_options.cfg` (D3 / P6.26, owner 2026-10-31): `sdmc:/switch/dw3/port_options.cfg`
  on the Switch and the top-level file beside the region folders on the PC, shared by both regions and
  **every profile** (options are not per profile). `dw3_port_options_persist_init(gdir)` with
  `gdir = dw3_optcfg_global_dir(DW3_CONFIG_DIR, root)` (the parent of the `us/` / `eu/` runtime root;
  `DW3_CONFIG_DIR` overrides). The old `us/` and `eu/` files are merged into it once, backed up as
  `port_options.cfg.us.bak` / `.eu.bak` and deleted. Store: `port/options/options_cfg.c`; rules, log lines and
  the migration in [PORT_OPTIONS.md](PORT_OPTIONS.md#one-global-file-d3) and
  [OPTIONS_REORG 6.2](OPTIONS_REORG.md#62-d3-one-global-options-file-as-built).
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

The crash report and the log carry a short id: 8 hex digits of FNV-1a 32 over
the 16 uid bytes (`dw3_uid_short_id`, one helper for both). It is not a prefix
of the uid, because every Switch account uid starts with the same bytes
(`10000000...`), so a prefix tells profiles apart badly. The folder name stays
the full 32 hex digits. The nickname is never logged.

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

## As built (slice 1, 2026-10-27)

Design "Order" steps 1 to 3, adapted. **OLED-unverified**: the Switch account
and `psel` calls have only been compiled, never run on a console. Every step
logs its facts (below) so the first run answers the open "check" items.

- **Seam.** `port/saves/user_saves.c` (`port/include/port_user_saves.h`):
  `dw3_switch_user_resolve(provider, &out)` returns uid, key (32 hex, or
  `shared`) and a reason (`preselected`, `single-user`, `selector`,
  `shared(account-init-failed)`, `shared(no-users)`, `cancelled`, `error`).
  Unit tests with a fake provider: `tools/tests/test_user_saves.py`.
  `dw3_pc_user_resolve()` gives the PC profile (`default`, or
  `DW3_SAVE_PROFILE`).
- **Switch wrapper.** `platforms/switch/source/user_switch.c`, called from
  `main()` before the GPU is created: `appletGetAppletType`,
  `accountInitialize(AccountServiceType_Application)` (failure -> `shared`),
  `accountGetPreselectedUser` (the forwarder-with-selector case), the only user
  from `accountListAllUsers`, else `pselShowUserSelector`. Cancel or error:
  the game does not start, `main()` returns 0 to the launcher, and nothing is
  created. `accountExit` runs at shutdown. Replay builds (`DW3_NX_REPLAY`) and
  `DW3_MEMCARD_DIR` skip the whole step. If the profile folder cannot be made
  the game falls back to the legacy folder (and logs it).
- **Layout deviation from the design.** P6.25's `card0.dw3s` store does not
  exist, so the per-region flat format stays:

  ```
  sdmc:/switch/dw3/saves/<uid32hex>/        (PC: runtime/saves/<profile>/)
    profile.txt          nickname= and first_seen=YYYY-MM-DD, written once
    us/ eu/              one folder per region, as today
      imported.txt       what was copied from the legacy folder, and when
      memcard/           save0.bin save1.bin autosave.bin (+ .cheats sidecars)
  ```

  Everything `memcard_host` and `port/autosave` write follows
  `memcard_host_set_dir`, so no other code changed. **P6.25 slice 2 (2026-10-30)**
  adds the `card0.dw3s` / `card1.dw3s` store **inside the profile folder**
  (`saves/<uid>/card0.dw3s`, `imported.txt`, `*.bak`), opt-in with `save_store=1`;
  the `<region>/memcard/` folders stay (they are the migration sources, read only,
  and the saves of a build with the option off), so the layout above is unchanged
  ([CROSS_REGION_SAVES](CROSS_REGION_SAVES.md#as-built-slice-2)). The import prompt
  below is still future work.
- **Legacy copy (interim, safe).** When a profile's region folder does not exist
  yet, the legacy files of `sdmc:/switch/dw3/<region>/memcard/` (`save0.bin`,
  `save1.bin`, `autosave.bin` and their `.cheats` files) are **copied** (never
  moved or deleted) into it. The folder is built under `<region>.tmp` and
  renamed, so a stop half way leaves no half-imported region. The same is done
  for the other region, so the P6.25 cross-region import keeps working from the
  user's own folder (`DW3_MEMCARD_IMPORT_DIR` is pointed at it; the legacy
  folder is never an import source for a profile). **Every profile that starts
  the game the first time gets a copy of the legacy saves**; on a shared console
  that means the second player starts with the first player's saves. The
  interactive "Import for <nickname>? [Import] [Not now]" prompt from the
  [migration section](#migration-of-the-existing-saves) is **future work**.
- **Options** stay per region (`port_options.cfg` in the runtime root); the
  global file and its migration (design step 4) are not part of this slice.
- **PC.** The default profile is `default`: `runtime/saves/default/<region>/memcard/`,
  filled with a copy of `runtime/<ver>/memcard/` the first time.
  `DW3_SAVE_PROFILE=<name>` picks another. `DW3_MEMCARD_DIR` / `--memcard-dir`
  still mean "this directory is the store" (replays).
- **Log lines** (`dw3.log`, Switch; the PC prints `dw3-pc: user: ...`):
  `user: applet type=<name> (<n>)`, `user: accountInitialize rc=`,
  `user: accountGetPreselectedUser rc= valid=`,
  `user: accountListAllUsers rc= count=`,
  `user: showing the system profile selector`,
  `user: pselShowUserSelector rc= (module desc) valid=`,
  `user: profile=<8 hex> reason=<reason> init_rc= preselect_rc= users= selector_rc=`,
  `user: <region> migration: new region folder, copied N legacy file(s) from <dir>`
  (or `region folder already exists: no migration`), `user: save dir=<path>`,
  and on a cancel `user: result=cancelled ... the game does not start; nothing written`
  followed by `no user chosen: leaving without starting the game`. The uid is
  logged only as its short id (8 hex digits, FNV-1a of the uid, see above), never
  the nickname. The crash report has `profile: <8 hex>` after the `disc:` line
  (`n/a` before a user is chosen). Two profiles must show different short ids.
- **Not known until the OLED run:** whether libnx gives a distinct result code for
  a cancelled selector (cancel and error are handled the same way); whether
  `pselShowUserSelector` works from applet mode (hbmenu started from the Album).
  If it fails there the log shows `reason=error` and the game does not start;
  the open question for the owner (in-game picker or title takeover) then applies.

## History moved from ROADMAP.md (2026-10-24)

Moved from the ROADMAP.md entries on 2026-10-24, word for word (two leading
spaces removed). "Above" and "below" refer to the old entry. The roadmap
keeps the current state and links here.

**Today:** one save folder per region (`sdmc:/switch/dw3/<ver>/memcard/`,
`boot_main.c` ~531; PC `runtime/<ver>/memcard/`) and `port_options.cfg` per
region; the Switch host has no account or applet-type code.
**Design.** libnx `accountInitialize(AccountServiceType_Application)`; the user
is `accountGetPreselectedUser` (title-override mode, expected), else the only
user, else the **Switch profile selector** (`pselShowUserSelector`; applet mode
has no preselected user, and whether the selector launches there is checked on
the OLED), else the `shared` profile. **Cancel:** if the player cancels the
selector the game does not start; it exits cleanly back to the launching menu,
with nothing created or written (owner decision 2026-10-24, "Se o jogador
cancelar a escolha, O jogo não deve ser iniciado";
[PER_USER_SAVES.md](PER_USER_SAVES.md#owner-decision-cancelling-the-profile-selector-2026-10-24)). Saves go to
`sdmc:/switch/dw3/saves/<uid>/card0.dw3s, card1.dw3s` (uid = 32 hex digits;
soft binding, not security). PC: profile `default`
(`runtime/saves/default/`, `DW3_SAVE_PROFILE` to change). Legacy saves are
**copied** to the user who accepts a prompt at first launch (second users can
import later; originals untouched). **Options stay global** (recommended): one
`port_options.cfg` next to the region folders, with the new keys
`region_default`, `last_region`, `last_user`, `user_default`; migrated by copying.
Replays and `DW3_MEMCARD_DIR` skip the profile logic.
