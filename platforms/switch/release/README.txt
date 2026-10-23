Digimon World 3 / 2003, Switch homebrew preview

Release notes for v0.6.5-preview.30. The release tag and boot version
identify the actual source revision.

New in this preview

  Where a note says "OLED", the owner played a candidate build of this
  preview (rc4 to rc8) on a Switch OLED. Everything else is checked on
  PC and in the Ryubing emulator only, and is marked unverified.

  Music loops again (#209). On the rc7 candidate the background
  music played once and then stopped, in the field, the inn and battle
  (not checked on preview.29). The sequence loop count is read again.
  OLED (US, rc8): the music loops.

  Win jingle and missing sounds (#215). The victory jingle on the
  post-battle results screen never played on any build: the sequence
  table rows were spaced wrong for 64-bit entries, so tracks 16 to 31 of
  the system bank hit a misaligned entry. They now play. That also
  brings the menu sounds (tracks 25, 26, 28) and the battle text blips
  (22, 23), which were silent too, and the jingle follows Music volume.
  Unverified on hardware: whether the jingle, blips and menu sounds are
  audible and at a good level. If the battle blips sound too loud,
  please report it. This is most likely the "silent after a won battle"
  seen on rc8.

  Widescreen reaches more screens. The area name banner (#205) and the
  technique backdrops (#202) now fill the 16:9 picture. OLED (US, rc8):
  both extend correctly. The post-battle results screen (#213) was 4:3
  with black side bars and now draws with the widescreen gutter. That
  one is unverified on hardware.

  EU title credits (#210). The EU title shows the game's own native
  three-line credits in every language bank. OLED (rc8): the three lines
  appear as expected.

  Battle cursor (#206). A new row on the Options screen (now eight rows):
  OFF, Command, or Command+Tech. With Command+Tech the battle menu opens
  on the last command and the technique list on the technique that
  partner used last. Off by default. OLED (US, rc7): works, and the
  eight rows fit.

  Items screen crash fix (#201). Opening Items in the field menu crashed
  the game on US and EU (preview.29 too) the first time a bag held an
  item. OLED (rc7): Items opens and works.

  Instant text and the area banner (#203, #196, #200). Port options gain
  Text speed (Normal, Fast, Instant) and Encounter rate (OFF, 0.5x, 1x,
  1.5x, 2x); both default to the original game. The 103-frame "slow
  pages" seen with Instant were the area name banner, not text, and are
  no longer logged as text. OLED: Instant text works (US, rc7); Encounter
  rate changes were exercised in rc4 and rc6 (EU). Fast is not checked on
  hardware.

  Music volume and SFX volume (#194). Two Port options rows, 0 to 10,
  10 is the original mix. Movie audio follows Music volume. The Options
  screen plays the game's own cursor and choice sounds (#200; this part
  is unverified on hardware). OLED (EU rc6, US rc7): the volume rows work.

  Port options are saved (#191). Widescreen, Fast-forward, Restart hotkey
  and the new rows are written to port_options.cfg in the region folder
  (sdmc:/switch/dw3/us or eu) on every change. OLED (EU, rc4): the file
  is created and rewritten on every change. Restoring the values after
  a relaunch from hbmenu is not yet confirmed on hardware.

  Crash reports (#204). crash_report.txt now has the module base,
  a backtrace, the build id, and the last log lines, and dw3.log ends
  with a CRASH line. tools/symbolize_crash.py in the source turns a
  report into file:line when you have the matching .elf. OLED (US and
  EU, test build with a deliberate fault): a full report was written and
  resolved. The .elf files are not part of this release.

  Battle fixes (#197, #198). Picking or Snapping Claw works on an enemy
  the same hit knocks out ("Stole X!" comes before "Defeated!"), and the
  battle HUD shows the Digimon the partner is fighting as after a
  Digivolution. Both unverified on hardware.

  Audio fix (#188). A technique sound effect no longer silences all audio
  until the next map change (voices that end without a loop are freed).
  Unverified in the game on hardware.

  Under the hood: NULL-read guards for actor marks and the camera (#192),
  loud failure when an overlay cannot be restored (#195), and the
  nxlink HUD fps and the 60-pump log windows count one frame per VSync
  again (#190; preview.29 showed double the real rate). Gameplay was
  not affected by the counter.

Known issues

  If the map stays silent after a battle (no field music once you are
  back), please send a --nxlink log. This was reported on rc8 (US) but
  could not be reproduced on PC or in Ryubing, so its cause is unknown.

  The Items menu is still 4:3: the transition into it and the menu
  itself, while the background is 16:9. The area-change transition
  (iris) is also 4:3 and was not checked on hardware.

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

  From preview.29: faster Port options, crash-report rotation, rumble
  (unverified), HOME/sleep save flush. From preview.28: EU language
  selection at cold boot and after restart, battle camera/portrait
  fixes, Options on its own game-style screen.

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

  - Music: it loops in the field, the inn and battle; win a battle: the
    win jingle should play on the results screen and the field music
    should be back on the map (if not, send an nxlink log). The results
    screen should have no black side bars. Menu sounds and the battle
    text blips are new: check their level.
  - Options (eight rows): change every row, close, quit the NRO, start it
    again from hbmenu: the values must come back (port_options.cfg).
  - Techniques in battle: the backdrop fills 16:9 and, after a tech,
    sound effects and music keep playing. Steal with Claw on a last hit.
  - Save, press HOME, come back; save, sleep, wake. The save must
    survive closing and relaunching the NRO.
  - EU: the title credits in each language.
  - Rumble in field and battle (Joy-Con handheld, Pro Controller).
  - After any crash, keep crash_report.txt and dw3.log before
    relaunching; the next boot keeps one previous report.

This is a prerelease, not a finished port. Use the CMake game image,
not the small Makefile host_hello sample built by CI job switch.
No clkrstSetClockRate or appletSetCpuBoostMode is used; CPU/GPU clocks
stay at the console's settings. No overclock is needed for testing.

Digimon and Digimon World are trademarks of their owners. This is an
unofficial fan project, not affiliated with Bandai Namco or Nintendo.
