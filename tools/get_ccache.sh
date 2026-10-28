#!/usr/bin/env bash
# Fetch a pinned, statically linked ccache (musl) into a bin dir and print its
# path. Static on purpose: the same binary runs on the host and inside the
# devkitPro container (tools/build_switch_game.sh mounts it), whatever libc.
#   tools/get_ccache.sh [DEST_DIR]     default ~/.cache/dw3-switch/bin
# No root needed. A ccache already on PATH works for the PC build without this.
set -euo pipefail

VERSION="4.14.2"
SHA256="b20ff6415151226ac5a10dfd6839700c694bd67d1871c8fddb266d366ff25bfb"
NAME="ccache-${VERSION}-linux-x86_64-musl-static"
URL="https://github.com/ccache/ccache/releases/download/v${VERSION}/${NAME}.tar.xz"
DEST="${1:-${XDG_CACHE_HOME:-$HOME/.cache}/dw3-switch/bin}"

if [[ "$(uname -m)" != "x86_64" || "$(uname -s)" != "Linux" ]]; then
  echo "get_ccache.sh: only Linux x86_64 is pinned; install ccache from your package manager" >&2
  exit 1
fi
if [[ -x "${DEST}/ccache" ]] && "${DEST}/ccache" --version 2>/dev/null | grep -q "version ${VERSION}\$"; then
  echo "${DEST}/ccache"
  exit 0
fi
tmp="$(mktemp -d)"
trap 'rm -rf "$tmp"' EXIT
curl -fsSL --retry 3 -o "${tmp}/ccache.tar.xz" "$URL"
echo "${SHA256}  ${tmp}/ccache.tar.xz" | sha256sum -c - >&2
tar -xJf "${tmp}/ccache.tar.xz" -C "$tmp"
mkdir -p "$DEST"
install -m 0755 "${tmp}/${NAME}/ccache" "${DEST}/ccache"
echo "${DEST}/ccache"
