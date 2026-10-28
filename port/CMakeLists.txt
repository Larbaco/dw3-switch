# Engine object library from the patched decomp copy (build/decomp-src).
# See docs/ROADMAP.md P2.1 and docs/superpowers/plans/2026-10-10-phase2-pc-bringup.md Task E2.
# Version select: -DDW3_VERSION=us|eu (default us). See docs/P3_EU_BUILD.md.

cmake_minimum_required(VERSION 3.21)

set(DW3_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/..")
set(DECOMP_SRC "${DW3_ROOT}/build/decomp-src")
set(PSYZ_INCLUDE "${DW3_ROOT}/vendor/psyz/psyz/include")
set(APPLY_PATCHES "${DW3_ROOT}/tools/apply_patches.sh")

set(DW3_VERSION "us" CACHE STRING "Game disc version: us or eu")
set_property(CACHE DW3_VERSION PROPERTY STRINGS us eu)
string(TOLOWER "${DW3_VERSION}" _dw3_ver)
if(_dw3_ver STREQUAL "us")
  set(DW3_VERSION_MACRO "VERSION_US=1")
  set(DW3_OVERLAY_TABLE "${CMAKE_CURRENT_SOURCE_DIR}/overlays/overlay_table_us.c")
elseif(_dw3_ver STREQUAL "eu")
  set(DW3_VERSION_MACRO "VERSION_EU=1")
  set(DW3_OVERLAY_TABLE "${CMAKE_CURRENT_SOURCE_DIR}/overlays/overlay_table_eu.c")
else()
  message(FATAL_ERROR "DW3_VERSION must be us or eu (got '${DW3_VERSION}')")
endif()
message(STATUS "DW3_VERSION=${_dw3_ver} (${DW3_VERSION_MACRO})")

if(NOT EXISTS "${APPLY_PATCHES}")
  message(FATAL_ERROR "missing ${APPLY_PATCHES}")
endif()

# Configure-time: refresh patched tree so GLOB sees sources.
execute_process(
  COMMAND "${APPLY_PATCHES}"
  WORKING_DIRECTORY "${DW3_ROOT}"
  RESULT_VARIABLE _dw3_apply_rc
  ERROR_VARIABLE _dw3_apply_err
  OUTPUT_VARIABLE _dw3_apply_out
)
if(NOT _dw3_apply_rc EQUAL 0)
  message(FATAL_ERROR "apply_patches.sh failed (${_dw3_apply_rc}):\n${_dw3_apply_out}${_dw3_apply_err}")
endif()

file(GLOB DW3_PATCH_FILES CONFIGURE_DEPENDS
  "${CMAKE_CURRENT_SOURCE_DIR}/patches/*.patch")
add_custom_command(
  OUTPUT "${DECOMP_SRC}/.dw3-patches-stamp"
  COMMAND "${APPLY_PATCHES}"
  COMMAND "${CMAKE_COMMAND}" -E touch "${DECOMP_SRC}/.dw3-patches-stamp"
  DEPENDS
    "${APPLY_PATCHES}"
    "${CMAKE_CURRENT_SOURCE_DIR}/patches/DECOMP_PIN"
    ${DW3_PATCH_FILES}
  WORKING_DIRECTORY "${DW3_ROOT}"
  COMMENT "Applying decomp patches into build/decomp-src"
  VERBATIM
)
add_custom_target(dw3_decomp_patched DEPENDS "${DECOMP_SRC}/.dw3-patches-stamp")

file(GLOB_RECURSE DW3_ENGINE_SOURCES CONFIGURE_DEPENDS
  "${DECOMP_SRC}/src/engine/*.c")
list(FILTER DW3_ENGINE_SOURCES EXCLUDE REGEX "\\.inc\\.c$")
list(LENGTH DW3_ENGINE_SOURCES _dw3_engine_count)
if(_dw3_engine_count EQUAL 0)
  message(FATAL_ERROR "no engine .c under ${DECOMP_SRC}/src/engine (run tools/apply_patches.sh)")
endif()

add_library(dw3_engine OBJECT ${DW3_ENGINE_SOURCES})
add_dependencies(dw3_engine dw3_decomp_patched)

# US title overlay (STDWTITL) — P2.8 / E6.
set(DW3_TITLE_SOURCES
  "${DECOMP_SRC}/src/menus/title/scene.c"
  "${DECOMP_SRC}/src/menus/title/logo.c"
  "${DECOMP_SRC}/src/menus/title/movie.c"
  "${DECOMP_SRC}/src/menus/title/glint.c"
  "${DECOMP_SRC}/src/menus/title/splash.c"
  "${DECOMP_SRC}/src/menus/title/title_loader.c"
  "${DECOMP_SRC}/src/menus/title/slides.c"
  "${DECOMP_SRC}/src/menus/title/menu.c"
  "${DECOMP_SRC}/src/menus/title/edge_fade.c"
  "${DECOMP_SRC}/src/menus/title/background.c"
  "${DECOMP_SRC}/src/menus/title/title.c"
  "${DECOMP_SRC}/src/menus/title/data/stdwtitl.c"
  "${DECOMP_SRC}/src/menus/title/data/title.c"
  "${DECOMP_SRC}/src/menus/title/data/movie.c"
)
add_library(dw3_title OBJECT ${DW3_TITLE_SOURCES})
add_dependencies(dw3_title dw3_decomp_patched)

# Host overlay address buffers (P2.3) + static restore model (P2.6) + title stubs.
add_library(dw3_host_overlays OBJECT
  "${CMAKE_CURRENT_SOURCE_DIR}/overlays/host_vram.c"
  "${CMAKE_CURRENT_SOURCE_DIR}/overlays/host_heap.c"
  "${CMAKE_CURRENT_SOURCE_DIR}/overlays/overlay_host.c"
  "${CMAKE_CURRENT_SOURCE_DIR}/overlays/overlay_sections.c"
  "${DW3_OVERLAY_TABLE}"
  "${CMAKE_CURRENT_SOURCE_DIR}/overlays/mode_stubs.c"
  "${CMAKE_CURRENT_SOURCE_DIR}/overlays/boot_tim_host.c"
)

# Psy-Q APIs missing from the pinned PSY-Z build (title bring-up).
add_library(dw3_psyz_missing OBJECT
  "${CMAKE_CURRENT_SOURCE_DIR}/psyz_missing/psyz_missing.c"
)

# P3.7 Wave 2–3: VLC (bsdec) + file-backed STR stream bridge.
set(BSDEC_DIR "${DW3_ROOT}/vendor/psyz/nugget/bsdec")
add_library(dw3_movies OBJECT
  "${CMAKE_CURRENT_SOURCE_DIR}/movies/dct_vlc.c"
  "${CMAKE_CURRENT_SOURCE_DIR}/movies/str_stream.c"
  "${CMAKE_CURRENT_SOURCE_DIR}/movies/str_restore.c"
  "${CMAKE_CURRENT_SOURCE_DIR}/movies/str_disc.c"
  "${BSDEC_DIR}/bsdec.c"
)
target_include_directories(dw3_movies PRIVATE
  "${BSDEC_DIR}"
  "${CMAKE_CURRENT_SOURCE_DIR}/cdfs"
  "${CMAKE_CURRENT_SOURCE_DIR}/movies"
  "${PSYZ_INCLUDE}"
)
target_compile_definitions(dw3_movies PRIVATE __psyz)
set_target_properties(dw3_movies PROPERTIES
  C_STANDARD 11
  C_STANDARD_REQUIRED ON
  C_EXTENSIONS ON
)

# CD → filesystem file-id reads (P2.5) and 128 KiB .mcr slots (P2.7).
add_library(dw3_cdfs OBJECT
  "${CMAKE_CURRENT_SOURCE_DIR}/cdfs/cdfs.c"
  "${CMAKE_CURRENT_SOURCE_DIR}/cdfs/runtime_extract.c"
)
target_include_directories(dw3_cdfs PRIVATE
  "${CMAKE_CURRENT_SOURCE_DIR}/movies"
)
add_library(dw3_memcard OBJECT
  "${CMAKE_CURRENT_SOURCE_DIR}/memcard/memcard_host.c"
  "${CMAKE_CURRENT_SOURCE_DIR}/memcard/save_convert.c"
  "${CMAKE_CURRENT_SOURCE_DIR}/memcard/memcard_libmcrd.c"
  "${CMAKE_CURRENT_SOURCE_DIR}/memcard/psyz_card_wrap.c"
  "${CMAKE_CURRENT_SOURCE_DIR}/saves/user_saves.c"
)
target_include_directories(dw3_memcard PRIVATE
  "${PSYZ_INCLUDE}"
  "${CMAKE_CURRENT_SOURCE_DIR}/include"
)
target_compile_definitions(dw3_memcard PRIVATE __psyz)

# Include order: port host → PSY-Z (Psy-Q names) → decomp headers/sources.
# port/include/common.h shadows decomp common.h so gnu89 does not redefine
# s8/u8 against PSY-Z's psyz/types.h.
set(_DW3_GAME_INCLUDES
  "${CMAKE_CURRENT_SOURCE_DIR}/include"
  "${CMAKE_CURRENT_SOURCE_DIR}/overlays"
  "${CMAKE_CURRENT_SOURCE_DIR}/cdfs"
  "${CMAKE_CURRENT_SOURCE_DIR}/memcard"
  "${PSYZ_INCLUDE}"
  "${DECOMP_SRC}/include"
  "${DECOMP_SRC}/src"
  "${DECOMP_SRC}/src/field/stages"
)
set(_DW3_GAME_DEFS
  ${DW3_VERSION_MACRO}
  PORT_HOST=1
  SKIP_ASM=1
  __psyz
)
# Log (once per site) when a PORT_HOST NULL guard is taken, see
# port/include/port_null_guard.h and docs/BALLOON_NULL_ACTOR.md. On for PC
# Debug builds and the Switch --nxlink build (DW3_NXLINK_STDIO), off for
# release; -DDW3_NULL_GUARD_LOG=ON|OFF overrides.
if(DW3_NXLINK_STDIO OR CMAKE_BUILD_TYPE STREQUAL "Debug")
  set(_dw3_null_guard_default ON)
else()
  set(_dw3_null_guard_default OFF)
endif()
option(DW3_NULL_GUARD_LOG "Log PORT_HOST NULL-guard hits (dev builds)" ${_dw3_null_guard_default})
if(DW3_NULL_GUARD_LOG)
  list(APPEND _DW3_GAME_DEFS DW3_NULL_GUARD_LOG=1)
  message(STATUS "PORT_HOST NULL-guard hit log enabled (DW3_NULL_GUARD_LOG)")
endif()
# P6.20: the overlays and stages compile through custom commands that take
# only these definitions (not the directory's), so patch 0077's hooks need it
# here too. Set by the platform CMakeLists (option DW3_CHEATS, default ON).
if(DW3_CHEATS)
  list(APPEND _DW3_GAME_DEFS DW3_CHEATS=1)
endif()
set(_DW3_GAME_OPTS
  -std=gnu89
  -fsigned-char
  -fno-builtin
  -fno-strict-aliasing
  -fwrapv
  -fno-delete-null-pointer-checks
  -fno-aggressive-loop-optimizations
  -fno-zero-initialized-in-bss
  -fno-common
  -fno-toplevel-reorder
  -Werror=implicit-function-declaration
  -Werror=int-conversion
  -Werror=pointer-to-int-cast
  -Werror=int-to-pointer-cast
)

target_include_directories(dw3_engine PRIVATE ${_DW3_GAME_INCLUDES})
target_include_directories(dw3_title PRIVATE ${_DW3_GAME_INCLUDES})
target_include_directories(dw3_host_overlays PRIVATE
  "${CMAKE_CURRENT_SOURCE_DIR}/overlays"
)
target_include_directories(dw3_cdfs PUBLIC
  "${CMAKE_CURRENT_SOURCE_DIR}/cdfs"
)
target_compile_definitions(dw3_cdfs PRIVATE ${DW3_VERSION_MACRO})
target_include_directories(dw3_memcard PUBLIC
  "${CMAKE_CURRENT_SOURCE_DIR}/memcard"
)
target_include_directories(dw3_psyz_missing PRIVATE
  "${PSYZ_INCLUDE}"
)
target_compile_definitions(dw3_psyz_missing PRIVATE __psyz)

target_compile_definitions(dw3_engine PRIVATE ${_DW3_GAME_DEFS})
target_compile_definitions(dw3_title PRIVATE ${_DW3_GAME_DEFS})
target_compile_definitions(dw3_host_overlays PRIVATE
  ${DW3_VERSION_MACRO}
  PORT_HOST=1
)

set_target_properties(dw3_engine PROPERTIES
  C_STANDARD 90
  C_STANDARD_REQUIRED ON
  C_EXTENSIONS ON
)
set_target_properties(dw3_title PROPERTIES
  C_STANDARD 90
  C_STANDARD_REQUIRED ON
  C_EXTENSIONS ON
)
set_target_properties(dw3_host_overlays PROPERTIES
  C_STANDARD 90
  C_STANDARD_REQUIRED ON
  C_EXTENSIONS ON
)
set_target_properties(dw3_cdfs PROPERTIES
  C_STANDARD 11
  C_STANDARD_REQUIRED ON
  C_EXTENSIONS ON
)
set_target_properties(dw3_memcard PROPERTIES
  C_STANDARD 11
  C_STANDARD_REQUIRED ON
  C_EXTENSIONS ON
)
set_target_properties(dw3_psyz_missing PROPERTIES
  C_STANDARD 11
  C_STANDARD_REQUIRED ON
  C_EXTENSIONS ON
)

# Host-relevant subset of vendor/dw3_decomp/mk/toolchain/gcc.mk GCC_CFLAGS.
target_compile_options(dw3_engine PRIVATE ${_DW3_GAME_OPTS})
target_compile_options(dw3_title PRIVATE ${_DW3_GAME_OPTS})

# P9.2 port options shell + shared 8×8 VRAM text (title Config/Extra).
add_library(dw3_port_options OBJECT
  "${CMAKE_CURRENT_SOURCE_DIR}/audio/audio_bus.c"
  "${CMAKE_CURRENT_SOURCE_DIR}/audio/volume.c"
  "${CMAKE_CURRENT_SOURCE_DIR}/autosave/autosave.c"
  "${CMAKE_CURRENT_SOURCE_DIR}/autosave/autosave_areas.c"
  "${CMAKE_CURRENT_SOURCE_DIR}/battle_menu/battle_menu_memory.c"
  "${CMAKE_CURRENT_SOURCE_DIR}/file_cache/file_cache_host.c"
  "${CMAKE_CURRENT_SOURCE_DIR}/items_menu/items_menu.c"
  "${CMAKE_CURRENT_SOURCE_DIR}/map_view/map_view.c"
  "${CMAKE_CURRENT_SOURCE_DIR}/map_view/map_view_core.c"
  "${CMAKE_CURRENT_SOURCE_DIR}/mods/mod_pack.c"
  "${CMAKE_CURRENT_SOURCE_DIR}/mods/png_stb.c"
  "${CMAKE_CURRENT_SOURCE_DIR}/options/battle_options.c"
  "${CMAKE_CURRENT_SOURCE_DIR}/options/confirm.c"
  "${CMAKE_CURRENT_SOURCE_DIR}/options/field_options.c"
  "${CMAKE_CURRENT_SOURCE_DIR}/options/options_cfg.c"
  "${CMAKE_CURRENT_SOURCE_DIR}/options/port_options.c"
  "${CMAKE_CURRENT_SOURCE_DIR}/options/restart.c"
  "${CMAKE_CURRENT_SOURCE_DIR}/options/screen_fade.c"
  "${CMAKE_CURRENT_SOURCE_DIR}/options/title_entry.c"
  "${CMAKE_CURRENT_SOURCE_DIR}/options/title_logo_capture.c"
  "${CMAKE_CURRENT_SOURCE_DIR}/options/turbo.c"
  "${CMAKE_CURRENT_SOURCE_DIR}/options/widescreen.c"
  "${CMAKE_CURRENT_SOURCE_DIR}/pacing/pacing.c"
  "${CMAKE_CURRENT_SOURCE_DIR}/pacing/pacing_policy.c"
  "${CMAKE_CURRENT_SOURCE_DIR}/options/title_background_capture.c"
  "${CMAKE_CURRENT_SOURCE_DIR}/ui/port_text.c"
)
# P6.20 Cheats screen and save mark; DW3_CHEATS is set by the platform
# CMakeLists (option DW3_CHEATS, default ON).
if(DW3_CHEATS)
  target_sources(dw3_port_options PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/options/cheats.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/options/cheat_saves.c"
  )
endif()
add_dependencies(dw3_port_options dw3_decomp_patched)
target_include_directories(dw3_port_options PRIVATE ${_DW3_GAME_INCLUDES}
  "${CMAKE_CURRENT_SOURCE_DIR}/map_view"
  "${CMAKE_CURRENT_SOURCE_DIR}/mods")
target_compile_definitions(dw3_port_options PRIVATE ${_DW3_GAME_DEFS})
target_compile_options(dw3_port_options PRIVATE ${_DW3_GAME_OPTS})
set_target_properties(dw3_port_options PROPERTIES
  C_STANDARD 90
  C_STANDARD_REQUIRED ON
  C_EXTENSIONS ON
)

# P3.1: compile-only audit of overlay/stage TUs under cast warning-as-error.
# Not linked into dw3_engine_objs until P3.2+ brings overlays in for real.
# Title sources stay on dw3_title (exclude to avoid duplicate OBJECT membership).
file(GLOB_RECURSE DW3_P31_SOURCES CONFIGURE_DEPENDS
  "${DECOMP_SRC}/src/field/*.c"
  "${DECOMP_SRC}/src/battle/*.c"
  "${DECOMP_SRC}/src/cardgame/*.c"
  "${DECOMP_SRC}/src/menus/*.c"
  "${DECOMP_SRC}/src/debug/*.c")
list(FILTER DW3_P31_SOURCES EXCLUDE REGEX "\\.inc\\.c$")
list(FILTER DW3_P31_SOURCES EXCLUDE REGEX "/menus/title/")
# US build: EU-only extra-chapter stages (WSTAG920–974) reference LANGUAGE.
# EU build keeps them (LANGUAGE comes from src/engine/data/eu_settings.c).
if(_dw3_ver STREQUAL "us")
  list(FILTER DW3_P31_SOURCES EXCLUDE REGEX
    "/wstag9(2[0-9]|3[0-9]|4[0-9]|5[0-9]|6[0-9]|7[0-4])\\.c$")
endif()
# PSY-Z / libgs gaps (not pointer↔int): see docs/P3.1_64BIT_AUDIT.md.
list(FILTER DW3_P31_SOURCES EXCLUDE REGEX
  "/battle/battle_mode/(draw|lights|model_mesh)\\.c$")
list(LENGTH DW3_P31_SOURCES _dw3_p31_count)
if(_dw3_p31_count EQUAL 0)
  message(FATAL_ERROR "no P3.1 audit .c under ${DECOMP_SRC}/src/{field,battle,cardgame,menus,debug}")
endif()
add_library(dw3_p31_audit OBJECT ${DW3_P31_SOURCES})
add_dependencies(dw3_p31_audit dw3_decomp_patched)
target_include_directories(dw3_p31_audit PRIVATE ${_DW3_GAME_INCLUDES})
target_compile_definitions(dw3_p31_audit PRIVATE ${_DW3_GAME_DEFS})
target_compile_options(dw3_p31_audit PRIVATE ${_DW3_GAME_OPTS})
set_target_properties(dw3_p31_audit PROPERTIES
  C_STANDARD 90
  C_STANDARD_REQUIRED ON
  C_EXTENSIONS ON
)

# ---------------------------------------------------------------------------
# P3.2: FIELDSTG + STPLNMET + WSTAG260/780/785/790 with prefixed stage symbols
# and named .data/.bss sections for static overlay restore.
# ---------------------------------------------------------------------------
set(PREFIX_STAGE_OBJ "${DW3_ROOT}/tools/prefix_stage_obj.py")
set(RENAME_OVL_SECTIONS "${DW3_ROOT}/tools/rename_ovl_sections.py")
set(GEN_STAGE_STUBS "${DW3_ROOT}/tools/gen_stage_stubs.py")
set(DW3_HOST_OVL_DIR "${CMAKE_CURRENT_BINARY_DIR}/host_ovl")
file(MAKE_DIRECTORY "${DW3_HOST_OVL_DIR}")

# The packing scripts below run `ld -r`/`objcopy`/`nm` over *target* objects, so
# a cross build has to point them at the target binutils — see the contract in
# tools/rename_ovl_sections.py. Nothing set here means the bare names, which is
# what the PC build uses and must keep using. The consumer's top-level
# CMakeLists supplies these; platforms/switch passes the absolute paths CMake
# detected for its toolchain, so nothing depends on PATH.
set(_dw3_pack_env_args "")
if(DW3_BINUTILS_PREFIX)
  list(APPEND _dw3_pack_env_args "DW3_BINUTILS_PREFIX=${DW3_BINUTILS_PREFIX}")
endif()
foreach(_dw3_tool IN ITEMS LD OBJCOPY NM)
  if(DW3_${_dw3_tool})
    list(APPEND _dw3_pack_env_args "DW3_${_dw3_tool}=${DW3_${_dw3_tool}}")
  endif()
endforeach()
set(DW3_PACK_ENV)
if(_dw3_pack_env_args)
  list(APPEND DW3_PACK_ENV "${CMAKE_COMMAND}" -E env ${_dw3_pack_env_args})
endif()

set(_DW3_COMPILE_INCLUDES "")
foreach(_inc IN LISTS _DW3_GAME_INCLUDES)
  list(APPEND _DW3_COMPILE_INCLUDES "-I${_inc}")
endforeach()
set(_DW3_COMPILE_DEFS "")
foreach(_def IN LISTS _DW3_GAME_DEFS)
  list(APPEND _DW3_COMPILE_DEFS "-D${_def}")
endforeach()

file(GLOB DW3_FIELD_SOURCES CONFIGURE_DEPENDS
  "${DECOMP_SRC}/src/field/field_mode/*.c"
  "${DECOMP_SRC}/src/field/field_mode/data/*.c")
list(FILTER DW3_FIELD_SOURCES EXCLUDE REGEX "\\.inc\\.c$")
list(LENGTH DW3_FIELD_SOURCES _dw3_field_count)
if(_dw3_field_count EQUAL 0)
  message(FATAL_ERROR "no FIELDSTG sources under ${DECOMP_SRC}/src/field/field_mode")
endif()

set(DW3_STPLNMET_SOURCES
  "${DECOMP_SRC}/src/menus/player_name/backdrop.c"
  "${DECOMP_SRC}/src/menus/player_name/choice.c"
  "${DECOMP_SRC}/src/menus/player_name/confirm.c"
  "${DECOMP_SRC}/src/menus/player_name/name_entry.c"
  "${DECOMP_SRC}/src/menus/player_name/scene.c"
  "${DECOMP_SRC}/src/menus/player_name/screen.c"
  "${DECOMP_SRC}/src/menus/player_name/welcome.c"
  "${DECOMP_SRC}/src/menus/player_name/data/stplnmet.c"
)

function(dw3_compile_c_objects out_var)
  set(_outs "")
  foreach(_src IN LISTS ARGN)
    get_filename_component(_base "${_src}" NAME_WE)
    get_filename_component(_dir "${_src}" DIRECTORY)
    string(REPLACE "${DECOMP_SRC}/" "" _rel "${_dir}")
    string(REPLACE "/" "_" _rel_flat "${_rel}")
    set(_obj "${DW3_HOST_OVL_DIR}/${_rel_flat}_${_base}.o")
    add_custom_command(
      OUTPUT "${_obj}"
      COMMAND ${CMAKE_C_COMPILER} -c
        ${_DW3_GAME_OPTS}
        ${_DW3_COMPILE_DEFS}
        ${_DW3_COMPILE_INCLUDES}
        -o "${_obj}"
        "${_src}"
      DEPENDS "${_src}" dw3_decomp_patched
      COMMENT "P3.2 compile ${_rel}/${_base}.c"
      VERBATIM
    )
    list(APPEND _outs "${_obj}")
  endforeach()
  set(${out_var} "${_outs}" PARENT_SCOPE)
endfunction()

dw3_compile_c_objects(DW3_FIELD_OBJECTS ${DW3_FIELD_SOURCES})
dw3_compile_c_objects(DW3_STPLNMET_OBJECTS ${DW3_STPLNMET_SOURCES})

add_custom_command(
  OUTPUT "${DW3_HOST_OVL_DIR}/fieldstg.host.o"
  COMMAND ${CMAKE_COMMAND} -E env PYTHONUNBUFFERED=1
    "${CMAKE_COMMAND}" -E rm -f "${DW3_HOST_OVL_DIR}/fieldstg.host.o"
  COMMAND ${DW3_PACK_ENV} python3 "${RENAME_OVL_SECTIONS}" fieldstg
    "${DW3_HOST_OVL_DIR}/fieldstg.host.o"
    ${DW3_FIELD_OBJECTS}
  DEPENDS ${DW3_FIELD_OBJECTS} "${RENAME_OVL_SECTIONS}"
  COMMENT "P3.2 pack FIELDSTG host overlay object"
  VERBATIM
)
add_custom_command(
  OUTPUT "${DW3_HOST_OVL_DIR}/stplnmet.host.o"
  COMMAND ${DW3_PACK_ENV} python3 "${RENAME_OVL_SECTIONS}" stplnmet
    "${DW3_HOST_OVL_DIR}/stplnmet.host.o"
    ${DW3_STPLNMET_OBJECTS}
  DEPENDS ${DW3_STPLNMET_OBJECTS} "${RENAME_OVL_SECTIONS}"
  COMMENT "P3.2 pack STPLNMET host overlay object"
  VERBATIM
)

function(dw3_host_stage stage src_rel)
  set(_src "${DECOMP_SRC}/src/field/stages/${src_rel}")
  set(_raw "${DW3_HOST_OVL_DIR}/${stage}.raw.o")
  set(_host "${DW3_HOST_OVL_DIR}/${stage}.host.o")
  add_custom_command(
    OUTPUT "${_host}"
    COMMAND ${CMAKE_C_COMPILER} -c
      ${_DW3_GAME_OPTS}
      ${_DW3_COMPILE_DEFS}
      ${_DW3_COMPILE_INCLUDES}
      -o "${_raw}"
      "${_src}"
    COMMAND ${DW3_PACK_ENV} python3 "${PREFIX_STAGE_OBJ}" "${stage}" "${_raw}" "${_host}"
    DEPENDS "${_src}" dw3_decomp_patched "${PREFIX_STAGE_OBJ}"
    COMMENT "P3.2 stage ${stage}"
    VERBATIM
  )
endfunction()

dw3_host_stage(wstag260 "wstag260.c")
dw3_host_stage(wstag780 "kusanagi_city/wstag780.c")
dw3_host_stage(wstag785 "kusanagi_city/wstag785.c")
dw3_host_stage(wstag790 "kusanagi_city/wstag790.c")
# P3.8 batch 1: early-game / SCRIPT_LEAVE-connected from 780/785/790
dw3_host_stage(wstag200 "central_sector/wstag200.c")
dw3_host_stage(wstag201 "central_sector/wstag201.c")
dw3_host_stage(wstag206 "central_sector/wstag206.c")
dw3_host_stage(wstag210 "asuka_city/wstag210.c")
dw3_host_stage(wstag212 "asuka_city/wstag212.c")
dw3_host_stage(wstag220 "asuka_city/wstag220.c")
dw3_host_stage(wstag225 "asuka_city/wstag225.c")
dw3_host_stage(wstag230 "asuka_city/wstag230.c")
dw3_host_stage(wstag235 "asuka_city/wstag235.c")
dw3_host_stage(wstag240 "asuka_city/wstag240.c")
dw3_host_stage(wstag245 "asuka_city/wstag245.c")
dw3_host_stage(wstag250 "asuka_city/wstag250.c")
dw3_host_stage(wstag255 "asuka_city/wstag255.c")
# P3.8 batch 2: remaining early Central/Asuka + Amaterasu twins
dw3_host_stage(wstag202 "central_sector/wstag202.c")
dw3_host_stage(wstag203 "central_sector/wstag203.c")
dw3_host_stage(wstag205 "central_sector/wstag205.c")
dw3_host_stage(wstag218 "central_sector/wstag218.c")
dw3_host_stage(wstag219 "central_sector/wstag219.c")
dw3_host_stage(wstag232 "asuka_city/wstag232.c")
dw3_host_stage(wstag237 "asuka_city/wstag237.c")
dw3_host_stage(wstag270 "asuka_city/wstag270.c")
dw3_host_stage(wstag275 "asuka_city/wstag275.c")
dw3_host_stage(wstag280 "asuka_city/wstag280.c")
dw3_host_stage(wstag285 "asuka_city/wstag285.c")
dw3_host_stage(wstag290 "asuka_city/wstag290.c")
dw3_host_stage(wstag295 "asuka_city/wstag295.c")
dw3_host_stage(wstag300 "asuka_city/wstag300.c")
dw3_host_stage(wstag305 "asuka_city/wstag305.c")
dw3_host_stage(wstag310 "asuka_city/wstag310.c")
dw3_host_stage(wstag315 "asuka_city/wstag315.c")
dw3_host_stage(wstag320 "asuka_city/wstag320.c")
dw3_host_stage(wstag325 "asuka_city/wstag325.c")
dw3_host_stage(wstag211 "amaterasu_city/wstag211.c")
dw3_host_stage(wstag221 "amaterasu_city/wstag221.c")
dw3_host_stage(wstag226 "amaterasu_city/wstag226.c")
dw3_host_stage(wstag231 "amaterasu_city/wstag231.c")
dw3_host_stage(wstag233 "amaterasu_city/wstag233.c")
dw3_host_stage(wstag236 "amaterasu_city/wstag236.c")
dw3_host_stage(wstag238 "amaterasu_city/wstag238.c")
dw3_host_stage(wstag241 "amaterasu_city/wstag241.c")
dw3_host_stage(wstag246 "amaterasu_city/wstag246.c")
dw3_host_stage(wstag251 "amaterasu_city/wstag251.c")
dw3_host_stage(wstag256 "amaterasu_city/wstag256.c")
dw3_host_stage(wstag261 "amaterasu_city/wstag261.c")
# P3.8 batch 3: Amaterasu twins of Asuka 270-325 + Central/East/Seiryu/South
dw3_host_stage(wstag271 "amaterasu_city/wstag271.c")
dw3_host_stage(wstag276 "amaterasu_city/wstag276.c")
dw3_host_stage(wstag281 "amaterasu_city/wstag281.c")
dw3_host_stage(wstag286 "amaterasu_city/wstag286.c")
dw3_host_stage(wstag291 "amaterasu_city/wstag291.c")
dw3_host_stage(wstag296 "amaterasu_city/wstag296.c")
dw3_host_stage(wstag301 "amaterasu_city/wstag301.c")
dw3_host_stage(wstag306 "amaterasu_city/wstag306.c")
dw3_host_stage(wstag311 "amaterasu_city/wstag311.c")
dw3_host_stage(wstag316 "amaterasu_city/wstag316.c")
dw3_host_stage(wstag321 "amaterasu_city/wstag321.c")
dw3_host_stage(wstag326 "amaterasu_city/wstag326.c")
dw3_host_stage(wstag330 "central_sector/wstag330.c")
dw3_host_stage(wstag335 "central_sector/wstag335.c")
dw3_host_stage(wstag340 "central_sector/wstag340.c")
dw3_host_stage(wstag345 "central_sector/wstag345.c")
dw3_host_stage(wstag350 "east_sector/wstag350.c")
dw3_host_stage(wstag355 "east_sector/wstag355.c")
dw3_host_stage(wstag360 "east_sector/wstag360.c")
dw3_host_stage(wstag365 "east_sector/wstag365.c")
dw3_host_stage(wstag370 "east_sector/wstag370.c")
dw3_host_stage(wstag375 "east_sector/wstag375.c")
dw3_host_stage(wstag380 "east_sector/wstag380.c")
dw3_host_stage(wstag385 "east_sector/wstag385.c")
dw3_host_stage(wstag395 "east_sector/wstag395.c")
dw3_host_stage(wstag400 "east_sector/wstag400.c")
dw3_host_stage(wstag405 "east_sector/wstag405.c")
dw3_host_stage(wstag410 "east_sector/wstag410.c")
dw3_host_stage(wstag415 "east_sector/wstag415.c")
dw3_host_stage(wstag420 "east_sector/wstag420.c")
dw3_host_stage(wstag425 "seiryu_city/wstag425.c")
dw3_host_stage(wstag430 "seiryu_city/wstag430.c")
dw3_host_stage(wstag435 "seiryu_city/wstag435.c")
dw3_host_stage(wstag440 "south_sector/wstag440.c")
dw3_host_stage(wstag445 "south_sector/wstag445.c")
dw3_host_stage(wstag450 "south_sector/wstag450.c")
dw3_host_stage(wstag455 "south_sector/wstag455.c")
dw3_host_stage(wstag475 "south_sector/wstag475.c")
dw3_host_stage(wstag465 "south_sector/wstag465.c")
dw3_host_stage(wstag470 "south_sector/wstag470.c")

# P3.8 batch 4: South remainder/Suzaku/Network + West/Byakko/North (skip WSTAG460)
dw3_host_stage(wstag480 "south_sector/wstag480.c")
dw3_host_stage(wstag485 "south_sector/wstag485.c")
dw3_host_stage(wstag490 "south_sector/wstag490.c")
dw3_host_stage(wstag495 "south_sector/wstag495.c")
dw3_host_stage(wstag500 "south_sector/wstag500.c")
dw3_host_stage(wstag505 "suzaku_city/wstag505.c")
dw3_host_stage(wstag520 "suzaku_city/wstag520.c")
dw3_host_stage(wstag525 "south_sector/wstag525.c")
dw3_host_stage(wstag530 "south_sector/wstag530.c")
dw3_host_stage(wstag535 "south_sector/wstag535.c")
dw3_host_stage(wstag537 "south_sector/wstag537.c")
dw3_host_stage(wstag540 "network_break/wstag540.c")
dw3_host_stage(wstag545 "network_break/wstag545.c")
dw3_host_stage(wstag550 "west_sector/wstag550.c")
dw3_host_stage(wstag555 "west_sector/wstag555.c")
dw3_host_stage(wstag560 "west_sector/wstag560.c")
dw3_host_stage(wstag565 "west_sector/wstag565.c")
dw3_host_stage(wstag570 "west_sector/wstag570.c")
dw3_host_stage(wstag575 "west_sector/wstag575.c")
dw3_host_stage(wstag580 "west_sector/wstag580.c")
dw3_host_stage(wstag585 "west_sector/wstag585.c")
dw3_host_stage(wstag590 "west_sector/wstag590.c")
dw3_host_stage(wstag595 "west_sector/wstag595.c")
dw3_host_stage(wstag600 "west_sector/wstag600.c")
dw3_host_stage(wstag605 "west_sector/wstag605.c")
dw3_host_stage(wstag610 "west_sector/wstag610.c")
dw3_host_stage(wstag615 "west_sector/wstag615.c")
dw3_host_stage(wstag620 "west_sector/wstag620.c")
dw3_host_stage(wstag625 "west_sector/wstag625.c")
dw3_host_stage(wstag630 "west_sector/wstag630.c")
dw3_host_stage(wstag635 "west_sector/wstag635.c")
dw3_host_stage(wstag640 "west_sector/wstag640.c")
dw3_host_stage(wstag645 "west_sector/wstag645.c")
dw3_host_stage(wstag650 "west_sector/wstag650.c")
dw3_host_stage(wstag655 "west_sector/wstag655.c")
dw3_host_stage(wstag660 "west_sector/wstag660.c")
dw3_host_stage(wstag675 "byakko_city/wstag675.c")
dw3_host_stage(wstag680 "byakko_city/wstag680.c")
dw3_host_stage(wstag685 "byakko_city/wstag685.c")
dw3_host_stage(wstag690 "north_sector/wstag690.c")
dw3_host_stage(wstag695 "north_sector/wstag695.c")
dw3_host_stage(wstag700 "north_sector/wstag700.c")
dw3_host_stage(wstag705 "north_sector/wstag705.c")
dw3_host_stage(wstag710 "north_sector/wstag710.c")
dw3_host_stage(wstag715 "north_sector/wstag715.c")
dw3_host_stage(wstag720 "north_sector/wstag720.c")
dw3_host_stage(wstag725 "north_sector/wstag725.c")
dw3_host_stage(wstag730 "north_sector/wstag730.c")
dw3_host_stage(wstag735 "north_sector/wstag735.c")
dw3_host_stage(wstag740 "north_sector/wstag740.c")
dw3_host_stage(wstag745 "north_sector/wstag745.c")
dw3_host_stage(wstag750 "north_sector/wstag750.c")
dw3_host_stage(wstag755 "north_sector/wstag755.c")
dw3_host_stage(wstag760 "north_sector/wstag760.c")

# P3.8 batch 5: Amaterasu twins Central→North (skip WSTAG460)
dw3_host_stage(wstag331 "central_sector/wstag331.c")
dw3_host_stage(wstag336 "central_sector/wstag336.c")
dw3_host_stage(wstag341 "central_sector/wstag341.c")
dw3_host_stage(wstag346 "central_sector/wstag346.c")
dw3_host_stage(wstag351 "east_sector/wstag351.c")
dw3_host_stage(wstag356 "east_sector/wstag356.c")
dw3_host_stage(wstag361 "east_sector/wstag361.c")
dw3_host_stage(wstag366 "east_sector/wstag366.c")
dw3_host_stage(wstag371 "east_sector/wstag371.c")
dw3_host_stage(wstag376 "east_sector/wstag376.c")
dw3_host_stage(wstag381 "east_sector/wstag381.c")
dw3_host_stage(wstag386 "east_sector/wstag386.c")
dw3_host_stage(wstag396 "east_sector/wstag396.c")
dw3_host_stage(wstag401 "east_sector/wstag401.c")
dw3_host_stage(wstag406 "east_sector/wstag406.c")
dw3_host_stage(wstag411 "east_sector/wstag411.c")
dw3_host_stage(wstag421 "east_sector/wstag421.c")
dw3_host_stage(wstag426 "qing_long_city/wstag426.c")
dw3_host_stage(wstag431 "qing_long_city/wstag431.c")
dw3_host_stage(wstag436 "qing_long_city/wstag436.c")
dw3_host_stage(wstag441 "south_sector/wstag441.c")
dw3_host_stage(wstag446 "south_sector/wstag446.c")
dw3_host_stage(wstag451 "south_sector/wstag451.c")
dw3_host_stage(wstag456 "south_sector/wstag456.c")
dw3_host_stage(wstag466 "south_sector/wstag466.c")
dw3_host_stage(wstag471 "south_sector/wstag471.c")
dw3_host_stage(wstag476 "south_sector/wstag476.c")
dw3_host_stage(wstag481 "south_sector/wstag481.c")
dw3_host_stage(wstag486 "south_sector/wstag486.c")
dw3_host_stage(wstag491 "south_sector/wstag491.c")
dw3_host_stage(wstag496 "south_sector/wstag496.c")
dw3_host_stage(wstag501 "south_sector/wstag501.c")
dw3_host_stage(wstag506 "zhu_que_city/wstag506.c")
dw3_host_stage(wstag521 "zhu_que_city/wstag521.c")
dw3_host_stage(wstag526 "south_sector/wstag526.c")
dw3_host_stage(wstag531 "south_sector/wstag531.c")
dw3_host_stage(wstag538 "south_sector/wstag538.c")
dw3_host_stage(wstag551 "west_sector/wstag551.c")
dw3_host_stage(wstag556 "west_sector/wstag556.c")
dw3_host_stage(wstag561 "west_sector/wstag561.c")
dw3_host_stage(wstag566 "west_sector/wstag566.c")
dw3_host_stage(wstag571 "west_sector/wstag571.c")
dw3_host_stage(wstag576 "west_sector/wstag576.c")
dw3_host_stage(wstag581 "west_sector/wstag581.c")
dw3_host_stage(wstag586 "west_sector/wstag586.c")
dw3_host_stage(wstag591 "west_sector/wstag591.c")
dw3_host_stage(wstag596 "west_sector/wstag596.c")
dw3_host_stage(wstag601 "west_sector/wstag601.c")
dw3_host_stage(wstag606 "west_sector/wstag606.c")
dw3_host_stage(wstag611 "west_sector/wstag611.c")
dw3_host_stage(wstag616 "west_sector/wstag616.c")
dw3_host_stage(wstag621 "west_sector/wstag621.c")
dw3_host_stage(wstag631 "west_sector/wstag631.c")
dw3_host_stage(wstag636 "west_sector/wstag636.c")
dw3_host_stage(wstag641 "west_sector/wstag641.c")
dw3_host_stage(wstag646 "west_sector/wstag646.c")
dw3_host_stage(wstag651 "west_sector/wstag651.c")
dw3_host_stage(wstag656 "west_sector/wstag656.c")
dw3_host_stage(wstag661 "west_sector/wstag661.c")
dw3_host_stage(wstag676 "bai_hu_city/wstag676.c")
dw3_host_stage(wstag686 "bai_hu_city/wstag686.c")
dw3_host_stage(wstag691 "north_sector/wstag691.c")
dw3_host_stage(wstag696 "north_sector/wstag696.c")
dw3_host_stage(wstag701 "north_sector/wstag701.c")
dw3_host_stage(wstag706 "north_sector/wstag706.c")
dw3_host_stage(wstag711 "north_sector/wstag711.c")
dw3_host_stage(wstag716 "north_sector/wstag716.c")
dw3_host_stage(wstag721 "north_sector/wstag721.c")
dw3_host_stage(wstag726 "north_sector/wstag726.c")
dw3_host_stage(wstag731 "north_sector/wstag731.c")
dw3_host_stage(wstag736 "north_sector/wstag736.c")
dw3_host_stage(wstag741 "north_sector/wstag741.c")
dw3_host_stage(wstag746 "north_sector/wstag746.c")
dw3_host_stage(wstag756 "north_sector/wstag756.c")
dw3_host_stage(wstag761 "north_sector/wstag761.c")
# P3.8 batch 6: undersea / spy-satellite / underground
dw3_host_stage(wstag795 "undersea_base/wstag795.c")
dw3_host_stage(wstag800 "undersea_base/wstag800.c")
dw3_host_stage(wstag805 "undersea_base/wstag805.c")
dw3_host_stage(wstag810 "spy_satellite/wstag810.c")
dw3_host_stage(wstag815 "spy_satellite/wstag815.c")
dw3_host_stage(wstag820 "spy_satellite/wstag820.c")
dw3_host_stage(wstag825 "underground/wstag825.c")
dw3_host_stage(wstag830 "underground/wstag830.c")
dw3_host_stage(wstag835 "underground/wstag835.c")
dw3_host_stage(wstag840 "underground/wstag840.c")
dw3_host_stage(wstag845 "underground/wstag845.c")
dw3_host_stage(wstag850 "underground/wstag850.c")
dw3_host_stage(wstag855 "underground/wstag855.c")
dw3_host_stage(wstag860 "underground/wstag860.c")
dw3_host_stage(wstag865 "underground/wstag865.c")
dw3_host_stage(wstag870 "underground/wstag870.c")
dw3_host_stage(wstag875 "underground/wstag875.c")
dw3_host_stage(wstag880 "underground/wstag880.c")
dw3_host_stage(wstag885 "underground/wstag885.c")
dw3_host_stage(wstag890 "underground/wstag890.c")
dw3_host_stage(wstag895 "underground/wstag895.c")
set(DW3_LINKED_STAGES
  WSTAG260,WSTAG780,WSTAG785,WSTAG790,WSTAG200,WSTAG201,WSTAG206,WSTAG210,WSTAG212,WSTAG220,WSTAG225,WSTAG230,WSTAG235,WSTAG240,WSTAG245,WSTAG250,WSTAG255,WSTAG202,WSTAG203,WSTAG205,WSTAG218,WSTAG219,WSTAG232,WSTAG237,WSTAG270,WSTAG275,WSTAG280,WSTAG285,WSTAG290,WSTAG295,WSTAG300,WSTAG305,WSTAG310,WSTAG315,WSTAG320,WSTAG325,WSTAG211,WSTAG221,WSTAG226,WSTAG231,WSTAG233,WSTAG236,WSTAG238,WSTAG241,WSTAG246,WSTAG251,WSTAG256,WSTAG261,WSTAG271,WSTAG276,WSTAG281,WSTAG286,WSTAG291,WSTAG296,WSTAG301,WSTAG306,WSTAG311,WSTAG316,WSTAG321,WSTAG326,WSTAG330,WSTAG335,WSTAG340,WSTAG345,WSTAG350,WSTAG355,WSTAG360,WSTAG365,WSTAG370,WSTAG375,WSTAG380,WSTAG385,WSTAG395,WSTAG400,WSTAG405,WSTAG410,WSTAG415,WSTAG420,WSTAG425,WSTAG430,WSTAG435,WSTAG440,WSTAG445,WSTAG450,WSTAG455,WSTAG475,WSTAG465,WSTAG470,WSTAG480,WSTAG485,WSTAG490,WSTAG495,WSTAG500,WSTAG505,WSTAG520,WSTAG525,WSTAG530,WSTAG535,WSTAG537,WSTAG540,WSTAG545,WSTAG550,WSTAG555,WSTAG560,WSTAG565,WSTAG570,WSTAG575,WSTAG580,WSTAG585,WSTAG590,WSTAG595,WSTAG600,WSTAG605,WSTAG610,WSTAG615,WSTAG620,WSTAG625,WSTAG630,WSTAG635,WSTAG640,WSTAG645,WSTAG650,WSTAG655,WSTAG660,WSTAG675,WSTAG680,WSTAG685,WSTAG690,WSTAG695,WSTAG700,WSTAG705,WSTAG710,WSTAG715,WSTAG720,WSTAG725,WSTAG730,WSTAG735,WSTAG740,WSTAG745,WSTAG750,WSTAG755,WSTAG760,WSTAG331,WSTAG336,WSTAG341,WSTAG346,WSTAG351,WSTAG356,WSTAG361,WSTAG366,WSTAG371,WSTAG376,WSTAG381,WSTAG386,WSTAG396,WSTAG401,WSTAG406,WSTAG411,WSTAG421,WSTAG426,WSTAG431,WSTAG436,WSTAG441,WSTAG446,WSTAG451,WSTAG456,WSTAG466,WSTAG471,WSTAG476,WSTAG481,WSTAG486,WSTAG491,WSTAG496,WSTAG501,WSTAG506,WSTAG521,WSTAG526,WSTAG531,WSTAG538,WSTAG551,WSTAG556,WSTAG561,WSTAG566,WSTAG571,WSTAG576,WSTAG581,WSTAG586,WSTAG591,WSTAG596,WSTAG601,WSTAG606,WSTAG611,WSTAG616,WSTAG621,WSTAG631,WSTAG636,WSTAG641,WSTAG646,WSTAG651,WSTAG656,WSTAG661,WSTAG676,WSTAG686,WSTAG691,WSTAG696,WSTAG701,WSTAG706,WSTAG711,WSTAG716,WSTAG721,WSTAG726,WSTAG731,WSTAG736,WSTAG741,WSTAG746,WSTAG756,WSTAG761,WSTAG795,WSTAG800,WSTAG805,WSTAG810,WSTAG815,WSTAG820,WSTAG825,WSTAG830,WSTAG835,WSTAG840,WSTAG845,WSTAG850,WSTAG855,WSTAG860,WSTAG865,WSTAG870,WSTAG875,WSTAG880,WSTAG885,WSTAG890,WSTAG895
)

set(DW3_STAGE_STUBS_C "${DW3_HOST_OVL_DIR}/stage_stubs.c")
set(DW3_STAGE_STUBS_O "${DW3_HOST_OVL_DIR}/stage_stubs.o")
add_custom_command(
  OUTPUT "${DW3_STAGE_STUBS_C}"
  COMMAND python3 "${GEN_STAGE_STUBS}"
    --stages-h "${DECOMP_SRC}/include/field/stages.h"
    --exclude "${DW3_LINKED_STAGES}"
    -o "${DW3_STAGE_STUBS_C}"
  DEPENDS
    "${GEN_STAGE_STUBS}"
    "${DECOMP_SRC}/include/field/stages.h"
    dw3_decomp_patched
  COMMENT "P3.2 generate weak stage stubs"
  VERBATIM
)
add_custom_command(
  OUTPUT "${DW3_STAGE_STUBS_O}"
  COMMAND ${CMAKE_C_COMPILER} -c -std=c11 -Wall
    -DPORT_HOST=1
    -o "${DW3_STAGE_STUBS_O}"
    "${DW3_STAGE_STUBS_C}"
  DEPENDS "${DW3_STAGE_STUBS_C}"
  COMMENT "P3.2 compile stage stubs"
  VERBATIM
)

set(DW3_P32_HOST_OBJECTS
  "${DW3_HOST_OVL_DIR}/fieldstg.host.o"
  "${DW3_HOST_OVL_DIR}/stplnmet.host.o"
  "${DW3_HOST_OVL_DIR}/wstag260.host.o"
  "${DW3_HOST_OVL_DIR}/wstag780.host.o"
  "${DW3_HOST_OVL_DIR}/wstag785.host.o"
  "${DW3_HOST_OVL_DIR}/wstag790.host.o"
  "${DW3_HOST_OVL_DIR}/wstag200.host.o"
  "${DW3_HOST_OVL_DIR}/wstag201.host.o"
  "${DW3_HOST_OVL_DIR}/wstag206.host.o"
  "${DW3_HOST_OVL_DIR}/wstag210.host.o"
  "${DW3_HOST_OVL_DIR}/wstag212.host.o"
  "${DW3_HOST_OVL_DIR}/wstag220.host.o"
  "${DW3_HOST_OVL_DIR}/wstag225.host.o"
  "${DW3_HOST_OVL_DIR}/wstag230.host.o"
  "${DW3_HOST_OVL_DIR}/wstag235.host.o"
  "${DW3_HOST_OVL_DIR}/wstag240.host.o"
  "${DW3_HOST_OVL_DIR}/wstag245.host.o"
  "${DW3_HOST_OVL_DIR}/wstag250.host.o"
  "${DW3_HOST_OVL_DIR}/wstag255.host.o"
  "${DW3_HOST_OVL_DIR}/wstag202.host.o"
  "${DW3_HOST_OVL_DIR}/wstag203.host.o"
  "${DW3_HOST_OVL_DIR}/wstag205.host.o"
  "${DW3_HOST_OVL_DIR}/wstag218.host.o"
  "${DW3_HOST_OVL_DIR}/wstag219.host.o"
  "${DW3_HOST_OVL_DIR}/wstag232.host.o"
  "${DW3_HOST_OVL_DIR}/wstag237.host.o"
  "${DW3_HOST_OVL_DIR}/wstag270.host.o"
  "${DW3_HOST_OVL_DIR}/wstag275.host.o"
  "${DW3_HOST_OVL_DIR}/wstag280.host.o"
  "${DW3_HOST_OVL_DIR}/wstag285.host.o"
  "${DW3_HOST_OVL_DIR}/wstag290.host.o"
  "${DW3_HOST_OVL_DIR}/wstag295.host.o"
  "${DW3_HOST_OVL_DIR}/wstag300.host.o"
  "${DW3_HOST_OVL_DIR}/wstag305.host.o"
  "${DW3_HOST_OVL_DIR}/wstag310.host.o"
  "${DW3_HOST_OVL_DIR}/wstag315.host.o"
  "${DW3_HOST_OVL_DIR}/wstag320.host.o"
  "${DW3_HOST_OVL_DIR}/wstag325.host.o"
  "${DW3_HOST_OVL_DIR}/wstag211.host.o"
  "${DW3_HOST_OVL_DIR}/wstag221.host.o"
  "${DW3_HOST_OVL_DIR}/wstag226.host.o"
  "${DW3_HOST_OVL_DIR}/wstag231.host.o"
  "${DW3_HOST_OVL_DIR}/wstag233.host.o"
  "${DW3_HOST_OVL_DIR}/wstag236.host.o"
  "${DW3_HOST_OVL_DIR}/wstag238.host.o"
  "${DW3_HOST_OVL_DIR}/wstag241.host.o"
  "${DW3_HOST_OVL_DIR}/wstag246.host.o"
  "${DW3_HOST_OVL_DIR}/wstag251.host.o"
  "${DW3_HOST_OVL_DIR}/wstag256.host.o"
  "${DW3_HOST_OVL_DIR}/wstag261.host.o"
  "${DW3_HOST_OVL_DIR}/wstag271.host.o"
  "${DW3_HOST_OVL_DIR}/wstag276.host.o"
  "${DW3_HOST_OVL_DIR}/wstag281.host.o"
  "${DW3_HOST_OVL_DIR}/wstag286.host.o"
  "${DW3_HOST_OVL_DIR}/wstag291.host.o"
  "${DW3_HOST_OVL_DIR}/wstag296.host.o"
  "${DW3_HOST_OVL_DIR}/wstag301.host.o"
  "${DW3_HOST_OVL_DIR}/wstag306.host.o"
  "${DW3_HOST_OVL_DIR}/wstag311.host.o"
  "${DW3_HOST_OVL_DIR}/wstag316.host.o"
  "${DW3_HOST_OVL_DIR}/wstag321.host.o"
  "${DW3_HOST_OVL_DIR}/wstag326.host.o"
  "${DW3_HOST_OVL_DIR}/wstag330.host.o"
  "${DW3_HOST_OVL_DIR}/wstag335.host.o"
  "${DW3_HOST_OVL_DIR}/wstag340.host.o"
  "${DW3_HOST_OVL_DIR}/wstag345.host.o"
  "${DW3_HOST_OVL_DIR}/wstag350.host.o"
  "${DW3_HOST_OVL_DIR}/wstag355.host.o"
  "${DW3_HOST_OVL_DIR}/wstag360.host.o"
  "${DW3_HOST_OVL_DIR}/wstag365.host.o"
  "${DW3_HOST_OVL_DIR}/wstag370.host.o"
  "${DW3_HOST_OVL_DIR}/wstag375.host.o"
  "${DW3_HOST_OVL_DIR}/wstag380.host.o"
  "${DW3_HOST_OVL_DIR}/wstag385.host.o"
  "${DW3_HOST_OVL_DIR}/wstag395.host.o"
  "${DW3_HOST_OVL_DIR}/wstag400.host.o"
  "${DW3_HOST_OVL_DIR}/wstag405.host.o"
  "${DW3_HOST_OVL_DIR}/wstag410.host.o"
  "${DW3_HOST_OVL_DIR}/wstag415.host.o"
  "${DW3_HOST_OVL_DIR}/wstag420.host.o"
  "${DW3_HOST_OVL_DIR}/wstag425.host.o"
  "${DW3_HOST_OVL_DIR}/wstag430.host.o"
  "${DW3_HOST_OVL_DIR}/wstag435.host.o"
  "${DW3_HOST_OVL_DIR}/wstag440.host.o"
  "${DW3_HOST_OVL_DIR}/wstag445.host.o"
  "${DW3_HOST_OVL_DIR}/wstag450.host.o"
  "${DW3_HOST_OVL_DIR}/wstag455.host.o"
  "${DW3_HOST_OVL_DIR}/wstag475.host.o"
  "${DW3_HOST_OVL_DIR}/wstag465.host.o"
  "${DW3_HOST_OVL_DIR}/wstag470.host.o"
  "${DW3_HOST_OVL_DIR}/wstag480.host.o"
  "${DW3_HOST_OVL_DIR}/wstag485.host.o"
  "${DW3_HOST_OVL_DIR}/wstag490.host.o"
  "${DW3_HOST_OVL_DIR}/wstag495.host.o"
  "${DW3_HOST_OVL_DIR}/wstag500.host.o"
  "${DW3_HOST_OVL_DIR}/wstag505.host.o"
  "${DW3_HOST_OVL_DIR}/wstag520.host.o"
  "${DW3_HOST_OVL_DIR}/wstag525.host.o"
  "${DW3_HOST_OVL_DIR}/wstag530.host.o"
  "${DW3_HOST_OVL_DIR}/wstag535.host.o"
  "${DW3_HOST_OVL_DIR}/wstag537.host.o"
  "${DW3_HOST_OVL_DIR}/wstag540.host.o"
  "${DW3_HOST_OVL_DIR}/wstag545.host.o"
  "${DW3_HOST_OVL_DIR}/wstag550.host.o"
  "${DW3_HOST_OVL_DIR}/wstag555.host.o"
  "${DW3_HOST_OVL_DIR}/wstag560.host.o"
  "${DW3_HOST_OVL_DIR}/wstag565.host.o"
  "${DW3_HOST_OVL_DIR}/wstag570.host.o"
  "${DW3_HOST_OVL_DIR}/wstag575.host.o"
  "${DW3_HOST_OVL_DIR}/wstag580.host.o"
  "${DW3_HOST_OVL_DIR}/wstag585.host.o"
  "${DW3_HOST_OVL_DIR}/wstag590.host.o"
  "${DW3_HOST_OVL_DIR}/wstag595.host.o"
  "${DW3_HOST_OVL_DIR}/wstag600.host.o"
  "${DW3_HOST_OVL_DIR}/wstag605.host.o"
  "${DW3_HOST_OVL_DIR}/wstag610.host.o"
  "${DW3_HOST_OVL_DIR}/wstag615.host.o"
  "${DW3_HOST_OVL_DIR}/wstag620.host.o"
  "${DW3_HOST_OVL_DIR}/wstag625.host.o"
  "${DW3_HOST_OVL_DIR}/wstag630.host.o"
  "${DW3_HOST_OVL_DIR}/wstag635.host.o"
  "${DW3_HOST_OVL_DIR}/wstag640.host.o"
  "${DW3_HOST_OVL_DIR}/wstag645.host.o"
  "${DW3_HOST_OVL_DIR}/wstag650.host.o"
  "${DW3_HOST_OVL_DIR}/wstag655.host.o"
  "${DW3_HOST_OVL_DIR}/wstag660.host.o"
  "${DW3_HOST_OVL_DIR}/wstag675.host.o"
  "${DW3_HOST_OVL_DIR}/wstag680.host.o"
  "${DW3_HOST_OVL_DIR}/wstag685.host.o"
  "${DW3_HOST_OVL_DIR}/wstag690.host.o"
  "${DW3_HOST_OVL_DIR}/wstag695.host.o"
  "${DW3_HOST_OVL_DIR}/wstag700.host.o"
  "${DW3_HOST_OVL_DIR}/wstag705.host.o"
  "${DW3_HOST_OVL_DIR}/wstag710.host.o"
  "${DW3_HOST_OVL_DIR}/wstag715.host.o"
  "${DW3_HOST_OVL_DIR}/wstag720.host.o"
  "${DW3_HOST_OVL_DIR}/wstag725.host.o"
  "${DW3_HOST_OVL_DIR}/wstag730.host.o"
  "${DW3_HOST_OVL_DIR}/wstag735.host.o"
  "${DW3_HOST_OVL_DIR}/wstag740.host.o"
  "${DW3_HOST_OVL_DIR}/wstag745.host.o"
  "${DW3_HOST_OVL_DIR}/wstag750.host.o"
  "${DW3_HOST_OVL_DIR}/wstag755.host.o"
  "${DW3_HOST_OVL_DIR}/wstag760.host.o"
  "${DW3_HOST_OVL_DIR}/wstag331.host.o"
  "${DW3_HOST_OVL_DIR}/wstag336.host.o"
  "${DW3_HOST_OVL_DIR}/wstag341.host.o"
  "${DW3_HOST_OVL_DIR}/wstag346.host.o"
  "${DW3_HOST_OVL_DIR}/wstag351.host.o"
  "${DW3_HOST_OVL_DIR}/wstag356.host.o"
  "${DW3_HOST_OVL_DIR}/wstag361.host.o"
  "${DW3_HOST_OVL_DIR}/wstag366.host.o"
  "${DW3_HOST_OVL_DIR}/wstag371.host.o"
  "${DW3_HOST_OVL_DIR}/wstag376.host.o"
  "${DW3_HOST_OVL_DIR}/wstag381.host.o"
  "${DW3_HOST_OVL_DIR}/wstag386.host.o"
  "${DW3_HOST_OVL_DIR}/wstag396.host.o"
  "${DW3_HOST_OVL_DIR}/wstag401.host.o"
  "${DW3_HOST_OVL_DIR}/wstag406.host.o"
  "${DW3_HOST_OVL_DIR}/wstag411.host.o"
  "${DW3_HOST_OVL_DIR}/wstag421.host.o"
  "${DW3_HOST_OVL_DIR}/wstag426.host.o"
  "${DW3_HOST_OVL_DIR}/wstag431.host.o"
  "${DW3_HOST_OVL_DIR}/wstag436.host.o"
  "${DW3_HOST_OVL_DIR}/wstag441.host.o"
  "${DW3_HOST_OVL_DIR}/wstag446.host.o"
  "${DW3_HOST_OVL_DIR}/wstag451.host.o"
  "${DW3_HOST_OVL_DIR}/wstag456.host.o"
  "${DW3_HOST_OVL_DIR}/wstag466.host.o"
  "${DW3_HOST_OVL_DIR}/wstag471.host.o"
  "${DW3_HOST_OVL_DIR}/wstag476.host.o"
  "${DW3_HOST_OVL_DIR}/wstag481.host.o"
  "${DW3_HOST_OVL_DIR}/wstag486.host.o"
  "${DW3_HOST_OVL_DIR}/wstag491.host.o"
  "${DW3_HOST_OVL_DIR}/wstag496.host.o"
  "${DW3_HOST_OVL_DIR}/wstag501.host.o"
  "${DW3_HOST_OVL_DIR}/wstag506.host.o"
  "${DW3_HOST_OVL_DIR}/wstag521.host.o"
  "${DW3_HOST_OVL_DIR}/wstag526.host.o"
  "${DW3_HOST_OVL_DIR}/wstag531.host.o"
  "${DW3_HOST_OVL_DIR}/wstag538.host.o"
  "${DW3_HOST_OVL_DIR}/wstag551.host.o"
  "${DW3_HOST_OVL_DIR}/wstag556.host.o"
  "${DW3_HOST_OVL_DIR}/wstag561.host.o"
  "${DW3_HOST_OVL_DIR}/wstag566.host.o"
  "${DW3_HOST_OVL_DIR}/wstag571.host.o"
  "${DW3_HOST_OVL_DIR}/wstag576.host.o"
  "${DW3_HOST_OVL_DIR}/wstag581.host.o"
  "${DW3_HOST_OVL_DIR}/wstag586.host.o"
  "${DW3_HOST_OVL_DIR}/wstag591.host.o"
  "${DW3_HOST_OVL_DIR}/wstag596.host.o"
  "${DW3_HOST_OVL_DIR}/wstag601.host.o"
  "${DW3_HOST_OVL_DIR}/wstag606.host.o"
  "${DW3_HOST_OVL_DIR}/wstag611.host.o"
  "${DW3_HOST_OVL_DIR}/wstag616.host.o"
  "${DW3_HOST_OVL_DIR}/wstag621.host.o"
  "${DW3_HOST_OVL_DIR}/wstag631.host.o"
  "${DW3_HOST_OVL_DIR}/wstag636.host.o"
  "${DW3_HOST_OVL_DIR}/wstag641.host.o"
  "${DW3_HOST_OVL_DIR}/wstag646.host.o"
  "${DW3_HOST_OVL_DIR}/wstag651.host.o"
  "${DW3_HOST_OVL_DIR}/wstag656.host.o"
  "${DW3_HOST_OVL_DIR}/wstag661.host.o"
  "${DW3_HOST_OVL_DIR}/wstag676.host.o"
  "${DW3_HOST_OVL_DIR}/wstag686.host.o"
  "${DW3_HOST_OVL_DIR}/wstag691.host.o"
  "${DW3_HOST_OVL_DIR}/wstag696.host.o"
  "${DW3_HOST_OVL_DIR}/wstag701.host.o"
  "${DW3_HOST_OVL_DIR}/wstag706.host.o"
  "${DW3_HOST_OVL_DIR}/wstag711.host.o"
  "${DW3_HOST_OVL_DIR}/wstag716.host.o"
  "${DW3_HOST_OVL_DIR}/wstag721.host.o"
  "${DW3_HOST_OVL_DIR}/wstag726.host.o"
  "${DW3_HOST_OVL_DIR}/wstag731.host.o"
  "${DW3_HOST_OVL_DIR}/wstag736.host.o"
  "${DW3_HOST_OVL_DIR}/wstag741.host.o"
  "${DW3_HOST_OVL_DIR}/wstag746.host.o"
  "${DW3_HOST_OVL_DIR}/wstag756.host.o"
  "${DW3_HOST_OVL_DIR}/wstag761.host.o"
  "${DW3_HOST_OVL_DIR}/wstag795.host.o"
  "${DW3_HOST_OVL_DIR}/wstag800.host.o"
  "${DW3_HOST_OVL_DIR}/wstag805.host.o"
  "${DW3_HOST_OVL_DIR}/wstag810.host.o"
  "${DW3_HOST_OVL_DIR}/wstag815.host.o"
  "${DW3_HOST_OVL_DIR}/wstag820.host.o"
  "${DW3_HOST_OVL_DIR}/wstag825.host.o"
  "${DW3_HOST_OVL_DIR}/wstag830.host.o"
  "${DW3_HOST_OVL_DIR}/wstag835.host.o"
  "${DW3_HOST_OVL_DIR}/wstag840.host.o"
  "${DW3_HOST_OVL_DIR}/wstag845.host.o"
  "${DW3_HOST_OVL_DIR}/wstag850.host.o"
  "${DW3_HOST_OVL_DIR}/wstag855.host.o"
  "${DW3_HOST_OVL_DIR}/wstag860.host.o"
  "${DW3_HOST_OVL_DIR}/wstag865.host.o"
  "${DW3_HOST_OVL_DIR}/wstag870.host.o"
  "${DW3_HOST_OVL_DIR}/wstag875.host.o"
  "${DW3_HOST_OVL_DIR}/wstag880.host.o"
  "${DW3_HOST_OVL_DIR}/wstag885.host.o"
  "${DW3_HOST_OVL_DIR}/wstag890.host.o"
  "${DW3_HOST_OVL_DIR}/wstag895.host.o"
  "${DW3_STAGE_STUBS_O}"
)
set_source_files_properties(${DW3_P32_HOST_OBJECTS} PROPERTIES
  EXTERNAL_OBJECT TRUE
  GENERATED TRUE
)
add_custom_target(dw3_p32_field_objs DEPENDS ${DW3_P32_HOST_OBJECTS})

# Whole-archive field/stage host objects so named .data/.bss sections land in
# the final link (orphan __start_/__stop_ symbols from overlay_sections.c).
add_library(dw3_p32_field STATIC ${DW3_P32_HOST_OBJECTS})
set_target_properties(dw3_p32_field PROPERTIES
  LINKER_LANGUAGE C
)
add_dependencies(dw3_p32_field dw3_p32_field_objs)

# ---------------------------------------------------------------------------
# P3.3: FIGHTSTG + WFIGHTMN + STFGTREP with named .data/.bss restore.
# ---------------------------------------------------------------------------
file(GLOB DW3_FIGHT_SOURCES CONFIGURE_DEPENDS
  "${DECOMP_SRC}/src/battle/battle_mode/*.c")
list(FILTER DW3_FIGHT_SOURCES EXCLUDE REGEX "\\.inc\\.c$")
list(LENGTH DW3_FIGHT_SOURCES _dw3_fight_count)
if(_dw3_fight_count EQUAL 0)
  message(FATAL_ERROR "no FIGHTSTG sources under ${DECOMP_SRC}/src/battle/battle_mode")
endif()

file(GLOB DW3_WFIGHTMN_SOURCES CONFIGURE_DEPENDS
  "${DECOMP_SRC}/src/battle/normal_battle/*.c"
  "${DECOMP_SRC}/src/battle/normal_battle/data/*.c")
list(FILTER DW3_WFIGHTMN_SOURCES EXCLUDE REGEX "\\.inc\\.c$")
list(LENGTH DW3_WFIGHTMN_SOURCES _dw3_wfightmn_count)
if(_dw3_wfightmn_count EQUAL 0)
  message(FATAL_ERROR "no WFIGHTMN sources under ${DECOMP_SRC}/src/battle/normal_battle")
endif()

file(GLOB DW3_STFGTREP_SOURCES CONFIGURE_DEPENDS
  "${DECOMP_SRC}/src/battle/report/*.c"
  "${DECOMP_SRC}/src/battle/report/data/*.c")
list(FILTER DW3_STFGTREP_SOURCES EXCLUDE REGEX "\\.inc\\.c$")
list(LENGTH DW3_STFGTREP_SOURCES _dw3_stfgtrep_count)
if(_dw3_stfgtrep_count EQUAL 0)
  message(FATAL_ERROR "no STFGTREP sources under ${DECOMP_SRC}/src/battle/report")
endif()

dw3_compile_c_objects(DW3_FIGHT_OBJECTS ${DW3_FIGHT_SOURCES})
dw3_compile_c_objects(DW3_WFIGHTMN_OBJECTS ${DW3_WFIGHTMN_SOURCES})
dw3_compile_c_objects(DW3_STFGTREP_OBJECTS ${DW3_STFGTREP_SOURCES})

add_custom_command(
  OUTPUT "${DW3_HOST_OVL_DIR}/fightstg.host.o"
  COMMAND ${CMAKE_COMMAND} -E env PYTHONUNBUFFERED=1
    "${CMAKE_COMMAND}" -E rm -f "${DW3_HOST_OVL_DIR}/fightstg.host.o"
  COMMAND ${DW3_PACK_ENV} python3 "${RENAME_OVL_SECTIONS}" fightstg
    "${DW3_HOST_OVL_DIR}/fightstg.host.o"
    ${DW3_FIGHT_OBJECTS}
  DEPENDS ${DW3_FIGHT_OBJECTS} "${RENAME_OVL_SECTIONS}"
  COMMENT "P3.3 pack FIGHTSTG host overlay object"
  VERBATIM
)
add_custom_command(
  OUTPUT "${DW3_HOST_OVL_DIR}/wfightmn.host.o"
  COMMAND ${DW3_PACK_ENV} python3 "${RENAME_OVL_SECTIONS}" wfightmn
    "${DW3_HOST_OVL_DIR}/wfightmn.host.o"
    ${DW3_WFIGHTMN_OBJECTS}
  DEPENDS ${DW3_WFIGHTMN_OBJECTS} "${RENAME_OVL_SECTIONS}"
  COMMENT "P3.3 pack WFIGHTMN host overlay object"
  VERBATIM
)
add_custom_command(
  OUTPUT "${DW3_HOST_OVL_DIR}/stfgtrep.host.o"
  COMMAND ${DW3_PACK_ENV} python3 "${RENAME_OVL_SECTIONS}" stfgtrep
    "${DW3_HOST_OVL_DIR}/stfgtrep.host.o"
    ${DW3_STFGTREP_OBJECTS}
  DEPENDS ${DW3_STFGTREP_OBJECTS} "${RENAME_OVL_SECTIONS}"
  COMMENT "P3.3 pack STFGTREP host overlay object"
  VERBATIM
)

set(DW3_P33_HOST_OBJECTS
  "${DW3_HOST_OVL_DIR}/fightstg.host.o"
  "${DW3_HOST_OVL_DIR}/wfightmn.host.o"
  "${DW3_HOST_OVL_DIR}/stfgtrep.host.o"
)
set_source_files_properties(${DW3_P33_HOST_OBJECTS} PROPERTIES
  EXTERNAL_OBJECT TRUE
  GENERATED TRUE
)
add_custom_target(dw3_p33_battle_objs DEPENDS ${DW3_P33_HOST_OBJECTS})

add_library(dw3_p33_battle STATIC ${DW3_P33_HOST_OBJECTS})
set_target_properties(dw3_p33_battle PROPERTIES
  LINKER_LANGUAGE C
)
add_dependencies(dw3_p33_battle dw3_p33_battle_objs)

# ---------------------------------------------------------------------------
# P3.4: menu / shop overlays with named .data/.bss restore.
# STPLNMET is already packed with P3.2 field objs.
# ---------------------------------------------------------------------------
function(dw3_host_menu_overlay name)
  set(_srcs ${ARGN})
  list(LENGTH _srcs _n)
  if(_n EQUAL 0)
    message(FATAL_ERROR "dw3_host_menu_overlay(${name}): no sources")
  endif()
  dw3_compile_c_objects(_objs ${_srcs})
  add_custom_command(
    OUTPUT "${DW3_HOST_OVL_DIR}/${name}.host.o"
    COMMAND ${CMAKE_COMMAND} -E env PYTHONUNBUFFERED=1
      "${CMAKE_COMMAND}" -E rm -f "${DW3_HOST_OVL_DIR}/${name}.host.o"
    COMMAND ${DW3_PACK_ENV} python3 "${RENAME_OVL_SECTIONS}" "${name}"
      "${DW3_HOST_OVL_DIR}/${name}.host.o"
      ${_objs}
    DEPENDS ${_objs} "${RENAME_OVL_SECTIONS}"
    COMMENT "P3.4 pack ${name} host overlay object"
    VERBATIM
  )
endfunction()

file(GLOB DW3_STSTATUS_SOURCES CONFIGURE_DEPENDS
  "${DECOMP_SRC}/src/menus/field_menu/*.c")
list(FILTER DW3_STSTATUS_SOURCES EXCLUDE REGEX "\\.inc\\.c$")

file(GLOB DW3_STITSHOP_SOURCES CONFIGURE_DEPENDS
  "${DECOMP_SRC}/src/menus/item_shop/*.c"
  "${DECOMP_SRC}/src/menus/item_shop/data/*.c")
list(FILTER DW3_STITSHOP_SOURCES EXCLUDE REGEX "\\.inc\\.c$")

file(GLOB DW3_STGTRAIN_SOURCES CONFIGURE_DEPENDS
  "${DECOMP_SRC}/src/menus/gym/*.c")
list(FILTER DW3_STGTRAIN_SOURCES EXCLUDE REGEX "\\.inc\\.c$")

file(GLOB DW3_STGDGLAB_SOURCES CONFIGURE_DEPENDS
  "${DECOMP_SRC}/src/menus/digimon_lab/*.c")
list(FILTER DW3_STGDGLAB_SOURCES EXCLUDE REGEX "\\.inc\\.c$")

file(GLOB DW3_STGMCARD_SOURCES CONFIGURE_DEPENDS
  "${DECOMP_SRC}/src/menus/memory_card/*.c"
  "${DECOMP_SRC}/src/menus/memory_card/data/*.c")
list(FILTER DW3_STGMCARD_SOURCES EXCLUDE REGEX "\\.inc\\.c$")

file(GLOB DW3_STDGNAME_SOURCES CONFIGURE_DEPENDS
  "${DECOMP_SRC}/src/menus/partner_name/*.c"
  "${DECOMP_SRC}/src/menus/partner_name/data/*.c")
list(FILTER DW3_STDGNAME_SOURCES EXCLUDE REGEX "\\.inc\\.c$")

dw3_host_menu_overlay(ststatus ${DW3_STSTATUS_SOURCES})
dw3_host_menu_overlay(stitshop ${DW3_STITSHOP_SOURCES})
dw3_host_menu_overlay(stgtrain ${DW3_STGTRAIN_SOURCES})
dw3_host_menu_overlay(stgdglab ${DW3_STGDGLAB_SOURCES})
dw3_host_menu_overlay(stgmcard ${DW3_STGMCARD_SOURCES})
dw3_host_menu_overlay(stdgname ${DW3_STDGNAME_SOURCES})

set(DW3_P34_HOST_OBJECTS
  "${DW3_HOST_OVL_DIR}/ststatus.host.o"
  "${DW3_HOST_OVL_DIR}/stitshop.host.o"
  "${DW3_HOST_OVL_DIR}/stgtrain.host.o"
  "${DW3_HOST_OVL_DIR}/stgdglab.host.o"
  "${DW3_HOST_OVL_DIR}/stgmcard.host.o"
  "${DW3_HOST_OVL_DIR}/stdgname.host.o"
)
set_source_files_properties(${DW3_P34_HOST_OBJECTS} PROPERTIES
  EXTERNAL_OBJECT TRUE
  GENERATED TRUE
)
add_custom_target(dw3_p34_menu_objs DEPENDS ${DW3_P34_HOST_OBJECTS})

add_library(dw3_p34_menus STATIC ${DW3_P34_HOST_OBJECTS})
set_target_properties(dw3_p34_menus PROPERTIES
  LINKER_LANGUAGE C
)
add_dependencies(dw3_p34_menus dw3_p34_menu_objs)

# ---------------------------------------------------------------------------
# P3.5: CARDGAME + STCRDDEK + STCRDSHP + STCRDABM with named .data/.bss restore.
# Reuses dw3_host_menu_overlay (same ld -r / section rename packing).
# ---------------------------------------------------------------------------
file(GLOB DW3_CARDGAME_SOURCES CONFIGURE_DEPENDS
  "${DECOMP_SRC}/src/cardgame/card_battle/*.c"
  "${DECOMP_SRC}/src/cardgame/card_battle/data/*.c")
list(FILTER DW3_CARDGAME_SOURCES EXCLUDE REGEX "\\.inc\\.c$")

file(GLOB DW3_STCRDDEK_SOURCES CONFIGURE_DEPENDS
  "${DECOMP_SRC}/src/cardgame/deck_editor/*.c"
  "${DECOMP_SRC}/src/cardgame/deck_editor/data/*.c")
list(FILTER DW3_STCRDDEK_SOURCES EXCLUDE REGEX "\\.inc\\.c$")

file(GLOB DW3_STCRDSHP_SOURCES CONFIGURE_DEPENDS
  "${DECOMP_SRC}/src/cardgame/card_packs/*.c")
list(FILTER DW3_STCRDSHP_SOURCES EXCLUDE REGEX "\\.inc\\.c$")

file(GLOB DW3_STCRDABM_SOURCES CONFIGURE_DEPENDS
  "${DECOMP_SRC}/src/cardgame/album/*.c")
list(FILTER DW3_STCRDABM_SOURCES EXCLUDE REGEX "\\.inc\\.c$")

list(LENGTH DW3_CARDGAME_SOURCES _dw3_cardgame_count)
list(LENGTH DW3_STCRDDEK_SOURCES _dw3_stcrddek_count)
list(LENGTH DW3_STCRDSHP_SOURCES _dw3_stcrdshp_count)
list(LENGTH DW3_STCRDABM_SOURCES _dw3_stcrdabm_count)
if(_dw3_cardgame_count EQUAL 0 OR _dw3_stcrddek_count EQUAL 0 OR
   _dw3_stcrdshp_count EQUAL 0 OR _dw3_stcrdabm_count EQUAL 0)
  message(FATAL_ERROR
    "P3.5 cardgame sources missing under ${DECOMP_SRC}/src/cardgame "
    "(cardgame=${_dw3_cardgame_count} dek=${_dw3_stcrddek_count} "
    "shp=${_dw3_stcrdshp_count} abm=${_dw3_stcrdabm_count})")
endif()

dw3_host_menu_overlay(cardgame ${DW3_CARDGAME_SOURCES})
dw3_host_menu_overlay(stcrddek ${DW3_STCRDDEK_SOURCES})
dw3_host_menu_overlay(stcrdshp ${DW3_STCRDSHP_SOURCES})
dw3_host_menu_overlay(stcrdabm ${DW3_STCRDABM_SOURCES})

set(DW3_P35_HOST_OBJECTS
  "${DW3_HOST_OVL_DIR}/cardgame.host.o"
  "${DW3_HOST_OVL_DIR}/stcrddek.host.o"
  "${DW3_HOST_OVL_DIR}/stcrdshp.host.o"
  "${DW3_HOST_OVL_DIR}/stcrdabm.host.o"
)
set_source_files_properties(${DW3_P35_HOST_OBJECTS} PROPERTIES
  EXTERNAL_OBJECT TRUE
  GENERATED TRUE
)
add_custom_target(dw3_p35_cardgame_objs DEPENDS ${DW3_P35_HOST_OBJECTS})

add_library(dw3_p35_cardgame STATIC ${DW3_P35_HOST_OBJECTS})
set_target_properties(dw3_p35_cardgame PROPERTIES
  LINKER_LANGUAGE C
)
add_dependencies(dw3_p35_cardgame dw3_p35_cardgame_objs)

# ---------------------------------------------------------------------------
# P3.8: STAGSLCT debug stage select with named .data/.bss restore.
# ---------------------------------------------------------------------------
file(GLOB DW3_STAGSLCT_SOURCES CONFIGURE_DEPENDS
  "${DECOMP_SRC}/src/debug/stage_select/*.c")
list(FILTER DW3_STAGSLCT_SOURCES EXCLUDE REGEX "\\.inc\\.c$")
list(LENGTH DW3_STAGSLCT_SOURCES _dw3_stagslct_count)
if(_dw3_stagslct_count EQUAL 0)
  message(FATAL_ERROR "P3.8 STAGSLCT sources missing under ${DECOMP_SRC}/src/debug/stage_select")
endif()

dw3_host_menu_overlay(stagslct ${DW3_STAGSLCT_SOURCES})

set(DW3_P38_HOST_OBJECTS
  "${DW3_HOST_OVL_DIR}/stagslct.host.o"
)
set_source_files_properties(${DW3_P38_HOST_OBJECTS} PROPERTIES
  EXTERNAL_OBJECT TRUE
  GENERATED TRUE
)
add_custom_target(dw3_p38_stagslct_objs DEPENDS ${DW3_P38_HOST_OBJECTS})

add_library(dw3_p38_stagslct STATIC ${DW3_P38_HOST_OBJECTS})
set_target_properties(dw3_p38_stagslct PROPERTIES
  LINKER_LANGUAGE C
)
add_dependencies(dw3_p38_stagslct dw3_p38_stagslct_objs)

# ---------------------------------------------------------------------------
# P6.2: CNTY_SEL country / language select with named .data/.bss restore.
# The EU build boots into it (patch 0055) and it sets LANGUAGE; the US build
# links it too (the source builds for both) so STAGSLCT's entry still works.
# ---------------------------------------------------------------------------
file(GLOB DW3_CNTY_SEL_SOURCES CONFIGURE_DEPENDS
  "${DECOMP_SRC}/src/menus/country_select/*.c")
list(FILTER DW3_CNTY_SEL_SOURCES EXCLUDE REGEX "\\.inc\\.c$")
list(LENGTH DW3_CNTY_SEL_SOURCES _dw3_cnty_sel_count)
if(_dw3_cnty_sel_count EQUAL 0)
  message(FATAL_ERROR "P6.2 CNTY_SEL sources missing under ${DECOMP_SRC}/src/menus/country_select")
endif()

dw3_host_menu_overlay(cnty_sel ${DW3_CNTY_SEL_SOURCES})

set(DW3_P62_HOST_OBJECTS
  "${DW3_HOST_OVL_DIR}/cnty_sel.host.o"
)
set_source_files_properties(${DW3_P62_HOST_OBJECTS} PROPERTIES
  EXTERNAL_OBJECT TRUE
  GENERATED TRUE
)
add_custom_target(dw3_p62_cnty_sel_objs DEPENDS ${DW3_P62_HOST_OBJECTS})

add_library(dw3_p62_cnty_sel STATIC ${DW3_P62_HOST_OBJECTS})
set_target_properties(dw3_p62_cnty_sel PROPERTIES
  LINKER_LANGUAGE C
)
add_dependencies(dw3_p62_cnty_sel dw3_p62_cnty_sel_objs)

# Archive for the PC host: engine + title + port glue (field/battle/menu objs separate).
add_library(dw3_engine_objs STATIC
  $<TARGET_OBJECTS:dw3_engine>
  $<TARGET_OBJECTS:dw3_title>
  $<TARGET_OBJECTS:dw3_host_overlays>
  $<TARGET_OBJECTS:dw3_cdfs>
  $<TARGET_OBJECTS:dw3_memcard>
  $<TARGET_OBJECTS:dw3_psyz_missing>
  $<TARGET_OBJECTS:dw3_movies>
  $<TARGET_OBJECTS:dw3_port_options>
)
set_target_properties(dw3_engine_objs PROPERTIES
  OUTPUT_NAME dw3_engine
  LINKER_LANGUAGE C
)
add_dependencies(dw3_engine_objs
  dw3_engine dw3_title dw3_host_overlays dw3_cdfs dw3_memcard dw3_psyz_missing
  dw3_movies dw3_port_options)

