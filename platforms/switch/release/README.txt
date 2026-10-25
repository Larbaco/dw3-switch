Digimon World 3 / 2003, Switch homebrew preview

Release notes for v0.6.5-preview.31. The release tag and boot version
identify the actual source revision.

New in this preview

  No item below has been checked on a Switch OLED yet. Each is checked on
  PC and in the Ryubing emulator only, and is marked unverified. Several
  answer the owner's rc1 OLED notes; those are named.

  Field rasterizer 4 to 5 times faster (P6.22, #220). The software
  rasterizer that draws the field is much faster, and its output is
  pixel-identical (0 differing frames over 154605 replay frames). Turbo
  should be much faster in the field. The estimate for the OLED is about
  160 to 240 fps at 4x in the New Game opening, from the PC numbers; it is
  not a measurement. Unverified on hardware: check the nxlink HUD fps in
  the Forest Inn, top and bottom.

  Widescreen reaches more screens (#218, #219, #224). The field menu
  dissolve into and out of the menu covers the whole 16:9 picture (#218).
  The menu panels sit at the picture edges on Items, Sort, Techniques,
  Status and Card folders (#219). The title menu fills the screen, the TV
  caption bar of the intro spans edge to edge, and the Digimon Online
  registration screen fills 16:9 (#224). This answers the rc1 note that
  the main menu did not fill the screen. Unverified on hardware.

  The field map, the battle results header and the save and load memory
  card screens are 16:9 (#240). The map is centred with black strips at
  the sides. The stray disc or card no longer shows in the right corner
  before the save or load animation, and the slots slide in from the 16:9
  edge. Unverified on hardware.

  Post-battle text and turbo (#223). The battle results screen runs at
  turbo speed with R3 on, as the battle does. Before, it dropped to 1x
  for the whole report. The win jingle plays at that speed too. With Text
  speed Instant, the area name banner after a battle, and at a door, shows
  its two names whole instead of typing them letter by letter. This answers
  the rc1 note that post-battle text still crawled with turbo and Instant
  on. Unverified on hardware.

  Partner portrait in gpu-tex (#229). In the battle, the partner portrait
  is no longer see-through where it is dark. Before, the stage showed
  through dark parts such as Kotemon's kendo mask grid. Unverified on
  hardware.

  File cache crash fixed (#235). Long sessions crashed when the file cache
  ran out of its 64 slots (about 2 hours and several battles on the
  owner's log). The cache now evicts the least recently used file, as the
  PS1 heap does. Found in the rc1 OLED log, EU nxlink build, in the battle
  report. The crash came in the second battle, and both US and EU builds
  were affected. Unverified on hardware.

  Crash reports (P5.4, #217, #231). Software aborts, asserts, fatal errors
  and exit codes now reach crash_report.txt and dw3.log. When the previous
  run crashed, the next boot shows "The last run crashed." It stays up to
  60 s with a countdown. A, B or + dismisses it after the first second, and
  HOME ends it at once. The notice is not shown at the moment of the crash.
  Unverified on hardware.

  Crash symbols (#216). Releases now include dw3-switch-elf.zip, with the
  unstripped US ELFs (dw3-switch-us.elf and dw3-switch-us-nxlink.elf) and
  BUILD_IDS.txt. Use it with tools/symbolize_crash.py on a crash report
  from the matching NRO. The build ids must match.

  Cheats screen in Options (P6.20, #236). Square (Y on Switch) opens it
  from the Options screen once the master switch is on. Entries: fill
  bag, instant win and battle now. Cheats are off by default. A save made
  with cheats gets a save<N>.cheats marker file. The build option
  DW3_CHEATS is on by default; tools/build_switch_game.sh --no-cheats
  builds without the screen. Unverified on hardware.

Known issues

  If the map stays silent after a battle (no field music once you are
  back), please send a --nxlink log. This was reported on rc8 (US) but
  could not be reproduced on PC or in Ryubing, so its cause is unknown.
  The win jingle and SEP fix (#215) are in this release. The underrun
  count is still open.

  The Map screen is 16:9 with black side strips (#240). The area-change
  transition (iris) is also 4:3 and was not checked on hardware.

  Sleep and resume work and play time counts only play time, but the
  "applet focus" lines do not appear in dw3.log (OLED, rc7). Rumble is
  still unverified on hardware.

Carried forward

  Widescreen starts ON; Port options can turn it OFF. Battle defaults
  to gpu-tex; Minus + ZL + ZR cycles rendering modes. Field rendering
  stays software. Gameplay starts at 1x; R3 toggles the configured 2x/3x/4x
  turbo, or OFF disables it. Movies, title and standalone menus stay
  at 1x; gameplay audio accelerates with turbo.

  Native title choices New Game / Options / Load Game, with EU labels
  from the current game language; Options opens Port options.
  The bridge "I need my partner" event (patch 0058) passes without a
  crash on US and EU hardware.

  Restart hotkey remains OFF by default. Enable it in Port options,
  then hold L + R + Plus + Minus. Dirty saves are flushed before the
  game returns to its title without quitting the NRO. A flush failure
  cancels restart. Unsaved progress is lost. Release before repeating.
  This chord can also toggle the nxlink debug HUD, whose own chord is
  Minus + L + R. See docs/HOTKEYS.md for all mapped controls.

  From preview.30: music loops (#209), the win jingle and the menu and
  battle sounds (#215), widescreen area banner and technique backdrops,
  EU title credits (#210), Battle cursor row (#206), Items screen crash
  fix (#201), Text speed and Encounter rate (#203), Music and SFX volume
  (#194), Port options saved to port_options.cfg (#191), crash-report
  backtrace (#204). From preview.29: faster Port options, crash-report
  rotation, rumble (unverified), HOME/sleep save flush. From preview.28:
  EU language selection at cold boot and after restart, battle
  camera/portrait fixes, Options on its own game-style screen.

Bundle

  dw3-switch.nro              US ordinary build
  dw3-switch-nxlink.nro       US nxlink logging build
  dw3-switch-eu.nro           EU ordinary build (attached by hand)
  dw3-switch-elf.zip          US unstripped ELFs and BUILD_IDS.txt
  README.txt                  these notes

  The US pair and dw3-switch-elf.zip come from the release workflow. The
  EU ordinary build is attached by hand to the release, as in earlier
  previews. The EU nxlink build is not published in this preview.
  No original game files, BIN/CUE/ISO, BIOS or extracted tree ship here.

Own-disc setup and extraction

  Use Digimon World 3 US (SLUS-01436) with the US NRO, or Digimon World
  2003 EU (SLES-03936) with the EU NRO. Region paths remain separate:

    US runtime: sdmc:/switch/dw3/us/
    EU runtime: sdmc:/switch/dw3/eu/

  US: place the matching Mode2 BIN/CUE beside us/, in switch/dw3/.
  EU: place the matching Mode2 BIN/CUE inside switch/dw3/eu/ (for
  example disc.bin + disc.cue). The EU NRO extracts SLES_039.36,
  boot_tim.bin, files_eu.json and mapped runtime files there.

  First boot draws an extraction progress bar. Leave it running.
  A wrong-region disc is rejected before accepting its map. Existing
  runtime files are validated before game startup; old 2048-byte-sector
  STR files may be restored from the BIN. The following boot reuses
  the validated runtime. Do not pre-extract files when testing the
  NRO's own extraction path.

  Preserve us/, eu/ and their memcard directories. Never delete or
  move save0.bin, save1.bin or .mcr.bak to force extraction. No save
  migration between US and EU is implied by this release.

Install / nxlink

  Copy the desired NRO to SD and launch it from your homebrew launcher.
  For PC logs, open the launcher's nxlink receiver (Sphaira waiting
  for nxlink, or hbmenu Y NetLoader), on the same network, then:

    nxlink -a <Switch-IP> -s dw3-switch-nxlink.nro

  This preview publishes only the US nxlink build. Do not probe TCP 28280
  first: an empty connection can break the receiver. A failed logging
  connection does not prevent boot; logs also go to
  sdmc:/switch/dw3/dw3.log.

  hbmenu's NACP version is limited to 15 characters. Build tooling
  derives it and the full boot-log version from the release tag. Check
  the dw3: version= line for the exact running release; an nxlink load
  may not refresh launcher metadata. Check [init_paths] cdfs root=
  for the expected us or eu region directory.

OLED checklist

  - Post-battle: with R3 on and Text speed Instant, win a battle. The
    results screen should run at battle speed and the win jingle should
    play. Back on the map the field music should be back (if not, send an
    nxlink log). The area name banner's two names should appear whole.
    dw3.log should have no "dw3-turbo: speed 1x" between the battle and the
    field unless you pressed R3.
  - Field menu: open Items, Sort, Techniques, Status and Card folders. The
    panels should reach the edges of the 16:9 picture, and the dissolve
    should cover the full width.
  - Title screen, the intro TV caption bar and the Digimon Online
    registration screen should fill 16:9, with no black side bars or seam.
    Movies and the still screen stay 4:3.
  - Battle in gpu-tex: Kotemon's portrait, in the first Kuwagamon fight and
    with a few other partners, should not be see-through where it is dark.
  - Field turbo: at 4x, note the nxlink HUD fps in the Forest Inn, top and
    bottom.
  - Long session: play 1 to 2 hours with several battles. dw3.log may show
    "dw3-file-cache: all 64 slots taken" lines; there should be no crash.
  - Crash notice: force a crash (docs/P5.4_CRASH.md, OLED checklist), boot
    again and read the panel. Press A once it is read. Leave it alone once
    to see the 60 s timeout. Press HOME during it once.
  - Music: it loops in the field, the inn and battle. Menu sounds and the
    battle text blips are new: check their level.
  - Options (eight rows): change every row, close, quit the NRO, start it
    again from hbmenu: the values must come back (port_options.cfg).
  - Techniques in battle: the backdrop fills 16:9 and, after a tech,
    sound effects and music keep playing. Steal with Claw on a last hit.
  - Save, press HOME, come back; save, sleep, wake. The save must
    survive closing and relaunching the NRO.
  - EU: the title credits in each language, and the title, caption bar and
    registration screen at 16:9.
  - Rumble in field and battle (Joy-Con handheld, Pro Controller).
  - After any crash, keep crash_report.txt and dw3.log before
    relaunching; the next boot keeps one previous report. Symbolize it
    with the matching ELF from dw3-switch-elf.zip.

This is a prerelease, not a finished port. Use the CMake game image,
not the small Makefile host_hello sample built by CI job switch.
No clkrstSetClockRate or appletSetCpuBoostMode is used; CPU/GPU clocks
stay at the console's settings. No overclock is needed for testing.

Digimon and Digimon World are trademarks of their owners. This is an
unofficial fan project, not affiliated with Bandai Namco or Nintendo.
