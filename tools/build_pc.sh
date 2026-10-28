#!/usr/bin/env bash
# Build the PC host (dw3-pc) with the flags this desktop uses, ccache and nice.
#   tools/build_pc.sh              US     -> build/pc/dw3-pc
#   tools/build_pc.sh --eu         EU     -> build/pc-eu/dw3-pc
#   tools/build_pc.sh --both       US+EU in one image -> build/pc-both/dw3-pc
#   DW3_BUILD_JOBS=8 (default 8), DW3_NO_CCACHE=1 to bypass the cache.
# These are the directories tools/replay_tests.sh looks in. The SDL options
# match docs/P3_EU_BUILD.md (no XScrnSaver / audio dev packages needed).
# See docs/BUILD_SPEED.md.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
# shellcheck source=tools/dw3_ccache_env.sh
source "${ROOT}/tools/dw3_ccache_env.sh"

VERSION=us
DIR="${ROOT}/build/pc"
case "${1:-}" in
  "") ;;
  --eu) VERSION=eu; DIR="${ROOT}/build/pc-eu" ;;
  --both) VERSION=both; DIR="${ROOT}/build/pc-both" ;;
  -h|--help) sed -n '2,9p' "$0"; exit 0 ;;
  *) echo "usage: $0 [--eu|--both]" >&2; exit 2 ;;
esac

dw3_ccache_setup "${ROOT}"
[[ -n "${DW3_CCACHE_BIN}" ]] && echo "ccache: ${DW3_CCACHE_BIN} (cache ${CCACHE_DIR})" \
  || echo "ccache: off (install ccache or run tools/get_ccache.sh)"

nice -n 10 cmake -S "${ROOT}/platforms/pc" -B "${DIR}" -G Ninja \
  -DDW3_VERSION="${VERSION}" -DPSYZ_RENDERER=sdl3_gpu \
  -DSDL_X11_XSCRNSAVER=OFF -DSDL_ALSA=OFF -DSDL_JACK=OFF \
  -DSDL_PIPEWIRE=OFF -DSDL_PULSEAUDIO=OFF -DSDL_SNDIO=OFF
nice -n 10 cmake --build "${DIR}" -j"${DW3_BUILD_JOBS:-8}" --target dw3-pc
[[ -z "${DW3_CCACHE_BIN}" ]] || "${DW3_CCACHE_BIN}" -s | sed -n '1,6p'
echo "built ${DIR}/dw3-pc"
