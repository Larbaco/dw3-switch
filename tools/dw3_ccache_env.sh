# Source this file; it only defines functions (docs/BUILD_SPEED.md).
#   source tools/dw3_ccache_env.sh
#   dw3_ccache_setup "$ROOT"      # exports CCACHE_*, sets DW3_CCACHE_BIN
#
# After the call DW3_CCACHE_BIN is the ccache to use, or empty when ccache is
# off (DW3_NO_CCACHE=1) or missing. The cache is shared by every worktree of
# the user: DW3_CCACHE_DIR, default ~/.cache/dw3-switch/ccache. Two worktrees
# hit each other's objects when the build directory is a real directory inside
# the worktree (the /mnt/cabal/tmp/dw3-<topic> layout of AGENTS.md): with
# CCACHE_BASEDIR=<worktree> every path in the command line becomes relative to
# the build directory, so it no longer names the worktree.
dw3_ccache_setup() {
  local root="$1" dir bin="" cand
  DW3_CCACHE_BIN=""
  [[ -z "${DW3_NO_CCACHE:-}" ]] || return 0
  dir="${DW3_CCACHE_DIR:-${XDG_CACHE_HOME:-$HOME/.cache}/dw3-switch/ccache}"
  for cand in "${DW3_CCACHE_BIN_PATH:-}" "$(command -v ccache || true)" \
              "${XDG_CACHE_HOME:-$HOME/.cache}/dw3-switch/bin/ccache"; do
    if [[ -n "$cand" && -x "$cand" ]]; then bin="$cand"; break; fi
  done
  if [[ -z "$bin" && "${DW3_CCACHE_FETCH:-0}" == "1" ]]; then
    bin="$("$root/tools/get_ccache.sh")" || bin=""
  fi
  [[ -n "$bin" ]] || return 0
  mkdir -p "$dir"
  DW3_CCACHE_BIN="$bin"
  export CCACHE_DIR="${CCACHE_DIR:-$dir}"
  export CCACHE_BASEDIR="${CCACHE_BASEDIR:-$root}"
  export CCACHE_NOHASHDIR="${CCACHE_NOHASHDIR:-1}"
  # SDL3 builds with a precompiled header (cmake_pch.h).
  export CCACHE_SLOPPINESS="${CCACHE_SLOPPINESS:-pch_defines,time_macros}"
  export CCACHE_MAXSIZE="${CCACHE_MAXSIZE:-10G}"
  export CCACHE_COMPRESS="${CCACHE_COMPRESS:-1}"
}

# True when the binary needs no shared libraries (safe to mount in a container).
dw3_ccache_is_static() {
  local out
  out="$(ldd "$1" 2>&1 || true)"
  [[ "$out" == *"not a dynamic executable"* || "$out" == *"statically linked"* ]]
}
