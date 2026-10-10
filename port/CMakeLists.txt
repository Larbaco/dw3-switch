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

# Host overlay address buffers (P2.3); restore semantics are E5 / P2.6.
add_library(dw3_host_overlays OBJECT
  "${CMAKE_CURRENT_SOURCE_DIR}/overlays/host_vram.c"
)

# Include order: port host → PSY-Z (Psy-Q names) → decomp headers/sources.
# port/include/common.h shadows decomp common.h so gnu89 does not redefine
# s8/u8 against PSY-Z's psyz/types.h.
target_include_directories(dw3_engine PRIVATE
  "${CMAKE_CURRENT_SOURCE_DIR}/include"
  "${PSYZ_INCLUDE}"
  "${DECOMP_SRC}/include"
  "${DECOMP_SRC}/src"
  "${DECOMP_SRC}/src/field/stages"
)

target_compile_definitions(dw3_engine PRIVATE
  VERSION_US=1
  PORT_HOST=1
  SKIP_ASM=1
  __psyz
)
target_compile_definitions(dw3_host_overlays PRIVATE
  PORT_HOST=1
)

set_target_properties(dw3_engine PROPERTIES
  C_STANDARD 90
  C_STANDARD_REQUIRED ON
  C_EXTENSIONS ON
)
set_target_properties(dw3_host_overlays PROPERTIES
  C_STANDARD 90
  C_STANDARD_REQUIRED ON
  C_EXTENSIONS ON
)

# Host-relevant subset of vendor/dw3_decomp/mk/toolchain/gcc.mk GCC_CFLAGS.
target_compile_options(dw3_engine PRIVATE
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
)

# Archive forces object compilation without linking into the PC stub (has its own main).
add_library(dw3_engine_objs STATIC
  $<TARGET_OBJECTS:dw3_engine>
  $<TARGET_OBJECTS:dw3_host_overlays>
)
set_target_properties(dw3_engine_objs PROPERTIES
  OUTPUT_NAME dw3_engine
  LINKER_LANGUAGE C
)
add_dependencies(dw3_engine_objs dw3_engine dw3_host_overlays)
