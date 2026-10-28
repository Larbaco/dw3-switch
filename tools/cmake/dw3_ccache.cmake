# Compiler cache for the PC and Switch builds (docs/BUILD_SPEED.md).
#
# include() this BEFORE project(). DW3_CCACHE is AUTO (default: use ccache when
# it is on PATH), ON (fail when it is missing) or OFF. The environment variable
# DW3_NO_CCACHE=1 also turns it off. The cache location and the path rewriting
# are set by tools/dw3_ccache_env.sh (CCACHE_DIR, CCACHE_BASEDIR, ...), not
# here, so a plain `cmake` run with a user's own ccache config still works.
# ccache never changes the compiler's output: a hit returns the object the same
# command line produced.
set(DW3_CCACHE "AUTO" CACHE STRING "Use ccache as compiler launcher: AUTO, ON or OFF")
set_property(CACHE DW3_CCACHE PROPERTY STRINGS AUTO ON OFF)
if("$ENV{DW3_NO_CCACHE}" STREQUAL "" AND NOT DW3_CCACHE STREQUAL "OFF")
  find_program(DW3_CCACHE_PROGRAM ccache)
  if(DW3_CCACHE_PROGRAM)
    set(CMAKE_C_COMPILER_LAUNCHER "${DW3_CCACHE_PROGRAM}")
    set(CMAKE_CXX_COMPILER_LAUNCHER "${DW3_CCACHE_PROGRAM}")
    message(STATUS "ccache: ${DW3_CCACHE_PROGRAM}")
  elseif(DW3_CCACHE STREQUAL "ON")
    message(FATAL_ERROR "DW3_CCACHE=ON but ccache is not on PATH")
  endif()
endif()
