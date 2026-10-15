# Подключается из build_apk.sh и scripts/mobile_bench.sh: SDK, пин NDK и префикс его тулчейна
# выбираются в одном месте, иначе на Linux без ANDROID_HOME сборка шла бы из ~/Android/Sdk, а adb
# искался бы по маковскому пути.
# shellcheck shell=bash
NDK_PIN=28.2.13676358
# SC2034: HOST_TAG читает build_apk.sh, а shellcheck видит один файл, а не связку.
# shellcheck disable=SC2034
case "$(uname -s):$(uname -m)" in
  Darwin:*) HOST_TAG="darwin-x86_64"; SDK_DEFAULT="$HOME/Library/Android/sdk" ;;
  Linux:x86_64) HOST_TAG="linux-x86_64"; SDK_DEFAULT="$HOME/Android/Sdk" ;;
  *) echo "android_sdk: no NDK host for $(uname -s) $(uname -m)" >&2; exit 1 ;;
esac
SDK="${ANDROID_HOME:-$SDK_DEFAULT}"
# Раннеры GitHub выставляют ANDROID_NDK_HOME на NDK своего образа: чужой clang и чужой
# libc++_shared.so собрались бы молча, поэтому ревизия выбранного NDK сверяется с пином.
NDK="${ANDROID_NDK_HOME:-$SDK/ndk/$NDK_PIN}"
ndk_rev=$(sed -n 's/^Pkg.Revision *= *//p' "$NDK/source.properties" 2>/dev/null || true)
[ "$ndk_rev" = "$NDK_PIN" ] || {
  echo "android_sdk: NDK at $NDK is '${ndk_rev:-missing}', need $NDK_PIN (sdkmanager \"ndk;$NDK_PIN\")" >&2
  exit 1
}
