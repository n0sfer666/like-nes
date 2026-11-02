#!/usr/bin/env bash
# Build the real mobile game against fresh installed prefixes; no emulator/device access.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
[ "$#" -eq 2 ] || { echo 'usage: android_gate_build.sh WORK arm64-v8a|x86_64' >&2; exit 2; }
mkdir -p "$1"
WORK="$(cd "$1" && pwd)"
ABI=$2
case "$ABI" in arm64-v8a|x86_64) ;; *) exit 2 ;; esac
# shellcheck source=platform/android/android_sdk.sh
. "$ROOT/platform/android/android_sdk.sh"
# shellcheck source=platform/android/android_ndk.sh
. "$ROOT/platform/android/android_ndk.sh"
# shellcheck source=platform/mobile/fresh_cache.sh
. "$ROOT/platform/mobile/fresh_cache.sh"
# shellcheck source=scripts/mobile_prefix_lib.sh
. "$ROOT/scripts/mobile_prefix_lib.sh"
run() {
    "$@" >> "$WORK/build.log" 2>&1 || {
        tail -25 "$WORK/build.log" >&2; echo "[android-emu] FAIL: build — $WORK/build.log" >&2; exit 1; }
}
HOST="$WORK/host"
PFX="$WORK/mobile"
[ ! -e "$HOST" ] && [ ! -e "$PFX" ] || {
    echo '[android-emu] FAIL: build needs a fresh WORK directory' >&2; exit 1; }
HBUILD="$ROOT/build-sdk-release"
MBUILD="$ROOT/build-sdk-android-$ABI"
FRESH=$(fresh_flag "$HBUILD" "$ROOT")
run cmake ${FRESH:+"$FRESH"} -S "$ROOT" -B "$HBUILD" -G Ninja -DCMAKE_BUILD_TYPE=Release \
    -DAUDIO_MINIAUDIO=OFF -DPLUGIN_UI=OFF -DPLUGIN_WASM=OFF -DIDE_POC=OFF
run cmake --build "$HBUILD" --target like_nes_sdk assetc
run cmake --install "$HBUILD" --component sdk --prefix "$HOST"
API=$(android_min_api)
FLAGS=(-DCMAKE_TOOLCHAIN_FILE="$NDK/build/cmake/android.toolchain.cmake" -DANDROID_ABI="$ABI"
    -DANDROID_PLATFORM="android-$API")
FRESH=$(fresh_flag "$MBUILD" "$ROOT")
run cmake ${FRESH:+"$FRESH"} -S "$ROOT" -B "$MBUILD" -G Ninja -DCMAKE_BUILD_TYPE=Release "${FLAGS[@]}"
run cmake --build "$MBUILD" --target like_nes_sdk
run cmake --install "$MBUILD" --component sdk --prefix "$PFX"
TOOL=$(bash "$ROOT/scripts/rumble_tool.sh" "$HOST" "$WORK/tool")
run cmake -S "$ROOT/games/neon-rumble" -B "$WORK/game" -G Ninja -DCMAKE_BUILD_TYPE=Release \
    "${FLAGS[@]}" -Dlike-nes_DIR="$PFX/lib/cmake/like-nes" -DLIKE_NES_HOST_PREFIX="$HOST" -DRUMBLE_ASSETC="$TOOL"
run cmake --build "$WORK/game"
[ -f "$WORK/game/neon_rumble.apk" ] || { echo '[android-emu] FAIL: no APK' >&2; exit 1; }
echo "[android-emu] built $WORK/game/neon_rumble.apk (ABI $ABI, minimum API $API)"
