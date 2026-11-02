#!/usr/bin/env bash
# Executes actual shell persistence code on macOS Foundation with a narrow UIKit task model.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
[ "$#" -ge 1 ] && [ "$#" -le 2 ] || { echo 'usage: ios_persistence_selftest.sh WEBGPU_INCLUDE [PERSISTENCE_MM]' >&2; exit 1; }
SOURCE=${2:-$ROOT/platform/ios/persistence.mm}
WORK=$(mktemp -d)
trap 'rm -rf "$WORK"' EXIT
clang++ -std=c++20 -fobjc-arc -fblocks -Wall -Wextra -Werror \
  -I"$ROOT/platform/ios/tests/persistence" -I"$ROOT/platform/ios" -I"$ROOT/platform/mobile" \
  -I"$ROOT/engine/platform" -I"$ROOT/engine/framework/haptic" -I"$1" \
  "$SOURCE" "$ROOT/platform/ios/tests/persistence/test.mm" -framework Foundation -o "$WORK/test"
"$WORK/test"
