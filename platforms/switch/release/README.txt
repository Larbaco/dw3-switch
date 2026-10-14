Digimon World 3 / 2003, Switch homebrew preview

Release candidate notes for v0.6.5-preview.25. These notes describe the
intended bundle; publication and OLED acceptance are separate steps.
The release tag and boot version identify the actual source revision.

New in this preview

  Widescreen is ON by default (#132). Field and battle extend to about
  16:9, including the EU 240-line display. Field-menu, shop and save
  backgrounds extend into the sides; HUD content stays centered. Other
  menus keep their original width. Port options can switch it OFF.
  Some original map layers still end at the old edge; report the area.

  Switch battle defaults to gpu-tex (#132). The right-panel Digimon
  portrait is restored. Known issue: the cliff/sky battle stage still
  has black sky gaps in gpu-tex; soft draws that sky correctly.
  Minus + ZL + ZR cycles soft / soft+bbox (stub) / gpu-solid / gpu-tex.
  Field rendering remains software RGB555. Performance acceptance and
  a full stage sweep remain open; no CPU/GPU clock change is made.

  Fast-forward (#133, P6.4). Gameplay starts at 1x. Right-stick click
  (R3) toggles turbo in field and battle. Port options selects 2x, 3x,
  4x or OFF; OFF disables R3. Title, movies, standalone menus, the
  memory-card screen and card game stay at 1x. Audio speeds up with
  gameplay. Port options are session-only, not saved between launches.

  Both US and EU game builds are included, each with an ordinary NRO
  and an nxlink logging NRO. Neither includes disc data. EU first boot
  can extract its runtime from the owner's matching Mode2 BIN/CUE.

Carried forward

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
  dw3-switch-eu.nro           EU ordinary build
  dw3-switch-eu-nxlink.nro    EU nxlink logging build
  README.txt                 these notes
  SHA256SUMS                 hashes of the published files
  SOURCE.txt                 exact build source revision and versions

  The release workflow creates the US pair. The matching EU pair and
  provenance files are added after verifying their source and hashes.
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

  - Let EU extraction and the full opening finish; check image, audio,
    progress completion, and the EU root/version in logs.
  - Check title choices and Options; Widescreen should start ON.
  - Walk map edges and inspect menus in widescreen; compare OFF.
  - Battle: check the right-panel portrait and side geometry. Report
    stage-specific gaps; compare gpu-tex with soft on the known sky.
  - Toggle R3 in field and battle; compare 2x/3x/4x and OFF. Check that
    movies, standalone menus and the save screen remain at 1x.
  - After an ordinary in-game save, enable Restart hotkey, use the
    chord, then Load Game/Continue the same save. Check a fresh launch
    and reload too. Do not interrupt an active save operation.

This is a prerelease, not a finished port. Use the CMake game image,
not the small Makefile host_hello sample built by CI job switch.
No clkrstSetClockRate or appletSetCpuBoostMode is used; CPU/GPU clocks
stay at the console's settings. No overclock is needed for testing.

Digimon and Digimon World are trademarks of their owners. This is an
unofficial fan project, not affiliated with Bandai Namco or Nintendo.
