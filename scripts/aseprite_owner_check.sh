#!/usr/bin/env bash
set -uo pipefail

ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
cd "$ROOT" || exit 1

BUILD_DIR=${BUILD_DIR:-build}
[ $# -eq 1 ] || { echo "usage: bash scripts/aseprite_owner_check.sh <directory with queen.json and queen.png>"; exit 2; }
DIR=$1
for f in queen.json queen.png; do
  [ -f "$DIR/$f" ] || { echo "aseprite-export: no $DIR/$f"; exit 1; }
done
cmake --build "$BUILD_DIR" --target assetc clip_dump > /dev/null || { echo "aseprite-export: assetc does not build in $BUILD_DIR"; exit 1; }
BIN="$BUILD_DIR/assetc"
[ -x "$BIN" ] || BIN="$BUILD_DIR/assetc.exe"
DUMP="$BUILD_DIR/clip_dump"
[ -x "$DUMP" ] || DUMP="$BUILD_DIR/clip_dump.exe"
printf 'texture|queen_sheet|pixel|queen.png\nclips|queen|aseprite|queen.json\n' > "$DIR/queen.manifest"
"$BIN" --manifest "$DIR/queen.manifest" "$DIR/queen.bundle" || { echo "aseprite-export: FAIL"; exit 1; }
TABLE=$("$DUMP" "$DIR/queen.bundle" | tr -d '\r') || { echo "aseprite-export: FAIL"; exit 1; }
missing=0
for want in "queen/Death frame=0 boxes=hit0 event=-" "queen/Death frame=1 boxes=hit0 event=fall" \
            "queen/Death frame=2 boxes=- event=-" "queen/Hit frame=0 boxes=hit0 event=-"; do
  if grep -qxF "$want" <<< "$TABLE"; then
    echo "  ok: $want"
  else
    echo "  missing: $want"
    missing=1
  fi
done
if [ "$missing" -eq 0 ]; then
  echo "aseprite-export: PASS"
else
  printf '%s\n' "$TABLE" | grep -E '^queen/(Death|Hit) '
  echo "aseprite-export: FAIL"
  exit 1
fi
