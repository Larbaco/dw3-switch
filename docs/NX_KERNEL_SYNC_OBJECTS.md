# `KernelInit: cannot create the kernel synchronisation objects`

Status: **open defect, hypothesis documented (2026-10-16)**. Logged once
during PSY-Z init on the preview.7 OLED boot (see the preview.7 section in
[STATIC_SD_LOAD.md](STATIC_SD_LOAD.md) and `docs/RELEASES.md`); the game
continued and it is listed as an open P5.2 follow-up. The failed object
(mutex or event) was not identified because nothing logs it.

## The code path

`vendor/psyz/psyz/src/psyz/kernel.c` `KernelInit()` creates four objects
and logs the aggregate warning if any is NULL:

| Object | Creator (Switch) | How it can fail |
|---|---|---|
| `post_lock` | `Psyz_OsLockCreate()` — `nx_os.c` | `malloc` OOM only |
| `kernel_lock` | `Psyz_OsLockCreate()` — `nx_os.c` | `malloc` OOM only |
| `bell_events` | `Psyz_OsBellCreate()` — `nx_os.c` | `malloc` OOM, **or libnx `eventCreate` failing** |
| `bell_irq` | `Psyz_OsBellCreate()` — `nx_os.c` | same as `bell_events` |

Note `KernelInit()` early-returns once `kernel_lock` is set, so a partially
created set is never retried; a NULL bell stays NULL for the process.

## Hypothesis (highest probability first)

1. **`svcCreateEvent` failed for one of the bells.** `Psyz_OsLockCreate` is
   a plain `malloc` + userland `mutexInit` — it cannot fail on hardware
   except on heap exhaustion, and a 60 MB+ homebrew heap was available at
   boot. `Psyz_OsBellCreate` however calls libnx `eventCreate`, which maps
   to the privileged `svcCreateEvent` syscall (`nx/source/kernel/event.c`);
   on `R_FAILED` it frees and returns NULL. On the kernel side
   (libmesosphere `CreateEvent`) creating an event reserves from the
   process resource limit (`LimitableResource_EventCountMax`) and returns
   `ResultLimitReached`/`ResultOutOfResource` when exhausted. The .nro's
   event budget at boot is shared with EGL, fsdev and other service
   events; a one-off transient exhaustion matches "logged once, boot
   continued".
2. **OOM at first PSY-Z init.** `KernelInit` runs very early (first
   `KernelLock`), but if it fired before the host had committed the
   main heap unused-region, `malloc` could still return NULL. Unlikely
   for 4 × small structs.

So: an **event** (bell), not a mutex, is the likely failed object — the
mutex path has no other failure mode on Switch.

## What the failure costs

`nx_os.c` tolerates NULL bells (`Psyz_OsBellRing`/`Psyz_OsBellWait` guard
`if (bell)`), so PSY-Z's bell-based waits (kernel event loop, IRQ thread
signaling) become no-wait returns. The game booting fine after the
warning is consistent with that, but it should not be trusted as "harmless
forever" until identified.

## Fix direction (ours, not vendored)

`nx_os.c` is where the Result is swallowed. Add one `psyz_log` line per
failed object — OsLockCreate's OOM case and OsBellCreate's
`eventCreate` rc (`DW3_NX_ERROR("OsBellCreate: eventCreate failed rc=0x%x",
rc)`) — and record which object it was, so the next hardware log answers
"mutex or event" in one line. If it is confirmed as `svcCreateEvent`
limit exhaustion, options are: create the two bells earlier/later in the
boot order, or trim event-hungry init between them; both are backend
decisions that need the rc in a log first.
