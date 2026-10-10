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
  "${CMAKE_CURRENT_SOURCE_DIR}/memcard/psyz_card_wrap.c"
)

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

set(DW3_STAGE_STUBS_C "${DW3_HOST_OVL_DIR}/stage_stubs.c")
set(DW3_STAGE_STUBS_O "${DW3_HOST_OVL_DIR}/stage_stubs.o")
add_custom_command(
  OUTPUT "${DW3_STAGE_STUBS_C}"
  COMMAND python3 "${GEN_STAGE_STUBS}"
    --stages-h "${DECOMP_SRC}/include/field/stages.h"
    --exclude WSTAG260,WSTAG780,WSTAG785,WSTAG790
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
# P3.3: FIGHTSTG + WFIGHTMN with named .data/.bss for static overlay restore.
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

dw3_compile_c_objects(DW3_FIGHT_OBJECTS ${DW3_FIGHT_SOURCES})
dw3_compile_c_objects(DW3_WFIGHTMN_OBJECTS ${DW3_WFIGHTMN_SOURCES})

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

set(DW3_P33_HOST_OBJECTS
  "${DW3_HOST_OVL_DIR}/fightstg.host.o"
  "${DW3_HOST_OVL_DIR}/wfightmn.host.o"
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

# Archive for the PC host: engine + title + port glue (field/battle objs separate).
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

