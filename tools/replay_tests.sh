#!/usr/bin/env bash
# P6.15: replay recorded pad input on the US and EU PC builds under Xvfb and
# diff checkpoint screenshots against references kept outside git; separate
# crash / hang / memory verdicts. See docs/P6.15_REPLAY_TESTS.md.
#   tools/replay_tests.sh --quick      title-menu scenarios only (~1 min)
#   tools/replay_tests.sh              + gameplay scenarios (opening, field walk, ...)
#   tools/replay_tests.sh --soak       + the 15-20 min soak scenarios
#   tools/replay_tests.sh --update     (re)generate references from this run
#   tools/replay_tests.sh --twice      run each scenario twice, require identical captures
# The Switch build in Ryubing: tools/replay_ryubing.sh
set -euo pipefail
exec python3 "$(dirname "$0")/replay/replay_tests.py" "$@"
