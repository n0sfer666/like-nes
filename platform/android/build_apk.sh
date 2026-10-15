#!/usr/bin/env bash
set -euo pipefail

HERE="$(cd "$(dirname "$0")" && pwd)"
# shellcheck source=platform/android/android_sdk.sh
. "$HERE/android_sdk.sh"
BT="$SDK/build-tools/35.0.0"
ANDROID_JAR="$SDK/platforms/android-35/android.jar"
REPO="$(cd "$HERE/../.." && pwd)"
TARGET=game
MANIFEST="$HERE/AndroidManifest.xml"
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
      DIR=build-android-bench; TARGET=bench; MANIFEST="$HERE/bench/AndroidManifest.xml"
      APK=like_nes_resim_bench.apk
      BENCH=(-DLIKE_NES_MOBILE_BENCH=ON "-DLIKE_NES_BENCH_AGAINST=$(cd "$(dirname "$2")" && pwd)/$(basename "$2")")
      shift 2 ;;
    *) usage ;;
  esac
done
# Каталог сборки без суффикса — у ABI телефона; у остальных свой: кэш помнит ANDROID_ABI.
case "$ABI" in
  arm64-v8a) TRIPLE=aarch64-linux-android ;;
  x86_64) TRIPLE=x86_64-linux-android; DIR="$DIR-x86_64" ;;
  *) echo "build_apk: unsupported ABI $ABI" >&2; usage ;;
esac
BUILD="$REPO/$DIR"
# Минимальный API — манифест игры: им же собираются .so и wgpu-native (сверку делает
# wgpu_native.cmake). Манифест бенча обязан совпасть, иначе бенч мерил бы другой bionic.
min_sdk() {
  local v
  v=$(sed -n 's/.*android:minSdkVersion="\([0-9]*\)".*/\1/p' "$1" | head -1)
  case "$v" in '' | *[!0-9]*) echo "build_apk: no android:minSdkVersion in $1" >&2; exit 1 ;; esac
  echo "$v"
}
API=$(min_sdk "$HERE/AndroidManifest.xml")
[ "$(min_sdk "$MANIFEST")" = "$API" ] || { echo "build_apk: minSdkVersion of $MANIFEST differs from $HERE/AndroidManifest.xml" >&2; exit 1; }
OUT="$BUILD/apk"
. "$REPO/platform/mobile/fresh_cache.sh"

# Оболочка — подкаталог корня: цели движка и игра приходят тем же графом, что на десктопе.
FRESH="$(fresh_flag "$BUILD" "$REPO")"
cmake ${FRESH:+"$FRESH"} -S "$REPO" -B "$BUILD" -G Ninja \
  -DCMAKE_TOOLCHAIN_FILE="$NDK/build/cmake/android.toolchain.cmake" \
  -DANDROID_ABI="$ABI" -DANDROID_PLATFORM="android-$API" ${BENCH[@]+"${BENCH[@]}"}
cmake --build "$BUILD" --target "$TARGET" -j8

rm -rf "$OUT"; mkdir -p "$OUT/lib/$ABI" "$OUT/assets/licenses"
cp "$BUILD/lib$TARGET.so" "$OUT/lib/$ABI/"
cp "$NDK/toolchains/llvm/prebuilt/$HOST_TAG/sysroot/usr/lib/$TRIPLE/libc++_shared.so" \
   "$OUT/lib/$ABI/"

# Бандлы игры — в корень assets/: native_main.cpp распаковывает их оттуда в песочницу.
if [ "$TARGET" = bench ]; then
  cp "$BUILD/resim_bench/crossplay.txt" "$OUT/assets/"
else
  for b in game library audio; do cp "$REPO/example_ugly_game/assets/$b.bundle" "$OUT/assets/"; done
fi
while read -r lic || [ -n "$lic" ]; do
  [ -n "$lic" ] || continue
  [ -f "$REPO/$lic" ] || { echo "spec #9: лицензионный файл отсутствует: $REPO/$lic" >&2; exit 1; }
  cp "$REPO/$lic" "$OUT/assets/licenses/"
done < "$REPO/cmake/licenses.manifest"

"$BT/aapt2" link -o "$OUT/base.apk" -I "$ANDROID_JAR" \
  --manifest "$MANIFEST" --target-sdk-version 35

( cd "$OUT" && zip -q -r base.apk lib assets )
"$BT/zipalign" -f 4 "$OUT/base.apk" "$OUT/$APK"

KS="$HOME/.android/debug.keystore"
if [ ! -f "$KS" ]; then
  keytool -genkeypair -keystore "$KS" -storepass android -keypass android \
    -alias androiddebugkey -dname "CN=Android Debug,O=Android,C=US" \
    -keyalg RSA -keysize 2048 -validity 10000
fi
"$BT/apksigner" sign --ks "$KS" --ks-pass pass:android --key-pass pass:android \
  --ks-key-alias androiddebugkey "$OUT/$APK"

echo "APK ready: $OUT/$APK"
