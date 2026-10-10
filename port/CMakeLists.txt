# US engine object library from the patched decomp copy (build/decomp-src).
# See docs/ROADMAP.md P2.1 and docs/superpowers/plans/2026-10-10-phase2-pc-bringup.md Task E2.

cmake_minimum_required(VERSION 3.21)

set(DW3_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/..")
set(DECOMP_SRC "${DW3_ROOT}/build/decomp-src")
set(PSYZ_INCLUDE "${DW3_ROOT}/vendor/psyz/psyz/include")
set(APPLY_PATCHES "${DW3_ROOT}/tools/apply_patches.sh")

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
  "${CMAKE_CURRENT_SOURCE_DIR}/overlays/overlay_table_us.c"
  "${CMAKE_CURRENT_SOURCE_DIR}/overlays/mode_stubs.c"
  "${CMAKE_CURRENT_SOURCE_DIR}/overlays/boot_tim_host.c"
)

# Psy-Q APIs missing from the pinned PSY-Z build (title bring-up).
add_library(dw3_psyz_missing OBJECT
  "${CMAKE_CURRENT_SOURCE_DIR}/psyz_missing/psyz_missing.c"
)

# CD → filesystem file-id reads (P2.5) and 128 KiB .mcr slots (P2.7).
add_library(dw3_cdfs OBJECT
  "${CMAKE_CURRENT_SOURCE_DIR}/cdfs/cdfs.c"
)
add_library(dw3_memcard OBJECT
  "${CMAKE_CURRENT_SOURCE_DIR}/memcard/memcard_host.c"
  "${CMAKE_CURRENT_SOURCE_DIR}/memcard/memcard_libmcrd.c"
  "${CMAKE_CURRENT_SOURCE_DIR}/memcard/psyz_card_wrap.c"
)
target_include_directories(dw3_memcard PRIVATE
  "${PSYZ_INCLUDE}"
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
  VERSION_US=1
  PORT_HOST=1
  SKIP_ASM=1
  __psyz
)
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
list(FILTER DW3_P31_SOURCES EXCLUDE REGEX
  "/wstag9(2[0-9]|3[0-9]|4[0-9]|5[0-9]|6[0-9]|7[0-4])\\.c$")
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
  COMMAND python3 "${RENAME_OVL_SECTIONS}" fieldstg
    "${DW3_HOST_OVL_DIR}/fieldstg.host.o"
    ${DW3_FIELD_OBJECTS}
  DEPENDS ${DW3_FIELD_OBJECTS} "${RENAME_OVL_SECTIONS}"
  COMMENT "P3.2 pack FIELDSTG host overlay object"
  VERBATIM
)
add_custom_command(
  OUTPUT "${DW3_HOST_OVL_DIR}/stplnmet.host.o"
  COMMAND python3 "${RENAME_OVL_SECTIONS}" stplnmet
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
    COMMAND python3 "${PREFIX_STAGE_OBJ}" "${stage}" "${_raw}" "${_host}"
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

set(DW3_LINKED_STAGES
  WSTAG260,WSTAG780,WSTAG785,WSTAG790,WSTAG200,WSTAG201,WSTAG206,WSTAG210,WSTAG212,WSTAG220,WSTAG225,WSTAG230,WSTAG235,WSTAG240,WSTAG245,WSTAG250,WSTAG255,WSTAG202,WSTAG203,WSTAG205,WSTAG218,WSTAG219,WSTAG232,WSTAG237,WSTAG270,WSTAG275,WSTAG280,WSTAG285,WSTAG290,WSTAG295,WSTAG300,WSTAG305,WSTAG310,WSTAG315,WSTAG320,WSTAG325,WSTAG211,WSTAG221,WSTAG226,WSTAG231,WSTAG233,WSTAG236,WSTAG238,WSTAG241,WSTAG246,WSTAG251,WSTAG256,WSTAG261,WSTAG271,WSTAG276,WSTAG281,WSTAG286,WSTAG291,WSTAG296,WSTAG301,WSTAG306,WSTAG311,WSTAG316,WSTAG321,WSTAG326,WSTAG330,WSTAG335,WSTAG340,WSTAG345,WSTAG350,WSTAG355,WSTAG360,WSTAG365,WSTAG370,WSTAG375,WSTAG380,WSTAG385,WSTAG395,WSTAG400,WSTAG405,WSTAG410,WSTAG415,WSTAG420,WSTAG425,WSTAG430,WSTAG435,WSTAG440,WSTAG445,WSTAG450,WSTAG455,WSTAG475,WSTAG465,WSTAG470
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
  COMMAND python3 "${RENAME_OVL_SECTIONS}" fightstg
    "${DW3_HOST_OVL_DIR}/fightstg.host.o"
    ${DW3_FIGHT_OBJECTS}
  DEPENDS ${DW3_FIGHT_OBJECTS} "${RENAME_OVL_SECTIONS}"
  COMMENT "P3.3 pack FIGHTSTG host overlay object"
  VERBATIM
)
add_custom_command(
  OUTPUT "${DW3_HOST_OVL_DIR}/wfightmn.host.o"
  COMMAND python3 "${RENAME_OVL_SECTIONS}" wfightmn
    "${DW3_HOST_OVL_DIR}/wfightmn.host.o"
    ${DW3_WFIGHTMN_OBJECTS}
  DEPENDS ${DW3_WFIGHTMN_OBJECTS} "${RENAME_OVL_SECTIONS}"
  COMMENT "P3.3 pack WFIGHTMN host overlay object"
  VERBATIM
)
add_custom_command(
  OUTPUT "${DW3_HOST_OVL_DIR}/stfgtrep.host.o"
  COMMAND python3 "${RENAME_OVL_SECTIONS}" stfgtrep
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
    COMMAND python3 "${RENAME_OVL_SECTIONS}" "${name}"
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

# Archive for the PC host: engine + title + port glue (field/battle/menu objs separate).
add_library(dw3_engine_objs STATIC
  $<TARGET_OBJECTS:dw3_engine>
  $<TARGET_OBJECTS:dw3_title>
  $<TARGET_OBJECTS:dw3_host_overlays>
  $<TARGET_OBJECTS:dw3_cdfs>
  $<TARGET_OBJECTS:dw3_memcard>
  $<TARGET_OBJECTS:dw3_psyz_missing>
)
set_target_properties(dw3_engine_objs PROPERTIES
  OUTPUT_NAME dw3_engine
  LINKER_LANGUAGE C
)
add_dependencies(dw3_engine_objs
  dw3_engine dw3_title dw3_host_overlays dw3_cdfs dw3_memcard dw3_psyz_missing)

