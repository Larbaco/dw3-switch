# Field tiles with more than 5 sprites (the black canal water)

Owner report, preview.31-rc1 OLED (EU): "Ao sair do underground path, a
textura da agua esta errada ate descer um pouco a tela, quando a textura volta
a ficar correta. Parece que tem um ponto em que a textura funciona/nao
funciona." (leaving the Underground Path, the water texture is wrong until the
screen scrolls down a bit, then it is right; a scroll position seems to switch
it). The renderer mode was unknown. It does not matter: this is not a renderer
bug (see "Renderers").

## Where

The Underground Path is `WSTAG237` (Asuka) / `WSTAG238` (Amaterasu), mode
`0x20B` (523) on Asuka. Its exit slots go to `0x201` (Central Sector, the
streets by the Cargo Tower, `WSTAG202`, file 509) at pixel (240, 264), facing
7, which is right next to the canal. The same wrong water shows when the same
spot is reached from the Inn or from another `0x201` field (the player's
arrival point decides the camera, not the path).

## Cause

A map tile is 128 x 128 px and carries a list of sprites for each of three
depths (`FIELDSTG_loadStreamSprites`, `StreamTask.sprites[3][5]`): a word
count, then six halfwords per sprite. A depth holds 5.

`WSTAG202`'s tile 12 (column 4, row 1) counts **6** sprites in depth 0. The PS1
code writes the sixth over the first slot of depth 1, which depth 1 rewrites
right after, so the extra sprite is never drawn and the parse stays in place.
Patch `0057` (16:9 field crash guard, preview.26) clamped the count to 5 to
keep the writes in the array, but then read the counts of depths 1 and 2 six
halfwords too early: `7340119` and `207750211`, clamped to 5 again, so depths
1 and 2 became 10 garbage sprites with sizes up to 6000 px. Those were drawn
over the map and hid the canal (black water in the captures); they depend on
which tiles are loaded, that is on the camera.

The 16:9 field (`0052`, widescreen on by default since preview.25) keeps 5
columns by 3 rows of tiles loaded around the camera. From the exit's arrival
point (camera at about (80, 124)) tile 12 is in view from the start; when the
player walks down far enough the tiles in view change and tile 12 is replaced
(and with it the garbage), so the water "comes back". In 4:3 (widescreen off)
tile 12 is loaded only further right, so the water was fine at the exit.

## Fix (patches `0110`, `0111`)

- `0111`: `FIELDSTG_loadStreamSprites` reads all `count` sprites of a depth and
  stores the first 5 (what the PS1 draws), so the next depth is read where it
  is. A count below 0 or above 800 (nothing a 10 KB tile can hold: the
  preview.26 case of a decompressor read through a buffer in use) ends the
  parse with the remaining depths empty. Two log lines make the next report
  self-explaining (first 16 per field entry, in `dw3.log`):
  `dw3-tile: tile N depth D counts C sprites, the first 5 are kept (as on the PS1)`
  and `dw3-tile: tile N depth D has a corrupt sprite count C, rest of the tile skipped`.
- `0110`: test hook `DW3_FORCE_EXIT=<mode>:<x>:<y>:<dir>:<frames>` leaves the
  field once like an exit slot (`FIELDSTG_leaveField(mode, -1, x << 8, y << 8,
  dir)`), after `<frames>` idle frames of the first field, so a replay does
  not have to walk to the exit.

No PSY-Z change (pin unchanged, `8bde35d`): the data was parsed wrongly before
any renderer saw it.

## Renderers

The field is always drawn by the primitive list the tiles produce, on the PC
`sdl3_gpu` renderer and on the Switch CPU rasterizer alike. The Switch battle
mode (`soft`, `gpu-tex`, ...) only affects battle meshes. So `gpu-tex` and
`soft` look the same here, and the owner's mode did not matter. The picture
was reproduced on the PC `sdl3_gpu` build; the CPU rasterizer was not run on
this tile (the Switch build only compiles it, see the PR), but its input is
the same corrupted sprite list, which is what is fixed.

## Test

`us-underground-exit` and `eu-underground-exit` (P6.15,
[P6.15_REPLAY_TESTS.md](P6.15_REPLAY_TESTS.md)): boot in the path
(`DW3_START_MODE=0x20B DW3_PROGRESS=6`), leave with `DW3_FORCE_EXIT=0x201:240:264:7:100`,
checkpoints `after-exit`, `scroll-mid`, `scroll-end` (Down held) with
widescreen on, and the log must show `tile 12 depth 0 counts 6 sprites` and no
`corrupt sprite count`. Without `0111` the log line is missing and all three
captures differ from the reference by 13 to 32 % of the pixels (black water).

## Results (2026-10-24, PC `sdl3_gpu` under Xvfb, US and EU)

- Reproduced before the fix: black canal water after the exit, both discs,
  also from the Inn (`0x20A`) and from `0x201` itself; fine in 4:3.
- After: both scenarios pass (log and the three captures per disc).
- Full replay suite (`tools/replay_tests.sh`, `--jobs 2`, `LP_NUM_THREADS=2`,
  no soak): 845 checks, 12 differences, all in `us/eu-load-menu`
  (`slot-choice`, `not-found`), `us-battle-report` (`report-b`),
  `eu-battle-report` (`report-a`) and `us/eu-menu-screens` (`map`). They
  differ by the same pixel counts on a build without patch `0111`, so they are
  stale references from earlier menu / widescreen changes and not from this
  fix; the references were not updated.
- Switch NROs for US (9,621,676 bytes) and EU (9,650,348 bytes) build with
  `tools/build_switch_game.sh` and `--eu`. `python3 -m pytest tools/tests`:
  304 passed, 2 skipped.
