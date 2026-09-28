#!/usr/bin/env bash
# Spec #8 Gate 3 — cross-compilation verification (local pinned-T4, macOS host).
#
# Desktop: native per-OS CI-matrix (см. .github/workflows/ci.yml). Здесь — замер
# single-node build-time; matrix wall-clock = max(per-OS), не sum (см. вывод).
# Mobile: true-cross toolchains (iOS CMAKE_SYSTEM_NAME=iOS + sim; Android NDK arm64-v8a).
# wgpu-native собирается ИЗ Rust-исходников под оба таргета (prebuilt под mobile нет).
# Проверяет арх выходных бинарей: iOS arm64 Mach-O (IOSSIMULATOR), Android aarch64 ELF.
# Запуск на устройствах/эмуляторе — S2b (симулятор) + S10 (owner-устройства).
set -euo pipefail

HERE="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$HERE/.." && pwd)"
fail() { echo "[xcompile] FAIL: $*" >&2; exit 1; }
ok() { echo "[xcompile] ok: $*"; }

# Без обеих библиотек приложение на устройстве не стартует, а APK собирается (аудит #21 B9:
# libc++_shared печаталась в `ok`, но не проверялась); без бандлов стартует и падает на первом
# ассете. Pure-shell case, не `unzip | grep -q`: grep короткозамыкает → unzip SIGPIPE → ложный pipefail.
BUNDLES="game.bundle library.bundle audio.bundle"
APK_ENTRIES="lib/arm64-v8a/libgame.so lib/arm64-v8a/libc++_shared.so"
for b in $BUNDLES; do APK_ENTRIES="$APK_ENTRIES assets/$b"; done
apk_missing() {
  local entry
  for entry in $APK_ENTRIES; do
    case "$1" in *"$entry"*) ;; *) echo "$entry"; return ;; esac
  done
}

# Вывод сборки — в лог, а не в /dev/null: упавший шаг без диагностики чинится вслепую.
logged() {
  local log="$1"; shift
  "$@" >"$log" 2>&1 || { tail -40 "$log" >&2; fail "$* (full log: $log)"; }
}

if [ "${1:-}" = "--selftest" ]; then
  FULL=""
  for entry in $APK_ENTRIES; do FULL="$(printf '%s\n  1  %s' "$FULL" "$entry")"; done
  [ -z "$(apk_missing "$FULL")" ] || fail "selftest: a complete APK listing is refused"
  for entry in $APK_ENTRIES; do
    [ "$(apk_missing "$(printf '%s\n' "$FULL" | grep -vF "$entry")")" = "$entry" ] \
      || fail "selftest: an APK without $entry passes"
  done
  ok "selftest: the APK listing check names each missing entry"
  exit 0
fi

echo "=== Desktop native-matrix build-time замер (single-node, CI-флаги) ==="
D="$ROOT/build-xctime"
rm -rf "$D"; mkdir -p "$D"
logged "$D/xcompile.log" cmake -S "$ROOT" -B "$D" -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DAUDIO_MINIAUDIO=OFF -DPLUGIN_UI=OFF -DPLUGIN_WASM=OFF
T0=$(date +%s); logged "$D/xcompile.log" cmake --build "$D"; T1=$(date +%s)
NODE=$((T1 - T0))
echo "single-node (macOS) full 'all' clean build: ${NODE}s"
echo "CI matrix (ubuntu|windows|macos concurrent, fail-fast:false):"
echo "  wall-clock = max(t_linux, t_win, t_mac)  — НЕ sum → ~3x экономия vs последовательного."
rm -rf "$D"

echo "=== Mobile true-cross: iOS (aarch64-apple-ios-sim) ==="
IOSB="$ROOT/build-ios"; mkdir -p "$IOSB"
# Оболочка — подкаталог корня (LIKE_NES_MOBILE): движок и игра приходят тем же графом, что на
# десктопе. Каталог мог остаться от схемы `-S platform/ios` — fresh_flag переконфигурирует его.
. "$ROOT/platform/mobile/fresh_cache.sh"
FRESH="$(fresh_flag "$IOSB" "$ROOT")"
logged "$IOSB/xcompile.log" cmake ${FRESH:+"$FRESH"} -S "$ROOT" -B "$IOSB" -G Ninja \
  -DCMAKE_SYSTEM_NAME=iOS -DCMAKE_OSX_SYSROOT=iphonesimulator -DCMAKE_OSX_ARCHITECTURES=arm64
logged "$IOSB/xcompile.log" cmake --build "$IOSB" --target like_nes_ios
IOSBIN="$IOSB/like_nes_ios.app/like_nes_ios"
[ -f "$IOSBIN" ] || fail "iOS binary not produced"
[ "$(lipo -archs "$IOSBIN")" = "arm64" ] || fail "iOS arch != arm64"
case "$(vtool -show-build "$IOSBIN")" in *"platform IOSSIMULATOR"*) ;; *) fail "iOS platform != IOSSIMULATOR" ;; esac
IOSWGPU="$IOSB/_deps/wgpu_native_src-src/target/aarch64-apple-ios-sim/release/libwgpu_native.a"
[ "$(lipo -archs "$IOSWGPU")" = "arm64" ] || fail "iOS wgpu-native (from Rust) arch != arm64"
for b in $BUNDLES; do
  [ -f "$IOSB/like_nes_ios.app/assets/$b" ] || fail "iOS .app missing assets/$b"
done
ok "iOS arm64 Mach-O (IOSSIMULATOR) + wgpu-native-from-Rust arm64 + .app assets/{$BUNDLES}"

echo "=== Mobile true-cross: Android (aarch64-linux-android, NDK arm64-v8a) ==="
mkdir -p "$ROOT/build-android"
logged "$ROOT/build-android/xcompile.log" bash "$ROOT/platform/android/build_apk.sh"
ANDSO="$ROOT/build-android/libgame.so"
[ -f "$ANDSO" ] || fail "Android .so not produced"
# macOS-хост: readelf нет в base; llvm-readelf, если есть, иначе file (портируемо). Вывод — в
# переменную, а не в `grep -q`: тот же SIGPIPE → pipefail, что у листинга APK выше.
RE="$(command -v llvm-readelf || true)"
if [ -n "$RE" ]; then
  case "$("$RE" -h "$ANDSO")" in *AArch64*) ;; *) fail "Android ELF machine != AArch64" ;; esac
else
  case "$(file "$ANDSO")" in *"ARM aarch64"*) ;; *) fail "Android ELF machine != aarch64" ;; esac
fi
APK="$ROOT/build-android/apk/like_nes.apk"
MISSING="$(apk_missing "$(unzip -l "$APK")")"
[ -z "$MISSING" ] || fail "APK missing $MISSING"
ok "Android aarch64 ELF + APK {$APK_ENTRIES}"

echo "[xcompile] PASS: desktop matrix (max-vs-sum) + iOS arm64 + Android aarch64"
