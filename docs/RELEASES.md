# Preview releases

All releases are GitHub prereleases. Each ships `.nro` files only, never game
data. Tags are named `v0.6.5-preview.N`; hbmenu shows the short form
`0.6.5-p.N`. Pick the newest unless a note below says otherwise.

All releases are on the
[releases page](https://github.com/Larbaco/dw3-switch/releases). Install
steps are in the [README](../README.md#play-it-on-switch). Preview numbers
that have no tag (11 to 17) were never published.

## Do not install

| Tag | Why |
|---|---|
| [preview.1](https://github.com/Larbaco/dw3-switch/releases/tag/v0.6.5-preview.1) | Crops the title and freezes the movie. |
| [preview.6](https://github.com/Larbaco/dw3-switch/releases/tag/v0.6.5-preview.6) | Raised the CPU to 1785 MHz. Reverted afterwards. |
| [preview.9](https://github.com/Larbaco/dw3-switch/releases/tag/v0.6.5-preview.9) | Enabled the battle mesh for every mode and broke the field picture. |
| [preview.19](https://github.com/Larbaco/dw3-switch/releases/tag/v0.6.5-preview.19) | A GPU diagnostic cut, not a playable baseline. |

## Index

Newest first.

| Tag | What it was for |
|---|---|
| [preview.27](https://github.com/Larbaco/dw3-switch/releases/tag/v0.6.5-preview.27) | Field streaming fix for the 16:9 crash (inferred from the preview.26 crash records, not yet reproduced). Adds the 4C DW3NX artwork. Hardware acceptance pending. |
| [preview.26](https://github.com/Larbaco/dw3-switch/releases/tag/v0.6.5-preview.26) | EU language selection at boot, battle camera and portrait fixes, Port options on its own screen. Notes: [RELEASE_PREVIEW26](RELEASE_PREVIEW26.md). Crashes: [PREVIEW26_CRASH_RECORDS](PREVIEW26_CRASH_RECORDS.md). |
| [preview.25](https://github.com/Larbaco/dw3-switch/releases/tag/v0.6.5-preview.25) | Widescreen on by default, battle defaults to `gpu-tex`, field and battle fast-forward, EU builds added. Notes: [RELEASE_PREVIEW25](RELEASE_PREVIEW25.md). |
| [preview.24](https://github.com/Larbaco/dw3-switch/releases/tag/v0.6.5-preview.24) | Native title menu on Switch (New Game / Options / Load Game), EU runtime extraction. US only. |
| [preview.23](https://github.com/Larbaco/dw3-switch/releases/tag/v0.6.5-preview.23) | Same play as preview.22 on Switch. PC title menu and docs only. |
| [preview.22](https://github.com/Larbaco/dw3-switch/releases/tag/v0.6.5-preview.22) | Widescreen option, GPU battle background fix, opt-in restart hotkey. Notes: [RELEASE_PREVIEW22](RELEASE_PREVIEW22.md). |
| [preview.21](https://github.com/Larbaco/dw3-switch/releases/tag/v0.6.5-preview.21) | Opening-movie read-race fix; movie pacing improved from 6.75 to about 15 fps. Notes: [RELEASE_PREVIEW21](RELEASE_PREVIEW21.md). |
| [preview.20](https://github.com/Larbaco/dw3-switch/releases/tag/v0.6.5-preview.20) | Title Port options shell (switches were stubs), empty `StGetNext` scan cache. Notes: [RELEASE_PREVIEW20](RELEASE_PREVIEW20.md). |
| [preview.19](https://github.com/Larbaco/dw3-switch/releases/tag/v0.6.5-preview.19) | GPU diagnostic cut. See the table above. |
| [preview.18](https://github.com/Larbaco/dw3-switch/releases/tag/v0.6.5-preview.18) | Battle mesh gated to FIGHTSTG; `Minus + ZL + ZR` cycles battle render modes; software battle default. |
| [preview.10](https://github.com/Larbaco/dw3-switch/releases/tag/v0.6.5-preview.10) | Battle mesh limited to FIGHTSTG, field back on the software blit, sampled `mesh dbg:` log lines. |
| [preview.9](https://github.com/Larbaco/dw3-switch/releases/tag/v0.6.5-preview.9) | See the table above. |
| [preview.8](https://github.com/Larbaco/dw3-switch/releases/tag/v0.6.5-preview.8) | Holds one field map in RAM so later tiles copy from that buffer. |
| [preview.7](https://github.com/Larbaco/dw3-switch/releases/tag/v0.6.5-preview.7) | Uploads only the sampled VRAM rectangle and loads one field tile per frame. First build to reach field and battle on an OLED. |
| [preview.6](https://github.com/Larbaco/dw3-switch/releases/tag/v0.6.5-preview.6) | See the table above. |
| [preview.5](https://github.com/Larbaco/dw3-switch/releases/tag/v0.6.5-preview.5) | Hardware test of the copy progress bar, left-stick input and the US BIN extraction. |
| [preview.2](https://github.com/Larbaco/dw3-switch/releases/tag/v0.6.5-preview.2) | Picture fix: display-mode title width and the full-height movie. |
| [preview.1](https://github.com/Larbaco/dw3-switch/releases/tag/v0.6.5-preview.1) | See the table above. |

Previews 3 and 4 have no separate notes in this repository.

Since preview.18, [`platforms/switch/release/README.txt`](../platforms/switch/release/README.txt)
holds the notes shipped with each release.

## Hardware observations

Dated notes from the maintainer's Switch OLED. They record what was seen on
one console and are not acceptance gates; gates live in
[ROADMAP](ROADMAP.md).

- **preview.5.** Copy progress bar, left stick (handheld Joy-Con and Pro
  Controller) and US BIN extraction were tested. Rumble is not implemented.
- **preview.7.** Reached field and battle. Across 156 samples of 60 frames
  the mean was about 59 FPS, with some windows near 25 ms/frame. PSY-Z logged
  `KernelInit: cannot create the kernel synchronisation objects` once and
  continued; this error is still open. The load design that followed is in
  [STATIC_SD_LOAD](STATIC_SD_LOAD.md).
- **preview.8.** Field about 60 Hz, battle Fight-menu idle about 46 Hz.
- **preview.14.** Flat saves: loading MC1 (`save0.bin`), spending money and
  saving to MC2 (`save1.bin`) slot 2 passed (money 26). A later overwrite to
  MC2 slot 1 saved money 2. Details: [P5.3_FLAT_SAVE](P5.3_FLAT_SAVE.md).
- **After the 100-frame load bar change.** The memory-card load bar felt
  right; save uses the same constants.
- **preview.20.** The Port options title entry opened with both stubs off and
  returned to the middle row. Plus opens the title choices, B confirms, X
  closes. Applet exit and save flush were observed. Checkpoint:
  [RELEASE_PREVIEW20](RELEASE_PREVIEW20.md).
- **preview.21.** The opening movie ran at about 15 fps (preview.20: 6.75
  fps) with correct XA audio up to frame 1110, where Start was pressed.
  Image quality and the end of the movie are not validated. The GPU battle
  renderer and the STR video changes were reported to work well in game.
  Details: [RELEASE_PREVIEW21](RELEASE_PREVIEW21.md) and
  [P3.7_MOVIE_PACING_DIAGNOSTICS](P3.7_MOVIE_PACING_DIAGNOSTICS.md).
- **EU, 2026-10-14.** On a build before preview.25, extraction from only
  `eu/disc.bin` and `eu/disc.cue` wrote 2383 files and early gameplay was
  reached. Details: [P3_EU_BUILD](P3_EU_BUILD.md).
- **preview.26.** Two `User Break` crashes were reported on EU. Raw records:
  [PREVIEW26_CRASH_RECORDS](PREVIEW26_CRASH_RECORDS.md). Preview.27 carries a
  fix for the suspected cause.

Still open on hardware: audio underrun count, the full overlay sweep, battle
at 60 Hz, the sync-object error above, a fresh-save relaunch reload, and EU
movie and playthrough acceptance. See [ROADMAP](ROADMAP.md).
