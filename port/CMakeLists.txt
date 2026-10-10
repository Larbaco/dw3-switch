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

# Archive for the PC host: engine + title + port glue (host main is separate).
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

