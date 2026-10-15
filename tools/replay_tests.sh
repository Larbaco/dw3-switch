#!/usr/bin/env bash
# P6.15: replay recorded pad input on the US and EU (5 languages) PC builds
# under Xvfb and diff checkpoint screenshots against references kept outside
# git. See docs/P6.15_REPLAY_TESTS.md.
#   tools/replay_tests.sh              compare against references, exit 1 on diff
#   tools/replay_tests.sh --update     (re)generate references from this run
set -euo pipefail
exec python3 "$(dirname "$0")/replay/replay_tests.py" "$@"
