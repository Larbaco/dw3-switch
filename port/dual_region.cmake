# P6.24: DW3_VERSION=both, the shared part of the platform CMakeLists.
#
#   include(<repo>/port/dual_region.cmake)
#   dw3_add_both_game(<repo>/port)       # after psyz, DW3_CHEATS and friends
#
# Adds port/ three times: the host part once (targets dw3_cdfs, dw3_memcard,
# dw3_movies, dw3_psyz_missing, dw3_region_rt, the archive dw3_host_objs) and
# the game part once per region (every game target gets the suffix _us / _eu,
# e.g. dw3_port_options_eu). Each game part ends in a custom target
# dw3_game_copy_<region> that makes one object, game_<region>.o, whose only
# global symbol is <region>_dw3_game_api (tools/localize_region_obj.py).
#
# Results, in the caller's scope:
#   DW3_GAME_COPY_O_us / DW3_GAME_COPY_O_eu   the two objects to link
#   DW3_GAME_COPY_TARGETS                     targets that make them
#   DW3_REGION_SUFFIXES                       _us;_eu, to loop over per-region targets
#
# The platform then links the objects with dw3_host_objs and PSY-Z, and sets
# DW3_BOTH=1 (and no VERSION_US / VERSION_EU) on its own sources so that they
# use dw3_region() and the dw3_game_api table (port/region/dw3_game_api.h).

function(dw3_add_both_game port_dir)
  set(DW3_PORT_PART host)
  add_subdirectory("${port_dir}" "${CMAKE_BINARY_DIR}/port-host")
  foreach(_r IN ITEMS us eu)
    set(DW3_PORT_PART game)
    set(DW3_REGION_BUILD ${_r})
    add_subdirectory("${port_dir}" "${CMAKE_BINARY_DIR}/port-${_r}")
  endforeach()
  set(DW3_GAME_COPY_O_us "${CMAKE_BINARY_DIR}/port-us/game_us.o" PARENT_SCOPE)
  set(DW3_GAME_COPY_O_eu "${CMAKE_BINARY_DIR}/port-eu/game_eu.o" PARENT_SCOPE)
  set(DW3_GAME_COPY_TARGETS dw3_game_copy_us dw3_game_copy_eu PARENT_SCOPE)
  set(DW3_REGION_SUFFIXES _us _eu PARENT_SCOPE)
endfunction()
