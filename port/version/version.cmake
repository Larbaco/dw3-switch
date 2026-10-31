# P6.37: dw3_version_gen.h and port_version.c for a host target.
#
#   include(<repo>/port/version/version.cmake)
#   dw3_version_setup(dw3-pc)
#
# The header comes from tools/dw3_nro_version.py, the source of the NACP
# version too (CI: RELEASE_TAG / GITHUB_SHA through --auto). It is written at
# configure time, and by a custom target before every build of the target on a
# host that has Python, so a commit made since the last configure shows up;
# the script rewrites the file only when its text changed, so nothing
# recompiles otherwise.
#
# tools/build_switch_game.sh runs the script on the host (the devkitPro
# container may have no Python) and passes -DDW3_VERSION_GEN_DIR=<dir> with the
# header already written; then CMake only adds the include path.
function(dw3_version_setup target)
  get_filename_component(_root "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/../.." ABSOLUTE)
  set(_script "${_root}/tools/dw3_nro_version.py")
  if(DW3_VERSION_GEN_DIR)
    set(_gen "${DW3_VERSION_GEN_DIR}")
  else()
    set(_gen "${CMAKE_BINARY_DIR}/dw3_version_gen")
    file(MAKE_DIRECTORY "${_gen}")
    find_package(Python3 COMPONENTS Interpreter QUIET)
    set(_cmd "${Python3_EXECUTABLE}" "${_script}" --repo "${_root}"
        --auto --format label --write-header "${_gen}/dw3_version_gen.h")
    if(Python3_Interpreter_FOUND AND EXISTS "${_script}")
      execute_process(COMMAND ${_cmd}
        OUTPUT_VARIABLE _label OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET RESULT_VARIABLE _rc)
      if(_rc EQUAL 0)
        message(STATUS "port version: ${_label}")
      endif()
      add_custom_target(${target}_version_gen
        COMMAND ${_cmd}
        BYPRODUCTS "${_gen}/dw3_version_gen.h"
        COMMENT "port version (dw3_version_gen.h)"
        VERBATIM)
      add_dependencies(${target} ${target}_version_gen)
    endif()
    if(NOT EXISTS "${_gen}/dw3_version_gen.h")
      file(WRITE "${_gen}/dw3_version_gen.h"
"#ifndef DW3_VERSION_GEN_H
#define DW3_VERSION_GEN_H
#define DW3_VERSION_RAW \"unknown\"
#define DW3_VERSION_LABEL \"unknown\"
#define DW3_VERSION_SHA \"\"
#define DW3_VERSION_BUILD_DATE \"unknown\"
#endif
")
    endif()
  endif()
  target_sources(${target} PRIVATE "${_root}/port/version/port_version.c")
  target_include_directories(${target} PRIVATE "${_gen}")
endfunction()
