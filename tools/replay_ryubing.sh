#!/usr/bin/env bash
# P6.15: replay the recorded pad input on the Switch build in Ryubing (own data
# roots, private Xvfb) and judge the guest log. See docs/P6.15_REPLAY_TESTS.md.
#   tools/replay_ryubing.sh --build --quick       # title scenarios, both discs
#   tools/replay_ryubing.sh --only us-field-walk  # one scenario
# Needs docker (devkitpro/devkita64) for --build, Xvfb, ImageMagick `import`.
set -euo pipefail
export DOTNET_ROOT="${DOTNET_ROOT:-$HOME/.dotnet}"
exec python3 "$(dirname "$0")/replay/ryubing.py" "$@"
