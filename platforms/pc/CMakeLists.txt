cmake_minimum_required(VERSION 3.21)
project(dw3-pc C)

set(CMAKE_C_STANDARD 11)
set(PSYZ_RENDERER "sdl3_gpu" CACHE STRING "PSY-Z renderer")

add_subdirectory(${CMAKE_SOURCE_DIR}/../../vendor/psyz/psyz ${CMAKE_BINARY_DIR}/psyz)
add_subdirectory(${CMAKE_SOURCE_DIR}/../../port ${CMAKE_BINARY_DIR}/port)

add_executable(dw3-pc
  src/main.c
  src/psyz_host_overrides.c
  # Direct TU so PadGetState / PadInitDirect win over PSY-Z stubs in libpsyz.a
  ${CMAKE_SOURCE_DIR}/../../port/psyz_missing/psyz_pad_host.c
)
target_include_directories(dw3-pc PRIVATE
  ${CMAKE_SOURCE_DIR}/../../port/cdfs
  ${CMAKE_SOURCE_DIR}/../../port/memcard
  ${CMAKE_SOURCE_DIR}/../../port/overlays
  ${CMAKE_SOURCE_DIR}/../../port/include
  ${CMAKE_SOURCE_DIR}/../../vendor/psyz/psyz/include
)
# Host overrides must appear before psyz so SsSetTableSize / pad APIs win.
target_link_libraries(dw3-pc PRIVATE dw3_engine_objs psyz)
target_link_options(dw3-pc PRIVATE
  "-Wl,--wrap=_card_read"
  "-Wl,--wrap=_card_write"
  # Override PSY-Z libetc stubs (same .o as PadRead pulls them in).
  "-Wl,--wrap=PadGetState"
  "-Wl,--wrap=PadStartCom"
  "-Wl,--wrap=PadInitMtap"
  "-Wl,--wrap=PadSetAct"
  "-Wl,--wrap=PadSetActAlign"
)
target_include_directories(psyz PRIVATE ${CMAKE_SOURCE_DIR}/../../vendor/psyz/psyz/include)
target_compile_definitions(psyz PRIVATE __psyz)
psyz_title(dw3-pc "dw3-pc Phase 2")
