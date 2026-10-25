# P6.24 slice 1: the PC front end of the single build (docs/SINGLE_BUILD.md,
# "As built"). A standalone project on purpose: it links no game code and no
# PSY-Z, and the existing US / EU builds do not know it exists.
#   cmake -S platforms/pc/launcher -B build/pc-launcher && cmake --build build/pc-launcher
# tools/build_pc_single.sh builds it next to the two game binaries.
cmake_minimum_required(VERSION 3.21)
project(dw3-pc-launcher C)

set(CMAKE_C_STANDARD 11)
set(DW3_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../../..")

add_executable(dw3-pc-launcher
  ../src/launcher_main.c
  ${DW3_ROOT}/port/region/dw3_region.c
  ${DW3_ROOT}/port/options/options_cfg.c
)
target_include_directories(dw3-pc-launcher PRIVATE
  ${DW3_ROOT}/port/region
  ${DW3_ROOT}/port/include
)
target_compile_options(dw3-pc-launcher PRIVATE -Wall -Wextra -Werror)
