cmake_minimum_required(VERSION 3.21)
project(dw3-pc C)

set(CMAKE_C_STANDARD 11)
set(PSYZ_RENDERER "sdl3_gpu" CACHE STRING "PSY-Z renderer")

add_subdirectory(${CMAKE_SOURCE_DIR}/../../vendor/psyz/psyz ${CMAKE_BINARY_DIR}/psyz)
add_subdirectory(${CMAKE_SOURCE_DIR}/../../port ${CMAKE_BINARY_DIR}/port)

add_executable(dw3-pc src/main.c)
target_link_libraries(dw3-pc PRIVATE psyz)
# Build engine objects with the PC host; do not link them yet (engine has its own main).
add_dependencies(dw3-pc dw3_engine_objs)
target_include_directories(psyz PRIVATE ${CMAKE_SOURCE_DIR}/../../vendor/psyz/psyz/include)
target_compile_definitions(psyz PRIVATE __psyz)
psyz_title(dw3-pc "dw3-pc Phase 1")
