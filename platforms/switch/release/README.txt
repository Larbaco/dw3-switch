Digimon World 3 / 2003, Switch homebrew preview

Release notes for v0.6.5-preview.51. The release tag and boot version
identify the actual source revision.

Player guide (setup, controls, Options, saves, crash reports):
https://github.com/Larbaco/dw3-switch/blob/main/docs/PLAYER_GUIDE.md

New in this preview

  The owner's OLED checks are recorded in docs/OLED_TEST_PLAN.md (rows marked
  PASSED). Items that are not there yet are checked on PC and in the Ryubing
  emulator only, and are marked unverified.

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

  Still open on the OLED (the rows of docs/OLED_TEST_PLAN.md without a PASSED
  mark): the help, plans H1 to H8; the EU language select confirm button
  (E1.6c); the map sprites at fixed size and the name box staying put during
  zoom (B16, section 11 and #326); the Cheats sub-screen (A13) and the group
  open after the prewarm (A10); the item shop at 16:9 (B18); equipment in the
  Items menu (B19); the map view close button (B20); the map mod (B17); the
  stage sweep (G10); the audio check after many techs on the EU build (#188,
  D13); save writes with HOME (G2); the faster turbo in scenes (C1 to C3, C10);
  and the combined .nro with one region present (G9.1, G9.2), the timeout
  (G9.3), the start after a choice without the picker (G9.4, G9.5), Minus
  (G9.6) and disc images in the parent folder (G9.11 to G9.13).

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
  EU audio after many techs is still open (#188, plan D13).

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

  Still open for this preview. Rows the owner has passed (PASSED in
  docs/OLED_TEST_PLAN.md) are not repeated. Send the whole dw3*.log set with
  every test, and say handheld or docked and the EU language used.

  Technical help (P6.30, plans H1 to H8):
  - H1 to H3: from the title, the field menu (OPTIONS) and battle Options,
    open Help; read Controls and FAQ with Down / Up, hold Down, Left / Right
    and L / R; X backs out. Check for stutter when opening and scrolling.
    Send the `dw3-help:` lines and a photo; say if the scroll felt slow.
  - H4 to H7: the Controls, Port hotkeys, Setup (US then EU), Saves, Map view
    and mods and FAQ pages; open every page once. No `[?...]` text, no
    cut-off line. Send the `dw3-pages:` lines.
  - H8: EU, each of the five languages: the screen words in that language.
    The pages are English for FR, DE, IT and ES (drafts; translations to come).

  Menus and map (plans E1.6c, A13, A10, B16 to B21):
  - E1.6c: EU language select. Confirm with the confirm button (Switch B) on
    one boot, and Plus (Start) on another. Log: `dw3-lang-select: language N
    chosen by Cross (confirm)` and `... by Start`.
  - A13: Cheats sub-screen, all ten rows, master off and on, Fill bag, Money,
    Warp, Item id. Values and the master must not stutter (`dirty` in the log,
    not `rebuild`, after the first open).
  - A10: the first open of each Options group must not stutter (`group-over-base`,
    compose under 10000 us). Passed for the old behaviour on preview.47; the
    slice 3 re-check is pending.
  - B16: full-screen map view (Y). Balloon and cursor are confirmed. Still
    open: the zoom steps and the right stick, ZL / ZR through the scalers
    (Lanczos2 and FSR 1 build without freezing), and R3 in the view.
  - B17: map upscale mod. With a local pack in the mods folder the map is the
    upscaled picture; with the folder renamed or `map_mod=0`, the original.
    Log: `dw3-map: mod: pack ... loaded` or `original map`.
  - B18: item shop at 16:9 (patch 0179): the BUY and SELL panels reach the
    16:9 edges, nothing is cut at the old 4:3 edge. Photo of the shop and the
    BUY list.
  - B19: equip from the Items menu (patch 0188): Cross on a weapon, pick a
    partner, check the result line, then Status > Equipment. Refused partner
    and Triangle. Log: `equip-from-items open`, `done partner=P slot=N`,
    `refused ... reason=partner-mask`, `cancelled at=partner`. Not confirmed.
  - B20: the Map view closes with X (cancel button); Y inside the view does
    nothing. Log: `dw3-map: close requested by the cancel button`. Not confirmed.
  - B21: two hand slots full (the first weapon goes back to the bag, log
    `rule=hand-replace`); `items_equip_from_items=0` makes Cross do nothing;
    EU FR, DE, IT and ES equip lines are readable (drafts). Not confirmed.

  Combined .nro (plan G9.x, P6.24, #322, #325):
  - G9.1 and G9.2: only US data, then only EU data; no picker, the game
    starts at once.
  - G9.3 to G9.5: the picker and "Always start this one" passed by log on
    preview.48. Still open: the 3 s countdown with no key pressed, and the
    default from the last choice on a later start (region_default cleared).
  - G9.6, G9.7: Minus asks again; + quits from the picker.
  - G9.8, G9.9, G9.10: both regions play and save; the crash report's region
    line; Joy-Cons and a Pro Controller give the same picker behaviour.
  - G9.11 to G9.13: disc images placed directly in sdmc:/switch/dw3/ (one
    disc, then both). Log: `data found ... disc image`.

  Stability (plans D13, G2, G10):
  - D13 (EU): 10 or more techs in a row across battles, then a battle, the
    field and the menus. Music and sounds keep playing. Log: `dw3-audio: health`
    and any `underruns`.
  - G2: HOME during a save write, eight times, nxlink build, US then EU.
    Log: `dw3-save: write ... committed` or `interrupted`.
  - G10: stage sweep (`stage_sweep=1` in port_options.cfg), US then EU. Leave
    the pad alone for 5 to 10 minutes. Log: `dw3-sweep: done X/Y ok`.

  Turbo (plans C1 to C3, C10):
  - C1 to C3: New Game at 4x, then the Forest Inn top and bottom, on an nxlink
    build. Send the `pace dbg:` and `defer dbg:` lines and the FPS you saw.
    C1 is partly reported (turbo works in the intro; no frame lines yet).
  - C10: a text-heavy scene at 4x, then the same with `DW3_NX_AUDIO_CORE=-2`
    (A/B). Log: `nx_audio: mixer thread on core 1`; no crackle, no growing
    underruns.

  Every other open row, and the items still open from earlier previews, is in
  the ordered OLED test plan. Follow it:
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
