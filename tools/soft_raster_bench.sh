#!/usr/bin/env bash
# P6.22: build tools/soft_raster_bench.c against the CPU rasterizer in
# vendor/psyz and, as a reference, the same file at another PSY-Z revision,
# then run it. See docs/P6.22_FIELD_CPU.md.
#
#   tools/soft_raster_bench.sh [--ref REV] [--out DIR] -- <bench args>
#   tools/soft_raster_bench.sh --ref ef67c4a -- trace.bin 400 2000
#   tools/soft_raster_bench.sh --ref ef67c4a -- --fuzz 2000000 7
#   tools/soft_raster_bench.sh --san --ref ef67c4a -- --fuzz 200000 7
#
# --san builds everything with AddressSanitizer (LeakSanitizer included) and
# UBSan, stopping at the first error.
#
# REV is a commit of vendor/psyz (default: the parent of its HEAD). Without
# --ref the bench only times the current rasterizer. Build output goes to
# --out (default /mnt/cabal/tmp/dw3-soft-bench, or $TMPDIR).
set -euo pipefail
root="$(cd "$(dirname "$0")/.." && pwd)"
psyz="$root/vendor/psyz/psyz"
ref="HEAD~1"
use_ref=1
out="${DW3_SOFT_BENCH_OUT:-/mnt/cabal/tmp/dw3-soft-bench}"
[ -d "$(dirname "$out")" ] || out="${TMPDIR:-/tmp}/dw3-soft-bench"
cc="${CC:-gcc}"
san=()
check=()
while [ $# -gt 0 ]; do
  case "$1" in
    --ref) ref="$2"; shift 2 ;;
    --no-ref) use_ref=0; shift ;;
    --check) check=(-DDEFER_CHECK); shift ;;
    --san) san=(-O1 -g -fno-omit-frame-pointer -fsanitize=address,undefined -fno-sanitize-recover=all); shift ;;
    --out) out="$2"; shift 2 ;;
    --) shift; break ;;
    *) break ;;
  esac
done
mkdir -p "$out"
flags=(-std=gnu11 -O3 -Wall -Wextra -D__psyz "-I$psyz/include" "-I$psyz/src" "-I$psyz/src/platform" "${san[@]}")
objs=()
"$cc" "${flags[@]}" "${check[@]}" -c "$psyz/src/platform/sdl3_software.c" -o "$out/new.o"
objs+=("$out/new.o")
defs=()
if [ "$use_ref" = 1 ]; then
  # its relative includes resolve through the -I paths
  refc="$out/sdl3_software_ref.c"
  git -C "$root/vendor/psyz" show "$ref:psyz/src/platform/sdl3_software.c" > "$refc"
  "$cc" "${flags[@]}" -c "$refc" -o "$out/ref.o" \
    -Dsoftware_rasterizer=ref_software_rasterizer \
    -DSoftwareRasterizer_Reset=ref_SoftwareRasterizer_Reset \
    -DSoftwareRasterizer_GetVram=ref_SoftwareRasterizer_GetVram \
    -DSoftwareRasterizer_SetGutter=ref_SoftwareRasterizer_SetGutter \
    -DSoftwareRasterizer_GetGutter=ref_SoftwareRasterizer_GetGutter \
    -DSoftwareRasterizer_GetGutterPlane=ref_SoftwareRasterizer_GetGutterPlane \
    -DSoftwareRasterizer_ClearGutterRows=ref_SoftwareRasterizer_ClearGutterRows \
    -DSoftwareRasterizer_GetVramRaw=ref_SoftwareRasterizer_GetVramRaw \
    -DSoftwareRasterizer_GetGutterPlaneRaw=ref_SoftwareRasterizer_GetGutterPlaneRaw \
    -DSoftwareRasterizer_SetDefer=ref_SoftwareRasterizer_SetDefer \
    -DSoftwareRasterizer_GetDefer=ref_SoftwareRasterizer_GetDefer \
    -DSoftwareRasterizer_SyncAll=ref_SoftwareRasterizer_SyncAll \
    -DSoftwareRasterizer_SyncRect=ref_SoftwareRasterizer_SyncRect \
    -DSoftwareRasterizer_GetDeferStats=ref_SoftwareRasterizer_GetDeferStats
  objs+=("$out/ref.o")
  defs+=(-DWITH_REF)
fi
"$cc" "${flags[@]}" "${defs[@]}" -c "$root/tools/soft_raster_bench.c" -o "$out/bench.o"
"$cc" "${san[@]}" -o "$out/soft_raster_bench" "$out/bench.o" "${objs[@]}"
exec "$out/soft_raster_bench" "$@"
