cmake_minimum_required(VERSION 3.21)
project(dw3-pc C)

set(CMAKE_C_STANDARD 11)
set(PSYZ_RENDERER "sdl3_gpu" CACHE STRING "PSY-Z renderer")
# Propagate before add_subdirectory(port). Default us; EU: -DDW3_VERSION=eu
# P6.24: -DDW3_VERSION=both links both game copies into one dw3-pc, region
# chosen at run time (--region / DW3_REGION, else the runtime that is present).
set(DW3_VERSION "us" CACHE STRING "Game disc version: us, eu or both")
set_property(CACHE DW3_VERSION PROPERTY STRINGS us eu both)
string(TOLOWER "${DW3_VERSION}" _dw3_pc_ver)
if(_dw3_pc_ver STREQUAL "both")
  set(_dw3_both ON)
  set(_dw3_sfx _us _eu)
else()
  set(_dw3_both OFF)
  set(_dw3_sfx "")
endif()

add_subdirectory(${CMAKE_SOURCE_DIR}/../../vendor/psyz/psyz ${CMAKE_BINARY_DIR}/psyz)
# P6.20 Cheats sub-screen of the Options screen (docs/PORT_OPTIONS.md
# "Cheats"). ON (the default, release builds too): the Cheats screen, its
# hooks (patch 0077) and the save mark are compiled in, every cheat off until
# its master switch is turned on. OFF (tools/build_switch_game.sh --no-cheats,
# or -DDW3_CHEATS=OFF): none of it is compiled and the game is the build
# without P6.20. Defined for every target after psyz (the game, port, host).
option(DW3_CHEATS "P6.20 Cheats screen in the Options screen (test aid)" ON)
if(DW3_CHEATS)
  add_compile_definitions(DW3_CHEATS=1)
  message(STATUS "P6.20 Cheats screen compiled in (DW3_CHEATS)")
endif()
if(_dw3_both)
  include(${CMAKE_SOURCE_DIR}/../../port/dual_region.cmake)
  dw3_add_both_game(${CMAKE_SOURCE_DIR}/../../port)
else()
  add_subdirectory(${CMAKE_SOURCE_DIR}/../../port ${CMAKE_BINARY_DIR}/port)
endif()

# PC-only: enable Psyz_VideoAlloc* TTB dump path in port/options (Switch
# game NRO does not link those PSY-Z capture symbols). In a `both` image every
# game target exists twice (suffix _us / _eu).
foreach(_s IN LISTS _dw3_sfx ITEMS "")
  if(_dw3_both AND _s STREQUAL "")
    continue()
  endif()
  target_compile_definitions(dw3_port_options${_s} PRIVATE DW3_PC_HOST=1)
  target_compile_definitions(dw3_engine${_s} PRIVATE DW3_PC_HOST=1)
  target_compile_definitions(dw3_title${_s} PRIVATE DW3_PC_HOST=1)
endforeach()

if(PSYZ_RENDERER STREQUAL "sdl3_gpu")
  foreach(_s IN LISTS _dw3_sfx ITEMS "")
    if(_dw3_both AND _s STREQUAL "")
      continue()
    endif()
    target_compile_definitions(dw3_title${_s} PRIVATE
      DW3_PC_SDL3_GPU=1 DW3_TITLE_NATIVE_OVERLAY=1)
    target_compile_definitions(dw3_port_options${_s} PRIVATE
      DW3_PC_SDL3_GPU=1 DW3_TITLE_NATIVE_OVERLAY=1)
  endforeach()
  set(_DW3_PC_TITLE_OVERLAY_SOURCES
    src/title_menu_overlay.c
    src/map_view_overlay.c
    src/title_overlay_art.c
    src/title_options_cache.c
    src/title_logo_resample.c
  )
  foreach(_s IN LISTS _dw3_sfx ITEMS "")
    if(_dw3_both AND _s STREQUAL "")
      continue()
    endif()
    target_include_directories(dw3_port_options${_s} PRIVATE
      ${CMAKE_CURRENT_LIST_DIR}/src
    )
  endforeach()
elseif(NOT PSYZ_RENDERER STREQUAL "sdl3_gl")
  message(FATAL_ERROR "Unsupported PSYZ_RENDERER: ${PSYZ_RENDERER}")
endif()

add_executable(dw3-pc
  src/main.c
  src/mem_sampler.c
  src/psyz_host_overrides.c
  src/psyz_dma_present.c
  # Direct TU so PadGetState / PadInitDirect win over PSY-Z stubs in libpsyz.a
  ${CMAKE_SOURCE_DIR}/../../port/psyz_missing/psyz_pad_host.c
  ${CMAKE_SOURCE_DIR}/../../port/replay/dw3_replay.c
)
if(PSYZ_RENDERER STREQUAL "sdl3_gpu")
  target_compile_definitions(dw3-pc PRIVATE DW3_PC_SDL3_GPU=1)
  target_sources(dw3-pc PRIVATE ${_DW3_PC_TITLE_OVERLAY_SOURCES})
endif()
target_include_directories(dw3-pc PRIVATE
  ${CMAKE_SOURCE_DIR}/../../port/map_view
  ${CMAKE_SOURCE_DIR}/../../port/cdfs
  ${CMAKE_SOURCE_DIR}/../../port/memcard
  ${CMAKE_SOURCE_DIR}/../../port/movies
  ${CMAKE_SOURCE_DIR}/../../port/overlays
  ${CMAKE_SOURCE_DIR}/../../port/include
  ${CMAKE_SOURCE_DIR}/../../vendor/psyz/psyz/include
)
if(PSYZ_RENDERER STREQUAL "sdl3_gpu")
  target_include_directories(dw3-pc PRIVATE
    ${CMAKE_SOURCE_DIR}/../../port/options
  )
endif()
target_include_directories(dw3-pc PRIVATE ${CMAKE_SOURCE_DIR}/../../port/region)
if(_dw3_both)
  # P6.24: no region at compile time; dw3_region() / dw3_game() decide.
  target_compile_definitions(dw3-pc PRIVATE DW3_BOTH=1)
elseif(_dw3_pc_ver STREQUAL "eu")
  target_compile_definitions(dw3-pc PRIVATE VERSION_EU=1 DW3_DEFAULT_RUNTIME="runtime/eu")
elseif(_dw3_pc_ver STREQUAL "us")
  target_compile_definitions(dw3-pc PRIVATE VERSION_US=1 DW3_DEFAULT_RUNTIME="runtime/us")
else()
  message(FATAL_ERROR "DW3_VERSION must be us, eu or both (got '${DW3_VERSION}')")
endif()
if(NOT _dw3_both)
  # dw3_region_name() / dw3_region_parse() for the log lines (options_cfg.c
  # comes from the game archive; a `both` image has both in dw3_region_rt).
  target_sources(dw3-pc PRIVATE ${CMAKE_SOURCE_DIR}/../../port/region/dw3_region.c)
endif()
# Host overrides must appear before psyz so SsSetTableSize / pad APIs win.
# whole-archive field/battle/menu/cardgame objs (named .data/.bss for overlay restore).
# start-group: engine↔field cross-archive undefineds (FLAGS_00, UI helpers).
if(_dw3_both)
  # The two game copies are plain objects (all their other symbols are local, so
  # no order or group is needed between them); the host archive and PSY-Z
  # follow the executable's own objects exactly as in a plain build.
  foreach(_r us eu)
    set_source_files_properties(${DW3_GAME_COPY_O_${_r}} PROPERTIES
      EXTERNAL_OBJECT TRUE GENERATED TRUE)
    target_sources(dw3-pc PRIVATE ${DW3_GAME_COPY_O_${_r}})
  endforeach()
  add_dependencies(dw3-pc ${DW3_GAME_COPY_TARGETS})
  target_link_libraries(dw3-pc PRIVATE
    "-Wl,--start-group"
    dw3_host_objs
    "-Wl,--end-group"
    psyz
  )
else()
target_link_libraries(dw3-pc PRIVATE
  "-Wl,--start-group"
  "-Wl,--whole-archive" dw3_p32_field dw3_p33_battle dw3_p34_menus dw3_p35_cardgame
    dw3_p38_stagslct dw3_p62_cnty_sel
  "-Wl,--no-whole-archive"
  dw3_engine_objs
  "-Wl,--end-group"
  psyz
)
endif()
target_link_options(dw3-pc PRIVATE
  "-Wl,--wrap=_card_read"
  "-Wl,--wrap=_card_write"
  # Override PSY-Z libetc stubs (same .o as PadRead pulls them in).
  "-Wl,--wrap=PadGetState"
  "-Wl,--wrap=PadStartCom"
  "-Wl,--wrap=PadInitMtap"
  "-Wl,--wrap=PadSetAct"
  "-Wl,--wrap=PadSetActAlign"
  # P3.7 present: MDEC-out DMA IRQ + RGB24 LoadImage for STR movies
  "-Wl,--wrap=DMACallback"
  "-Wl,--wrap=DecDCTReset"
  "-Wl,--wrap=DecDCTout"
  "-Wl,--wrap=LoadImage"
  "-Wl,--wrap=PutDispEnv"
)
# A `both` image needs none of the --undefined lines: overlay_sections.c of
# each game copy refers to its own __start_dw3data_<region>_* symbols.
if(NOT _dw3_both)
target_link_options(dw3-pc PRIVATE
  # Keep P3.2–P3.5 named overlay .data/.bss orphan sections (overlay_sections.c).
  "-Wl,--undefined=__start_dw3data_fieldstg"
  "-Wl,--undefined=__start_dw3data_stplnmet"
  "-Wl,--undefined=__start_dw3data_wstag260"
  "-Wl,--undefined=__start_dw3data_wstag780"
  "-Wl,--undefined=__start_dw3data_wstag785"
  "-Wl,--undefined=__start_dw3data_wstag790"
  "-Wl,--undefined=__start_dw3data_fightstg"
  "-Wl,--undefined=__start_dw3data_wfightmn"
  "-Wl,--undefined=__start_dw3data_stfgtrep"
  "-Wl,--undefined=__start_dw3data_ststatus"
  "-Wl,--undefined=__start_dw3data_stitshop"
  "-Wl,--undefined=__start_dw3data_stgtrain"
  "-Wl,--undefined=__start_dw3data_stgdglab"
  "-Wl,--undefined=__start_dw3data_stgmcard"
  "-Wl,--undefined=__start_dw3data_stdgname"
  "-Wl,--undefined=__start_dw3data_cardgame"
  "-Wl,--undefined=__start_dw3data_stcrddek"
  "-Wl,--undefined=__start_dw3data_stcrdshp"
  "-Wl,--undefined=__start_dw3data_stcrdabm"
  "-Wl,--undefined=__start_dw3data_stagslct"
  "-Wl,--undefined=__start_dw3data_cnty_sel"
)
endif()
# P6.22: field CPU profiler (docs/P6.22_FIELD_CPU.md). Off by default. Runs a
# shadow copy of the CPU rasterizer the Switch draws with, next to the PC
# renderer, and times it per game frame; never in release or replay builds.
option(DW3_PC_FIELD_PROF "P6.22 field CPU profiler (shadow CPU rasterizer)" OFF)
if(DW3_PC_FIELD_PROF)
  target_sources(dw3-pc PRIVATE src/field_prof.c)
  target_include_directories(dw3-pc PRIVATE
    ${CMAKE_SOURCE_DIR}/../../vendor/psyz/psyz/src
    ${CMAKE_SOURCE_DIR}/../../vendor/psyz/psyz/src/platform
  )
  foreach(_dw3_prof_sym
      Draw_Reset Draw_SetTexpageMode Draw_SetTextureWindow Draw_SetAreaStart
      Draw_SetAreaEnd Draw_SetOffset Draw_SetMask Draw_ClearImage
      Draw_LoadImage Draw_StoreImage Draw_MoveImage Draw_PushPrim
      Draw_DisplayArea Draw_SetDisplayMode Psyz_VideoSetGutter
      Psyz_VideoVSync Psyz_SpuPullSamples)
    target_link_options(dw3-pc PRIVATE "-Wl,--wrap=${_dw3_prof_sym}")
  endforeach()
  target_link_libraries(dw3-pc PRIVATE rt)
endif()

target_include_directories(psyz PRIVATE ${CMAKE_SOURCE_DIR}/../../vendor/psyz/psyz/include)
target_compile_definitions(psyz PRIVATE __psyz)
if(_dw3_pc_ver STREQUAL "eu")
  psyz_title(dw3-pc "dw3-pc EU")
elseif(_dw3_both)
  psyz_title(dw3-pc "dw3-pc")
else()
  psyz_title(dw3-pc "dw3-pc Phase 2")
endif()
