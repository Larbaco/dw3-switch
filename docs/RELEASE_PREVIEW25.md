# Preview.25 release checkpoint

Published 2026-10-14: [v0.6.5-preview.25](https://github.com/Larbaco/dw3-switch/releases/tag/v0.6.5-preview.25).
Exact source: `705afcf695193ef35ab12b0e7b0fdaf680d2cfb6`.
Full boot version: `0.6.5-preview.25`; NACP: `0.6.5-p.25`.

## Published bundle and provenance

- `dw3-switch.nro`: US ordinary build.
- `dw3-switch-nxlink.nro`: US build with nxlink logging.
- `dw3-switch-eu.nro`: EU ordinary build.
- `dw3-switch-eu-nxlink.nro`: EU build with nxlink logging.
- `README.txt`: own-disc setup, controls and OLED checklist.
- `SOURCE.txt`: source/version and build provenance.
- `SHA256SUMS`: checksums for the other six published files.

The US pair comes from successful [release run 38560666585](https://github.com/Larbaco/dw3-switch/actions/runs/38560666585).
The EU pair was built separately with `tools/build_switch_game.sh --eu`
(and `--nxlink` for logs), using the same source and release tag.
`SOURCE.txt` records the local `devkitpro/devkita64:latest` image as
`sha256:551247dfe0524421b8cb608117a767c77823e76608fa2058f0eb5d5398293641`.
Published files were downloaded again and all six checksum entries verified.
No BIN/CUE/ISO, BIOS or extracted game tree is included.

## Changes and remaining acceptance

PR #132 makes widescreen ON and Switch battle `gpu-tex` by default, restores
the battle portrait, and extends supported UI backgrounds. Known cliff/sky
battle gaps remain; compare software rendering when reporting a stage.
PR #133 implements field/battle fast-forward (R3, configurable 2x/3x/4x/OFF).
PR #137 records its documentation; all controls are in [HOTKEYS](HOTKEYS.md).
Port options remain session-only. Restart stays OFF until enabled.

Publication and successful build are not OLED acceptance. At this checkpoint,
EU nxlink delivery and OLED boot are confirmed: logs identify boot version
`0.6.5-preview.25`, EU runtime root and `SLES_039.36`, and the EU title is active.
Movie image/audio, widescreen, battle-stage geometry, turbo speed/audio, and
save → restart → Load Game remain hardware gates.
The owner reported completing a first EU save on the preceding test build;
that is not evidence of preview.25 reload or restart correctness.
Preserve both region trees and their memcard directories. Testing the NRO's
own EU extraction requires the owner's matching BIN/CUE, not a pre-extracted
runtime; see [DISC_SETUP](DISC_SETUP.md). Do not remove saves to force extraction.
