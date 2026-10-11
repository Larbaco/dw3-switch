Digimon World 3 / 2003, Switch homebrew preview

This release has two files.

  dw3-switch.nro    the game. CMake build. source/boot_main.c calls dw3_game_main.
  README.txt        this file.

There is no game data in either file. No disc image, no BIOS, no extracted
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

Install the homebrew

  Copy dw3-switch.nro to the SD card:

    sdmc:/switch/dw3-switch.nro

  Launch it from hbmenu.

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

Known gap on this build

  Menus are cropped to 256 pixels wide. The fix is pull request 45
  (https://github.com/Larbaco/dw3-switch/pull/45) and is not in main yet.
  This preview is cut from main, so it still has the crop.

Digimon and Digimon World are trademarks of their owners. This is an
unofficial fan project. It is not affiliated with Bandai Namco or Nintendo.
