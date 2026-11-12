Digimon World 3 / 2003 for Switch (homebrew) - v0.9.0 public beta

The first public beta: the PS1 game's own code, rebuilt from the decompilation
to run natively on the Switch. Playable from the start past the first boss on
a Switch OLED. Expect bugs and the occasional crash; reports are welcome.

## Highlights

- Widescreen 16:9 in the field, 3D battles and most menus.
- Fast-forward (R3, 2x to 4x), text speed and encounter-rate options.
- Auto-save per area, Continue on the title, US and EU saves carry over.
- Nintendo or PlayStation button layout, any button remappable, Switch glyphs.
- Full-screen map, in-game Help and a spoiler-safe walkthrough.
- One .nro for US and EU; five languages on the EU disc.

## Fixed

- Quitting with an nxlink host connected: a second crash cause is fixed.
- Output filter Nearest: a few pixels could come from the neighbouring texel.

## Changed

- Version numbers leave the preview series: this is v0.9.0. Settings, saves
  and region folders from the 0.9.0 previews carry over unchanged.

## Known issues

- Quitting through Sphaira or restarting into the other region may still show
  a crash report (fix included, not yet confirmed on a console).
- The opening movie runs at about 15 fps.
- EU: the sound can go wrong after many techs in battle (issue #188).
- Internal resolution 2x does not always hold 60 fps in handheld battles.
- The Portuguese wording is an AI draft, not yet reviewed (text pack EU only).

## Install / update

Copy dw3-switch.nro to sdmc:/switch/ and start it from your launcher.
Put your disc (BIN + CUE) in sdmc:/switch/dw3/; the first start extracts it.
Setup, controls, saves and crash reports:
https://github.com/Larbaco/dw3-switch/blob/main/docs/PLAYER_GUIDE.md

## Downloads

- dw3-switch.nro        US + EU in one file; the region is chosen at boot
- dw3-switch-elf.zip    symbols to read a crash report (matches this .nro only)
- README.txt            these notes

## Full changes

https://github.com/Larbaco/dw3-switch/compare/v0.9.0-preview.2...v0.9.0
PRs: #421 #423 #424 #425 #426 #427

No game files are included: bring your own disc. Unofficial fan project, not
affiliated with Bandai Namco or Nintendo. Digimon is a trademark of its owners.
