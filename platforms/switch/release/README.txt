Digimon World 3 / 2003, Switch homebrew preview

Release notes for v0.6.5-preview.33. The release tag and boot version
identify the actual source revision.

Player guide (setup, controls, Options, saves, crash reports):
https://github.com/Larbaco/dw3-switch/blob/main/docs/PLAYER_GUIDE.md

New in this preview

  No item below has been checked on a Switch OLED yet. Each is checked on
  PC and in the Ryubing emulator only, and is marked unverified.

  Quit game on the title menu (P6.28). The title has a fourth plaque, Quit
  game, below Load Game. It asks Yes or No (the cursor starts on No). Yes
  flushes the dirty save slots and returns to hbmenu, as the HOME exit
  does. A failed flush cancels the quit. On a PC build it closes the
  window. The title plaques are now 48 px tall instead of 54, so four fit.
  Unverified on hardware.

  Return to title in the field menu (P6.27). The field menu (Plus) has a
  last row, TITLE SCREEN (RETOUR TITRE, TORNA AL TITOLO, TITELBILD, VOLVER
  AL INICIO). It asks Yes or No, then runs the same reset as the restart
  hotkey: dirty saves are flushed, a failed flush cancels it, and unsaved
  progress is lost. On the EU disc the game lands on the language select,
  as the hotkey does. Unverified on hardware.

  Both confirmations are defaults chosen while the owner was away. The
  owner can change them.

  Cross-region saves, slice 1 (P6.25, #252). When a region has no save of
  its own, the build imports the other region's save once. It converts the
  save in memory and writes a new native saveN.bin plus a saveN.import
  marker. The original save is never modified. New saves stay native to
  the region and do not flow back to the other one. Bad or corrupt sources
  are refused, with no crash. EU extra-chapter saves are not imported into
  US. The details and the OLED checklist are in
  docs/CROSS_REGION_SAVES.md#as-built-slice-1. Unverified on hardware.

  Restart hotkey validated on PC, US and EU (P6.9, #251). Checked before,
  inside and after a battle, after an in-game save then Load Game, and
  after New Game. No bugs found. Test only; no game change.

  PC only: the map no longer reappears over the grid cover after a door
  iris closes (#253). This showed on the PC sdl3_gpu renderer, in 4:3 too.
  It is fixed in PSY-Z (pin e3a7e83). The Switch is unaffected.

Known issues

  If the map stays silent after a battle (no field music once you are
  back), please send a --nxlink log. This was reported on rc8 (US) but
  could not be reproduced on PC or in Ryubing, so its cause is unknown.
  The win jingle and SEP fix (#215) are in this release. The underrun
  count is still open.

  The Map screen is 16:9 with black side strips (#240). The encounter
  spiral is 4:3 with black bars (#247), by design for now.

  The Digimon Lab's chart, entry and skill screens keep their left panels
  at the 4:3 edge (#245). Open Booster was not captured in the 16:9 pass.

  Cheats page 2 Warp can leave a story-flagged map looking unfinished,
  like the debug stage select.

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

  From preview.32: canal water in the Cargo Tower streets after the
  Underground Path, fixed (#242, patch 0111); 16:9 for the card album, deck
  editor, card shop, gym and Digimon Lab (#245; the Lab's chart, entry and
  skill screens are still 4:3 at the edges); 16:9 door iris and blue grid
  cover (#247); Cheats screen page 2 (P6.20, #248): Money, Warp to 13
  areas and Give item, behind the same master switch, which is off by
  default. From preview.31: field rasterizer 4 to 5 times faster (P6.22,
  #220); 16:9 for the field menu dissolve, the Items, Sort, Techniques,
  Status and Card folders, the title menu, the intro caption bar and the
  registration screen (#218, #219, #224); 16:9 for the map, battle results
  header and save and load screens (#240); post-battle report at turbo
  speed (#223); partner portrait in gpu-tex (#229); file cache eviction
  instead of the 64-slot crash (#235); crash reports and the "last run
  crashed" notice (P5.4, #217, #231); dw3-switch-elf.zip (#216); Cheats
  screen page 1 in Options (P6.20, #236). From preview.30: music loops
  (#209), the win jingle and the menu and battle sounds (#215), widescreen
  area banner and technique backdrops, EU title credits (#210), Battle
  cursor row (#206), Items screen crash fix (#201), Text speed and
  Encounter rate (#203), Music and SFX volume (#194), Port options saved
  to port_options.cfg (#191), crash-report backtrace (#204). From
  preview.29: faster Port options, crash-report rotation, rumble
  (unverified), HOME/sleep save flush. From preview.28: EU language
  selection at cold boot and after restart, battle camera/portrait fixes,
  Options on its own game-style screen.

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
  move save0.bin, save1.bin or .mcr.bak to force extraction. Saves are
  not shared between US and EU, except the one-time import of a region
  that has no save of its own (see Cross-region saves above).

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

  New in this preview:
  - Title: Down to Quit game (the last entry), Cross: the window opens on
    No. No closes it. Yes returns to hbmenu; dw3.log ends with "dw3-port-quit:
    saves flushed". Saves are still there on the next launch. HOME and
    "close the software" must still exit as before. Check US and EU.
  - Return to title: open the field menu with Plus: the last row asks Yes or
    No; Yes returns to the title (the language select on EU). The text should
    fit in every language; with the card folders row the panel has seven rows.
  - Cross-region import: start EU with only a US save present (in us/),
    then the reverse. The imported save should Continue with that progress,
    and the import should happen once.

  Still pending from preview.32:
  - Door transitions: the iris and the grid cover should fill the full 16:9
    width. The encounter spiral keeps black bars; that is known.
  - Card screens: the card album, deck editor, card shop, gym and Digimon
    Lab panels should reach the edges of the 16:9 picture.
  - Underground Path: the canal water in the Cargo Tower streets should be
    right at once, without scrolling.
  - Cheats page 2: with the master switch on, try Money, Warp and Give item.
    A save after use gets the save<N>.cheats marker.

  After any crash, keep crash_report.txt and dw3.log before relaunching;
  the next boot keeps one previous report. Symbolize it with the matching
  ELF from dw3-switch-elf.zip.

This is a prerelease, not a finished port. Use the CMake game image,
not the small Makefile host_hello sample built by CI job switch.
No clkrstSetClockRate or appletSetCpuBoostMode is used; CPU/GPU clocks
stay at the console's settings. No overclock is needed for testing.

Digimon and Digimon World are trademarks of their owners. This is an
unofficial fan project, not affiliated with Bandai Namco or Nintendo.
