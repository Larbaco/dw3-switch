#!/usr/bin/env bash
# P6.24 slice 2: build the Switch region launcher NRO (platforms/switch/launcher)
# in the same devkitpro/devkita64 image as tools/build_switch_game.sh.
#
#   tools/build_switch_launcher.sh            -> build/switch-launcher/out/dw3.nro
#   DW3_LAUNCHER_BUILD_DIR=/mnt/cabal/tmp/x tools/build_switch_launcher.sh
#
# No submodules are needed (no game code, no PSY-Z). The few files it uses are
# copied into <build dir>/src first, so an NTFS checkout never has to hold
# build output. DW3_BUILD_JOBS=N limits make's jobs (default 4: five files).
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
# Pinned by digest (= tag 20260219, devkitA64 r29.2, gcc 15.2); see docs/DEPENDENCIES.md.
IMAGE="${DW3_DK_IMAGE:-devkitpro/devkita64@sha256:1fc388c3a0d34bd2045a6dadcb1020e069d5f876a187fd705de14b4440c00282}"
BUILD_DIR="${DW3_LAUNCHER_BUILD_DIR:-${ROOT}/build/switch-launcher}"
SRC="${BUILD_DIR}/src"
OUT="${BUILD_DIR}/out"

if ! command -v docker >/dev/null 2>&1; then
  echo "docker is required (devkitpro/devkita64). This VM has no daemon; CI does." >&2
  exit 1
fi

# The container runs as root (user namespaces map it to an id the host user
# does not own), so it leaves the tree world-writable and a later run can
# clean it; the fallback below covers an older tree.
if ! rm -rf "${SRC}" "${OUT}" 2>/dev/null; then
  docker run --rm -v "${BUILD_DIR}:${BUILD_DIR}" "${IMAGE}" rm -rf "${SRC}" "${OUT}"
fi
mkdir -p "${SRC}/platforms/switch" "${SRC}/port/region" "${SRC}/port/options" \
         "${SRC}/port/include" "${OUT}"
cp -r "${ROOT}/platforms/switch/launcher" "${SRC}/platforms/switch/launcher"
cp "${ROOT}/port/region/"*.c "${ROOT}/port/region/"*.h "${SRC}/port/region/"
cp "${ROOT}/port/options/options_cfg.c" "${SRC}/port/options/"
cp "${ROOT}/port/include/port_options_cfg.h" "${SRC}/port/include/"

docker run --rm \
  -e DEVKITPRO=/opt/devkitpro \
  -e DW3_BUILD_JOBS="${DW3_BUILD_JOBS:-4}" \
  -e PATH="/opt/devkitpro/tools/bin:/opt/devkitpro/devkitA64/bin:/usr/bin:/bin" \
  -v "${BUILD_DIR}:${BUILD_DIR}" \
  -w "${SRC}/platforms/switch/launcher" \
  "${IMAGE}" \
  bash -c 'make -j"${DW3_BUILD_JOBS}" OUTDIR="'"${OUT}"'" && chmod -R a+rwX "'"${BUILD_DIR}"'"'

NRO="${OUT}/dw3.nro"
if [[ ! -f "${NRO}" ]]; then
  echo "missing ${NRO}" >&2
  exit 1
fi
# The launcher must not carry game code: it holds no runtime path of a game
# image and stays small (the game NRO is about 9 MB).
bytes="$(stat -c%s "${NRO}")"
if [[ "${bytes}" -gt 2000000 ]]; then
  echo "launcher .nro is ${bytes} bytes; expected well under 1 MB" >&2
  exit 1
fi
echo "launcher .nro ${NRO} (${bytes} bytes)"
