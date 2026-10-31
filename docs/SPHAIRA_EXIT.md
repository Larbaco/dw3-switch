# BUG-1: the crash on exit when started from Sphaira

Status: **hardening and proof lines shipped; the cause is a strong hypothesis
that only the console can confirm** (no console access for the author; the
author could read the crash reports, the logs, our code and the Sphaira and
libnx sources). The OLED steps that settle it are at the end (row **G13** in
[OLED_TEST_PLAN](OLED_TEST_PLAN.md)).

## What the owner sees

p56, Switch OLED, US. Leaving by **Quit game > Yes** and by **HOME** both
end with an Atmosphere report, **only when the game was started from
Sphaira's full app**. A Sphaira-made forwarder for the same `.nro` is fine.
`dw3.log` ends normally (`dw3_game_main returned 0; shutting down`, `audio
stopped`, `gpu closed; leaving main`): the crash comes **after `main` returns**.
`crash_report.txt` is empty because the break is not in our code.

Evidence: `tmp/evidence/20261030-1613-p56-crash/` (not in git). Both reports:

| Field | Value |
|---|---|
| Result | `0xD401` = `2001-0106`, a kernel result, "invalid current memory" (the memory state or permission of a range does not allow the call) |
| Exception | User Break (`svcBreak`), `Break Reason 0x0` (panic), PC `hbl + 0x4044`, LR `hbl + 0x3c7c`, the main thread |
| Module list | one module, `hbl`, 0x6000 bytes: the **loader**, not our NRO (our code is in the NRO region, `0xea0...` in the return addresses) |
| Program ID | `05446530aca7e000`: the `0x05...` prefix is the title ID class Sphaira gives the forwarders it installs |
| Threads | `Number of Threads: 01`: when it broke, only the main thread was alive |

## What `hbl` is here

Sphaira ships **its own** loader, `hbl/source/main.c` in
[ITotalJustice/sphaira](https://github.com/ITotalJustice/sphaira) (a slimmed
nx-hbloader, "this uses 16KiB less than nx-hbloader"). The Sphaira app runs
as an application (the `0x05...` program ID above); to start a homebrew `.nro` it calls
`envSetNextLoad` and returns, and `hbl` maps the new NRO into the **same
process**. This is the flow in `hbl`'s `loadNro()`:

1. Our NRO returns from `main`, libnx runs `exit` and the atexit handlers,
   `__appExit`, then jumps back into `loadNro()` (the trampoline).
2. `g_nextArgv` still holds the "exit detection" string, so it is not a
   NextLoad. The next NRO is `g_defaultNroPath`, Sphaira itself (the "return"
   path) when the game was started from Sphaira.
3. `loadNro()` unmaps our NRO (`svcUnmapProcessCodeMemory`, failures are
   `Module_HomebrewLoader` 24/25/26, **not** the raw `0xD401`).
4. It then opens the next NRO and does
   `fsFileRead(&f, 0, start, g_heapSize, ...)` with `start` at
   `g_heapAddr`: **one IPC read over the whole process heap**, the same heap
   our libnx `malloc` and our thread stacks live in. Any failure there is
   passed raw to `diagAbortWithResult(rc)`, which is the `svcBreak` the
   reports show.

If instead the NRO was started by a **forwarder for our NRO**, `g_nextNroPath`
equals `g_defaultNroPath` after the exit, so `loadNro()` takes `selfExit()`
(an applet `Exit` call and `svcExitProcess`) and never reaches step 4. That is
why the forwarder is fine and the full-app launch is not, and why the crash is
after `main`.

## Most likely cause

**Worker threads that finished but were never `threadClose()`d.** libnx
`threadCreate` with no stack allocates the stack from the heap and
`svcMapMemory`s it to a stack address; the kernel then reprotects the source
heap pages to `Perm_None` and marks them borrowed
(`svc.h`: "Source range gets reprotected to Perm_None ... MemAttr_IsBorrowed
is set"). Only `threadClose()` (`svcUnmapMemory`) gives the pages back.
`hbl`'s read over the whole heap then hits `Perm_None` pages and the kernel
fails the IPC with a memory-state result: `0xD401`.

Threads this port started and never closed before `main` returned (all had
finished by then, which matches "1 thread" in the report):

| Thread | Where | Closed before |
|---|---|---|
| map-view precompute (core 2) | `boot_main.c` `s_map_thread` | never |
| options prewarm (core 2) | `nx_title_overlay.c` `s_prewarm_thread` | only when the next one was created, so the last one was never closed |
| map scaler workers (core 2, 2 slots) | `boot_main.c` `s_map_slots` | only on the next start of that slot |
| PSY-Z IRQ thread | `nx_os.c` (`Psyz_OsThreadStart`) | never; it loops forever, so it would still be alive. Not started in the p56 logs (no `thread started` line), but possible whenever the kernel bells exist |
| audio mixer (core 1) | `nx_audio.c` | joined and closed (P38) |

Why "only Sphaira": nothing else reads the heap after us. A forwarder ends the
process (below). The stock hbmenu path also reloads a menu NRO over the heap,
so it may be affected too: the p56 repro line says "opened from hbmenu" and the
owner's later decision says only Sphaira, and the report cannot tell. The
`dw3-exit: loader: info=` line and the final heap probe say which loader ran
and whether the heap was clean.

Why it was not seen at p41 to p47 with the full app ("shows errors", OLED A7):
that also had the then-current threads; the list above grew since, and the
exact set at p41 was not checked. The logs of the next run decide.

### Other suspects, and why they are not the lead

| Suspect | Verdict |
|---|---|
| `envSetNextLoad` / NextLoad left set | Only called when a region relaunch was asked (P6.38) and then it is meant. Not in the crashing runs (`quit path: title Quit game`, `HOME`). |
| nxlink socket not closed | Not opened without a host (`dw3: no nxlink host`); `dw3_nxlink_stdio_shutdown` closes it when it was. |
| Transfer memory (socket, other) | None allocated in that run. |
| `__nx_applet_exit_mode`, `appletSetFocusHandlingMode` | Not set (`appletLockExit`/`appletUnlockExit` are paired). |
| Heap size / `__nx_heap_size` override | No override; libnx uses the loader's heap override. |
| The kernel event limit (`OsBellCreate: eventCreate failed rc=0x10801`, OBS-7) | `0x10801` is the kernel "limit reached" result for `svcCreateEvent`; it shows the process starts with its event budget used up, which is a Sphaira-side fact. It is a different result from `0xD401`; it may point at kernel objects that outlive an NRO, which the new `dw3-exit:` lines do not measure. Noted, not acted on. |
| romfs / fs not unmounted | `fsdev` is the libnx default (`__appExit`); the SD log is closed (now at the very end). |

## What changed

No decomp patch. Files: `platforms/switch/psyz_backend/nx_exit.{c,h}` (new),
`nx_os.c`, `nx_audio.c`, `nx_log.c`, `nx_title_overlay.{c,h}`,
`platforms/switch/source/boot_main.c`, `port/map_view/map_view.{c,h}`.

- Every worker thread is **joined and `threadClose()`d before `main`
  returns**, in `dw3_nx_workers_shutdown()`: map precompute (asked to stop at
  its next safe point, `dw3_map_view_precompute_cancel`, at most one scaler
  build of 2 to 6 s), the two scaler slots, the options prewarm thread, and
  the PSY-Z threads (a stop flag the thread sees at its next bell wait, plus a
  wake, then join). The wait is bounded (`DW3_NX_EXIT_BUDGET_MS`, 10 s in
  total, under HOME's 15 s); a thread still running after that is logged by
  name and left (the stack stays mapped, so that case can still crash).
  No worker starts after this point.
- `timeInitialize` in the log header is paired with `timeExit`.
- `dw3.log` is closed by the **last atexit handler** (`dw3_nx_exit_final`),
  so the final heap probe and the "leaving to the loader" line are written;
  before, the log was closed while handlers still ran.
- The same cleanup runs when `init_paths` fails (it left the audio and EGL
  open and the workers running).
- No change to NextLoad: it is still only set when the player says Yes to a
  region relaunch.

### The proof lines (`dw3-exit:`)

Every step of the exit path logs, so the next run proves the order and the
outcome. In a normal Quit game, in this order (`dw3.log` tail):

```
dw3_game_main returned 0; shutting down (quit path: title Quit game)
dw3-exit: game loop ended (return 0); closing in reverse order
dw3-exit: heap probe (before cleanup): N region(s) ..., M abnormal ...      <- the leaks, EGL still open
dw3-exit: memcard flushed
dw3-exit: thread map precompute joined and closed in 0 ms (threadClose rc=0x0)
dw3-exit: thread map scaler slot 0 joined and closed ...                    <- only if the map view built a scaler
dw3-exit: thread options prewarm joined and closed in 0 ms (threadClose rc=0x0)
dw3-exit: thread psyz os: none started (nothing to close)
dw3-exit: worker threads closed in N ms, 0 still running
dw3-exit: thread audio mixer joined and closed
dw3-exit: audren closed
audio stopped
gpu closed; leaving main
dw3-exit: gpu closed (EGL terminated), user service closed next
dw3-exit: nxlink: socket service never opened (no nxlink host)
dw3-exit: appletUnlockExit done
dw3-exit: heap probe (after cleanup): N region(s) ..., 0 abnormal (plain read/write heap all through)
dw3-exit: heap probe (final, after the exit handlers): ..., 0 abnormal ...
dw3-exit: leaving to the loader: return 0, envHasNextLoad=1, N ms since the game loop ended
```

The **heap probe** walks the loader's heap (`svcQueryMemory` over
`envGetHeapOverrideAddr/Size`) and prints every region that is not plain
read/write heap with no borrow, IPC or device reference (`ABNORMAL 0x<addr>
+0x<size> type=0x5 perm=0 attr=0x1 ...` is a borrowed thread stack). It is
the state `hbl`'s read needs: **0 abnormal in the final probe = the heap is
clean**. If the Sphaira run still crashes with a clean final probe, the
heap is not the cause and the abnormal list is empty, which is an answer too.

At startup one block says who started us (also useful for NextLoad):

```
dw3-exit: loader: info='sphaira <version>' envHasNextLoad=1 (a relaunch can be queued) lastLoadResult=0x0 applet_type=0
dw3-exit: loader: heap override=1 addr=0x... size=N MiB (the next NRO is read over this range when we return)
```

## NextLoad and the P6.38 region relaunch under Sphaira

- libnx `envHasNextLoad()` is true when the loader passes the NextLoadPath
  config entry. Sphaira's `hbl` always does (`entries[5]` in its `loadNro`,
  pointing at its own `g_nextNroPath` and `g_nextArgv`), for the full app and
  for a forwarder alike; so does nx-hbloader. It is false only when the code
  runs as a plain title with no homebrew loader. So `envHasNextLoad()` is
  honest in the sense that matters: when it is false nothing can be queued
  and the "Restart the game to apply." notice shows (A17.4); when it is true,
  `hbl` will load what we queue. The new `dw3-exit: loader:` line logs the
  answer for every start, so the claim is checked on the console.
- The documents said a **forwarder** has no NextLoad ("A17.4"): from
  Sphaira's source that is **not so for a Sphaira forwarder** (its loader is
  the same `hbl`), only for a forwarder that is not hbl-based. Unverified on
  the console: row G13 asks for the line.
- With a Sphaira forwarder for the NRO, a queued relaunch works in `hbl`: it
  sees a changed `g_nextArgv`, loads the queued path (the NRO again, with
  `--relaunch-region`), and when that run ends `g_nextNroPath` equals
  `g_defaultNroPath`, so `selfExit()`.
- A relaunch goes through the same unmap and read-the-whole-heap step as the
  crash above, so **before this change a Sphaira relaunch very likely hit
  BUG-1 too**; the hardening covers it. A17.2 should be re-checked from
  Sphaira (row G13, step 4).

## Ryubing

Ryubing runs the NRO directly (HLE) and has no hbl, so there is no
unmap-and-reload step and BUG-1 cannot show there. **Not run for this
change:** the only quit scenario (`us-title-quit`) is PC-only
(`replay_ryubing.sh` skips it: "the game quits by itself"), and the Ryubing
seed root (keys, firmware, profile: `/mnt/cabal/tmp/dw3-preview29-ryu`) no
longer exists on the build machine, so a start-and-quit run was not cheap. A
Ryubing run could only check that the new exit lines appear and no thread is
"still running"; it could not show the Sphaira crash.

PC: `tools/replay_tests.sh --changed --jobs 2 --fail-fast` (EU default, the
canary, both regions where selected, `--bin-both`): 81 scenarios, 1396 checks,
0 failed. The PC host's exit path is untouched (the changes are Switch-only
apart from the map-view cancel flag and the Game API table).

## OLED steps (row G13)

See [OLED_TEST_PLAN](OLED_TEST_PLAN.md) row **G13**: one run started from
Sphaira's full app and one from the forwarder (or hbmenu), each leaving by
Quit game and by HOME, then compare the `dw3-exit:` lines and the
Atmosphere reports.
