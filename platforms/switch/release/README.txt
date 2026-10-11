Digimon World 3 / 2003, Switch homebrew preview

This release is a hardware test of the opening movie. It is cut from
the movie-str-pace branch, not from main. Both game files pace the CD
at 150 sectors/s and, on first launch, rewrite 2048-byte STR files from
your disc image so the soundtrack keeps all 18 XA blocks.

v0.6.5-preview.3 selected us/boot_tim.bin, the only .bin inside the
runtime tree, and did not reach the disc image one directory up. This
build ignores boot_tim.bin. The CUE may name a BIN that is not the file
on the card; the only other .bin next to that CUE is used when it is
large enough to cover the STR files.

This release has three files.

  dw3-switch-nxlink.nro    the test build. Same game, log on the PC.
  dw3-switch.nro           the same game, no network.
  README.txt               this file.

There is no game data in these files. No disc image, no BIOS, no extracted
tree. You supply that from a copy you own.

This is a prerelease. The title screen boots in Ryubing. It is not a
playable port on hardware. It is also not the small EGL + audren sample
that `make -C platforms/switch` produces. CI job `switch` builds that
sample. Do not install the sample and expect the game.

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

  The first boot copies about 260 MB. The screen stays dark blue and the
  log prints "restore" until that finishes. The next boot only checks
  file sizes. Then let the opening movie play. Picture and sound should
  stay together. A failed nxlink connection still boots, and the same
  lines go to sdmc:/switch/dw3/dw3.log.

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
