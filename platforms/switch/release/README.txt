Digimon World 3 / 2003, Switch homebrew preview

This release is v0.6.5-preview.22, built from main after PR #123
(widescreen). It also carries #114, #115 and #122, merged after
preview.21. Battle defaults to software RasterTriangle; field stays
software RGB555. GPU diagnostic instrumentation is disabled.

New in this preview

  Widescreen (#123, P6.1). Port options on the title menu has a
  Widescreen switch, OFF by default and not saved between boots. When
  it is on, battles draw 53 extra pixels on each side (426x240, about
  16:9) and the field draws 32 (384x240, 16:10). The HUD and menus stay
  in the middle 320 columns. Title, menus, movies and the card game stay
  4:3. The log (and sdmc:/switch/dw3/dw3.log) prints
  "dw3-widescreen: gutter 53 px per side" in battle and 32 on the field.
  Checked in the PC build and in Ryubing with the disc owner's data. Not
  yet on hardware. Things can pop in at the side edges; please report
  where. A 1x3 pixel black mark at the left edge of one field map is a
  map tile seam, not this feature.

  Restart hotkey (#122, P6.9). Port options has Restart hotkey, OFF by
  default. With it on, hold L + R + Plus + Minus (PS1 L1 + R1 + Start +
  Select): saves are flushed first, then the game returns to the title
  without leaving the .nro. On the nxlink build the same buttons also
  toggle the debug HUD. Validated on PC; not yet on Switch.

  GPU battle modes (#114). gpu-solid and gpu-tex no longer show a flat
  blue battle: the background clear now goes to the mesh FBO. Soft is
  still the cold-boot default. Minus + ZL + ZR cycles
  soft / soft+bbox (stub) / gpu-solid / gpu-tex.
  Toggle the nxlink debug HUD with Minus + L + R.

  Port options row (#115): a uniform, easier-to-read row on the title.

  Carried forward: the STR read-race fix (#111), the empty StGetNext
  scan cache (#106), own-disc extraction, left-stick input, flat saves
  and the STGMCARD load/save progress bar.

This build does not call clkrstSetClockRate or appletSetCpuBoostMode.
The CPU and GPU stay at the rate the console set.

Every 60 frames the log prints one line, frame dt us=. About 1000000
is 60 Hz. About 2000000 is 30 Hz. On field, mesh dbg: should show
tris_enq near 0 and frames_blit dominant. On FIGHTSTG with soft,
expect frames_blit / soft=. With gpu-solid or gpu-tex, frames_mesh
and tris may rise.

boot_tim.bin is not a disc image. A CUE whose FILE name is missing still
selects the only other .bin in that directory when the file is large
enough.

This release has three files.

  dw3-switch-nxlink.nro    the test build. Same game, log on the PC.
  dw3-switch.nro           the same game, no network.
  README.txt               this file.

There is no game data in these files. No disc image, no BIOS, no extracted
tree. You supply that from a copy you own.

This is a prerelease. An OLED has reached the title, the field, and
battle. It is not a finished port. It is also not the small EGL + audren
sample that `make -C platforms/switch` produces. CI job `switch` builds
that sample. Do not install the sample and expect the game.

What you need

  Atmosphere and the Homebrew Menu, the usual homebrew setup.
  A legally obtained US disc, Digimon World 3, product code SLUS-01436.
  This US binary looks for sdmc:/switch/dw3/us/. An EU Digimon World 2003
  (SLES-03936) NRO is built with tools/build_switch_game.sh --eu and reads
  a pre-packed tree at sdmc:/switch/dw3/eu/ (no BIN auto-extract for EU).

Install the test build

  Copy dw3-switch-nxlink.nro to:

    sdmc:/switch/dw3-switch-nxlink.nro

  Put your own Mode2 BIN and CUE next to the US tree, not inside it:

    sdmc:/switch/dw3/Digimon World 3.cue
    sdmc:/switch/dw3/Digimon World 3.bin
    sdmc:/switch/dw3/us/

  Open hbmenu, press Y for NetLoader, and leave it waiting. On the PC,
  same network:

    nxlink -a <Switch-IP> -s dw3-switch-nxlink.nro

  Do not probe TCP 28280 before nxlink (empty connect breaks hbmenu
  NetLoader with err=11). Prefer hbmenu Y NetLoader over Sphaira grid.

  If us/ is missing, the first boot builds it from the BIN. The screen
  stays dark blue and draws a bar. The log says "extracting". That copy
  is the whole runtime, not only the movies. If us/ is already there,
  boot only checks sizes, then rewrites any STR that is still 2048 bytes
  per sector. That rewrite logs "restore" and uses the same bar. The
  next boot only stats.

  The left stick should walk the player. The d-pad, face buttons, Start
  and Select still work with the stick centred. nxlink should show
  kind=0x73. A failed nxlink connection still boots, and the same lines
  go to sdmc:/switch/dw3/dw3.log.

  dw3-switch.nro is the same test without a socket. Copy it to
  sdmc:/switch/dw3-switch.nro and launch it from hbmenu when you do not
  want the host log.

  hbmenu shows the NACP display version for SD installs (this preview:
  0.6.5-p.22 — libnx allows 15 characters, so "preview" becomes "p").
  nxlink loads may not refresh that metadata; look for
  dw3: version=0.6.5-preview.22 on the host log or in
  sdmc:/switch/dw3/dw3.log.

  Do not delete OLED saves (save0.bin / save1.bin) for this test.

OLED checklist (preview.22)

  1. Boot with existing us/ and flat saves. Title, Port options, turn
     Widescreen on, back, Continue.
  2. Field: the picture is wider than before (16:10), the map fills both
     sides, the HUD is centred. Walk to a map edge.
  3. A fight: wider still (about 16:9). Watch the side edges for models
     that pop in. HUD battle=soft if nxlink.
  4. Minus+ZL+ZR to gpu-solid and gpu-tex: the battle should no longer be
     flat blue. Compare FPS; soft is still the default.
  5. Optional: turn Restart hotkey on, hold L+R+Plus+Minus in the field,
     then Continue the save you were on.

Install your own disc data

  Pack the disc with the tools in the dw3-switch repository. The steps
  are docs/DISC_SETUP.md and then:

    python3 tools/pack_runtime.py --version us --disks disks/us --out runtime/us

  Copy that packed tree to the SD card. The boot path reads it here:

    sdmc:/switch/dw3/us/
      files_us.json
      SLUS_014.36
      boot_tim.bin
      AAA/...

  That directory is DW3_DEFAULT_RUNTIME in
  platforms/switch/source/boot_main.c. The same relative layout is
  documented in docs/RUNTIME_LAYOUT.md. The PC host uses runtime/us
  instead of the sdmc path.

  A log from a boot that found the card prints:

    [init_paths] cdfs root=sdmc:/switch/dw3/us

Picture

  A release cut from current main shows the title at the display-mode
  width and the opening movie at full height (4:3, side bars only).
  v0.6.5-preview.1 was cut earlier, from main before that picture work.
  That file still crops the title to 256 pixels and freezes the movie.

Digimon and Digimon World are trademarks of their owners. This is an
unofficial fan project. It is not affiliated with Bandai Namco or Nintendo.
