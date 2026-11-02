#!/usr/bin/env bash
# Actual host implementation; only the two Android OS-info APIs are stubbed on the host compiler.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
WORK=$(mktemp -d)
trap 'rm -rf "$WORK"' EXIT
mkdir -p "$WORK/android" "$WORK/sys"
cat > "$WORK/android/api-level.h" <<'HEADER'
#pragma once
extern "C" int android_get_device_api_level();
HEADER
cat > "$WORK/sys/system_properties.h" <<'HEADER'
#pragma once
#define PROP_VALUE_MAX 92
extern "C" int __system_property_get(const char*, char*);
HEADER
build() {
    "${CXX:-c++}" -std=c++20 -Wall -Wextra -Werror -pedantic -pthread \
        -I "$WORK" -I "$ROOT/platform/android" -I "$ROOT/platform/mobile" \
        "$1" "$ROOT/platform/android/android_host_test.cpp" -o "$2"
}
build "$ROOT/platform/android/android_host.cpp" "$WORK/android_host_test"
"$WORK/android_host_test"
sed 's/if (word == flag)/if (word.find(flag) != std::string::npos)/' \
    "$ROOT/platform/android/android_host.cpp" > "$WORK/mutant.cpp"
cmp -s "$ROOT/platform/android/android_host.cpp" "$WORK/mutant.cpp" && {
    echo 'android-host: FAIL mutant changed nothing' >&2; exit 1; }
build "$WORK/mutant.cpp" "$WORK/mutant"
if "$WORK/mutant" > "$WORK/mutant.log"; then
    echo 'android-host: FAIL substring flag mutant survived' >&2; exit 1
fi
grep -q '^android-host: FAIL no implicit demo or substring matching' "$WORK/mutant.log"
echo 'android-host: PASS substring flag mutant refused'
