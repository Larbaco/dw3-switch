Digimon World 3 / 2003, Switch homebrew preview

Release notes for v0.6.5-preview.48. The release tag and boot version
identify the actual source revision.

Player guide (setup, controls, Options, saves, crash reports):
https://github.com/Larbaco/dw3-switch/blob/main/docs/PLAYER_GUIDE.md

New in this preview

  Except log rotation, no item below has been checked on a Switch OLED yet.
  Each is checked on PC and in the Ryubing emulator only, and is marked
  unverified. Log rotation (#296, #301) was confirmed on the OLED with
  preview.45: dw3.1.log is kept and the rotated line shows rc=0x0.

  One .nro for US and EU (P6.24, #314, #316): dw3-switch-both.nro carries
  both games. It looks for the data under sdmc:/switch/dw3/us/ and
  sdmc:/switch/dw3/eu/. With only one of them present it starts that game at
  once. With both, a picker shows for 3 seconds and starts the last choice
  (the default). Press Y to stop asking (start that region every time); hold
  Minus while you press A on the .nro in hbmenu to ask again; + quits. Every
  decision is a `dw3-region:` line in dw3.log. The separate US and EU .nro
  files are still published until the owner confirms the combined one on the
  OLED. It has not yet been run on a Switch OLED: see plan G9.x in the OLED
  test plan before relying on it.

  First mod: map upscale loader (#313, plan B17). The map can use an
  upscaled picture pack in sdmc:/switch/dw3/mods/<pack>/. Without a pack, or
  if a pack is bad, the original map is used. map_mod=0 in port_options.cfg
  turns it off. The pack itself is NOT in this release: it is made locally
  from your own disc with tools/mods/extract_map_assets.py.

  Audio health and the #188 leak signature (P4.3, #315, plans D13 and D14):
  dw3.log has a `dw3-audio: health` line every 30 s, `dw3-audio: underruns`
  lines every minute and a `dw3-audio: exit:` line at quit. A rising voices
  count with nothing playing, or `WARNING SsUtKeyOn found no free voice`, is
  the #188 voice leak. Save writes also log `dw3-save: write ... committed`
  lines (P10.2, plan G2).

  Docs only (#310, #311, #312, #317, #318): the roadmap Priority section, the
  in-game walkthrough design, OLED bookkeeping, the controls bundle design
  and the technical help design with its first English text draft. Nothing in
  the game changed for them.

  Map view has the game's own balloon, cursor and name box (#306, plan
  B16). The balloon sits on your current area and the cursor (an arrow, or a
  hexagon on a location) moves with the D-pad or left stick; the view follows
  it when zoomed. The right stick and L / R zoom, ZL / ZR cycle the scaler,
  and Circle hides the HUD. On a location the game's name box appears. The
  balloon, cursor and name box scale and pan with the map. Nearest is the
  default scaler. Lanczos2 and FSR 1 build on a worker thread: the HUD shows
  BUILDING and the buttons stay live while they build.

  In-game Options opens faster (#308, plan B15b): the Options screen has its
  own 960x720 texture, so opening it from the field no longer builds the big
  title atlas. The log line `dw3-title-overlay: 960x720 RGBA options texture
  (GLES), create N us` shows the cost. Preview.45 had the first frame about
  93 ms after the open.

  EU language select is 16:9 (patch 0177, #308, plan E1.6b): the tile
  pattern, the header and logo bands and the flags' panel run to the side
  edges, with no 4:3 frame before the title.

  Title lettering and verbs (#308, plans E2.0 and E2.2): on EU, every plaque
  uses the port's lettering, the same as Options and Quit game. Without an
  autosave the title shows New Game, Options, Load Game and Quit game. With an
  autosave it shows New Game, Options, Continue, Load Game and Quit game, in
  the five EU languages.

  Docs only (#305, #307): the MOD_MAP_UPSCALE design note (the first mod: an
  AI-upscaled map pack, not in any release) and a docs index fix. Nothing in
  the game changed for them.

  From preview.46: Options is a list of groups (P9.9 slice 1, #303): Options from the title
  menu opens a list of plaques in the title menu's style, Display, Sound,
  Gameplay and Saves, plus Cheats once its master switch is on. B opens a
  group; X goes back to the list, with the cursor kept on the group you left. The same groups appear from the field menu OPTIONS and from the
  battle Options (Minus at the command menu). Widescreen and Cheats are greyed
  in game, as before. Every setting, key and port_options.cfg entry is
  unchanged, and the five EU languages carry the new labels. Plans A5, A9 to
  A12. Unverified on hardware.

  Options opens without the stutter (#298): the static Options window is
  prepared ahead of time on a worker thread, so an open is a copy. Expected
  about 10 ms on the Switch, not measured on hardware yet (plan B15).

  Return to title fades through black (#298): the field menu entry and the
  restart chord fade the picture to black, wait for the title (EU: the
  language select) and fade it in, so no squeezed 4:3 frame should show.
  Plan E1.

  Map view shows the revealed map (#299): on the Map screen, Y draws the
  towns and the visited areas the way the game's Map does. Before, only the
  dark base map showed. The right analog stick zooms (up in, down out); L and
  R still step. Plan B16.

  Log rotation fixed (#296): dw3.log is closed at exit. If the system refuses
  to rename it, the build copies and deletes it instead, and never truncates
  it. Each launch writes `dw3-log: rotated (...)`, with r (renamed), c
  (copied) or - (nothing or failed) for each step, and the count of kept
  previous runs. After two launches sdmc:/switch/dw3/ should hold dw3.log and
  dw3.1.log.

  Log rotation paths sized for the Switch file service (#301): the rotation
  paths were 48-byte stack arrays, but libnx passes the Fs service a fixed
  0x301-byte path buffer. That read past them and crashed the Ryubing replay
  right after boot. The paths are now FS_MAX_PATH bytes. This may also be why
  dw3.1.log was never kept on hardware; not confirmed yet. Check the OLED
  item for the log below.

  Profile id in the log (#294): the short id in dw3.log (`user: profile=`)
  and in crash_report.txt (`profile:`) is now a hash of the user id, so each
  profile shows its own value. The saves folder name is unchanged.

  Docs only (#295, #297): README refresh; roadmap decisions for P6.30 and a
  new walkthrough help goal, P6.35.

  Per-user saves (P6.26, #292): saves are kept per Switch user profile, in
  sdmc:/switch/dw3/saves/<profile>/<us|eu>/memcard/ (save0, save1 and
  autosave). The profile comes from the forwarder's profile selector, else
  the only user on the console, else the system selector. Cancelling the
  system selector exits without starting the game. Legacy saves (the old
  <us|eu>/memcard/ folder) are COPIED into each profile's folder on that
  profile's first launch; the originals are not touched. On a shared console
  each profile gets its own copy, so A's later saves are not in B's copy.
  Unverified on hardware: test plan E5.

  Logs (#290): sdmc:/switch/dw3/dw3.log keeps the last 5 runs (dw3.log is
  the newest, then dw3.1.log ... dw3.4.log). Game messages for the map,
  options and saves now reach the log, and new proof lines match the OLED
  test plan items. Send the whole set of sdmc:/switch/dw3/dw3*.log files.

  Still open (checked against the OLED test plan and the owner's preview.47
  log): the app name (#289, look only), the full-screen map view's balloon,
  cursor and zoom (plan B16; it was not opened in the preview.47 log), the EU
  language select at 16:9 (plan E1.6b, needs an EU run), the audio check after
  many techs on the EU build (#188, plan D13), and the items_partner_switch=0
  sub-check of the partner cycle (plan G8). Log rotation (#296, #301) was
  confirmed on the OLED with preview.45.

Known issues

  If the map stays silent after a battle (no field music once you are
  back), please send a --nxlink log. This was reported on rc8 (US) but
  could not be reproduced on PC or in Ryubing, so its cause is unknown.
  The win jingle and SEP fix (#215) are in this release. The underrun
  count is still open.

  The Map screen is 16:9 with black side strips (#240).

  The Digimon Lab's chart, entry and skill screens keep their left panels
  at the 4:3 edge (#245). Open Booster was not captured in the 16:9 pass.

  Cheats page 2 Warp can leave a story-flagged map looking unfinished,
  like the debug stage select.

  Sleep and resume work and play time counts only play time, but the
  "applet focus" lines do not appear in dw3.log (OLED, rc7). Rumble is
  still unverified on hardware.

  In-game Options: the first frame after the open took about 93 ms on the
  Switch in preview.45. preview.47 aims for under 20 ms (plan B15b), not
  measured on the OLED yet. The map HUD strip covers the top of the map at
  1.00x. EU audio after many techs is still open (#188, plan D13).

Carried forward

  Widescreen starts ON; Port options can turn it OFF. Battle defaults
  to gpu-tex; Minus + ZL + ZR cycles rendering modes. Field rendering
  stays software. Gameplay starts at 1x; R3 toggles the configured 2x/3x/4x
  turbo, or OFF disables it. Movies, title and standalone menus stay
  at 1x; gameplay audio accelerates with turbo.

  Native title choices New Game / Options / Load Game, with EU labels
  from the current game language; Options opens its group list (#303).
  The bridge "I need my partner" event (patch 0058) passes without a
  crash on US and EU hardware.

  Restart hotkey remains OFF by default. Enable it in Port options,
  then hold L + R + Plus + Minus. Dirty saves are flushed before the
  game returns to its title without quitting the NRO. A flush failure
  cancels restart. Unsaved progress is lost. Release before repeating.
  This chord can also toggle the nxlink debug HUD, whose own chord is
  Minus + L + R. See docs/HOTKEYS.md for all mapped controls.

  From preview.42: App name (#289): the title in hbmenu and Sphaira is now
  Digimon World 3 NX. The nxlink build shows Digimon World 3 NX (nxlink). The
  .nro file names are unchanged (dw3-switch.nro, dw3-switch-eu.nro,
  dw3-switch-nxlink.nro). A forwarder made from an older build keeps the old
  name, so recreate the forwarder in Sphaira (or re-add the .nro in hbmenu) to
  see the new name. Unverified on hardware.

  From preview.41: Turbo: menus run at 1.5x (P6.4, #286). With turbo on, menus
  in the field (the Start list, Options and Cheats, the Return to title
  question) and the battle menus (the command menu and the technique list) run
  at 1.5x instead of full turbo. Walking, text, cutscenes and battle
  animations keep full turbo. Map, Items and shops stay at 1x. Plan C7-C9.

  Title: menu plaques evenly spaced, constant logo size (patch 0129,
  #287). All menu plaques sit on one spacing, 4 or 5 rows. The logo no
  longer shrinks when the menu appears. The EU logo sits where the game
  places it and is about 6% smaller than before. Needs OLED confirmation.

  From preview.38: encounter swirl covers the 16:9 picture (P6.1, #275). At
  the start of a random battle, the swirl now fills the full widescreen
  picture. Before, the side gutters went black on its first frame and the
  swirl stayed in a 4:3 box. Now the tiles slide off across the full width,
  in the same time as before. Checked on PC and in the Ryubing emulator at
  turbo off, 2x, 3x and 4x. Widescreen off is unchanged. Unverified on
  hardware. OLED check B12 is still open.

  From preview.37: Options in battle (P9.7 slice 2, #272): at the battle
  command menu, Minus (Select) opens Options over the frozen battle; do not
  hold L, R, ZL or ZR. Triangle closes it and the battle goes on. Widescreen
  and Cheats are greyed; there is no on-screen hint, the key is in
  docs/HOTKEYS.md. Save write safety (P10.2, #273): an interrupted save write
  recovers at boot, and HOME or restart during the game's save burst waits up
  to 8 s. If dw3.log shows `recovered ... from an interrupted write`, keep the
  log and tell the owner. Both unverified on hardware.

  From preview.36: Options from the field menu (P9.7 slice 1, #270): an
  OPTIONS row above TITLE SCREEN opens Options over the field. Fast-forward,
  Restart hotkey, volumes, Encounter rate, Text speed, Battle cursor and
  Auto-save apply at once and are saved. Widescreen is greyed in game, and
  Cheats stays title-only. Title screen textures (about 17 MB) are freed
  outside the title on Switch and rebuilt on return (#268); the first title
  frame after a return may be a little slower. PC soak of 2 h US and 1 h EU
  with no crash or leak, and the Ryubing memory check no longer flags the
  one-time title texture allocation (#266, #267). Unverified on hardware.

  From preview.35: Faster turbo outside battle (P6.22 pass 2, #263): with
  turbo above 1x, the Switch no longer draws the frames it will not show.
  Drawing is deferred and dropped unless something reads the pixels, such as
  the present or a screen copy. The picture is identical, and the colour
  conversion at present is faster. Expected gain in the field and opening at
  4x, not measured on Switch yet; battle is unchanged. Player guide covers
  Auto-save and the region launcher (#262); the region launcher is still not
  in a release. Unverified on hardware.

  From preview.34: Auto-save, off by default (P6.31, #260): a ninth Options
  row, OFF (default) or Each area. With it on, the first idle field frame of
  each new area saves to the slot you last saved to or loaded from, which
  overwrites that slot; the first time, the build keeps a copy of your version
  as save<N>.bin.pre-auto. It never runs before the first manual save or load
  of a session, and never during battles, events, menus or fades. Cheat saves
  carry the cheat mark. The slot choice and timing are defaults the owner can
  change. Region launcher (P6.24, #258, #259): not in this release. Docs
  (#255, #256, #257): player guide, CONTRIBUTING.md, a doc-link test, and the
  P6.32 map zoom proposal. Nothing in the game changed.

  From preview.33: Quit game on the title menu (P6.28, #250): a fourth
  plaque, Yes or No with the cursor on No; Yes flushes the dirty saves and
  returns to hbmenu, a failed flush cancels, and on PC it closes the window.
  Return to title in the field menu (P6.27, #250): same reset as the restart
  hotkey, Yes or No; EU lands on the language select. Cross-region saves,
  slice 1 (P6.25, #252): a region with no save of its own imports the other
  region's save once, one way, into a new native saveN.bin plus a
  saveN.import marker; the original is never modified, corrupt sources are
  refused, and EU extra-chapter saves are not imported into US (details in
  docs/CROSS_REGION_SAVES.md#as-built-slice-1). Restart hotkey validated on
  PC, US and EU (P6.9, #251), test only. PC only: the map no longer shows
  over the grid cover after a door iris closes (#253, PSY-Z pin e3a7e83);
  the Switch was unaffected. Both confirmations are defaults the owner can
  change. From preview.32: canal water in the Cargo Tower streets after the
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

  dw3-switch.nro                 US ordinary build
  dw3-switch-nxlink.nro          US nxlink logging build
  dw3-switch-eu.nro              EU ordinary build (attached by hand)
  dw3-switch-eu-nxlink.nro       EU nxlink logging build (attached by hand)
  dw3-switch-elf.zip             US unstripped ELFs and BUILD_IDS.txt
  dw3-switch-eu-elf.zip          EU unstripped ELF and BUILD_IDS.txt
  dw3-switch-eu-nxlink-elf.zip   EU nxlink unstripped ELF and BUILD_IDS.txt
  dw3-switch-both.nro            US + EU in one image (new; region chosen at boot)
  dw3-switch-both-nxlink.nro     the same, nxlink logging build
  dw3-switch-both-elf.zip        unstripped ELFs of both combined builds
  README.txt                     these notes

  The US pair, dw3-switch-elf.zip and the dw3-switch-both* files come from
  the release workflow. The EU files are attached by hand to the release, as
  in earlier previews. No original game files, BIN/CUE/ISO, BIOS or extracted
  tree ship here.

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
  move save0.bin, save1.bin, any save<N>.bin.pre-auto or .mcr.bak to force
  extraction. From this preview the game reads and writes each profile's
  copy in sdmc:/switch/dw3/saves/<profile>/<us|eu>/memcard/. The legacy
  <us|eu>/memcard/ files stay in place and are copied, never moved, into a
  profile on its first launch. Saves are not shared between US and EU, except
  the one-time import of a region that has no save of its own (see Cross-region
  saves above).

Install / nxlink

  Copy the desired NRO to SD and launch it from your homebrew launcher.
  For PC logs, open the launcher's nxlink receiver (Sphaira waiting
  for nxlink, or hbmenu Y NetLoader), on the same network, then:

    nxlink -a <Switch-IP> -s dw3-switch-nxlink.nro

  The US and EU nxlink builds are published (dw3-switch-eu-nxlink.nro is the
  EU one); the combined dw3-switch-both-nxlink.nro is the same logging build
  for either region. Do not probe TCP 28280
  first: an empty connection can break the receiver. A failed logging
  connection does not prevent boot; logs also go to
  sdmc:/switch/dw3/dw3.log. That folder keeps the last 5 runs (dw3.log is
  the newest, then dw3.1.log ... dw3.4.log); send all of them.

  hbmenu's NACP version is limited to 15 characters. Build tooling
  derives it and the full boot-log version from the release tag. Check
  the dw3: version= line for the exact running release; an nxlink load
  may not refresh launcher metadata. Check [init_paths] cdfs root=
  for the expected us or eu region directory.

OLED checklist

  New in this preview:
  - Options groups (plan A5, A9 to A12): from the title, the field menu
    (OPTIONS) and battle (Minus at the command menu), the four groups open
    and go back as described above, the plaques look like the title menu's,
    and the first open of a group is no black or garbage frame. Send the
    `dw3-port-options:` and `dw3-title-overlay:` lines, and a photo of the
    group list in handheld and docked.
  - Log rotation (#296, #301): confirmed on the OLED with preview.45 (dw3.1.log
    kept, rc=0x0). Keep sending the whole set of dw3*.log files.
  - Options open timing (plan B15): the `dw3-port-options:` lines give the
    prewarm, the first frame after the open request, the longest frame gap
    and the time the window stayed open. Expected about 10 ms to open, with
    no `rebuild` line (that was the old stutter).
  - Return to title fade (plan E1): the field fades to black, then the
    title or language select fades in, with no 4:3 picture ever on screen.
  - Map reveal, stick zoom and scaler preference (plan B16): the revealed
    towns and areas show on the Map, the right stick zooms, and the scaler
    keeps its choice. Send the `dw3-map:` lines.
  - Per-user saves (plan E5, E5.1 to E5.9): the profile selector, the
    per-profile folders, the copy of legacy saves, cancelling the selector
    and the crash report's profile line. Send dw3*.log and the folder listing
    of sdmc:/switch/dw3/saves/.
  - Logs (#290): each test item names the dw3*.log lines that prove it; send
    the whole set of dw3*.log files after the run.
  - Combined .nro (plan G9.1 to G9.6, P6.24): dw3-switch-both.nro with only
    one region's data, with both (picker, 3 s default, Y, Minus) and the
    stored choice. Send the `dw3-region:` lines.
  - Map upscale mod (plan B17): with a locally made pack the map is the
    upscaled picture; without it, or with map_mod=0, the original map. Send
    the `dw3-map:` lines.
  - Audio (plans D13 and D14, #188): the `dw3-audio:` health, underrun and
    exit lines after many techs and in a 10 minute session.
  - Save writes (plan G2, P10.2): the `dw3-save: write ... committed` and
    `interrupted` lines after HOME during a save.

  From preview.42, still open:
  - App name (#289): recreate the Sphaira forwarder (or re-add the .nro in
    hbmenu); the title reads Digimon World 3 NX, and the nxlink build reads
    Digimon World 3 NX (nxlink).

  From preview.41, still open:
  - Turbo menus at 1.5x (plan C7-C9): menus run slower than walking at 4x,
    and Map, Items and shops stay at 1x. The `dw3-turbo` log lines show it.
  - Title spacing and logo (#287, no separate plan): the plaques are evenly
    spaced and the logo keeps its size when the menu appears.

  Still open from preview.40:
  - Equipment ZL / ZR partner cycle (plan G8): the partner changes on each
    press, and nothing changes with items_partner_switch=0.
  - Auto-save and Load Game (plan E2): Continue loads autosave.bin, and
    Load Game opens your own slots.
  - Full-screen map view (plan B16): Y opens it, B or Y closes it, and the
    scaler names and cache folder are as described above.

  From preview.39, still open:
  - Quit game (plan A7): the exit must end with no Horizon error, and the
    log must show `audio stopped` and `gpu closed; leaving main`.
  - Field menu OPTIONS (plan B15): no black square when it opens.

  Everything else in this preview, and the items still open from earlier
  previews, is in the ordered OLED test plan. Follow it:
  https://github.com/Larbaco/dw3-switch/blob/main/docs/OLED_TEST_PLAN.md

  After any crash, keep crash_report.txt and the dw3*.log files (the last 5 runs are kept
  automatically, but copy them off before 5 more launches);
  the next boot keeps one previous report. Symbolize it with the matching
  ELF from dw3-switch-elf.zip.

This is a prerelease, not a finished port. Use the CMake game image,
not the small Makefile host_hello sample built by CI job switch.
No clkrstSetClockRate or appletSetCpuBoostMode is used; CPU/GPU clocks
stay at the console's settings. No overclock is needed for testing.

Digimon and Digimon World are trademarks of their owners. This is an
unofficial fan project, not affiliated with Bandai Namco or Nintendo.
