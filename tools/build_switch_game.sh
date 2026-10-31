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
# 2003. EU extracts its matching owner BIN/CUE into sdmc:/switch/dw3/eu/.
# Pass --both (or DW3_VERSION=both) for the P6.24 combined image: one NRO with
# both game copies (build/switch-both/), the region chosen at boot from the
# data on the card (sdmc:/switch/dw3/us and /eu) and the boot picker.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
# shellcheck source=tools/dw3_ccache_env.sh
source "${ROOT}/tools/dw3_ccache_env.sh"
# Pinned by digest (= tag 20260219, devkitA64 r29.2, gcc 15.2); see docs/DEPENDENCIES.md.
IMAGE="${DW3_DK_IMAGE:-devkitpro/devkita64@sha256:1fc388c3a0d34bd2045a6dadcb1020e069d5f876a187fd705de14b4440c00282}"
NXLINK_STDIO=OFF
DW3_NX_FORCE_CRASH=OFF
DW3_NX_REPLAY=OFF
# P6.20: the Cheats screen is built in by default. --no-cheats compiles the
# whole feature out (CMake DW3_CHEATS=OFF; docs/PORT_OPTIONS.md).
DW3_CHEATS=ON
# DW3_BUILD_JOBS=N limits the ninja jobs in the container (default: nproc).
# ccache (docs/BUILD_SPEED.md): used when a ccache is found (PATH, or
# ~/.cache/dw3-switch/bin from tools/get_ccache.sh; DW3_CCACHE_FETCH=1 fetches
# it). It is mounted into the container, with the shared cache dir
# (DW3_CCACHE_DIR, default ~/.cache/dw3-switch/ccache). DW3_NO_CCACHE=1 turns it
# off. The NRO/ELF are the same with or without it.
# Development-only K=V,K=V environment baked into the NRO (CMake
# DW3_NX_TEST_ENV). Always passed, so a build dir never keeps an old value.
DW3_NX_TEST_ENV="${DW3_NX_TEST_ENV:-}"
DW3_VERSION="$(echo "${DW3_VERSION:-us}" | tr '[:upper:]' '[:lower:]')"

usage() {
  echo "usage: $0 [--eu|--both] [--nxlink] [--replay] [--force-crash] [--no-cheats]"
  echo "  default       US game NRO at build/switch/dw3-switch.nro"
  echo "  --eu          EU game NRO at build/switch-eu/dw3-switch.nro"
  echo "  --both        P6.24 combined NRO, US and EU in one image, region chosen at boot"
  echo "                (build/switch-both/dw3-switch.nro; NACP \"Digimon World 3 NX\")"
  echo "  --nxlink      nxlink development NRO (build/switch[-eu|-both]-nxlink/)"
  echo "  --replay      P6.15 test NRO: reads sdmc:/switch/dw3/replay.rec"
  echo "                (build/switch[-eu][-nxlink]-replay/dw3-switch[-nxlink]-replay.nro;"
  echo "                never release/preview, see docs/P6.15_REPLAY_TESTS.md)"
  echo "  --force-crash P5.4 smoke only: deliberate null write after boot"
  echo "                (pair with --nxlink; never in release/preview NROs)"
  echo "  --no-cheats   P6.20: build without the Cheats screen (CMake DW3_CHEATS=OFF,"
  echo "                the game is identical to a build before the feature;"
  echo "                build/switch[-eu][-nxlink]-nocheats/)"
  echo "env DW3_NX_TEST_ENV=K=V,K=V bakes a dev-only environment into the NRO"
  echo "  (e.g. DW3_FORCE_CRASH=sim for Ryubing; see docs/P5.4_CRASH.md)"
}

while (( $# > 0 )); do
  case "$1" in
    --eu)
      DW3_VERSION=eu
      ;;
    --both)
      DW3_VERSION=both
      ;;
    --nxlink)
      NXLINK_STDIO=ON
      ;;
    --force-crash)
      DW3_NX_FORCE_CRASH=ON
      ;;
    --replay)
      DW3_NX_REPLAY=ON
      ;;
    --no-cheats)
      DW3_CHEATS=OFF
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

if [[ "${DW3_VERSION}" != "us" && "${DW3_VERSION}" != "eu" && "${DW3_VERSION}" != "both" ]]; then
  echo "DW3_VERSION must be us, eu or both (got '${DW3_VERSION}')" >&2
  exit 2
fi

if [[ "${DW3_NX_FORCE_CRASH}" == "ON" && "${NXLINK_STDIO}" != "ON" ]]; then
  echo "--force-crash without --nxlink: the NRO will fatal right after boot" \
      "and there will be no live log; recommended: --nxlink --force-crash" >&2
fi

if [[ "${DW3_VERSION}" == "both" ]]; then
  if [[ "${NXLINK_STDIO}" == "ON" ]]; then
    BUILD_DIR="${ROOT}/build/switch-both-nxlink"
    NRO_NAME="dw3-switch-nxlink"
  else
    BUILD_DIR="${ROOT}/build/switch-both"
    NRO_NAME="dw3-switch"
  fi
  # No region at compile time: the base folder that holds us/ and eu/.
  RUNTIME_PATH="sdmc:/switch/dw3"
elif [[ "${DW3_VERSION}" == "eu" ]]; then
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
if [[ "${DW3_NX_REPLAY}" == "ON" ]]; then
  BUILD_DIR="${BUILD_DIR}-replay"
  NRO_NAME="${NRO_NAME}-replay"
fi
if [[ "${DW3_CHEATS}" == "OFF" ]]; then
  BUILD_DIR="${BUILD_DIR}-nocheats"
fi
NRO="${BUILD_DIR}/${NRO_NAME}.nro"
NACP="${BUILD_DIR}/${NRO_NAME}.nacp"

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

# Resolve hbmenu NACP version (≤15 chars) + full label for the boot log.
# CI: set DW3_NRO_VERSION or RELEASE_TAG / GITHUB_* (see --github).
VERSION_ARGS=(--repo "${ROOT}")
if [[ -n "${DW3_NRO_VERSION:-}" ]]; then
  VERSION_ARGS+=(--version "${DW3_NRO_VERSION}")
elif [[ -n "${RELEASE_TAG:-}${DW3_RELEASE_TAG:-}${GITHUB_SHA:-}" ]]; then
  VERSION_ARGS+=(--github)
fi
DW3_NRO_VERSION=""
DW3_NACP_VERSION=""
while IFS= read -r _dw3_ver_line; do
  case "${_dw3_ver_line}" in
    DW3_NRO_VERSION=*) DW3_NRO_VERSION="${_dw3_ver_line#DW3_NRO_VERSION=}" ;;
    DW3_NACP_VERSION=*) DW3_NACP_VERSION="${_dw3_ver_line#DW3_NACP_VERSION=}" ;;
  esac
done < <(python3 "${ROOT}/tools/dw3_nro_version.py" "${VERSION_ARGS[@]}" --format export)
if [[ -z "${DW3_NRO_VERSION}" || -z "${DW3_NACP_VERSION}" ]]; then
  echo "failed to resolve DW3_NRO_VERSION / DW3_NACP_VERSION" >&2
  exit 1
fi
echo "NRO version full=${DW3_NRO_VERSION} nacp=${DW3_NACP_VERSION}"
# P6.37: the in-game label (Options footer, title corner, dw3-version: log
# line), written here with the same arguments so the devkitPro container needs
# no Python. The script rewrites the header only when its text changed.
DW3_VERSION_GEN_DIR="${BUILD_DIR}/dw3_version_gen"
mkdir -p "${DW3_VERSION_GEN_DIR}"
python3 "${ROOT}/tools/dw3_nro_version.py" "${VERSION_ARGS[@]}" --format label \
  --write-header "${DW3_VERSION_GEN_DIR}/dw3_version_gen.h"

# Worktrees store their common gitdir outside the checkout. Mount it at the
# same absolute path so submodule gitlinks and apply_patches.sh still resolve.
GIT_MOUNT_ARGS=()
if [[ -f "${ROOT}/.git" ]]; then
  GIT_COMMON_DIR="$(git -C "${ROOT}" rev-parse --path-format=absolute --git-common-dir)"
  GIT_MOUNT_ARGS=(-v "${GIT_COMMON_DIR}:${GIT_COMMON_DIR}:ro")
fi

# build/ may be a symlink to a local ext4 directory (an NTFS checkout cannot
# hold the decomp tree). Mount its target at the same absolute path, or the
# container sees a dangling link.
BUILD_LINK_ARGS=()
if [[ -L "${ROOT}/build" ]]; then
  BUILD_LINK_TARGET="$(readlink -f "${ROOT}/build")"
  BUILD_LINK_ARGS=(-v "${BUILD_LINK_TARGET}:${BUILD_LINK_TARGET}")
fi

# ccache: the container needs a static binary (no libc match with the host).
CCACHE_ARGS=()
CCACHE_PATH_SUFFIX=""
dw3_ccache_setup "${ROOT}"
if [[ -n "${DW3_CCACHE_BIN}" ]] && ! dw3_ccache_is_static "${DW3_CCACHE_BIN}"; then
  DW3_CCACHE_BIN="$("${ROOT}/tools/get_ccache.sh" 2>/dev/null || true)"
fi
if [[ -n "${DW3_CCACHE_BIN}" ]] && dw3_ccache_is_static "${DW3_CCACHE_BIN}"; then
  CCACHE_ARGS=(
    -v "${DW3_CCACHE_BIN}:/usr/local/bin/ccache:ro"
    -v "${CCACHE_DIR}:${CCACHE_DIR}"
    -e CCACHE_DIR -e CCACHE_BASEDIR -e CCACHE_NOHASHDIR -e CCACHE_SLOPPINESS
    -e CCACHE_MAXSIZE -e CCACHE_COMPRESS
  )
  CCACHE_PATH_SUFFIX=":/usr/local/bin"
  echo "ccache: ${DW3_CCACHE_BIN} (cache ${CCACHE_DIR})"
else
  echo "ccache: off (not found or not static; see docs/BUILD_SPEED.md)"
fi

# No login shell: bash -l lets /etc/profile drop devkitA64/bin from PATH,
# and aarch64-none-elf-nm lives there. Mount at the host absolute path so
# worktree gitlinks and tools/apply_patches.sh still resolve.
docker run --rm \
  -e DEVKITPRO=/opt/devkitpro \
  -e DW3_SWITCH_BUILD_DIR="${BUILD_DIR}" \
  -e DW3_NXLINK_STDIO="${NXLINK_STDIO}" \
  -e DW3_VERSION="${DW3_VERSION}" \
  -e DW3_NX_FORCE_CRASH="${DW3_NX_FORCE_CRASH}" \
  -e DW3_NX_REPLAY="${DW3_NX_REPLAY}" \
  -e DW3_CHEATS="${DW3_CHEATS}" \
  -e DW3_BUILD_JOBS="${DW3_BUILD_JOBS:-}" \
  -e DW3_NX_TEST_ENV="${DW3_NX_TEST_ENV}" \
  -e DW3_NRO_VERSION="${DW3_NRO_VERSION}" \
  -e DW3_NACP_VERSION="${DW3_NACP_VERSION}" \
  -e DW3_VERSION_GEN_DIR="${DW3_VERSION_GEN_DIR}" \
  -e GIT_CONFIG_COUNT=1 \
  -e GIT_CONFIG_KEY_0=safe.directory \
  -e GIT_CONFIG_VALUE_0='*' \
  -e PATH="/opt/devkitpro/tools/bin:/opt/devkitpro/devkitA64/bin:/usr/bin:/bin${CCACHE_PATH_SUFFIX}" \
  -v "${ROOT}:${ROOT}" \
  "${GIT_MOUNT_ARGS[@]}" \
  "${BUILD_LINK_ARGS[@]}" \
  "${CCACHE_ARGS[@]}" \
  -w "${ROOT}" \
  "${IMAGE}" \
  bash -c 'command -v ccache >/dev/null && ccache -z >/dev/null; \
    cmake -S platforms/switch -B "$DW3_SWITCH_BUILD_DIR" -G Ninja \
    -DCMAKE_TOOLCHAIN_FILE=/opt/devkitpro/cmake/Switch.cmake \
    -DCMAKE_BUILD_TYPE=Release \
    -DDW3_VERSION="$DW3_VERSION" \
    -DDW3_NXLINK_STDIO="$DW3_NXLINK_STDIO" \
    -DDW3_NX_FORCE_CRASH="$DW3_NX_FORCE_CRASH" \
    -DDW3_NX_REPLAY="$DW3_NX_REPLAY" \
    -DDW3_CHEATS="$DW3_CHEATS" \
    -DDW3_NX_TEST_ENV="$DW3_NX_TEST_ENV" \
    -DDW3_NRO_VERSION="$DW3_NRO_VERSION" \
    -DDW3_NACP_VERSION="$DW3_NACP_VERSION" \
    -DDW3_VERSION_GEN_DIR="$DW3_VERSION_GEN_DIR" && \
    cmake --build "$DW3_SWITCH_BUILD_DIR" -j"${DW3_BUILD_JOBS:-$(nproc)}" && \
    { command -v ccache >/dev/null && ccache -s | sed -n "1,6p"; true; }'

if ! grep -F -x -q "DW3_NXLINK_STDIO:BOOL=${NXLINK_STDIO}" "${BUILD_DIR}/CMakeCache.txt"; then
  echo "CMake cache does not match requested nxlink mode (${NXLINK_STDIO})" >&2
  exit 1
fi
if ! grep -F -x -q "DW3_NX_REPLAY:BOOL=${DW3_NX_REPLAY}" "${BUILD_DIR}/CMakeCache.txt"; then
  echo "CMake cache does not match requested replay mode (${DW3_NX_REPLAY})" >&2
  exit 1
fi
if ! grep -F -x -q "DW3_CHEATS:BOOL=${DW3_CHEATS}" "${BUILD_DIR}/CMakeCache.txt"; then
  echo "CMake cache does not match requested cheats mode (${DW3_CHEATS})" >&2
  exit 1
fi
if ! grep -F -x -q "DW3_NX_FORCE_CRASH:BOOL=${DW3_NX_FORCE_CRASH}" "${BUILD_DIR}/CMakeCache.txt"; then
  echo "CMake cache does not match requested force-crash mode (${DW3_NX_FORCE_CRASH})" >&2
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
  echo "game .nro is ${bytes} bytes; expected about 8.9MB (about 12MB with --both)" >&2
  exit 1
fi
if [[ "${DW3_VERSION}" == "both" && "${bytes}" -lt 10500000 ]]; then
  echo "combined .nro is ${bytes} bytes; two game copies should make it about 12MB" >&2
  exit 1
fi

# P6.37: the in-game label (header written above) must be inside the image; the
# replay build shows the fixed text "replay" instead.
if [[ "${DW3_NX_REPLAY}" != "ON" ]]; then
  ver_label="$(sed -n 's/^#define DW3_VERSION_LABEL "\(.*\)"$/\1/p' "${DW3_VERSION_GEN_DIR}/dw3_version_gen.h")"
  if [[ -z "${ver_label}" ]] || ! grep -a -F -q "${ver_label}" "${NRO}"; then
    echo "${NRO} does not contain the port version label '${ver_label}' (P6.37)" >&2
    exit 1
  fi
  echo "port version label in the image: dw3-switch ${ver_label}"
fi

if [[ ! -f "${NACP}" ]]; then
  echo "missing ${NACP}" >&2
  exit 1
fi
# libnx display_version lives at 0x3060 in the .nacp (15 chars + NUL).
nacp_ver="$(python3 - "${NACP}" "${DW3_NACP_VERSION}" <<'PY'
import sys
from pathlib import Path
path, expect = sys.argv[1], sys.argv[2]
data = Path(path).read_bytes()
got = data[0x3060:0x3060 + 0x10].split(b"\0", 1)[0].decode("ascii", "replace")
if got != expect:
    raise SystemExit(f"nacp display_version={got!r} expected={expect!r}")
print(got)
PY
)"

echo "game .nro ${NRO} (${bytes} bytes, DW3_VERSION=${DW3_VERSION}) nacp_version=${nacp_ver}"
# P5.4: the unstripped .elf next to the .nro is what tools/symbolize_crash.py
# needs for this build's crash_report.txt (match by build_id).
ELF="${BUILD_DIR}/dw3-switch.elf"
if [[ -f "${ELF}" ]]; then
  echo "game .elf ${ELF} (keep it with the .nro to symbolize crash reports)"
else
  echo "warning: missing ${ELF}; crash reports from this .nro cannot be symbolized" >&2
fi
