# Preview.22 release checkpoint

Tag `v0.6.5-preview.22`, cut from `main` after PR #123 and published as a
prerelease by `.github/workflows/switch-release.yml` (tag push). Assets are
the standard and nxlink game `.nro` plus `README.txt`
(`platforms/switch/release/README.txt`). No game data, disc images, BIOS,
or extracted trees are published. Boot version `0.6.5-preview.22`; NACP
display version `0.6.5-p.22`. Console clocks are untouched.

## Contents since preview.21

- **P6.1 widescreen** (#123): Port options **Widescreen**, OFF by default,
  session-only. Battle 53 px gutters per side (about 16:9), field 32 px
  (16:10), everything else 4:3. See [P6.1_WIDESCREEN.md](P6.1_WIDESCREEN.md).
- **GPU battle bg fix** (#114): the DRAWENV background clear goes to the
  mesh FBO, so gpu-solid / gpu-tex are no longer flat blue. Soft remains
  the default.
- **Port options row** (#115): title presentation pass.
- **P6.9 restart hotkey** (#122): opt-in, PC-validated. See
  [P6.9_RESTART.md](P6.9_RESTART.md).

## Validation before the cut

- CI green on #123 (`pc`, `switch`, `switch-game`).
- Widescreen on the owner's machine with the owner's disc, on a private
  Xvfb display: PC build (battle 53 px, field 32 px, off = unchanged 4:3)
  and the game `.nro` in Ryubing (same picture; `dw3.log` prints the
  gutter). Evidence kept locally under `tmp/evidence/p61-widescreen-pr123/`.

## Still open (hardware)

- Widescreen on the OLED: picture, pop-in at the side edges in long fights
  and busy maps, and enabling it through the Port options menu.
- Restart hotkey on Switch and Continue of a save written before restart.
- GPU battle modes after #114 on the OLED.
