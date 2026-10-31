Digimon World 3 / 2003 for Switch (homebrew) - v0.9.0-preview.1

First preview of the 0.9.0 series (SemVer tags). Options are regrouped, the
pause menus exit at once, and the title waits for the game before PRESS START.
Preview: unfinished, and not every new item has been run on a console yet.

## Highlights

- Options is regrouped: a new Menus group, Map buttons under Controls, Shared
  saves (experimental) under Saves, and Version label and Map mod pack under Display.
- Instant menu exit: leaving the pause menus (Items, Map...) no longer plays the
  dissolve. Options > Menus, ON by default.
- PRESS START appears only when the game is ready to take it.
- The walkthrough is condensed into fuller pages, with corrected steps.
- Version numbers are now 0.9.0-preview.N (the title corner shows them).

## Fixed

- On US with the Nintendo layout and Switch glyphs, the Load and Save prompt showed X instead of A (BUG-4).
- Quitting when started from Sphaira could end in a crash report; the relaunch
  into the other region is hardened too (BUG-1, BUG-p57-1).
- The name box on the map view had an artefact on its border (OBS-3).
- Boot no longer writes [E] error lines to dw3.log (OBS-7).
- The title's version label is short and sits at the picture edge.
- Walkthrough: the East Station route (ch01) and the Digi-Egg step (ch04) text is corrected.

## Changed

- The Restart hotkey moved to Options > Controls.
- Region at start moved to Options > Saves (combined .nro only).
- Instant menu exit is ON by default. The fold of the panels still plays.
- cfg keys are unchanged; your port_options.cfg keeps working.

## Known issues

- Quitting from Sphaira, or restarting into the other region, may still show a
  crash report: the fix is in but not yet run on a console.
- Checked on PC only so far: PRESS START timing, the US Load prompt glyph,
  the map name box border and the clean boot log.
- The walkthrough's FR, DE, IT and ES text is machine-drafted and not yet checked by a native reader.
- The opening movie runs at about 15 fps.

## Install / update

Copy dw3-switch.nro to sdmc:/switch/ and start it from your launcher. Coming from
an older preview, delete the old -eu / -both / -nxlink .nro files. Saves stay.
You need your own disc image (US SLUS-01436 or EU SLES-03936). Setup, controls
and crash reports: https://github.com/Larbaco/dw3-switch/blob/main/docs/PLAYER_GUIDE.md

## Downloads

- dw3-switch.nro        US + EU in one file; the region is chosen at boot
- dw3-switch-elf.zip    symbols to read a crash report (matches this .nro only)
- README.txt            these notes

## Full changes

https://github.com/Larbaco/dw3-switch/compare/v0.6.5-preview.57...v0.9.0-preview.1
PRs: #391 #394 #395 #396 #397 #402 #403 #404 #405 #406

No game files are included: bring your own disc. Unofficial fan project, not
affiliated with Bandai Namco or Nintendo. Digimon is a trademark of its owners.
