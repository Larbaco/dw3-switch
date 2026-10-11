Digimon World 3 / 2003, Switch homebrew preview

This release is v0.6.5-preview.8. On top of preview.7, each field area
reads its map file from the SD card once. Later tiles in that area are
copies from that buffer. The next area replaces the buffer. Movies still
arrive at 150 sectors per second. This build does not call
clkrstSetClockRate or appletSetCpuBoostMode. The CPU and GPU stay at the
rate the console set.

Preview.7 already uploaded only the sampled VRAM rectangle, and it loaded
one field tile per frame. An OLED log from that build is already recorded:
mean about 59 FPS, and some 60-frame windows of about 1.51 s. The
KernelInit error from that boot is still open. This preview.8 file has
not been timed on the OLED.

Preview.6 raised the CPU to 1785 MHz. That is removed. Reboot the Switch
once before this test if preview.6 already ran: a HOME press on that
build can leave 1785 MHz set, and this build will not change it back.

Preview.5 already had the progress bar, the left stick, and the US
runtime extract. Those stay. The opening-video picture is unchanged.
On the OLED, the menu picture looked right, the left stick walked on
handheld Joy-Con and on a Pro Controller, a DuckStation card loaded,
and a slot 2 save loaded after the memory-card fix that is in this
build.

Every 60 frames the log prints one line, frame dt us=. About 1000000
is 60 Hz. About 2000000 is 30 Hz. Preview.7 already recorded that line
on the OLED. This preview.8 file has not been timed there.

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
