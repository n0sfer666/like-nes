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
BUILD="$HERE/../build-android"
OUT="$BUILD/apk"

cmake -S "$HERE" -B "$BUILD" -G Ninja \
  -DCMAKE_TOOLCHAIN_FILE="$NDK/build/cmake/android.toolchain.cmake" \
  -DANDROID_ABI="$ABI" -DANDROID_PLATFORM="android-$API"
cmake --build "$BUILD" -j8

rm -rf "$OUT"; mkdir -p "$OUT/lib/$ABI"
cp "$BUILD/libgame.so" "$OUT/lib/$ABI/"
cp "$NDK/toolchains/llvm/prebuilt/$HOST_TAG/sysroot/usr/lib/aarch64-linux-android/libc++_shared.so" \
   "$OUT/lib/$ABI/"

"$BT/aapt2" link -o "$OUT/base.apk" -I "$ANDROID_JAR" \
  --manifest "$HERE/AndroidManifest.xml" \
  --min-sdk-version "$API" --target-sdk-version 35

( cd "$OUT" && zip -q -r base.apk lib )
"$BT/zipalign" -f 4 "$OUT/base.apk" "$OUT/like_nes.apk"

KS="$HOME/.android/debug.keystore"
if [ ! -f "$KS" ]; then
  keytool -genkeypair -keystore "$KS" -storepass android -keypass android \
    -alias androiddebugkey -dname "CN=Android Debug,O=Android,C=US" \
    -keyalg RSA -keysize 2048 -validity 10000
fi
"$BT/apksigner" sign --ks "$KS" --ks-pass pass:android --key-pass pass:android \
  --ks-key-alias androiddebugkey "$OUT/like_nes.apk"

echo "APK ready: $OUT/like_nes.apk"
