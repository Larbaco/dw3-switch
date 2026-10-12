#!/usr/bin/env bash
# Build the CMake Switch game image (source/boot_main.c -> dw3_game_main).
#
# This is not `make -C platforms/switch`. That Makefile links source/main.c
# (host_hello, EGL + audren) and is what CI job `switch` builds.
#
# GitHub-hosted runners have an ext4 checkout, so the container writes
# build/switch in place. On NTFS, use the volume overlay in
# platforms/switch/README.md instead of this script.
#
# Submodules, from a fresh worktree, before running:
#   git submodule update --init vendor/dw3_decomp vendor/psyz
#   git -C vendor/psyz submodule update --init nugget external/SDL
# Do not pass --recursive to the nugget init. codeberg.org returns HTTP 429
# for vendor/psyz/nugget/third_party/xmake-psx. Missing
# nugget/bsdec/bsdec.c fails configure as
# "No SOURCES given to target: dw3_movies".
#
# Disc version: default US. Pass --eu (or DW3_VERSION=eu) for Digimon World
# 2003. EU still needs a pre-packed sdmc:/switch/dw3/eu/ tree — boot extract
# from BIN is US-only (see docs/RUNTIME_LAYOUT.md).
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
IMAGE="${DW3_DK_IMAGE:-devkitpro/devkita64:latest}"
NXLINK_STDIO=OFF
DW3_VERSION="$(echo "${DW3_VERSION:-us}" | tr '[:upper:]' '[:lower:]')"

usage() {
  echo "usage: $0 [--eu] [--nxlink]"
  echo "  default   US game NRO at build/switch/dw3-switch.nro"
  echo "  --eu      EU game NRO at build/switch-eu/dw3-switch.nro"
  echo "  --nxlink  nxlink development NRO (build/switch[-eu]-nxlink/)"
}

while (( $# > 0 )); do
  case "$1" in
    --eu)
      DW3_VERSION=eu
      ;;
    --nxlink)
      NXLINK_STDIO=ON
      ;;
    -h|--help)
      usage
      exit 0
      ;;
    *)
      echo "unknown option: $1" >&2
      usage >&2
      exit 2
      ;;
  esac
  shift
done

if [[ "${DW3_VERSION}" != "us" && "${DW3_VERSION}" != "eu" ]]; then
  echo "DW3_VERSION must be us or eu (got '${DW3_VERSION}')" >&2
  exit 2
fi

if [[ "${DW3_VERSION}" == "eu" ]]; then
  if [[ "${NXLINK_STDIO}" == "ON" ]]; then
    BUILD_DIR="${ROOT}/build/switch-eu-nxlink"
    NRO_NAME="dw3-switch-nxlink"
  else
    BUILD_DIR="${ROOT}/build/switch-eu"
    NRO_NAME="dw3-switch"
  fi
  RUNTIME_PATH="sdmc:/switch/dw3/eu"
else
  if [[ "${NXLINK_STDIO}" == "ON" ]]; then
    BUILD_DIR="${ROOT}/build/switch-nxlink"
    NRO_NAME="dw3-switch-nxlink"
  else
    BUILD_DIR="${ROOT}/build/switch"
    NRO_NAME="dw3-switch"
  fi
  RUNTIME_PATH="sdmc:/switch/dw3/us"
fi
NRO="${BUILD_DIR}/${NRO_NAME}.nro"

if ! command -v docker >/dev/null 2>&1; then
  echo "docker is required (devkitpro/devkita64). This VM has no daemon; CI does." >&2
  exit 1
fi

if [[ ! -f "${ROOT}/vendor/psyz/nugget/bsdec/bsdec.c" ]]; then
  echo "missing vendor/psyz/nugget/bsdec/bsdec.c" >&2
  echo "git submodule update --init vendor/dw3_decomp vendor/psyz" >&2
  echo "git -C vendor/psyz submodule update --init nugget external/SDL" >&2
  echo "do not recursive-init nugget (codeberg xmake-psx HTTP 429)" >&2
  exit 1
fi

# Worktrees store their common gitdir outside the checkout. Mount it at the
# same absolute path so submodule gitlinks and apply_patches.sh still resolve.
GIT_MOUNT_ARGS=()
if [[ -f "${ROOT}/.git" ]]; then
  GIT_COMMON_DIR="$(git -C "${ROOT}" rev-parse --path-format=absolute --git-common-dir)"
  GIT_MOUNT_ARGS=(-v "${GIT_COMMON_DIR}:${GIT_COMMON_DIR}:ro")
fi

# No login shell: bash -l lets /etc/profile drop devkitA64/bin from PATH,
# and aarch64-none-elf-nm lives there. Mount at the host absolute path so
# worktree gitlinks and tools/apply_patches.sh still resolve.
docker run --rm \
  -e DEVKITPRO=/opt/devkitpro \
  -e DW3_SWITCH_BUILD_DIR="${BUILD_DIR}" \
  -e DW3_NXLINK_STDIO="${NXLINK_STDIO}" \
  -e DW3_VERSION="${DW3_VERSION}" \
  -e GIT_CONFIG_COUNT=1 \
  -e GIT_CONFIG_KEY_0=safe.directory \
  -e GIT_CONFIG_VALUE_0='*' \
  -e PATH="/opt/devkitpro/tools/bin:/opt/devkitpro/devkitA64/bin:/usr/bin:/bin" \
  -v "${ROOT}:${ROOT}" \
  "${GIT_MOUNT_ARGS[@]}" \
  -w "${ROOT}" \
  "${IMAGE}" \
  bash -c 'cmake -S platforms/switch -B "$DW3_SWITCH_BUILD_DIR" -G Ninja \
    -DCMAKE_TOOLCHAIN_FILE=/opt/devkitpro/cmake/Switch.cmake \
    -DCMAKE_BUILD_TYPE=Release \
    -DDW3_VERSION="$DW3_VERSION" \
    -DDW3_NXLINK_STDIO="$DW3_NXLINK_STDIO" && \
    cmake --build "$DW3_SWITCH_BUILD_DIR" -j"$(nproc)"'

if ! grep -F -x -q "DW3_NXLINK_STDIO:BOOL=${NXLINK_STDIO}" "${BUILD_DIR}/CMakeCache.txt"; then
  echo "CMake cache does not match requested nxlink mode (${NXLINK_STDIO})" >&2
  exit 1
fi
if ! grep -F -q "DW3_VERSION:STRING=${DW3_VERSION}" "${BUILD_DIR}/CMakeCache.txt"; then
  echo "CMake cache does not match requested DW3_VERSION (${DW3_VERSION})" >&2
  exit 1
fi

if [[ ! -f "${NRO}" ]]; then
  echo "missing ${NRO}" >&2
  exit 1
fi

# boot_main.c is the only TU that embeds this path. The Makefile sample
# does not compile that file, so a hit here means this is the game image.
if ! grep -a -F -q "${RUNTIME_PATH}" "${NRO}"; then
  echo "${NRO} does not contain the game runtime path (${RUNTIME_PATH}); refusing to treat it as the game" >&2
  exit 1
fi

bytes="$(stat -c%s "${NRO}")"
if [[ "${bytes}" -lt 6000000 ]]; then
  echo "game .nro is ${bytes} bytes; expected about 8.9MB" >&2
  exit 1
fi

echo "game .nro ${NRO} (${bytes} bytes, DW3_VERSION=${DW3_VERSION})"
