Digimon World 3 / 2003, Switch homebrew preview

Release candidate notes for v0.6.5-preview.28. OLED acceptance of the
changes below remains pending. The release tag and boot version identify
the actual source revision.

New in this preview

  EU restart returns to a clean language selection (#162). The title
  overlay is hidden on restart. This change applies to the EU build only;
  the US build has no language selection and is unchanged by it.

  EU cold boot uses the original language-selection screen (#143).
  Choose the language before the opening movie and title. US boot remains
  unchanged. Continue loads the existing EU save using the selected boot
  language.

  Camera/portrait/render-state fixes (#148): correct battle idle-camera
  transforms, preserve opaque black portrait pixels (including pupils),
  and restore additive blending for the debug HUD and title after battle
  fades. Verify battle -> save -> restart: logo and menu must be visible.
  Builds and automated tests passed; OLED acceptance is still pending.

  Options has its own game-style screen (#149): dimmed title backdrop,
  choice boxes, values and contextual help. The #152 follow-up to #151
  corrects the restart-help wording. OLED navigation and presentation are test targets,
  not release acceptance already completed.

  The field-turbo raster skipping experiment from #146 is excluded.
  Existing R3 turbo remains; an indicated frame rate does not by itself
  prove the requested gameplay-speed multiplier.

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
  Minus + L + R. OLED save -> restart -> Continue acceptance remains
  to be recorded. See docs/HOTKEYS.md for all mapped controls.

Bundle

  dw3-switch.nro              US ordinary build
  dw3-switch-nxlink.nro       US nxlink logging build
  README.txt                  these notes

  This preview publishes the US pair only, through the release workflow.
  The EU pair is NOT in this release. It is built from the same source
  with --eu, and may be attached later after its source and hashes are
  verified. Do not expect dw3-switch-eu.nro in this preview.
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

    nxlink -a <Switch-IP> -s dw3-switch-eu-nxlink.nro

  Use dw3-switch-nxlink.nro instead for US. Do not probe TCP 28280
  first: an empty connection can break the receiver. A failed logging
  connection does not prevent boot; logs also go to
  sdmc:/switch/dw3/dw3.log.

  hbmenu's NACP version is limited to 15 characters. Build tooling
  derives it and the full boot-log version from the release tag. Check
  the dw3: version= line for the exact running release; an nxlink load
  may not refresh launcher metadata. Check [init_paths] cdfs root=
  for the expected us or eu region directory.

OLED checklist

  - EU language selection -> full opening -> title -> Load Game.
    Check language, video/audio and the EU root/version in logs.
  - Options: move through every row, change values, inspect the help
    text, press X to return and confirm Load Game still opens.
  - Battle: inspect Agumon's eyes and the idle camera. After leaving
    battle, check that the debug HUD remains readable.
  - Complete an ordinary save, enable Restart hotkey, then hold
    L + R + Plus + Minus. Logo/menu must stay visible; load the same
    save. Never interrupt an active save operation.
  - Toggle R3 in field and battle and cross a battle transition.
    Compare widescreen ON/OFF and check for regressions in Inn fades.

This is a prerelease, not a finished port. Use the CMake game image,
not the small Makefile host_hello sample built by CI job switch.
No clkrstSetClockRate or appletSetCpuBoostMode is used; CPU/GPU clocks
stay at the console's settings. No overclock is needed for testing.

Digimon and Digimon World are trademarks of their owners. This is an
unofficial fan project, not affiliated with Bandai Namco or Nintendo.
