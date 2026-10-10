#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
IMAGE="${DW3_DK_IMAGE:-devkitpro/devkita64:latest}"

if ! command -v docker >/dev/null 2>&1; then
  echo "docker is required" >&2
  exit 1
fi

if [[ $# -lt 1 ]]; then
  echo "usage: $0 <command> [args...]" >&2
  echo "example: $0 make -C platforms/switch" >&2
  exit 1
fi

docker pull "$IMAGE" >/dev/null
exec docker run --rm -t \
  -u "$(id -u):$(id -g)" \
  -e DEVKITPRO=/opt/devkitpro \
  -e DEVKITARM=/opt/devkitpro/devkitARM \
  -e PATH="/opt/devkitpro/tools/bin:/opt/devkitpro/devkitA64/bin:/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin" \
  -v "$ROOT:/dw3-switch" \
  -w /dw3-switch \
  "$IMAGE" \
  "$@"
