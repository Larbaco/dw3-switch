Digimon World 3 / 2003 for Switch (homebrew) - v0.9.0-preview.2

Second preview of the 0.9.0 series: Brazilian Portuguese, a 3D resolution
option, a sharper picture filter and a crash fix. Preview: unfinished, and
the new items have not all been run on a console yet.

## Highlights

- Brazilian Portuguese: Options > Display > Text language. The port's own
  screens (Options, Help, Walkthrough, title) are in Portuguese with no extra
  files. The game's own text needs a local text pack (not included, EU only for
  now) at sdmc:/switch/dw3/eu/l10n/ptbr/; see docs/LOCALIZATION.md.
- Internal resolution (Options > Display): renders the battle 3D at 2x or 3x.
  On Switch it is capped at 2x for now.
- Native res while turbo (Options > Display): fast-forward drops to normal
  resolution and returns to your choice afterwards. ON by default.
- Output filter (Options > Display): Nearest, Bilinear or Sharp bilinear
  for the stretch to your screen. Nearest stays the default.
- The Options background scrolls like the Items menu.

## Fixed

- Quitting with an nxlink host connected could end in a crash report.

## Changed

- Nintendo button layout and Switch glyphs are now the default (a saved
  setting moves once; a custom layout is kept).
- Map scaler default is FSR 1.
- One port_options.cfg for both regions; the old us/eu files are merged once
  and backed up as .bak.

## Known issues

- The Portuguese wording is an AI draft, not yet reviewed. The text pack is EU only.
- Internal resolution: handheld 2x did not hold 60 fps in 95% of battle
  windows in the first OLED measurement, 3x is slower. The 2x cap and the
  docked limit are not final; the OLED measurement is pending.
- The nxlink quit fix is not yet confirmed on the OLED. Quitting through Sphaira or a
  region restart may still show a crash report.
- The opening movie runs at about 15 fps.

## Install / update

Copy dw3-switch.nro to sdmc:/switch/ and start it from your launcher. Saves and
region folders stay; your settings are migrated on the first start.
You need your own disc image (US SLUS-01436 or EU SLES-03936). Setup, controls
and crash reports: https://github.com/Larbaco/dw3-switch/blob/main/docs/PLAYER_GUIDE.md

## Downloads

- dw3-switch.nro        US + EU in one file; the region is chosen at boot
- dw3-switch-elf.zip    symbols to read a crash report (matches this .nro only)
- README.txt            these notes

## Full changes

https://github.com/Larbaco/dw3-switch/compare/v0.9.0-preview.1...v0.9.0-preview.2
PRs: #408 #410 #412 #413 #414 #416 #417 #418 #419 #420

No game files are included: bring your own disc. Unofficial fan project, not
affiliated with Bandai Namco or Nintendo. Digimon is a trademark of its owners.
