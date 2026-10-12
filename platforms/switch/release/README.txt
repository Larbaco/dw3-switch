Digimon World 3 / 2003, Switch homebrew preview

This release is v0.6.5-preview.18. It keeps battle mesh gated to FIGHTSTG
only (field stays software RGB555). Cold-boot battle default is still
software RasterTriangle (soft) so the fight picture stays readable.

New in this preview

  Battle render A/B (FIGHTSTG only). Edge chord Minus + ZL + ZR cycles:

    soft       software RasterTriangle (default; picture OK)
    soft+bbox  stub today — same as soft; logs "not yet"
    gpu-solid  mesh FBO with solid vertex colour (ignore texture/CLUT)
    gpu-tex    existing textured mesh path (may be black; OK to compare)

  Each change logs once: nx_mesh: battle_mode=…
  On the nxlink HUD, the line battle= shows the current token.
  Field never uses the mesh path. Do not overclock.

  Debug HUD is slightly smaller than preview.10+ (#78): 2× glyphs at
  720p handheld, 3× at 1080p docked. LSB-left glyph bits (#81) stay.
  Toggle HUD with Minus + L + R (nxlink build).

  STGMCARD load/save bar (option B, 100 frames, #84) was confirmed OK
  on OLED in preview.17 for both load and save. Unchanged here.

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
  This binary looks for the US tree only. EU, Digimon World 2003,
  SLES-03936, is a later boot option. When that exists the tree goes in
  sdmc:/switch/dw3/eu/.

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

  Do not delete OLED saves (save0.bin / save1.bin) for this test.

OLED A/B checklist (preview.18)

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
