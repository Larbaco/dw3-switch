Digimon World 3 / 2003, Switch homebrew preview

Release notes for v0.6.5-preview.52. The release tag and boot version
identify the actual source revision.

Player guide (setup, controls, Options, saves, crash reports):
https://github.com/Larbaco/dw3-switch/blob/main/docs/PLAYER_GUIDE.md

New in this preview

  The owner's OLED checks are recorded in docs/OLED_TEST_PLAN.md (rows marked
  PASSED). Items that are not there yet are checked on PC and in the Ryubing
  emulator only, and are marked unverified. The OLED checklist at the end lists
  every row of that plan without a PASSED mark.

  Built locally, attached by hand: the files of this release were built on the
  build machine with tools/build_switch_game.sh and ccache on (docs/BUILD_SPEED.md).
  The release workflow run for the tag was cancelled before it published
  anything. The workflow does not use ccache, so a CI build of this tag can
  differ in the file-path strings that the cache rewrites.

  Equip from the Items menu (P6.34 phase 1, #339, patch 0188; plans B19 and
  B21): in Items, Cross on a weapon, armor or accessory opens the partner step,
  and Cross on a partner equips it through the game's own equip code. The list
  is rebuilt after the equip. When both hand slots are full, the first one is
  replaced and the result line says so. A partner who cannot equip the item is
  refused with a "cannot equip" line. Nothing is unequipped. The setting is
  items_equip_from_items in port_options.cfg (1 by default; 0 restores the old
  behaviour, where selecting equipment does nothing and is logged). These are
  defaults taken for now, pending the owner's confirmation, and the French,
  German, Italian and Spanish strings need a native review.

  Controls group and button layout (P9.8 phase 1, #338; plans L1 to L8): Options
  has a Controls group, between Cheats and Help, with a Button layout row.
  PlayStation (the default, unchanged): Cross = Switch B, Circle = Switch A,
  Triangle = Switch X, Square = Switch Y. Nintendo: A confirms, B cancels and X
  is Circle. The Help pages and the restart chord show the names of the layout
  in use. Options, Cheats, Quit, Help, battle Select, R3 fast-forward, the
  restart chord, the map view's cancel, the EU language select and the movie
  skip read the layout. The Switch debug chords, the region picker, the
  launcher and the crash notice keep the physical buttons. The setting is
  button_layout in port_options.cfg, per region; the press that changes it is
  ignored until released. The Options hint row does not follow the layout yet.
  No custom remap yet (phase 2).

  Battle Items and technique lists draw their selection bar again (#343, patch
  0189; plan D15). The bar is built by the battle code, but the drawing call
  was an empty stub since P3.3, so it never showed. Host code only; the log
  lines are `dw3-battle-cursor:`.

  Cheats group always listed (#344, plan A16): the Cheats group shows on the
  Options list with the master switch OFF. In game it stays greyed.

  Region at start (#344, plan A17, combined .nro only): the Gameplay group has
  a Region row (Ask, US or EU) for the start. It writes region_default in the
  top-level port_options.cfg. Holding ZL and ZR together at launch also asks for
  the picker; the ZL + ZR try is not verified on the console.

  Map view opens at 1.5x with the bars hidden (#344, plan B16): Circle shows or
  hides the bars, and the name box has its own flag. Logged as
  `dw3-map: open ... zoom=1.50 hud=off`.

  stage_sweep in the top-level port_options.cfg is logged once and ignored
  (plan G10). The key belongs in the region's own file.

  One map mod pack for US and EU (#345, plan B17): a pack can hold map files
  shared by both discs, and a region's own file wins where the discs differ
  (13 of the reveal sprites differ; tools/mods/extract_map_assets.py reports
  the count). The pack is still made locally from your own disc. No pack is in
  this release.

  Options background (#346, plan A18): at the title, the Options, Cheats and
  Help screens show the game's own menu background, the dimmed blue panel tile
  of the field status menu, behind the plaques. It is static, not scrolling.
  Logged once at boot as `dw3-options: menu background tile`. Not run on the
  console yet.

  Docs only, nothing in the game changed for them: the P6.34 menu-flow design
  (#335, docs/P6.34_MENU_FLOW.md); a docs CI job that runs the doc-link check on
  docs-only PRs (#337); the P6.36 goal, music at normal speed while turbo is on
  (#340; not built yet); and the preview.51 feedback design with OLED
  bookkeeping (#342, docs/P51_FEEDBACK.md).

  From preview.51:

  Technical help (P6.30, #333): Options has a Help group, the last row of the
  list (also from the field menu and the battle Options). It opens a page
  list with 10 pages in English: Setup, Controls, Port hotkeys, Turbo, Options
  explained, Saves, Widescreen, Map view and mods, Logs and bug reports, and
  FAQ. Up and Down move through the list and Cross (Switch B) opens a page.
  On a page, Up and Down scroll a line at a time, Left and Right a screenful,
  L and R change the page, and the cancel button (Switch X) goes back. Setup
  shows only the lines for this disc (US or EU). French, German, Italian and
  Spanish show the English pages for now; the translations come later. The
  game waits behind the help, as it does behind Options. Not yet checked on
  the OLED (plans H1 to H8).

  Build and test speed (#334, docs/BUILD_SPEED.md): ccache for the builds,
  incremental decomp patches and a faster replay selection. No change to the
  game in this preview.

  From preview.50:

  EU language select takes the confirm button (#330, patch 0187, plan E1.6c).
  On the EU language select, Cross (Switch B, PS1 Cross) picks the language as
  well as Start, which still works. The confirm button follows a future
  button remap. The log line `dw3-lang-select: language N chosen by Cross
  (confirm)` (or `by Start`) shows which one was used.

  Map view: the cursor and the balloon keep their size at zoom 1.0 at any
  zoom, and their positions follow the map. The revealed areas still scale
  with the map. One `dw3-map: sprites at fixed size` line is logged per open.
  DW3_MAP_SPRITES_FIXED=0 restores the old look, where the sprites scale with
  the map (#330, plan B16 section 11).

  Faster turbo in scenes (P6.22 pass 3, #331): a textured triangle fast path
  in PSY-Z draws the same pixels with less raster work (about 30% less in
  dialogue and about 10% in the field, measured on PC). The audio mixer runs
  on its own thread on core 1. The new `pace dbg:` line (game_fps,
  presented and skipped frames, sync, present, swap and other times in us,
  busy_pct, mix_us) is logged next to `frame dt us=`. Plans C1 to C3 and C10.
  To compare the mixer on the game thread's core, set DW3_NX_AUDIO_CORE=-2 in
  the nxlink environment (C10).

  From preview.49:

  Stage sweep (P5.5, #320): opt-in. Set stage_sweep=1 in the region's
  port_options.cfg (builds with the Cheats screen). The sweep loads all 237
  stages in turn, logs a `dw3-sweep:` line per stage and a summary, resumes
  after a crash and never writes saves. A run ends with `dw3-sweep: done`
  (plan G10).

  Item shop at 16:9 (#323, patch 0179, plan B18), with a widescreen log line
  for it.

  Items menu: selecting equipment does nothing. That is the original game,
  not a port regression, and it is now logged (plan B19). Equip from
  Status > Equipment.

  Map view closes with the cancel button (Switch X, PS1 Triangle). Y inside
  the view does nothing; it only opens the view (plan B20).

  Combined .nro finds a disc image placed directly in sdmc:/switch/dw3/
  (#325, fixes #322). The region is read from the executable in the image
  (plans G9.11 to G9.13).

  Map name box no longer jumps during right-stick zoom (#326): the box stays
  where it was when the cursor reached the location (plan B16).

  Options: Cheats is a scrolling sub-screen in the title menu's style
  (P9.9 slice 2, #327, plan A13). Opening any Options group no longer costs a
  50-80 ms rebuild; the group is drawn over a shared layer prepared ahead
  (plan A10).

  Walkthrough data layer (P6.35 phase 1, #328): the data and an English
  draft only. There is no screen yet, so nothing changes on the screen.

  One .nro for US and EU (P6.24, #314, #316): dw3-switch-both.nro carries
  both games. It looks for the data under sdmc:/switch/dw3/us/ and
  sdmc:/switch/dw3/eu/. With only one of them present it starts that game at
  once. With both, a picker shows for 3 seconds and starts the last choice
  (the default). Press Y to stop asking (start that region every time); hold
  Minus while you press A on the .nro in hbmenu to ask again; + quits. Every
  decision is a `dw3-region:` line in dw3.log. The separate US and EU .nro
  files are still published until the owner confirms the combined one on the
  OLED. The owner ran it on a Switch OLED with preview.48 (EU chosen): the
  picker with both regions, the button choice and "always" work. The other
  G9 rows (only one region present, timeout, Minus, disc image in the parent
  folder) are still open in the OLED test plan.

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

  In-game Options: the first frame after the open is 27 to 28 ms on the
  Switch with preview.47 (it was 93 ms in preview.45); the owner accepts the
  feel (plan B15b). The map HUD strip covers the top of the map at 1.00x.
  EU audio after many techs passed on the OLED with preview.51 (#188, plan D13:
  31 min, 0 underruns, 0 key-on failures).

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

  This release's files were built locally and attached by hand; the release
  workflow run for the tag was cancelled before it published (see New in this
  preview). No original game files, BIN/CUE/ISO, BIOS or extracted tree ship
  here.

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

  Still open on the OLED: 123 rows of docs/OLED_TEST_PLAN.md without a PASSED
  mark. The list is built from those rows only; the checklist below gives each
  one.
  A: A1, A2, A3, A4, A6, A8, A13, A18, A16, A17
  L: L1, L2, L3, L4, L5, L6, L7, L8
  B: B1, B2, B3, B4, B5, B6, B7, B8, B9, B10, B12, B13, B14, B15, B17, B19,
    B21, B20
  C: C1, C2, C3, C4, C10, C5, C6, C8, C9
  D: D1, D2, D3, D4, D5, D6, D7, D8, D9, D10, D11, D12, D14, D15
  E: E1.1, E1.2, E1.4, E1.5, E1.6, E2.0, E2.3, E2.4, E2.4b, E2.5, E2.6, E3.1,
    E3.2, E3.3, E3.4, E3.5, E3.6, E4.1, E4.2, E4.3, E4.4, E4.5, E4.6, E4.7,
    E4.8, E4.9, E5.1, E5.2, E5.3, E5.4, E5.6, E5.7, E5.8, E5.9
  F: F1, F2, F3, F4, F5, F6, F7
  G: G1, G2, G3, G4, G5, G6, G7.1, G7.2, G7.3, G7.4, G7.5, G7.6, G7.7, G9.1,
    G9.2, G9.6, G9.7, G9.8, G9.9, G9.10, G9.11, G9.12, G9.13

  Button names: Cross = Switch B, Circle = Switch A, Triangle = Switch X,
  Square = Switch Y (PlayStation layout, the default). Each row of
  docs/OLED_TEST_PLAN.md has the steps and the log lines to send.

  Session A, boot and title:
  - A1 Launch the NRO (first run: wait out the extraction bar)
  - A2 EU: pick a language at the select; repeat later for a second language
  - A3 Title: Down through the choices
  - A4 Cross on Quit game
  - A6 Set Encounter rate OFF and Text speed Instant, close from the HOME ...
  - A8 Relaunch, HOME, close the software
  - A13 Cheats sub-screen (P9.9 slice 2)
  - A18 Options background (P51 item 6)
  - A16 Cheats is on the Options list with the master OFF (P51 item 5)
  - A17 Region at start row (combined .nro only, P51 item 4)

  A15, button layout (P9.8):
  - L1 Fresh launch (no button_layout key), US then EU
  - L2 Options, Controls, Button layout: press B (it says Nintendo), X twice ...
  - L3 Nintendo, in the field and in a battle: open the field menu with Plus, ...
  - L4 Leave with the title's Quit game (or HOME), relaunch, US and EU (set ...
  - L5 Nintendo: the full-screen map (Plus, Map, Y, then B) and the restart ...
  - L6 Nintendo: Help (Options, Help), the Controls and Port hotkeys pages
  - L7 nxlink build, both layouts: Minus + L + R (debug HUD) and Minus + ZL + ...
  - L8 Developer: nxlink build with DW3_BUTTON_MAP=default in DW3_NX_TEST_ENV, ...

  Session B, field and 16:9 screens:
  - B1 Title menu, intro news caption bar, registration screen (New Game)
  - B2 Field menu (Plus): the dissolve, then Items, Sort, Techniques, Status, ...
  - B3 Map screen
  - B4 Save and load screens (save point, Continue)
  - B5 Area banner on entering an area, and a technique backdrop
  - B6 Card folders: card album and deck editor (all panels, card details, ...
  - B7 Card shop, Buy One Card
  - B8 Gym; Digimon Lab (menu, party, digivolve)
  - B9 Other shops and the Forest Inn
  - B10 Door iris: walk through several doors and area exits
  - B12 Encounter spiral (patch 0175)
  - B13 In a battle, a technique's magic circle
  - B14 nxlink build: a door and then a battle
  - B15 Options from the field (#270, P9.7)
  - B17 Map upscale mod (P8.5 first mod, P6.32)
  - B19 Equip from the Items menu (P6.34 phase 1, patch 0188; replaces the ...
  - B21 Equip from the Items menu, both hand slots full, key off, and the EU ...
  - B20 Map view closes with the cancel button (owner decision 2026-10-28)

  Session C, turbo speed:
  - C1 New Game opening at 4x
  - C2 Forest Inn, top and bottom, at 4x
  - C3 Read the defer dbg: queued= dropped= drawn= lines
  - C4 Only if C1 looks wrong: A/B with DW3_NX_DEFER=0 in DW3_NX_TEST_ENV ...
  - C10 A text-heavy scene at 4x (the opening's dialogue, a long NPC talk), ...
  - C5 Field-to-battle transition at 2x, 3x and 4x
  - C6 Watch the field picture at 4x
  - C8 Same turbo, battle: at the command menu move the cursor, open Tech and ...
  - C9 Turbo on: open the field menu Map, Items and a shop, then walk

  Session D, battles:
  - D1 First Kuwagamon fight with Kotemon, then later battles: partner ...
  - D2 Win a battle with R3 turbo on; watch the results screen
  - D3 Text speed Instant: read the post-battle report and the area banner ...
  - D4 Instant at a door: the area banner
  - D5 Encounter rate OFF: walk about 400 steps; then a story or boss fight
  - D6 Cheats (Y on Options, master ON): Battle now Ready, walk
  - D7 Cheats Instant win ON, enter a random battle
  - D8 Partner at Champion or higher (Digivolve in the field menu first): the ...
  - D9 Digivolve command in battle; a Nickname Shop name; a default-name Rookie
  - D10 Growlmon or WarGrowlmon with a Picking or Snapping Claw (needs the item)
  - D11 After a won battle: is there field music?
  - D12 Options in battle (P9.7 slice 2)
  - D14 Audio underruns (P4.3): Any build, handheld, nxlink or the log rotation
  - D15 Battle Items and technique lists show the selection bar (P51 item 1, ...

  Session E, saves:
  - E1.1 In the field press Plus: the last row (TITLE SCREEN; EU: RETOUR TITRE ...
  - E1.2 Cross on it
  - E1.4 Options: Restart hotkey ON
  - E1.5 Note: the chord contains Minus + L + R, so it can also toggle the ...
  - E1.6 After the Yes in E1.3, at the title (US) or the language select and ...
  - E2.0 No autosave.bin in the card folder: look at the title (US and each EU ...
  - E2.3 Continue
  - E2.4 Load Game
  - E2.4b New Game with the option on, reach progress 4, change area
  - E2.5 Option OFF with the file still there: Continue, play, change area
  - E2.6 Feel: does the write stall a frame at an area change?
  - E3.1 A US save in us/memcard/, none in eu/memcard/ (move the EU ones aside)
  - E3.2 Title, Continue, slot 1
  - E3.3 Reverse: an EU save, no US save, start US
  - E3.4 A save in both folders
  - E3.5 If you have one, an EU extra-chapter save going to US
  - E3.6 Start the same build again
  - E4.1 Options: Y with the master OFF
  - E4.2 Y opens the Cheats screen: title and rows in the game language (EU: all ...
  - E4.3 Fill bag Ready, then field menu Items
  - E4.4 Down from "Cheat saves" to page 2 ("CHEATS 2/2"); with master OFF the ...
  - E4.5 Money: arm +10,000 (Cross twice steps +10,000, +100,000), enter a ...
  - E4.6 Warp: to Digimon Lab, load a game
  - E4.7 Give item: (Left/Right, L/R by ten, a count), Ready, load
  - E4.8 Save after a cheat, relaunch
  - E4.9 Note: Warp can leave a story-flagged map looking unfinished
  - E5.1 Create a forwarder in sphaira with the profile selector option, pick ...
  - E5.2 First launch of A with old saves in <region>/memcard/
  - E5.3 As A: save in slot 2 (or play to an auto-save), quit
  - E5.4 Back as A: Continue / Load
  - E5.6 Start from hbmenu in applet mode (hold nothing, launch from the Album) ...
  - E5.7 Hbmenu in title-override mode (hold R on a game), several users
  - E5.8 A console with one profile
  - E5.9 Cause a crash if safe (nxlink force-crash build) or read an old report

  Session F, the 2 hour memory run:
  - F1 Play 2 h of wall time without restarting the NRO, at 1x (or note how ...
  - F2 Every 15 min note the HUD line or take it from the log: heap=/hw=, ...
  - F3 Pass test
  - F4 Report
  - F5 Reference for F3
  - F6 [GATE]: New Game to the first boss, US and EU (in this run or a ...
  - F7 If the boot shows "The last run crashed."

  Session G, anything else:
  - G1 P5.4 PR 2 forced crash: Two special --force-crash nxlink builds (US ...
  - G2 P10.2: HOME during a save write
  - G3 P10.2: HOME (not the power button) pause mid-game, nxlink run
  - G4 P4.3: nxlink session of 10 min or more: title 1 min, opening movie 2 ...
  - G5 P7.7 HUD name and Claw steal
  - G6 Rumble (optional, still unverified on hardware)
  - G7.1 Both us/ and eu/ present
  - G7.2 Y until "always" is ON, A
  - G7.3 One region only (move eu/ away, then us/)
  - G7.4 No region
  - G7.5 Remove dw3-eu.nro, choose EU
  - G7.6 Start dw3.nro as a title takeover (hold R on a game)
  - G7.7 Saves and options of both regions after going through the launcher; ...
  - G9.1 Only US data: move eu/ away
  - G9.2 Only EU data: put eu/ back, move us/ away
  - G9.6 Minus asks again: with "always" stored, hold Minus while pressing A on ...
  - G9.7 Quit from the picker and no data
  - G9.8 Both regions play and save: from the combined .nro, start US, load or ...
  - G9.9 Crash report names the region
  - G9.10 Handheld Joy-Cons and a Pro Controller
  - G9.11 Disc in the parent folder, one region
  - G9.12 Both discs in the parent folder
  - G9.13 One disc in the parent, the other region already extracted

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
