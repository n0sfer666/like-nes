#!/usr/bin/env bash
set -euo pipefail

HERE="$(cd "$(dirname "$0")" && pwd)"
# shellcheck source=platform/android/android_sdk.sh
. "$HERE/android_sdk.sh"
# shellcheck source=platform/android/android_ndk.sh
. "$HERE/android_ndk.sh"
REPO="$(cd "$HERE/../.." && pwd)"
TARGET=game
APK=like_nes.apk
ABI=arm64-v8a
DIR=build-android
BENCH=()
usage() { echo "usage: bash platform/android/build_apk.sh [--abi arm64-v8a|x86_64] [--bench <crossplay file>]" >&2; exit 1; }
while [ "$#" -gt 0 ]; do
  case "$1" in
    --abi) [ "$#" -ge 2 ] || usage; ABI=$2; shift 2 ;;
    --bench)
      [ "$#" -ge 2 ] || usage
      [ -f "$2" ] || { echo "build_apk: no crossplay file: $2" >&2; exit 1; }
      DIR=build-android-bench; TARGET=bench
      APK=like_nes_resim_bench.apk
      BENCH=(-DLIKE_NES_MOBILE_BENCH=ON "-DLIKE_NES_BENCH_AGAINST=$(cd "$(dirname "$2")" && pwd)/$(basename "$2")")
      shift 2 ;;
    *) usage ;;
  esac
done
# Каталог сборки без суффикса — у ABI телефона; у остальных свой: кэш помнит ANDROID_ABI.
case "$ABI" in
  arm64-v8a) ;;
  x86_64) DIR="$DIR-x86_64" ;;
  *) echo "build_apk: unsupported ABI $ABI" >&2; usage ;;
esac
BUILD="$REPO/$DIR"
# Минимальный API — шаблон манифеста приложений: им же собираются .so и wgpu-native (сверку делает
# wgpu_native.cmake), и он же уезжает в префикс SDK.
API=$(sed -n 's/.*android:minSdkVersion="\([0-9]*\)".*/\1/p' "$HERE/AndroidManifest.xml.in" | head -1)
case "$API" in '' | *[!0-9]*) echo "build_apk: no android:minSdkVersion in $HERE/AndroidManifest.xml.in" >&2; exit 1 ;; esac
OUT="$BUILD/apk"
. "$REPO/platform/mobile/fresh_cache.sh"

# Оболочка — подкаталог корня: цели движка и игра приходят тем же графом, что на десктопе.
FRESH="$(fresh_flag "$BUILD" "$REPO")"
cmake ${FRESH:+"$FRESH"} -S "$REPO" -B "$BUILD" -G Ninja \
  -DCMAKE_TOOLCHAIN_FILE="$NDK/build/cmake/android.toolchain.cmake" \
  -DANDROID_ABI="$ABI" -DANDROID_PLATFORM="android-$API" ${BENCH[@]+"${BENCH[@]}"}
# Упаковку делает цель `<цель>_apk` (like_nes_mobile_app → package_apk.sh); сюда — копия под
# прежним именем, которое ждут mobile_bench.sh, xcompile_verify.sh и runbook.
cmake --build "$BUILD" --target "${TARGET}_apk" -j8
mkdir -p "$OUT"
cp "$BUILD/platform/android/$TARGET.apk" "$OUT/$APK"
echo "APK ready: $OUT/$APK"
