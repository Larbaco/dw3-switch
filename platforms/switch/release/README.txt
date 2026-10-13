Digimon World 3 / 2003, Switch homebrew preview

This release is v0.6.5-preview.21, built from main at b597c67 after
PR #111 (STR bridge read-race fix) and PR #112 (docs).
Battle defaults to software RasterTriangle; field stays software RGB555.
GPU diagnostic instrumentation is disabled in this release. PR #89 is
still separate and its OLED investigation is not part of this cut.

New in this preview

  Title menu: Start / Port options / Continue (#100). The port-options
  shell was validated on PC. Its current switches are stubs: they have
  no gameplay effects and are not persisted. Switch navigation remains
  to be confirmed on hardware.

  Opening movie: the STR file bridge is now locked between the video and
  XA readers (#111). Video sectors are no longer dropped. On the OLED
  (nxlink build) the opening held 14.994-15.006 fps over 18 steady
  windows; preview.20 measured 6.75 fps. The XA audio sounded correct.
  The owner pressed Start at frame 1110 of about 1776, so the last part
  of the movie was not checked on hardware. Image quality was not
  separately assessed. Empty StGetNext scan cache (#106) is still in.

  Battle render A/B remains FIGHTSTG-only. Minus + ZL + ZR cycles:
    soft / soft+bbox (stub) / gpu-solid / gpu-tex.
  Soft is the cold-boot default. GPU modes remain experimental.
  Toggle the nxlink debug HUD with Minus + L + R.

  Existing own-disc extraction, left-stick input, flat saves, and the
  STGMCARD load/save progress bar remain available. The opening-video
  picture has known errors; the port is still a prerelease.

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
  0.6.5-p.21 — libnx allows 15 characters, so "preview" becomes "p").
  nxlink loads may not refresh that metadata; look for
  dw3: version=0.6.5-preview.21 on the host log or in
  sdmc:/switch/dw3/dw3.log.

  Do not delete OLED saves (save0.bin / save1.bin) for this test.

OLED A/B checklist (carried forward from preview.18)

  1. Boot with existing us/ and flat saves. Reach field, then a fight.
  2. Confirm soft looks OK (default). HUD battle=soft if nxlink.
  3. Minus+ZL+ZR once → soft+bbox (same picture; log "not yet").
  4. Again → gpu-solid (solid colours / shapes; compare FPS).
  5. Again → gpu-tex (may be black; compare only).
  6. Again → soft. HUD scale should feel a bit smaller than before.
  7. Optional: STGMCARD load/save — bar still ~tracks I/O (option B OK).

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
