# Reporting issues (testers)

How to turn something you saw on the Switch into an issue that can be fixed.
The [bug report form](https://github.com/Larbaco/dw3-switch/issues/new?template=bug_report.yml)
asks for the same fields. This page explains them and shows a filled example.

Never attach disc images, BIOS files, extracted game data or save files that
contain them.

## Before you open one

1. **Check the release.** Use the latest preview from
   [RELEASES](RELEASES.md). That page also lists builds to avoid.
2. **Search open issues.** If the bug is already there, add your details as a
   comment instead of opening a new one. A second build, region or console is
   still useful.
3. **Try to reproduce it.** Note whether it happens every time, sometimes, or
   only once.

## What to include

| Field | Why it matters | Example |
|---|---|---|
| Version | Each preview changes code paths. | `v0.6.5-preview.30` |
| Region | The US and EU builds are different `.nro` files. | EU (SLES-03936) |
| Build type | Only `-nxlink` builds print logs. | `-nxlink` |
| Atmosphère | Some crashes depend on the firmware stack. | 1.12 |
| Console + mode | GPU and audio timing differ between models and modes. | OLED, handheld |
| Steps | The fix starts from a reproduction. | Numbered list, one action per line |
| Expected / actual | Says what is wrong, not only what happened. | "SFX plays" / "all audio stops" |
| Frequency | Separates a logic bug from a timing bug. | Always / 3 of 5 tries / once |
| What recovers it | Often points at the system that is broken. | "changing maps brings audio back" |
| Logs / files | Proof of the state at the time of the bug. | See below |

Be specific about names: which tech, which Digimon, which map or NPC, which
menu. "A tech" is much harder to find than "Fire Tower on Agumon, first turn".

## Logs and files

- **nxlink log** (`-nxlink` builds only). Launch with
  `nxlink -a <switch-ip> -s dw3-switch-nxlink.nro` and save the terminal
  output to a file, for example by adding `2>&1 | tee dw3.log`. Network setup
  and pitfalls: [SWITCH_OLED_NETWORK](SWITCH_OLED_NETWORK.md),
  [NXLINK_DEBUG](NXLINK_DEBUG.md). Attach the whole file, not only the last
  lines.
- **Crash report.** After a crash, attach
  `sdmc:/switch/dw3/crash_report.txt`. The previous one is kept as
  `crash_report_prev.txt`. Atmosphère may also write a report under
  `sdmc:/atmosphere/crash_reports/`. See [P5.4_CRASH](P5.4_CRASH.md).
- **Debug overlay** (`-nxlink` builds). Toggle with **Minus + L + R**.
  A photo of the overlay at the moment of the bug shows FPS, mode file and
  build tags. See [DEBUG_OVERLAY](DEBUG_OVERLAY.md).
- **Screenshot or video.** Capture button or a phone. For visual bugs, a
  short clip helps more than a description.

## Title

Use `[REGION] Area: what goes wrong (build, console)`. For example:

```
[EU] Battle: using a tech silences all audio until a map change (preview 29, OLED, nxlink)
```

Areas in use: Boot, Title, Field, Battle, Menu, Card game, Movies, Audio,
Save, Input, Crash.

## Example

Based on [#188](https://github.com/Larbaco/dw3-switch/issues/188). The tech,
map, frequency and log details are made up to show what a complete report
looks like.

```markdown
### Summary
In battle, using Fire Tower makes all audio stop. Sound does not come back
after the battle, in the field or at the title. Using another tech plays
only that tech's SFX. Changing maps brings all audio back.

### Build
- Version: v0.6.5-preview.29
- Region: EU (SLES-03936)
- Build type: -nxlink
- Atmosphère: 1.12

### Hardware
- Console: Switch OLED
- Mode: handheld (also happens docked)

### Steps to reproduce
1. Load a save in Asuka City.
2. Walk into the field until a battle starts.
3. On Agumon's first turn, use Fire Tower.
4. Wait for the tech animation to end.

### Expected
Battle music and SFX keep playing.

### Actual
All audio stops after the tech. Only the next tech's SFX plays.

### Frequency
Every time with Fire Tower (5 of 5). Normal attacks do not trigger it.

### What recovers it
Changing maps.

### Logs / files
- dw3.log (nxlink, full session, bug at ~02:10)
- photo of the debug overlay right after the audio stopped
```

## After you report

- Answer follow-up questions on the same issue.
- When a fix lands, the issue says which preview has it. Retest on that
  preview and comment whether it works. Issues that need hardware are closed
  only after a tester confirms the fix.
