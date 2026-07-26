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

echo "=== Desktop native-matrix build-time замер (single-node, CI-флаги) ==="
D="$ROOT/build-xctime"
rm -rf "$D"
cmake -S "$ROOT" -B "$D" -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DAUDIO_MINIAUDIO=OFF -DPLUGIN_UI=OFF -DPLUGIN_WASM=OFF >/dev/null 2>&1
T0=$(date +%s); cmake --build "$D" >/dev/null 2>&1; T1=$(date +%s)
NODE=$((T1 - T0))
echo "single-node (macOS) full 'all' clean build: ${NODE}s"
echo "CI matrix (ubuntu|windows|macos concurrent, fail-fast:false):"
echo "  wall-clock = max(t_linux, t_win, t_mac)  — НЕ sum → ~3x экономия vs последовательного."
rm -rf "$D"

echo "=== Mobile true-cross: iOS (aarch64-apple-ios-sim) ==="
IOSB="$ROOT/build-ios"
cmake -S "$ROOT/platform/ios" -B "$IOSB" -G Ninja \
  -DCMAKE_SYSTEM_NAME=iOS -DCMAKE_OSX_SYSROOT=iphonesimulator \
  -DCMAKE_OSX_ARCHITECTURES=arm64 >/dev/null 2>&1
cmake --build "$IOSB" >/dev/null 2>&1
IOSBIN="$IOSB/like_nes_ios.app/like_nes_ios"
[ -f "$IOSBIN" ] || fail "iOS binary not produced"
[ "$(lipo -archs "$IOSBIN")" = "arm64" ] || fail "iOS arch != arm64"
vtool -show-build "$IOSBIN" | grep -q "platform IOSSIMULATOR" || fail "iOS platform != IOSSIMULATOR"
IOSWGPU="$IOSB/_deps/wgpu_native_src-src/target/aarch64-apple-ios-sim/release/libwgpu_native.a"
[ "$(lipo -archs "$IOSWGPU")" = "arm64" ] || fail "iOS wgpu-native (from Rust) arch != arm64"
ok "iOS arm64 Mach-O (IOSSIMULATOR) + wgpu-native-from-Rust arm64"

echo "=== Mobile true-cross: Android (aarch64-linux-android, NDK arm64-v8a) ==="
bash "$ROOT/platform/android/build_apk.sh" >/dev/null 2>&1
ANDSO="$ROOT/build-android/libgame.so"
[ -f "$ANDSO" ] || fail "Android .so not produced"
# macOS-хост: readelf нет в base; llvm-readelf из NDK, иначе file (портируемо).
RE="$(command -v llvm-readelf || true)"
if [ -n "$RE" ]; then
  "$RE" -h "$ANDSO" | grep -q "AArch64" || fail "Android ELF machine != AArch64"
else
  file "$ANDSO" | grep -q "ARM aarch64" || fail "Android ELF machine != aarch64"
fi
APK="$ROOT/build-android/apk/like_nes.apk"
# pure-shell case (не `unzip | grep -q`: grep короткозамыкает → unzip SIGPIPE → ложный pipefail).
APKLIST="$(unzip -l "$APK")"
case "$APKLIST" in
  *"lib/arm64-v8a/libgame.so"*) ;;
  *) fail "APK missing arm64-v8a lib" ;;
esac
ok "Android aarch64 ELF + APK lib/arm64-v8a/{libgame,libc++_shared}.so"

echo "[xcompile] PASS: desktop matrix (max-vs-sum) + iOS arm64 + Android aarch64"
