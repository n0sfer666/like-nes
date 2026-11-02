#!/usr/bin/env bash
# Мок таблицы JNI: исключение на каждом вызове не допускает следующих вызовов с pending exception.
# Хостовый JDK уже нужен aapt2/apksigner; JVM тест не запускает, нужны только публичные jni.h.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
JDK=$(java -XshowSettings:properties -version 2>&1 | sed -n 's/^ *java.home = //p')
case "$(uname -s)" in
  Darwin) JNI_OS=darwin ;;
  Linux) JNI_OS=linux ;;
  *) echo 'android-jni: FAIL host must be Darwin or Linux' >&2; exit 1 ;;
esac
[ -f "$JDK/include/jni.h" ] || { echo 'android-jni: FAIL no JDK headers' >&2; exit 1; }
WORK=$(mktemp -d)
trap 'rm -rf "$WORK"' EXIT
"${CXX:-c++}" -std=c++20 -Wall -Wextra -Werror -pedantic \
  -I "$JDK/include" -I "$JDK/include/$JNI_OS" -I "$ROOT/platform/android" \
  "$ROOT/platform/android/android_jni_test.cpp" -o "$WORK/android_jni_test"
"$WORK/android_jni_test"
# Тот же дефект, что нашло ревью: сырой NewStringUTF после предыдущего исключения.
cp "$ROOT/platform/android/android_jni_test.cpp" "$WORK/"
sed 's/return check() ? env_->NewStringUTF(value) : nullptr;/return env_->NewStringUTF(value);/' \
  "$ROOT/platform/android/android_jni.hpp" > "$WORK/android_jni.hpp"
cmp -s "$ROOT/platform/android/android_jni.hpp" "$WORK/android_jni.hpp" && {
  echo 'android-jni: FAIL mutant changed nothing' >&2; exit 1;
}
"${CXX:-c++}" -std=c++20 -Wall -Wextra -Werror -pedantic \
  -I "$JDK/include" -I "$JDK/include/$JNI_OS" \
  "$WORK/android_jni_test.cpp" -o "$WORK/mutant"
if "$WORK/mutant" > "$WORK/mutant.log"; then
  echo 'android-jni: FAIL unchecked NewStringUTF mutant survived' >&2; exit 1
fi
grep -q '^android-jni: FAIL exception at call ' "$WORK/mutant.log"
echo 'android-jni: PASS unchecked NewStringUTF mutant refused'
