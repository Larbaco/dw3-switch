# Balloon NULL actor crash (patch 0058)

Status: fixed on the PC host and in the Switch build (checked on Ryubing, US
and EU); OLED pass pending. Found in the preview.28 / preview.29 Ryubing sweep (2026-10-22).

## Symptom

New Game on either disc, walking down the Digital World bridge to the gate
where the hero is stopped ("Oh yeah! I need my partner Digimon data!"): the
game dies in `FIELDSTG_drawBalloon` (`src/field/field_mode/balloon.c`),
called from `FIELDSTG_updateBalloon` through `executeTask` / `runChildTasks`.
Deterministic. PC (`dw3-pc`) dies with SIGSEGV at the same spot; on the
Switch build Ryubing stops with "Invalid memory access at virtual address
0x0".

## Root cause

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

## Behaviour chosen

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

## Other hazards of the same shape (not changed)

A pointer set by a script command and read without a NULL check, where the
PS1's readable low RAM hides a NULL:

| Where | Hazard |
|---|---|
| `src/field/stages/kusanagi_city/wstag785.c` `drawActorMark` / `handleCommand807` | same pattern: `actor` is set by `0x328/0x329` and read by every draw; a `0x32A` close before an open would fault. The shipped scripts are balanced (`wstag785.c:495-516`), so it does not trigger |
| `src/field/field_mode/camera.c` `FIELDSTG_followWithCamera` | `target = find(FIELD_TASK_ACTOR, id, -1)` is NULL for an absent character; `FIELDSTG_updateCamera` then reads `target->tile` every frame. `SCRIPT_FOLLOW` ids all come from the stage scripts; none known to be absent |
| `src/field/field_mode/script.c` `FIELDSTG_waitAnimDone` / `waitWalkDone` | `FIELDSTG_findActor(id)` result used without a check (`event.c` has its own NULL checks) |
| `FIELDSTG_clearScriptFlag` (`script.c`) | tries character 1, then 2, and derefs the result if both are absent |
| `update_event9015.inc.c:22`, `wstag926.c:54`, `wstag680.c:98/120`, `battle.c:19/43`, `field.c:234`, `map_streamer.c:244` | `find(FIELD_TASK_ACTOR, -1, 0)` (the player) used unchecked; the player exists whenever these run |

Other `->actor` readers in the balloon path are safe: `FIELDSTG_updateBalloon`
`TASK_INIT` falls back to the player and stays in `TASK_INIT` if there is
none; `FIELDSTG_balloonCommand` only writes. The `ActorChildren.balloon`
users in `trigger.c` and `actor_control.c` check or create the pointer
before use. An `actor` that is destroyed while a balloon still points at it
(stage change with a live balloon) was not examined.

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
