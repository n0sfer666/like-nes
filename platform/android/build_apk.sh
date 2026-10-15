#!/usr/bin/env bash
set -euo pipefail

HERE="$(cd "$(dirname "$0")" && pwd)"
SDK="${ANDROID_HOME:-$HOME/Library/Android/sdk}"
NDK="${ANDROID_NDK_HOME:-$SDK/ndk/28.2.13676358}"
HOST_TAG="darwin-x86_64"
ABI="arm64-v8a"
API=24
BT="$SDK/build-tools/35.0.0"
ANDROID_JAR="$SDK/platforms/android-35/android.jar"
REPO="$(cd "$HERE/../.." && pwd)"
BUILD="$REPO/build-android"
TARGET=game
MANIFEST="$HERE/AndroidManifest.xml"
APK=like_nes.apk
BENCH=()
case "$#:${1:-}" in
  0:) ;;
  2:--bench)
    [ -f "$2" ] || { echo "build_apk: no crossplay file: $2" >&2; exit 1; }
    BUILD="$REPO/build-android-bench"; TARGET=bench; MANIFEST="$HERE/bench/AndroidManifest.xml"
    APK=like_nes_resim_bench.apk
    BENCH=(-DLIKE_NES_MOBILE_BENCH=ON "-DLIKE_NES_BENCH_AGAINST=$(cd "$(dirname "$2")" && pwd)/$(basename "$2")") ;;
  *) echo "usage: bash platform/android/build_apk.sh [--bench <crossplay file>]" >&2; exit 1 ;;
esac
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
cp "$NDK/toolchains/llvm/prebuilt/$HOST_TAG/sysroot/usr/lib/aarch64-linux-android/libc++_shared.so" \
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
  --manifest "$MANIFEST" \
  --min-sdk-version "$API" --target-sdk-version 35

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
