#!/usr/bin/env bash
# Настоящий Android source на host C++, публичные заголовки пиннутого NDK, мок лишь ввода ОС/JNI.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
[ "$#" -eq 1 ] || { echo 'usage: android_gamepad_selftest.sh DESKTOP_SDK_PREFIX' >&2; exit 1; }
PREFIX=$1
# shellcheck source=platform/android/android_sdk.sh
. "$ROOT/platform/android/android_sdk.sh"
# shellcheck source=platform/android/android_ndk.sh
. "$ROOT/platform/android/android_ndk.sh"
JDK=$(java -XshowSettings:properties -version 2>&1 | sed -n 's/^ *java.home = //p')
case "$(uname -s)" in
  Darwin) JNI_OS=darwin; NDK_HOST=darwin-x86_64; JVM_LIB="$JDK/lib/server/libjvm.dylib" ;;
  Linux) JNI_OS=linux; NDK_HOST=linux-x86_64; JVM_LIB="$JDK/lib/server/libjvm.so" ;;
  *) echo 'android-pad: FAIL host must be Darwin or Linux' >&2; exit 1 ;;
esac
WORK=$(mktemp -d)
trap 'rm -rf "$WORK"' EXIT
if ! { cmake -S "$ROOT/platform/android/tests/gamepad" -B "$WORK" -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -Dlike-nes_DIR="$PREFIX/lib/cmake/like-nes" -DJNI_HEADERS="$JDK/include" -DJNI_OS="$JNI_OS" \
  -DJNI_JVM_LIBRARY="$JVM_LIB" -DANDROID_PAD_SOURCE="$ROOT/platform/android/android_gamepad.cpp" \
  -DANDROID_PAD_DEVICE_SOURCE="$ROOT/platform/android/android_pad_device.cpp" \
  -DNDK_HEADERS="$NDK/toolchains/llvm/prebuilt/$NDK_HOST/sysroot/usr/include" && cmake --build "$WORK"; } \
  > "$WORK/build.log" 2>&1; then
  tail -25 "$WORK/build.log"; exit 1
fi
"$WORK/android_pad_test"
mkdir "$WORK/classes"
"$JDK/bin/javac" -encoding UTF-8 -d "$WORK/classes" \
  "$ROOT/platform/android/tests/jvm/android/view/InputDevice.java" \
  "$ROOT/platform/android/tests/jvm/android/os/VibrationEffect.java" \
  "$ROOT/platform/android/tests/jvm/android/os/Vibrator.java"
"$JDK/bin/javac" -encoding UTF-8 -cp "$WORK/classes" -d "$WORK/classes" \
  "$ROOT/platform/android/tests/jvm/android/content/Context.java"
if ! python3 -c 'import subprocess, sys; subprocess.run(sys.argv[1:], check=True, timeout=30)' \
  "$WORK/android_pad_jvm_test" "$WORK/classes" > "$WORK/jvm.log" 2>&1
then cat "$WORK/jvm.log"; exit 1; fi
cat "$WORK/jvm.log"
if grep -Eq 'WARNING in native method|FATAL ERROR in native method' "$WORK/jvm.log"; then exit 1; fi
