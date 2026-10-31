Digimon World 3 / 2003 for Switch (homebrew) - v0.6.5-preview.57

Fixes the crash when you digivolve in battle, adds a "restart in the other
region" prompt, and gets the full-screen map and the walkthrough closer to done.
Preview: unfinished, and not every new item has been run on a console yet.

## Highlights

- Digivolving in battle no longer crashes (US and EU).
- Options > Gameplay > Region at start can restart the game in the region you picked.
- Minus (Select) opens the full-screen map straight from the walking field.
- Walkthrough: new last-battle chapter, the EU extra chapter and six optional rows.
- The build label (version, commit, region) shows in the title corner and Options.

## Fixed

- Command menu > DV > pick a form could crash about 3 seconds in (BUG-6).

## Changed

- Minus-to-map needs Full-screen map ON (Options > Display) and widescreen.
- Add show_version=0 to port_options.cfg to hide the label on the title.

## Known issues

- The new items above are checked on PC and in the emulator, not yet on a console.
- Walkthrough: FR/DE/IT/ES text is machine-drafted; two steps have wrong text.
- US, Nintendo layout, Switch glyphs ON: the Load screen shows X instead of A.
- Full-screen map: a small artefact at the edge of the area-name balloon.
- Full list: https://github.com/Larbaco/dw3-switch/blob/main/docs/OLED_TEST_PLAN.md

## Install / update

Copy dw3-switch.nro to sdmc:/switch/ and start it from your launcher. Coming from
an older preview, delete the old -eu / -both / -nxlink .nro files. Saves stay.
You need your own disc image (US SLUS-01436 or EU SLES-03936). Setup, controls,
saves and crash reports:
https://github.com/Larbaco/dw3-switch/blob/main/docs/PLAYER_GUIDE.md

## Downloads

- dw3-switch.nro        US + EU in one file; the region is chosen at boot
- dw3-switch-elf.zip    symbols to read a crash report (matches this .nro only)
- README.txt            these notes

## Full changes

https://github.com/Larbaco/dw3-switch/compare/v0.6.5-preview.56...v0.6.5-preview.57
PRs: #380 #381 #382 #383 #384 #385 #386 #387

No game files are included: bring your own disc. Unofficial fan project, not
affiliated with Bandai Namco or Nintendo. Digimon is a trademark of its owners.
