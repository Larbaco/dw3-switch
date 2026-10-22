Digimon World 3 / 2003, Switch homebrew preview

Release notes for v0.6.5-preview.29. The release tag and boot version
identify the actual source revision.

New in this preview

  Crash fix at the first story gate (#187). On the new-game walk to the
  Digital World bridge, the "Oh yeah! I need my partner" event crashed
  the game on US and EU (preview.28 too). Port patch 0058 skips drawing
  a speech balloon that was closed before it ever opened. OLED handheld:
  the owner passed the gate without a crash on the candidate build.

  Faster Port options (#185). The Options screen keeps its picture and
  redraws only what changed: a cursor move uploads about 434 KB instead
  of 2.7 MB, a value change about 30 KB. OLED handheld (EU): the owner
  reports the menu is much more responsive; the log shows ~6 ms per
  cursor move. Opening the title menu builds the cache once (~150 ms).

  Crash reports (#182). crash_report.txt now keeps the previous run as
  crash_report_prev.txt, dw3.log says whether the previous run crashed,
  and the report includes frame counters. Hardware-unverified.

  Rumble (#183). PS1 vibration requests go to HD rumble on handheld
  Joy-Con and player 1. Hardware-unverified; the motor mapping may feel
  wrong. Report what you feel (or do not feel), and on which controller.

  HOME / sleep (#181). Losing focus (HOME, sleep) flushes pending saves
  and the frame timer ignores the suspended gap. Hardware-unverified:
  test with a save you can afford to lose.

  Automated replay tests now cover the new-game opening, map walks, the
  bridge gate and a long field soak on PC, and the Switch build can be
  replayed in the Ryubing emulator (#186). These do not change the game.

Carried forward

  Widescreen starts ON; Port options can turn it OFF. Battle defaults
  to gpu-tex; Minus + ZL + ZR cycles rendering modes. Known black sky
  gaps on some battle stages remain. Field rendering stays software.
  Gameplay starts at 1x; R3 toggles the configured 2x/3x/4x turbo, or
  OFF disables it. Port options are session-only. Movies, title and
  standalone menus stay at 1x; gameplay audio accelerates with turbo.

  Native title choices New Game / Options / Load Game, with EU labels
  from the current game language; Options opens Port options.
  Opening-video read-race fix, left-stick input, flat saves and the
  memory-card load/save progress bar.

  Restart hotkey remains OFF by default. Enable it in Port options,
  then hold L + R + Plus + Minus. Dirty saves are flushed before the
  game returns to its title without quitting the NRO. A flush failure
  cancels restart. Unsaved progress is lost. Release before repeating.
  This chord can also toggle the nxlink debug HUD, whose own chord is
  Minus + L + R. See docs/HOTKEYS.md for all mapped controls.

  From preview.28: EU language selection at cold boot and after restart,
  battle camera/portrait fixes, Options on its own game-style screen.

Bundle

  dw3-switch.nro              US ordinary build
  dw3-switch-nxlink.nro       US nxlink logging build
  dw3-switch-eu.nro           EU ordinary build
  README.txt                  these notes

  This preview publishes the US pair from the release workflow, plus the
  EU ordinary build attached to the release by hand. The EU nxlink build
  is not published in this preview.
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

  - US: New Game, walk to the bridge "need my partner" event, close the
    box and keep walking (the OLED check so far was EU only).
  - Options: hold and tap Up/Down, change every row, close and reopen;
    on EU also in French or German. Compare with preview.28.
  - Save, press HOME, come back; save, sleep, wake. The save must
    survive closing and relaunching the NRO. dw3.log should show
    "applet focus: lost ... flushed" and "applet focus: restored".
  - Rumble in field and battle (Joy-Con handheld, Pro Controller).
  - After any crash, keep crash_report.txt and dw3.log before
    relaunching; the next boot keeps one previous report.

This is a prerelease, not a finished port. Use the CMake game image,
not the small Makefile host_hello sample built by CI job switch.
No clkrstSetClockRate or appletSetCpuBoostMode is used; CPU/GPU clocks
stay at the console's settings. No overclock is needed for testing.

Digimon and Digimon World are trademarks of their owners. This is an
unofficial fan project, not affiliated with Bandai Namco or Nintendo.
