# Nintendo Switch host (libnx) — P4.7.
#
# Links the Phase 3 game image (decomp engine + overlays + stages + port/ glue)
# against PSY-Z built for aarch64. The platform layer and the renderer are
# supplied by psyz_backend/ — P4.1 decided the backend lives in this tree rather
# than in the PSY-Z submodule (see docs/SWITCH_BACKEND.md). The PC host
# (platforms/pc) remains the bring-up target; this one has to produce the .nro.
#
# Cross-compile with devkitA64's toolchain file. tools/dk.sh runs exactly this
# inside the devkitPro image, so no local devkitPro install is needed:
#
#   cmake -S platforms/switch -B build/switch -G Ninja \
#     -DCMAKE_TOOLCHAIN_FILE=$DEVKITPRO/cmake/Switch.cmake
#   cmake --build build/switch
#   # → build/switch/dw3-switch.nro
#
# Architecture, -D__SWITCH__, switch.specs and -lnx/-lm all come from the
# toolchain's Platform/NintendoSwitch.cmake; only the portlibs are listed here.
# See platforms/switch/README.md.

cmake_minimum_required(VERSION 3.21)
# CXX is needed for the link, not for our sources: libEGL.a carries nouveau's
# nv50_ir codegen, which is C++ and needs libstdc++. The devkitPro Makefile
# build works around the same thing with `LD := $(CXX)`.
project(dw3-switch C CXX)

set(CMAKE_C_STANDARD 11)
set(DW3_VERSION "us" CACHE STRING "Game disc version: us or eu")
set_property(CACHE DW3_VERSION PROPERTY STRINGS us eu)
option(DW3_NXLINK_STDIO "Redirect game stdout/stderr through nxlink (development only)" OFF)
# Default ON for a fresh --nxlink configure (separate build dir); OFF for the
# standard release NRO. Override with -DDW3_NX_DEBUG_MESH=OFF|ON.
if(DW3_NXLINK_STDIO)
  set(_dw3_mesh_dbg_default ON)
else()
  set(_dw3_mesh_dbg_default OFF)
endif()
option(DW3_NX_DEBUG_MESH "Sampled GLES mesh/present debug summary (nxlink)"
       ${_dw3_mesh_dbg_default})
# One-shot SD dumps of the battle display rect (CPU VRAM + mesh FBO). Same
# default as mesh debug: on for --nxlink, off for the release NRO.
option(DW3_NX_DUMP_VRAM "Write battle VRAM/mesh FBO dumps under sdmc:/switch/dw3/"
       ${_dw3_mesh_dbg_default})
# On-screen HUD (FPS / mode / mesh). Same nxlink default as mesh debug.
option(DW3_NX_DEBUG_OVERLAY "On-screen debug overlay HUD (nxlink)"
       ${_dw3_mesh_dbg_default})
# P5.1 sampled heap / file-cache / VRAM-emulation budget (nxlink ~1 Hz).
option(DW3_NX_DEBUG_MEM "Sampled memory-budget summary (nxlink)"
       ${_dw3_mesh_dbg_default})
# P3.7 movie pacing: per-60-pump StGetNext hand-over / skipped-frame and
# STR-bridge contention counters. Same nxlink default as mesh debug.
option(DW3_NX_DEBUG_MOVIE "Sampled movie pacing/bridge summary (nxlink)"
       ${_dw3_mesh_dbg_default})
# Battle boots in gpu-tex (GLES mesh) since the preview.22 OLED feedback.
# -DDW3_NX_MESH_SOFT_BATTLE=ON seeds soft (software RasterTriangle) instead.
# Runtime: DW3_MESH_SOFT=0|1 seeds initial mode; Minus+ZL+ZR cycles
# soft → soft+bbox → gpu-solid → gpu-tex (see docs/DEBUG_OVERLAY.md).
option(DW3_NX_MESH_SOFT_BATTLE
       "Seed software RasterTriangle in FIGHTSTG instead of gpu-tex"
       OFF)
# Development only: "K=V,K=V" evidence-aid environment baked into the NRO
# (setenv at the top of main). Ryubing has no way to pass an environment to a
# homebrew NRO. Comma-separated (a ';' would split the CMake list), e.g.
# "DW3_START_MODE=0x600,DW3_FORCE_ENCOUNTER=1,DW3_MESH_SOFT=0" boots straight
# into a GLES-mesh battle. Never set for release/preview NROs.
set(DW3_NX_TEST_ENV "" CACHE STRING
    "Development-only K=V,K=V environment applied at boot (empty = off)")
# P5.4: deliberate null write after boot so the exception handler can be
# verified on hardware. Soft default OFF — never on in release/preview NROs.
option(DW3_NX_FORCE_CRASH
       "Force a CPU fault after boot (P5.4 crash-report smoke only)"
       OFF)
# P10.4: one short HD rumble pulse at boot (nx_rumble.c) to prove the
# actuator path without the in-game SHOCKTST stage. nxlink smoke only —
# never on in release/preview NROs.
option(DW3_NX_RUMBLE_SELFTEST
       "Send one HD rumble pulse at boot (P10.4 rumble smoke only)"
       OFF)

# P6.15 Switch replay: feeds sdmc:/switch/dw3/replay.rec (the PC .rec format,
# keyed by the same game-frame counter) to the pad path and logs
# `replay shot <frame> <name>` / `replay end` lines, with guest stderr routed
# to svcOutputDebugString so an emulator log carries them. Compile-time only:
# a default or release build contains none of this code, and the artifact is
# named dw3-switch[-nxlink]-replay so it cannot be mistaken for a release NRO.
option(DW3_NX_REPLAY
       "Replay-driven test NRO (P6.15, Ryubing regression only; never release)"
       OFF)

if(NOT CMAKE_SYSTEM_NAME STREQUAL "NintendoSwitch")
  message(FATAL_ERROR
    "configure platforms/switch with devkitA64's toolchain file "
    "(-DCMAKE_TOOLCHAIN_FILE=$DEVKITPRO/cmake/Switch.cmake); "
    "got CMAKE_SYSTEM_NAME='${CMAKE_SYSTEM_NAME}'")
endif()

# The overlay packing steps in port/ run `ld -r`/`objcopy`/`nm` over *target*
# objects, so they must use the target binutils — see the DW3_PACK_ENV block in
# port/CMakeLists.txt. Pass the absolute paths CMake detected for this toolchain
# rather than a prefixed name looked up on PATH: devkitA64's `nm` lives in
# devkitA64/bin, which a login shell's /etc/profile drops from PATH (its gcc is
# unaffected only because CMake invokes the compiler by absolute path).
set(DW3_LD "${CMAKE_LINKER}")
set(DW3_OBJCOPY "${CMAKE_OBJCOPY}")
set(DW3_NM "${CMAKE_NM}")
foreach(_dw3_tool IN ITEMS DW3_LD DW3_OBJCOPY DW3_NM)
  if(NOT EXISTS "${${_dw3_tool}}")
    message(FATAL_ERROR
      "${_dw3_tool} is '${${_dw3_tool}}' — CMake did not detect this "
      "toolchain's binutils. Pass -D${_dw3_tool}=<path> (or "
      "DW3_BINUTILS_PREFIX) pointing at the target tools.")
  endif()
endforeach()
message(STATUS "target binutils: ld=${DW3_LD} objcopy=${DW3_OBJCOPY} nm=${DW3_NM}")

add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../../vendor/psyz/psyz
                 ${CMAKE_BINARY_DIR}/psyz)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../../port ${CMAKE_BINARY_DIR}/port)

string(TOLOWER "${DW3_VERSION}" _dw3_ver)
if(_dw3_ver STREQUAL "us")
  set(_dw3_ver_macro VERSION_US=1)
  set(_dw3_default_runtime "sdmc:/switch/dw3/us")
elseif(_dw3_ver STREQUAL "eu")
  set(_dw3_ver_macro VERSION_EU=1)
  set(_dw3_default_runtime "sdmc:/switch/dw3/eu")
else()
  message(FATAL_ERROR "DW3_VERSION must be us or eu (got '${DW3_VERSION}')")
endif()
message(STATUS "DW3_VERSION=${_dw3_ver} (${_dw3_ver_macro})")
message(STATUS "DW3_DEFAULT_RUNTIME=${_dw3_default_runtime}")

# Host backend units. Globbed to match the devkitPro Makefile, which compiles
# psyz_backend/*.c wholesale — P4.4-P4.6 fill in the placeholder units.
file(GLOB DW3_NX_BACKEND_SOURCES CONFIGURE_DEPENDS
  "${CMAKE_CURRENT_SOURCE_DIR}/psyz_backend/*.c")
# P5.4: keep the crash TU visible in the target recipe (also matched by the
# glob above — list() REMOVE_DUPLICATES drops the second path).
list(APPEND DW3_NX_BACKEND_SOURCES
  "${CMAKE_CURRENT_SOURCE_DIR}/psyz_backend/nx_crash.c")
list(REMOVE_DUPLICATES DW3_NX_BACKEND_SOURCES)

add_executable(dw3-switch
  # boot_main.c is the game boot path and owns main(). source/main.c stays the
  # bring-up sample for the devkitPro Makefile build, so it is deliberately not
  # listed here — two main()s would collide.
  source/boot_main.c
  ${DW3_NX_BACKEND_SOURCES}
  # Shared 64-bit host glue: PSY-Z's SeqStruct grows with 64-bit pointers and
  # would overflow the game's PS1-sized table. Not PC-specific (aarch64 is LP64
  # too) — it should move to port/; see docs/SWITCH_BACKEND.md.
  ${CMAKE_CURRENT_SOURCE_DIR}/../pc/src/psyz_host_overrides.c
  # Direct TU so PadGetState / PadInitDirect win over PSY-Z's libetc stubs in
  # libpsyz.a, exactly as the PC host does it.
  ${CMAKE_CURRENT_SOURCE_DIR}/../../port/psyz_missing/psyz_pad_host.c
  ${CMAKE_CURRENT_SOURCE_DIR}/../../port/replay/dw3_replay.c
)

# Native title menu/logo (shared with the PC SDL3 GPU overlay): the canvas and
# logo resampler are plain C under platforms/pc/src; nx_title_overlay.c draws
# them over the GLES present. Same macro as PC selects the title patches.
target_sources(dw3-switch PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/../pc/src/title_overlay_art.c
  ${CMAKE_CURRENT_SOURCE_DIR}/../pc/src/title_options_cache.c
  ${CMAKE_CURRENT_SOURCE_DIR}/../pc/src/title_logo_resample.c
)
target_compile_definitions(dw3_title PRIVATE DW3_TITLE_NATIVE_OVERLAY=1)
target_compile_definitions(dw3_port_options PRIVATE DW3_TITLE_NATIVE_OVERLAY=1)
target_include_directories(dw3_port_options PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/../pc/src)

target_compile_definitions(dw3-switch PRIVATE
  ${_dw3_ver_macro}
  DW3_NX_GAME_IMAGE
  DW3_DEFAULT_RUNTIME="${_dw3_default_runtime}")

# NRO / NACP version for hbmenu (display_version is 15 printable chars).
# Prefer -DDW3_NRO_VERSION=... from tools/build_switch_game.sh; otherwise
# derive from git describe. See tools/dw3_nro_version.py.
set(DW3_NRO_VERSION "" CACHE STRING
    "Full port version label (git tip / preview tag); also used in boot log")
set(DW3_NACP_VERSION "" CACHE STRING
    "NACP display_version (≤15 chars); empty = derive from DW3_NRO_VERSION")
if(DW3_NRO_VERSION STREQUAL "")
  find_package(Python3 COMPONENTS Interpreter QUIET)
  set(_dw3_ver_script
      "${CMAKE_CURRENT_SOURCE_DIR}/../../tools/dw3_nro_version.py")
  if(Python3_Interpreter_FOUND AND EXISTS "${_dw3_ver_script}")
    execute_process(
      COMMAND "${Python3_EXECUTABLE}" "${_dw3_ver_script}"
              --repo "${CMAKE_CURRENT_SOURCE_DIR}/../.."
              --format export
      OUTPUT_VARIABLE _dw3_ver_export
      OUTPUT_STRIP_TRAILING_WHITESPACE
      ERROR_QUIET
      RESULT_VARIABLE _dw3_ver_rc
    )
    if(_dw3_ver_rc EQUAL 0)
      string(REGEX REPLACE "\n" ";" _dw3_ver_lines "${_dw3_ver_export}")
      foreach(_dw3_ver_line IN LISTS _dw3_ver_lines)
        if(_dw3_ver_line MATCHES "^DW3_NRO_VERSION=(.*)$")
          set(DW3_NRO_VERSION "${CMAKE_MATCH_1}")
        elseif(_dw3_ver_line MATCHES "^DW3_NACP_VERSION=(.*)$")
          set(DW3_NACP_VERSION "${CMAKE_MATCH_1}")
        endif()
      endforeach()
    endif()
  endif()
endif()
if(DW3_NRO_VERSION STREQUAL "")
  set(DW3_NRO_VERSION "unknown")
endif()
if(DW3_NACP_VERSION STREQUAL "")
  find_package(Python3 COMPONENTS Interpreter QUIET)
  set(_dw3_ver_script
      "${CMAKE_CURRENT_SOURCE_DIR}/../../tools/dw3_nro_version.py")
  if(Python3_Interpreter_FOUND AND EXISTS "${_dw3_ver_script}")
    execute_process(
      COMMAND "${Python3_EXECUTABLE}" "${_dw3_ver_script}"
              --version "${DW3_NRO_VERSION}" --format nacp
      OUTPUT_VARIABLE DW3_NACP_VERSION
      OUTPUT_STRIP_TRAILING_WHITESPACE
      ERROR_QUIET
    )
  endif()
endif()
if(DW3_NACP_VERSION STREQUAL "")
  string(SUBSTRING "${DW3_NRO_VERSION}" 0 15 DW3_NACP_VERSION)
endif()
# Escape for a C string literal in compile definitions.
string(REPLACE "\\" "\\\\" _dw3_nro_ver_c "${DW3_NRO_VERSION}")
string(REPLACE "\"" "\\\"" _dw3_nro_ver_c "${_dw3_nro_ver_c}")
target_compile_definitions(dw3-switch PRIVATE
  "DW3_NRO_VERSION_STR=\"${_dw3_nro_ver_c}\"")
message(STATUS "NRO version full='${DW3_NRO_VERSION}' nacp='${DW3_NACP_VERSION}'")

set(DW3_NRO_NAME "dw3-switch")
set(DW3_NACP_NAME "dw3-switch")
if(DW3_NXLINK_STDIO)
  target_compile_definitions(dw3-switch PRIVATE DW3_NXLINK_STDIO=1)
  set(DW3_NRO_NAME "dw3-switch-nxlink")
  set(DW3_NACP_NAME "dw3-switch nxlink")
  message(STATUS "nxlink stdio enabled for development NRO")
endif()
if(DW3_NX_REPLAY)
  target_compile_definitions(dw3-switch PRIVATE DW3_NX_REPLAY=1)
  set(DW3_NRO_NAME "${DW3_NRO_NAME}-replay")
  set(DW3_NACP_NAME "${DW3_NACP_NAME} replay")
  message(STATUS "nx REPLAY test build (DW3_NX_REPLAY): reads sdmc:/switch/dw3/replay.rec; not for release")
endif()
if(DW3_NX_DEBUG_MESH)
  target_compile_definitions(dw3-switch PRIVATE DW3_NX_DEBUG_MESH=1)
  message(STATUS "nx mesh debug summary enabled (DW3_NX_DEBUG_MESH)")
endif()
if(DW3_NX_DUMP_VRAM)
  target_compile_definitions(dw3-switch PRIVATE DW3_NX_DUMP_VRAM=1)
  message(STATUS "nx battle VRAM/mesh dump enabled (DW3_NX_DUMP_VRAM)")
endif()
if(DW3_NX_DEBUG_OVERLAY)
  target_compile_definitions(dw3-switch PRIVATE DW3_NX_DEBUG_OVERLAY=1)
  message(STATUS "nx on-screen debug overlay enabled (DW3_NX_DEBUG_OVERLAY)")
endif()
if(DW3_NX_DEBUG_MEM)
  target_compile_definitions(dw3-switch PRIVATE DW3_NX_DEBUG_MEM=1)
  message(STATUS "nx memory-budget summary enabled (DW3_NX_DEBUG_MEM)")
endif()
if(DW3_NX_DEBUG_MOVIE)
  target_compile_definitions(dw3-switch PRIVATE DW3_NX_DEBUG_MOVIE=1)
  message(STATUS "nx movie pacing summary enabled (DW3_NX_DEBUG_MOVIE)")
endif()
if(DW3_NX_MESH_SOFT_BATTLE)
  target_compile_definitions(dw3-switch PRIVATE DW3_NX_MESH_SOFT_BATTLE=1)
  message(STATUS "nx battle mesh soft RasterTriangle (DW3_NX_MESH_SOFT_BATTLE)")
else()
  target_compile_definitions(dw3-switch PRIVATE DW3_NX_MESH_SOFT_BATTLE=0)
  message(STATUS "nx battle GLES mesh path enabled (DW3_NX_MESH_SOFT_BATTLE=OFF)")
endif()
if(NOT DW3_NX_TEST_ENV STREQUAL "")
  string(REPLACE "\\" "\\\\" _dw3_test_env_c "${DW3_NX_TEST_ENV}")
  string(REPLACE "\"" "\\\"" _dw3_test_env_c "${_dw3_test_env_c}")
  target_compile_definitions(dw3-switch PRIVATE
    "DW3_NX_TEST_ENV_STR=\"${_dw3_test_env_c}\"")
  message(STATUS "nx DEVELOPMENT test env baked in: ${DW3_NX_TEST_ENV}")
endif()
if(DW3_NX_FORCE_CRASH)
  target_compile_definitions(dw3-switch PRIVATE DW3_NX_FORCE_CRASH=1)
  message(STATUS "nx FORCE crash after boot enabled (DW3_NX_FORCE_CRASH) — P5.4 only")
else()
  target_compile_definitions(dw3-switch PRIVATE DW3_NX_FORCE_CRASH=0)
endif()
if(DW3_NX_RUMBLE_SELFTEST)
  target_compile_definitions(dw3-switch PRIVATE DW3_NX_RUMBLE_SELFTEST=1)
  message(STATUS "nx rumble boot selftest enabled (DW3_NX_RUMBLE_SELFTEST) — P10.4 only")
endif()

# EGL/GLES2 come from devkitPro's switch-* portlibs, not from libnx, and they
# ship no CMake config packages — so the toolchain leaves them off the search
# path and the include/link dirs have to be explicit.
if(NOT DEVKITPRO)
  set(DEVKITPRO "$ENV{DEVKITPRO}")
endif()
set(DW3_NX_PORTLIBS "${DEVKITPRO}/portlibs/switch")
if(NOT IS_DIRECTORY "${DW3_NX_PORTLIBS}")
  message(FATAL_ERROR
    "portlibs not found at '${DW3_NX_PORTLIBS}' — install the switch-* "
    "portlibs (switch-mesa / switch-egl) or set DEVKITPRO")
endif()

target_include_directories(dw3-switch PRIVATE
  ${DW3_NX_PORTLIBS}/include
  ${CMAKE_CURRENT_SOURCE_DIR}/psyz_backend
  ${CMAKE_CURRENT_SOURCE_DIR}/../../port/cdfs
  ${CMAKE_CURRENT_SOURCE_DIR}/../../port/memcard
  ${CMAKE_CURRENT_SOURCE_DIR}/../../port/movies
  ${CMAKE_CURRENT_SOURCE_DIR}/../../port/overlays
  ${CMAKE_CURRENT_SOURCE_DIR}/../../port/include
  ${CMAKE_CURRENT_SOURCE_DIR}/../../vendor/psyz/psyz/include
  # PSY-Z's private renderer seam: src/draw.h (the Draw_* contract) and
  # src/platform/rasterizer.h (the CPU rasterizer's vtable). switch_gpu.c is the
  # only consumer — the rasterizer has no public API to include instead.
  ${CMAKE_CURRENT_SOURCE_DIR}/../../vendor/psyz/psyz/src
  ${CMAKE_CURRENT_SOURCE_DIR}/../../vendor/psyz/psyz/src/platform
)

target_link_directories(dw3-switch PRIVATE ${DW3_NX_PORTLIBS}/lib)

# Same archive strategy as platforms/pc: --whole-archive keeps the per-overlay
# objects (their named .data/.bss sections are what overlay_sections.c snapshots
# for restore), and --start-group resolves the engine↔field cross-references.
# Link through g++ so libstdc++ comes along (nouveau codegen inside libEGL.a).
set_target_properties(dw3-switch PROPERTIES LINKER_LANGUAGE CXX)

target_link_libraries(dw3-switch PRIVATE
  "-Wl,--start-group"
  "-Wl,--whole-archive" dw3_p32_field dw3_p33_battle dw3_p34_menus dw3_p35_cardgame
    dw3_p38_stagslct dw3_p62_cnty_sel
  "-Wl,--no-whole-archive"
  dw3_engine_objs
  "-Wl,--end-group"
  psyz
  # libnx's -lnx/-lm come from the platform module.
  EGL GLESv2 glapi drm_nouveau
)

target_link_options(dw3-switch PRIVATE
  # Memcard routing (port/memcard/psyz_card_wrap.c).
  "-Wl,--wrap=_card_read"
  "-Wl,--wrap=_card_write"
  # Beat PSY-Z's libetc pad stubs.
  "-Wl,--wrap=PadGetState"
  "-Wl,--wrap=PadStartCom"
  "-Wl,--wrap=PadInitMtap"
  "-Wl,--wrap=PadSetAct"
  "-Wl,--wrap=PadSetActAlign"
  # P3.7 MDEC movie present.
  "-Wl,--wrap=DMACallback"
  "-Wl,--wrap=DecDCTReset"
  "-Wl,--wrap=DecDCTout"
  "-Wl,--wrap=LoadImage"
  "-Wl,--wrap=PutDispEnv"
  "-Wl,--wrap=StGetNext"
  # Invalidate empty-ring verdict before producer/reset writes (separate TUs).
  "-Wl,--wrap=CdReady"
  "-Wl,--wrap=StSetRing"
  "-Wl,--wrap=StClearRing"
  "-Wl,--wrap=data_ready_callback"
  "-Wl,--wrap=StFreeRing"
  # TEMPORARY boot-stop trace (nx_present.c); see the comment there.
  "-Wl,--wrap=SetDispMask"
  "-Wl,--wrap=CdInit"
  "-Wl,--wrap=CdSetDebug"
  "-Wl,--wrap=SetGraphDebug"
  "-Wl,--wrap=CdControl"
  # Keep the named overlay .data/.bss orphan sections (overlay_sections.c).
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

# .nacp + .nro from the devkitPro platform module. Passing NACP is what stops
# elf2nro falling back to its stock "HOMEBREW" asset section — the trap the
# Makefile build hit (see platforms/switch/README.md). ICON is a 256×256 JPEG
# (platforms/switch/icon.jpg); regenerate from docs/assets/icon-256.png if the
# art changes. Without ICON, nx_create_nro embeds libnx's default_icon.jpg.
nx_generate_nacp(
  OUTPUT "${CMAKE_BINARY_DIR}/${DW3_NRO_NAME}.nacp"
  NAME "${DW3_NACP_NAME}"
  AUTHOR "dw3-switch"
  VERSION "${DW3_NACP_VERSION}"
)
# nx_generate_nacp does not DEPEND on NAME/VERSION; drop a stale .nacp so the
# next build re-runs nacptool after a version bump.
file(REMOVE "${CMAKE_BINARY_DIR}/${DW3_NRO_NAME}.nacp")
nx_create_nro(dw3-switch
  OUTPUT "${CMAKE_BINARY_DIR}/${DW3_NRO_NAME}.nro"
  NACP "${CMAKE_BINARY_DIR}/${DW3_NRO_NAME}.nacp"
  ICON "${CMAKE_CURRENT_SOURCE_DIR}/icon.jpg"
)
