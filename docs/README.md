# Docs index

One line per doc. Start with the [player guide](PLAYER_GUIDE.md) if you want to
play. The [ROADMAP](ROADMAP.md) is the complete map of items and their status.
Files are named by roadmap item id (`P6.4_...`) or by topic.

## Player

- [PLAYER_GUIDE](PLAYER_GUIDE.md): setup, controls, menus, saves, crash reports.
- [HOTKEYS](HOTKEYS.md): controls, port hotkeys and diagnostic chords.
- [PORT_OPTIONS](PORT_OPTIONS.md): the Options screen, the Cheats screen, Quit and Return to title.
- [DISC_SETUP](DISC_SETUP.md): from your disc image to the extracted tree and file map.
- [RUNTIME_LAYOUT](RUNTIME_LAYOUT.md): the runtime folder on PC and on the SD card.
- [STATIC_SD_LOAD](STATIC_SD_LOAD.md): static files on the SD card.
- [CROSS_REGION_SAVES](CROSS_REGION_SAVES.md): US and EU saves, and the import as built.
- [REPORTING_ISSUES](REPORTING_ISSUES.md): how testers report bugs.
- [RELEASES](RELEASES.md): preview index, builds to avoid.
- [RELEASE_PREVIEW20](RELEASE_PREVIEW20.md), [21](RELEASE_PREVIEW21.md), [22](RELEASE_PREVIEW22.md), [25](RELEASE_PREVIEW25.md), [26](RELEASE_PREVIEW26.md): release checkpoints of those previews.
- [SWITCH_FEATURES](SWITCH_FEATURES.md): native Switch features.
- [NXLINK_DEBUG](NXLINK_DEBUG.md): nxlink debug logging.
- [SWITCH_OLED_NETWORK](SWITCH_OLED_NETWORK.md): Switch OLED network access.
- [DEBUG_OVERLAY](DEBUG_OVERLAY.md): the on-screen debug overlay.

## Contributor and porting

- [CONTRIBUTING](../CONTRIBUTING.md): short guide for contributors and AI agents (build, patches, replay tests, PRs).
- [ARCHITECTURE](ARCHITECTURE.md): how the decomp, PSY-Z and the hosts fit together.
- [ROADMAP](ROADMAP.md): phases, gates and what is next.
- [OLED_TEST_PLAN](OLED_TEST_PLAN.md): one ordered owner checklist for the OLED, preview.35.
- [PATCHES](PATCHES.md): the decomp patch series.
- [PSYZ](PSYZ.md): the PSY-Z shim pin and renderer notes.
- [SWITCH_BACKEND](SWITCH_BACKEND.md): the Switch PSY-Z backend.
- [CI_SUBMODULES](CI_SUBMODULES.md): CI submodule initialization.
- [CDFS_NOTES](CDFS_NOTES.md): CD and file serving notes.
- [SINGLE_BUILD](SINGLE_BUILD.md): one build for US and EU (P6.24).
- [PER_USER_SAVES](PER_USER_SAVES.md): per-user saves design (P6.26).
- [MENU_COMPOSITION_PLAN](MENU_COMPOSITION_PLAN.md): native menu composition plan.
- [TITLE_MENU_DESIGN](TITLE_MENU_DESIGN.md): the title menu design direction.
- [PC_TITLE_OVERLAY_ART](PC_TITLE_OVERLAY_ART.md): native-resolution PC title overlay art.
- [PC_TITLE_LOGO_RESAMPLING](PC_TITLE_LOGO_RESAMPLING.md): PC original-logo resampling.
- [PC_TITLE_WIDESCREEN](PC_TITLE_WIDESCREEN.md): PC title widescreen background.
- [BRANDING](BRANDING.md): project artwork.
- [GUIDE_HTML](GUIDE_HTML.md): local GameFAQs guide HTML conversion.

## Item docs (P*.md)

- [P2.8_TITLE_BOOT](P2.8_TITLE_BOOT.md): boot to title on PC.
- [P3.1_64BIT_AUDIT](P3.1_64BIT_AUDIT.md): 64-bit audit of overlays and stages.
- [P3.2_FIELD](P3.2_FIELD.md): field and first stage (PC).
- [P3.3_BATTLE](P3.3_BATTLE.md): battle (PC).
- [P3.4_MENUS](P3.4_MENUS.md): menus and shops (PC).
- [P3.5_CARDGAME](P3.5_CARDGAME.md): card game (PC).
- [P3.6_AUDIO](P3.6_AUDIO.md): audio (libsnd and SEP).
- [P3.7_MOVIES](P3.7_MOVIES.md): movies (title STR, libpress).
- [P3.7_MOVIE_PACING_DIAGNOSTICS](P3.7_MOVIE_PACING_DIAGNOSTICS.md): movie pacing diagnostics.
- [P3.8_STAGES](P3.8_STAGES.md): stage sweep (PC, US).
- [P3.9_REFERENCE](P3.9_REFERENCE.md): reference comparison of title, field and battle.
- [P3_EU_BUILD](P3_EU_BUILD.md): the EU build and Switch own-disc extraction.
- [P3_GATE](P3_GATE.md): phase 3 gate, start to first boss.
- [P3_GPU_GLITCHES](P3_GPU_GLITCHES.md): PC GPU and present glitch inventory.
- [P4.2_VIDEO](P4.2_VIDEO.md): EGL present path.
- [P4.3_AUDIO](P4.3_AUDIO.md): audren from the PSY-Z SPU.
- [P4.3_AUDIO_HW](P4.3_AUDIO_HW.md): audio underrun measurement and hardware-listen plan.
- [P4.4_INPUT_STATUS](P4.4_INPUT_STATUS.md): hid to PS1 pad, mode table, rumble status.
- [P4.5_SD_SUITE](P4.5_SD_SUITE.md): SD suite plan.
- [P4.5_STATUS](P4.5_STATUS.md): filesystem, log and time status.
- [P4.6_MODULES_STATUS](P4.6_MODULES_STATUS.md): modules on Switch.
- [P4.7_BOOT](P4.7_BOOT.md): booting the game on Switch.
- [P5.1_MEMORY](P5.1_MEMORY.md): memory budget instrumentation.
- [P5.2_BATTLE_GPU](P5.2_BATTLE_GPU.md): battle mesh on GLES.
- [P5.2_BATTLE_GPU_IDEAS](P5.2_BATTLE_GPU_IDEAS.md): battle GPU ideas beyond approach A.
- [P5.3_FLAT_SAVE](P5.3_FLAT_SAVE.md): the flat save host.
- [P5.3_HOME_QUIT](P5.3_HOME_QUIT.md): HOME exit, focus and sleep.
- [P5.4_CRASH](P5.4_CRASH.md): crash report to the SD card.
- [P5.4_ERROR_REPORT](P5.4_ERROR_REPORT.md): error reporting research.
- [P6.1_WIDESCREEN](P6.1_WIDESCREEN.md): widescreen in field and battle.
- [P6.1_WIDESCREEN_MENU_AUDIT](P6.1_WIDESCREEN_MENU_AUDIT.md): widescreen menu and transition audit.
- [P6.4_FAST_FORWARD](P6.4_FAST_FORWARD.md): fast-forward (turbo).
- [P6.5_NRO_FORMAT](P6.5_NRO_FORMAT.md): NRO format notes.
- [P6.5_PACKAGING](P6.5_PACKAGING.md): release packaging.
- [P6.9_RESTART](P6.9_RESTART.md): in-process game restart.
- [P6.15_REPLAY_TESTS](P6.15_REPLAY_TESTS.md): regression replay tests.
- [P6.17_VOLUME](P6.17_VOLUME.md): music and SFX volume.
- [P6.18_PACING](P6.18_PACING.md): encounter rate and text speed.
- [P6.22_FIELD_CPU](P6.22_FIELD_CPU.md): where a field frame's CPU time goes.
- [P6.33_ITEMS_MENU](P6.33_ITEMS_MENU.md): Items menu redesign proposal.
- [P6.35_WALKTHROUGH](P6.35_WALKTHROUGH.md): in-game walkthrough help: step mapping, data format, spoiler UX, phased plan.
- [P6.32_MAP_ZOOM](P6.32_MAP_ZOOM.md): map zoom and full-screen map proposal.
- [P6.31_AUTOSAVE](P6.31_AUTOSAVE.md): auto-save, off by default: design, defaults, checklist.
- [P7.7_BUG_FIXES](P7.7_BUG_FIXES.md): bug and text fixes.
- [P7.9_BATTLE_MENU_MEMORY](P7.9_BATTLE_MENU_MEMORY.md): battle menu memory.
- [P9.2_MENU](P9.2_MENU.md): menu entry and shell.
- [P10.4_RUMBLE](P10.4_RUMBLE.md): rumble design.

## Research

- [SURVEY](SURVEY.md): port-oriented repository survey.
- [COMMUNITY_QOL](COMMUNITY_QOL.md): community quality-of-life patches and companion projects.
- [TEXTURE_PACKS](TEXTURE_PACKS.md): texture pack compatibility.
- [MOD_MAP_UPSCALE](MOD_MAP_UPSCALE.md): first mod, an upscaled map pack: design, loader as built, how to make and install the pack.
- [NSP_PACKAGING](NSP_PACKAGING.md): NSP packaging research (P10.7).
- [MENU_COMPOSITION_INVESTIGATION](MENU_COMPOSITION_INVESTIGATION.md): native menu composition investigation.
- [BALLOON_NULL_ACTOR](BALLOON_NULL_ACTOR.md): NULL reads the PS1 tolerated.
- [FIELD_TILE_SPRITES](FIELD_TILE_SPRITES.md): field tiles with more than 5 sprites.
- [NX_KERNEL_SYNC_OBJECTS](NX_KERNEL_SYNC_OBJECTS.md): the `KernelInit` sync objects error.
- [PREVIEW26_CRASH_RECORDS](PREVIEW26_CRASH_RECORDS.md): preview.26 OLED crash records.
