# NDK сборки дерева — после android_sdk.sh, из build_apk.sh. Раннеры GitHub выставляют
# ANDROID_NDK_HOME на NDK своего образа: чужой clang и чужой libc++_shared.so собрались бы молча,
# поэтому ревизия выбранного NDK сверяется с пином.
# shellcheck shell=bash
NDK_PIN=28.2.13676358
NDK="${ANDROID_NDK_HOME:-$SDK/ndk/$NDK_PIN}"
ndk_rev=$(sed -n 's/^Pkg.Revision *= *//p' "$NDK/source.properties" 2>/dev/null || true)
[ "$ndk_rev" = "$NDK_PIN" ] || {
  echo "android_ndk: NDK at $NDK is '${ndk_rev:-missing}', need $NDK_PIN (sdkmanager \"ndk;$NDK_PIN\")" >&2
  exit 1
}
