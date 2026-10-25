#!/usr/bin/env bash
# P6.24 slice 1: assemble the PC "single build" folder. Not part of CI or the
# release flow; the plain US / EU builds do not depend on it.
#
#   tools/build_pc_single.sh [OUT_DIR]     (default build/pc-single)
#
# OUT_DIR gets `dw3-pc` (the region launcher), `dw3-pc-us` and `dw3-pc-eu`
# (the unchanged per-region game binaries). Builds go under OUT_DIR/.work.
# Run it with nice, one build at a time, DW3_BUILD_JOBS=8 (AGENTS.md).
# DW3_PC_PREBUILT_DIR=DIR reuses DIR/build-us/dw3-pc and DIR/build-eu/dw3-pc.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
OUT="$(realpath -m "${1:-$ROOT/build/pc-single}")"
JOBS="${DW3_BUILD_JOBS:-8}"
mkdir -p "$OUT/.work"
for v in us eu; do
  if [[ -n "${DW3_PC_PREBUILT_DIR:-}" && -x "$DW3_PC_PREBUILT_DIR/build-$v/dw3-pc" ]]; then
    cp -f "$DW3_PC_PREBUILT_DIR/build-$v/dw3-pc" "$OUT/dw3-pc-$v"
    continue
  fi
  cmake -S "$ROOT/platforms/pc" -B "$OUT/.work/$v" -G Ninja -DDW3_VERSION=$v \
    -DPSYZ_RENDERER=sdl3_gpu -DSDL_X11_XSCRNSAVER=OFF -DSDL_ALSA=OFF \
    -DSDL_JACK=OFF -DSDL_PIPEWIRE=OFF -DSDL_PULSEAUDIO=OFF -DSDL_SNDIO=OFF
  ninja -C "$OUT/.work/$v" -j"$JOBS" dw3-pc
  cp -f "$OUT/.work/$v/dw3-pc" "$OUT/dw3-pc-$v"
done
cmake -S "$ROOT/platforms/pc/launcher" -B "$OUT/.work/launcher" -G Ninja
ninja -C "$OUT/.work/launcher" -j"$JOBS"
cp -f "$OUT/.work/launcher/dw3-pc-launcher" "$OUT/dw3-pc"
echo "single PC build in $OUT: dw3-pc (launcher), dw3-pc-us, dw3-pc-eu"
