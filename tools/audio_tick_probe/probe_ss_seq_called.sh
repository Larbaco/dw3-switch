#!/usr/bin/env bash
# Count SsSeqCalledTbyT hits on dw3-pc title boot (P3.6 tick probe).
# No CMake / vendor/psyz changes — uses gdb breakpoints only.
set -euo pipefail

HITS=120
TIMEOUT_SEC=60
DW3_BIN=""
RUNTIME=""

usage() {
  cat <<'EOF'
Usage: probe_ss_seq_called.sh [--bin PATH] [--runtime DIR] [--hits N] [--timeout SEC]

Requires: gdb, a built dw3-pc, owner runtime (never committed).
Prints AUDIO_TICK_HITS=<n> and exits 0 when the hit count reaches N.
EOF
}

while [[ $# -gt 0 ]]; do
  case "$1" in
    --bin) DW3_BIN=$2; shift 2 ;;
    --runtime) RUNTIME=$2; shift 2 ;;
    --hits) HITS=$2; shift 2 ;;
    --timeout) TIMEOUT_SEC=$2; shift 2 ;;
    -h|--help) usage; exit 0 ;;
    *) echo "unknown arg: $1" >&2; usage >&2; exit 2 ;;
  esac
done

ROOT=$(cd "$(dirname "$0")/../.." && pwd)
if [[ -z "$DW3_BIN" ]]; then
  for cand in \
    "$ROOT/.worktrees/phase3-pc-bringup/build/pc/dw3-pc" \
    "$ROOT/build/pc/dw3-pc" \
    "$ROOT/../phase3-pc-bringup/build/pc/dw3-pc"
  do
    if [[ -x "$cand" ]]; then DW3_BIN=$cand; break; fi
  done
fi
if [[ -z "$RUNTIME" ]]; then
  for cand in \
    "$ROOT/.worktrees/phase3-pc-bringup/runtime/us" \
    "$ROOT/runtime/us" \
    "$ROOT/../phase3-pc-bringup/runtime/us"
  do
    if [[ -d "$cand" ]]; then RUNTIME=$cand; break; fi
  done
fi

if [[ -z "$DW3_BIN" || ! -x "$DW3_BIN" ]]; then
  echo "probe: no dw3-pc binary (pass --bin)" >&2
  exit 2
fi
if [[ -z "$RUNTIME" || ! -d "$RUNTIME" ]]; then
  echo "probe: no runtime dir (pass --runtime)" >&2
  exit 2
fi
if ! command -v gdb >/dev/null; then
  echo "probe: gdb required" >&2
  exit 2
fi

GDB_SCRIPT=$(mktemp)
trap 'rm -f "$GDB_SCRIPT"' EXIT
cat >"$GDB_SCRIPT" <<EOF
set pagination off
set confirm off
set \$hits = 0
break SsSeqCalledTbyT
commands
  silent
  set \$hits = \$hits + 1
  if \$hits >= $HITS
    printf "AUDIO_TICK_HITS=%d\\n", \$hits
    quit 0
  end
  continue
end
run
printf "AUDIO_TICK_HITS=%d (run ended early)\\n", \$hits
quit 1
EOF

echo "probe: bin=$DW3_BIN runtime=$RUNTIME hits=$HITS" >&2
# Allow dummy audio when no DAC (tick proof does not need a real device).
export SDL_AUDIODRIVER="${SDL_AUDIODRIVER:-}"
set +e
timeout "${TIMEOUT_SEC}s" gdb -batch -nx -x "$GDB_SCRIPT" \
  --args "$DW3_BIN" --runtime "$RUNTIME"
rc=$?
set -e
if [[ $rc -eq 124 ]]; then
  echo "probe: timed out after ${TIMEOUT_SEC}s" >&2
  exit 1
fi
exit "$rc"
