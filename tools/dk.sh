#!/usr/bin/env bash
# Run a command inside the devkitPro Switch Docker image.
#
# NTFS / drvfs checkouts often cannot mkdir/write platforms/switch/build as the
# container user. Set DW3_DK_TMPBUILD to an ext4 path to build the Switch host
# out-of-tree and copy artifacts back:
#
#   DW3_DK_TMPBUILD=/tmp/dw3-dk-build ./tools/dk.sh make -C platforms/switch
#
# TMPBUILD runs the container as root (Docker userns often maps host uid 1000
# to container root ownership, so -u $(id -u) cannot write the mirror). Artifacts
# are chown'd back to the invoking user before copy.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
# Pinned by digest (= tag 20260219, devkitA64 r29.2, gcc 15.2); see docs/DEPENDENCIES.md.
IMAGE="${DW3_DK_IMAGE:-devkitpro/devkita64@sha256:1fc388c3a0d34bd2045a6dadcb1020e069d5f876a187fd705de14b4440c00282}"
HOST_UID="$(id -u)"
HOST_GID="$(id -g)"

if ! command -v docker >/dev/null 2>&1; then
  echo "docker is required" >&2
  exit 1
fi

if [[ $# -lt 1 ]]; then
  echo "usage: $0 <command> [args...]" >&2
  echo "example: $0 make -C platforms/switch" >&2
  echo "NTFS:    DW3_DK_TMPBUILD=/tmp/dw3-dk-build $0 make -C platforms/switch" >&2
  exit 1
fi

docker pull "$IMAGE" >/dev/null

run_docker() {
  local workdir="$1"
  local as_root="$2"
  shift 2
  local user_args=()
  if [[ "$as_root" != "1" ]]; then
    user_args=(-u "${HOST_UID}:${HOST_GID}")
  fi
  docker run --rm -t \
    "${user_args[@]}" \
    -e DEVKITPRO=/opt/devkitpro \
    -e DEVKITARM=/opt/devkitpro/devkitARM \
    -e PATH="/opt/devkitpro/tools/bin:/opt/devkitpro/devkitA64/bin:/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin" \
    -v "$workdir:/dw3-switch" \
    -w /dw3-switch \
    "$IMAGE" \
    "$@"
}

# Detect: DW3_DK_TMPBUILD + make -C platforms/switch
if [[ -n "${DW3_DK_TMPBUILD:-}" && "$1" == make ]]; then
  switch_make=0
  args=("$@")
  for i in "${!args[@]}"; do
    if [[ "${args[$i]}" == "-C" && "${args[$((i + 1))]:-}" == "platforms/switch" ]]; then
      switch_make=1
      break
    fi
    if [[ "${args[$i]}" == "-Cplatforms/switch" || "${args[$i]}" == "--directory=platforms/switch" ]]; then
      switch_make=1
      break
    fi
  done

  if [[ "$switch_make" -eq 1 ]]; then
    if ! command -v rsync >/dev/null 2>&1; then
      echo "rsync is required when DW3_DK_TMPBUILD is set" >&2
      exit 1
    fi
    SRC_HOST="${DW3_DK_TMPBUILD}/src"
    # Drop a prior root-owned mirror (userns) so the host user can rewrite it.
    rm -rf "$SRC_HOST"
    mkdir -p "$SRC_HOST/platforms/switch"
    # No -g/-o: Docker userns often makes chgrp fail on the mirror.
    rsync -rlptD --delete \
      --exclude build \
      --exclude 'dw3-switch.*' \
      "$ROOT/platforms/switch/" "$SRC_HOST/platforms/switch/"

    echo "dk.sh: Switch build via DW3_DK_TMPBUILD=${DW3_DK_TMPBUILD} (container root)" >&2
    set +e
    run_docker "$SRC_HOST" 1 "$@"
    status=$?
    set -e

    # Fix ownership after root build so the host user can read/copy.
    run_docker "$SRC_HOST" 1 \
      chown -R "${HOST_UID}:${HOST_GID}" /dw3-switch/platforms/switch || true

    if [[ $status -eq 0 ]]; then
      for f in dw3-switch.nro dw3-switch.elf dw3-switch.nacp; do
        if [[ -f "$SRC_HOST/platforms/switch/${f}" ]]; then
          if ! cp -f "$SRC_HOST/platforms/switch/${f}" "$ROOT/platforms/switch/${f}" 2>/dev/null; then
            echo "note: built ${f} at $SRC_HOST/platforms/switch/${f} (host copy skipped)" >&2
          fi
        fi
      done
    fi
    exit "$status"
  fi
fi

run_docker "$ROOT" 0 "$@"
