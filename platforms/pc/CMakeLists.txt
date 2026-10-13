cmake_minimum_required(VERSION 3.21)
project(dw3-pc C)

set(CMAKE_C_STANDARD 11)
set(PSYZ_RENDERER "sdl3_gpu" CACHE STRING "PSY-Z renderer")
# Propagate before add_subdirectory(port). Default us; EU: -DDW3_VERSION=eu
set(DW3_VERSION "us" CACHE STRING "Game disc version: us or eu")
set_property(CACHE DW3_VERSION PROPERTY STRINGS us eu)

add_subdirectory(${CMAKE_SOURCE_DIR}/../../vendor/psyz/psyz ${CMAKE_BINARY_DIR}/psyz)
add_subdirectory(${CMAKE_SOURCE_DIR}/../../port ${CMAKE_BINARY_DIR}/port)

# PC-only: enable Psyz_VideoAlloc* TTB dump path in port/options (Switch
# game NRO does not link those PSY-Z capture symbols).
target_compile_definitions(dw3_port_options PRIVATE DW3_PC_HOST=1)
target_compile_definitions(dw3_engine PRIVATE DW3_PC_HOST=1)
target_compile_definitions(dw3_title PRIVATE DW3_PC_HOST=1)

if(PSYZ_RENDERER STREQUAL "sdl3_gpu")
  target_compile_definitions(dw3_title PRIVATE DW3_PC_SDL3_GPU=1)
  set(_DW3_PC_TITLE_OVERLAY_SOURCES
    src/title_menu_overlay.c
    src/title_overlay_art.c
  )
elseif(NOT PSYZ_RENDERER STREQUAL "sdl3_gl")
  message(FATAL_ERROR "Unsupported PSYZ_RENDERER: ${PSYZ_RENDERER}")
endif()

add_executable(dw3-pc
  src/main.c
  src/psyz_host_overrides.c
  src/psyz_dma_present.c
  # Direct TU so PadGetState / PadInitDirect win over PSY-Z stubs in libpsyz.a
  ${CMAKE_SOURCE_DIR}/../../port/psyz_missing/psyz_pad_host.c
)
if(PSYZ_RENDERER STREQUAL "sdl3_gpu")
  target_compile_definitions(dw3-pc PRIVATE DW3_PC_SDL3_GPU=1)
  target_sources(dw3-pc PRIVATE ${_DW3_PC_TITLE_OVERLAY_SOURCES})
endif()
target_include_directories(dw3-pc PRIVATE
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
string(TOLOWER "${DW3_VERSION}" _dw3_pc_ver)
if(_dw3_pc_ver STREQUAL "eu")
  target_compile_definitions(dw3-pc PRIVATE VERSION_EU=1 DW3_DEFAULT_RUNTIME="runtime/eu")
elseif(_dw3_pc_ver STREQUAL "us")
  target_compile_definitions(dw3-pc PRIVATE VERSION_US=1 DW3_DEFAULT_RUNTIME="runtime/us")
else()
  message(FATAL_ERROR "DW3_VERSION must be us or eu (got '${DW3_VERSION}')")
endif()
# Host overrides must appear before psyz so SsSetTableSize / pad APIs win.
# whole-archive field/battle/menu/cardgame objs (named .data/.bss for overlay restore).
# start-group: engine↔field cross-archive undefineds (FLAGS_00, UI helpers).
target_link_libraries(dw3-pc PRIVATE
  "-Wl,--start-group"
  "-Wl,--whole-archive" dw3_p32_field dw3_p33_battle dw3_p34_menus dw3_p35_cardgame
    dw3_p38_stagslct
  "-Wl,--no-whole-archive"
  dw3_engine_objs
  "-Wl,--end-group"
  psyz
)
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
)
target_include_directories(psyz PRIVATE ${CMAKE_SOURCE_DIR}/../../vendor/psyz/psyz/include)
target_compile_definitions(psyz PRIVATE __psyz)
if(_dw3_pc_ver STREQUAL "eu")
  psyz_title(dw3-pc "dw3-pc EU")
else()
  psyz_title(dw3-pc "dw3-pc Phase 2")
endif()
