# NULL reads the PS1 tolerated (patches 0058 and 0059)

(The file keeps its old name, `BALLOON_NULL_ACTOR.md`, so links stay valid.)

On the PS1, reading through a NULL pointer does not fault: low RAM is
readable kernel memory, so the original game silently survived a few
script-driven lookups that return NULL. On a 64-bit host (PC and Switch) the
same read is a SIGSEGV or a data abort. This note covers the one that
crashed in the field (the balloon, patch `0058`), the audit of the rest of
the shape, and the preventive guards of patch `0059`. Rules used for every
site:

* Only **reads** are guarded. A NULL that the PS1 would then **call through**
  (`ptr->setState(...)`, a function pointer read from kernel RAM) or write
  through would have crashed or hung the PS1 as well, so shipped data cannot
  contain it and the port has nothing to preserve.
* The guard does what the PS1 effectively did when that is knowable (draw
  nothing), otherwise the most conservative no-op, and says so.
* Only `PORT_HOST` code changes; the non-NULL path is untouched.

## Patch 0058: balloon NULL actor crash

Status: fixed on the PC host and in the Switch build (checked on Ryubing, US
and EU); OLED pass pending. Found in the preview.28 / preview.29 Ryubing sweep (2026-10-22).

### Symptom

New Game on either disc, walking down the Digital World bridge to the gate
where the hero is stopped ("Oh yeah! I need my partner Digimon data!"): the
game dies in `FIELDSTG_drawBalloon` (`src/field/field_mode/balloon.c`),
called from `FIELDSTG_updateBalloon` through `executeTask` / `runChildTasks`.
Deterministic. PC (`dw3-pc`) dies with SIGSEGV at the same spot; on the
Switch build Ryubing stops with "Invalid memory access at virtual address
0x0".

### Root cause

* The stage is WSTAG210 (mode `0x203`, stage file 453 US / 468 EU). The
  gate is event `0x1E`, `script30` in
  `src/field/stages/asuka_city/wstag210.c`.
* The script shows the hero's "!" balloon with script command `0x323`
  (`FIELDSTG_createPlayerBalloon` / `FIELDSTG_balloonCommand`): `0x325`
  opens it over the actor of a character id, `0x326` pops it shut. Near the
  end it sends `0x326` **a second time**, after the balloon already closed
  and was destroyed.
* `FIELDSTG_setEventPose` then finds no task with id `0x323`, creates a new
  `Balloon` and hands it the close at once. `FIELDSTG_balloonCommand` only
  sets `TASK_DONE` for `0x326`; it does not touch `actor`. The task goes
  `TASK_INIT` -> `TASK_DONE` without `TASK_INIT` ever running, so `actor`
  stays NULL (and `popEnd` stays 0).
* `FIELDSTG_updateBalloon` in `TASK_DONE` pops the balloon shut (immediately,
  `popEnd == 0`), sets `TASK_KILL` and calls `FIELDSTG_drawBalloon` once.
  That reads `task->actor->tile.x` (offset `0x58`), `tile.y` (`0x5C`) and
  `z` (`0x64`).
* On the PS1 the read goes to low RAM (kernel area), which is readable, so
  the original draws one frame of sprite 0 at whatever those words hold and
  carries on. On a 64-bit host `NULL + 0x58` is unmapped.
* The same script is stray-close free on the other `0x323..0x326` users: a
  scan of every `SCRIPT_COMMAND(0x323..0x326, 0x325|0x326|0x327, id)` array
  found no other close without a preceding open (WSTAG331 `script970` looks
  like one but is balanced per `VERSION_US` / `VERSION_EU`).

### Behaviour chosen

Under `PORT_HOST`, `FIELDSTG_drawBalloon` returns when `task->actor` is
NULL. The update around it is untouched, so the stray balloon is still
killed in the same frame; non-`PORT_HOST` code is unchanged.

The struct comment says `Balloon.actor` is "the player when NULL", and
`TASK_INIT` implements that. It was **not** used at draw time: a NULL actor
in `TASK_DONE` means `TASK_INIT` never ran, i.e. this was never a balloon
meant to be seen, and the original shows nothing of it (the stray sprite 0
lands wherever kernel RAM points, not on the player). Falling back to the
player would add a one-frame sprite 0 over the hero that the PS1 never
drew. Skipping is also the smallest change. What the PS1 words at
`0x58/0x5C/0x64` hold was not measured (no PS1 RAM dump here); the choice
rests on the code path, not on that value.

## Audit of the same shape (patch 0059)

Method: (1) every lookup that returns a task or actor pointer
(`TASK_REGISTRY.funcs.find`, `FIELDSTG_findActor`, `findNext` loops) in
`src/field`, `src/battle`, `src/engine` was listed (~100 sites) and an
automated pass flagged those whose result is dereferenced within 25 lines
without an `== NULL` / `!= NULL` / `if (p)` test (24 candidates, all read by
hand); (2) a data scan of every stage script (`SCRIPT_*` arrays, 131 files,
365 scripts per disc, 1487 `SCRIPT_COMMAND` words, 328 `SCRIPT_FOLLOW`s, US
and EU, `#if VERSION_*` resolved with `cpp`); (3) the actor-presence
conditions of the stand-in table, where the answerer can depend on game
state. Ranking is by "can shipped data or state produce NULL here":

| Site | NULL source | Shipped data / state | Decision |
|---|---|---|---|
| `balloon.c` `FIELDSTG_drawBalloon` (0x323..0x326) | close (0x326) with no open | **yes**: WSTAG210 `script30`, the only close-without-open in the 0x323..0x326 scripts (US and EU) | patched in 0058 |
| `wstag785.c` `drawActorMark` (0x327; 0x328/0x329 open, 0x32A close) | close with no open, or open for an absent character | no: the one script that uses it (event 1, progress 0) opens before it closes and names actor 0xC, which the stage lists for progress 0 | patched in 0059 (read-only, same shape and same protocol as the balloon; cheap) |
| `camera.c` `FIELDSTG_updateCamera` via `FIELDSTG_followWithCamera` (`SCRIPT_FOLLOW`) | `find(ACTOR, id)` is NULL when the character is not on the map | not found: all 328 follow ids exist in their stage's actor list (or are the player, 2); WSTAG260's story scripts follow 0x1E, 0x35..0x37, 0x39 from `FIELDSTG_actors`. Whether an entry's flag/item/progress conditions hold at the moment the event runs was not modelled | patched in 0059: the camera holds its position |
| `script.c` `FIELDSTG_waitAnimDone`, `waitWalkDone`, `clearScriptFlag` | `FIELDSTG_findActor` unchecked | **dead code**: they sit at indices 2 to 4 of `FIELDSTG_scriptHelpers`, and nothing in the decomp ever calls index 1 to 4 (only `[0]`, `FIELDSTG_toScreenPos`, from `speech.c`); `ScriptTimer.findActor` is never read; the scripts' own waits (`SCRIPT_WAIT_WALK`/`WAIT_ANIM`) go through `FIELDSTG_findEventActor`, which is checked | left |
| `actor_control.c` `FIELDSTG_talkToActorAt` (stand-ins) | answerer actor absent | state dependent (e.g. WSTAG270's 0xDC always exists, its answerer 0xCE only for some item/progress states) but the next lines call `target->setSubstate`, a function pointer: the PS1 would jump to a garbage address | left: not a tolerated read, cannot be in shipped play |
| `battle.c` `FIELDSTG_startAreaBattle`, `FIELDSTG_countBattleSteps` | player lookup | the player exists whenever the field steps; `FIELDSTG_checkBattle` is called from the player actor's own step control (`actor_control.c`) | left |
| `field.c` `FIELDSTG_closeField` | player lookup | already guarded (`player != NULL` decides `centerOnPlayer`, the later read is conditional on it); the warp path kills the player first and relies on that | left |
| `lift.c:50`, `update_tile_lift.inc.c:44`, `wstag926.c:54`, `wstag680.c:98/120`, `update_event9015.inc.c:22`, `map_streamer.c:244`, `commands.c:184` (icon task) | player / singleton task lookups | the player or the singleton exists for as long as these tasks run (the field task creates the party before its children draw; the warp path kills the player and the map streamer in the same step); the map streamer draws on every frame of every field replay | left |
| battle: `FIGHTSTG` models / camera / stage lookups (`entrance.c`, `camera_task.c`, `shot_camera.c`, `digimon_change.c`, `partner_view.c`, `menu.c`, `battle_script.c`, `camera_turn.h`) | singleton task | created by the battle setup before any of these run; no script data picks them | left |
| `hidden_spots.c`, `trigger.c`, `balloon.c` `TASK_INIT`, `actor_control.c` `followLeader`/`drainTrail`/`haltPartners`, `event.c`, `actor.c` | actor lookups | not flagged by the pass: each is tested for NULL before use | nothing to do |

What the guards do, and what is not known:

* `drawActorMark` returns when `actor` is NULL; the update's state machine
  still runs, so the mark pops shut and ends exactly as before. The PS1 draws
  one off-map frame of the mark (kernel RAM position), the host draws nothing.
* `FIELDSTG_updateCamera` (follow substate) keeps `center` when `target` is
  NULL. The PS1 would centre on whatever words kernel RAM holds at the
  `Actor.tile` / `z` offsets and then clamp to the map; that value was not
  measured (no PS1 RAM dump here), and holding is the smallest change. A
  target that is destroyed while the camera still follows it is a dangling
  pointer (stale, not NULL) and is not handled.
* `FIELDSTG_createBalloon`'s balloon `actor` that is destroyed under a live
  balloon, and `FILE_CACHE.getEntry` returning NULL for an uncached file in a
  draw call (a much larger class: every `sprite.draw(getEntry(...))`), were
  not audited.

### Diagnostic

`port/include/port_null_guard.h` defines `DW3_NULL_GUARD_HIT("site")`: with
`DW3_NULL_GUARD_LOG` defined it prints `dw3: null-guard hit: <site>` on
stderr once per site and process, otherwise it expands to nothing. The CMake
option `DW3_NULL_GUARD_LOG` (`port/CMakeLists.txt`) defaults to ON for PC
Debug builds and for the Switch `--nxlink` build (`DW3_NXLINK_STDIO`), OFF
for release builds; `-DDW3_NULL_GUARD_LOG=ON|OFF` overrides. Sites:
`balloon.draw-null-actor` (the 0058 guard; fires on the new-game bridge, so it
doubles as a positive control), `wstag785.mark-null-actor`,
`camera.follow-null-target`. Grep an OLED nxlink log or a PC stderr log for
`null-guard hit` to see whether the preventive guards ever fire in play.

## Regression tests

* PC: `tools/replay_tests.sh --only us-bridge-partner-gate --only
  eu-bridge-partner-gate` ([P6.15_REPLAY_TESTS.md](P6.15_REPLAY_TESTS.md)).
  Without patch `0058` both scenarios die with SIGSEGV between the `gate` and
  `dialogue` checkpoints (US and EU); with it they run to the end, and a
  second run against the first run's references is byte-identical (0 px).
  The draft PR #186 scenario `us-repro-balloon` (a different US walk) also
  crashes without the patch and runs clean with it.
* Switch (Ryubing 1.3.3, private Xvfb and data root, keys held with xdotool,
  2026-10-22): B-spam through the opening, then the walk. With the patch: US
  2 of 2 and EU 2 of 2 reach the gate, show the "!" balloon and the dialogue,
  close it with B and keep playing; no crash report. Without it (preview.29
  EU NRO): Ryubing aborts at the gate with "Invalid memory access at virtual
  address 0x0". The walk is wall-clock timed, so it is a smoke test, not a
  regression gate; the PC replays are the gate.
* Not covered: OLED, the other four EU languages (the walk and the patch are
  language independent, but only English was driven), `--replay` Switch
  build of #186.

## Regression tests for patch 0059 (2026-10-22)

* PC US and EU release builds, `tools/replay_tests.sh` (quick + full tiers:
  title, EU languages, new-game field, field walk, load menu, both
  bridge-gate scenarios, `us-repro-balloon`) against the references made
  before the patch: 207 checks, 0 failed, every capture 0 px different, all
  process and overlay-marker verdicts PASS. Soak scenarios were not run.
* PC US build with `-DDW3_NULL_GUARD_LOG=ON` on `us-bridge-partner-gate`:
  still 0 px different, and the log has exactly one line,
  `dw3: null-guard hit: balloon.draw-null-actor`. The mark and camera guards
  did not fire in any replay (as the data scan predicts).
* Switch: `tools/build_switch_game.sh` US, `--eu` and `--nxlink` build; the
  release NROs contain no `null-guard hit` string, the nxlink NRO has the
  three. Not run on Ryubing or OLED: the guards are on paths the shipped
  scripts do not take, so there is nothing to observe beyond the build.
