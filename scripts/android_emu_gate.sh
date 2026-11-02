#!/usr/bin/env bash
# Dedicated rootable AVD only. No connected phone or user AVD is selected or reset.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
REQUIRED=0
IMAGE=
CACHE=
fail() { echo "[android-emu] FAIL: $*" >&2; exit 1; }
missing() {
    [ "$REQUIRED" -eq 0 ] || fail "$*"
    echo "[android-emu] SKIP: $*"; exit 0
}
while [ "$#" -gt 0 ]; do
    case "$1" in
        --required) REQUIRED=1; shift ;;
        --image) [ "$#" -ge 2 ] || fail '--image needs a package'; IMAGE=$2; shift 2 ;;
        --avd-cache) [ "$#" -ge 2 ] || fail '--avd-cache needs a directory'; CACHE=$2; shift 2 ;;
        *) fail 'usage: android_emu_gate.sh [--required] [--image PACKAGE] [--avd-cache DIR]' ;;
    esac
done
case "$(uname -s):$(uname -m)" in
    Darwin:arm64) ARCH=arm64-v8a ;;
    Linux:x86_64) ARCH=x86_64 ;;
    *) missing 'Android emulator gate supports macOS arm64 and Linux x86_64 hosts' ;;
esac
# shellcheck source=platform/android/android_sdk.sh
. "$ROOT/platform/android/android_sdk.sh"
for program in python3 cmake ninja cargo java; do
    command -v "$program" >/dev/null || missing "no $program in PATH"
done
for binary in platform-tools/adb emulator/emulator cmdline-tools/latest/bin/avdmanager; do
    [ -x "$SDK/$binary" ] || missing "no Android SDK $binary"
done
if [ -z "$IMAGE" ]; then
    for api in 35 36 34 33 32 31 30 29; do
        for tag in google_apis default; do
            if [ -f "$SDK/system-images/android-$api/$tag/$ARCH/system.img" ]; then
                IMAGE="system-images;android-$api;$tag;$ARCH"; break 2
            fi
        done
    done
fi
[ -n "$IMAGE" ] || missing "no rootable API29+ image for $ARCH (SDK Manager > System Images)"
[ "${IMAGE##*;}" = "$ARCH" ] || fail "image ABI must be $ARCH on this host"
IMAGE_PATH=${IMAGE//;/\/}
[ -f "$SDK/$IMAGE_PATH/system.img" ] || missing "system image is not installed: $IMAGE"
# The real build checks the pinned NDK and build-tools; do not download tools during preflight.
# shellcheck source=platform/android/android_ndk.sh
. "$ROOT/platform/android/android_ndk.sh"
WORK=$(mktemp -d "${TMPDIR:-/tmp}/like-nes-android-touch.XXXXXX")
ARGS=(--sdk "$SDK" --work "$WORK" --image "$IMAGE")
[ -z "$CACHE" ] || ARGS+=(--avd-cache "$CACHE")
echo "[android-emu] own AVD, $IMAGE, Vulkan/SwiftShader; evidence $WORK"
python3 "$ROOT/scripts/android_gate_run.py" "${ARGS[@]}"
