#!/usr/bin/env bash
# Spec #8 Gate 4 — per-OS self-contained bundle (macOS .app / Linux tarball / Windows folder).
# Собирает game_sidescroller, ставит self-contained бандл (exe + wgpu dylib + baked game.bundle
# + version-stamp) через install()-правила. macOS → .app, Linux → tarball. Запуск бандла читает
# бейкнутые ассеты (шов assetc→билд, гейт 2). Пре-реквизит: game.bundle испечён (assetc --game).
set -euo pipefail

HERE="$(cd "$(dirname "$0")" && pwd)"
POC="$(cd "$HERE/.." && pwd)"
BUILD="${1:-$POC/build}"
OUT="${2:-$POC/dist}"

cmake -S "$POC" -B "$BUILD" -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DAUDIO_MINIAUDIO=OFF -DPLUGIN_UI=OFF -DPLUGIN_WASM=OFF >/dev/null
cmake --build "$BUILD" --target game_sidescroller
rm -rf "$OUT"
cmake --install "$BUILD" --prefix "$OUT" --component game >/dev/null

echo "=== bundle tree ==="
find "$OUT" -type f | sed "s|$OUT/||" | sort

case "$(uname -s)" in
  Darwin)
    EXE="$OUT/like-nes.app/Contents/MacOS/game_sidescroller"
    echo "=== .app self-containment ==="
    otool -l "$EXE" | grep -A2 LC_RPATH | grep "path " || true
    # dylib-ссылка обязана быть @rpath-резолвимой (иначе .app не стартует несмотря на rpath).
    otool -L "$EXE" | grep -q "@rpath/libwgpu_native.dylib" \
      || { echo "FAIL: wgpu dylib не @rpath-резолвимый"; exit 1; }
    # Реальный запуск из ПРОИЗВОЛЬНОГО cwd (headless demo) — доказывает self-containment.
    SMOKE="$(mktemp -d)"
    ( cd / && "$EXE" --demo "$SMOKE" --frames 2 ) | grep -q "baked bundle" \
      || { echo "FAIL: .app не стартует/не грузит baked из чужого cwd"; exit 1; }
    rm -rf "$SMOKE"
    echo "self-contained: @rpath dylib + baked bundle из foreign cwd — ok"
    echo "bundle: $OUT/like-nes.app"
    ;;
  Linux)
    ( cd "$OUT" && tar czf like-nes-linux.tar.gz like-nes )
    echo "tarball: $OUT/like-nes-linux.tar.gz"
    ;;
esac
echo "=== version-stamp ==="
find "$OUT" -name version.txt -exec cat {} \;
