#!/usr/bin/env bash
# Пин wgpu-native (аудит #21 A·3·2): конфигурация принимает только блоб, чья sha256 совпадает с
# артефактом релиза gfx-rs/wgpu-native из cmake/wgpu_native.sha256. Фикстуры — свой файл сумм и
# свой «блоб»; позитивный контроль на настоящем блобе — если каталог сборки уже сконфигурирован.
#
#   bash scripts/check_wgpu_pin.sh [<каталог сборки>]
set -uo pipefail

ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
cd "$ROOT" || exit 1
BUILD=${1:-build}
tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT
fail=0
native() { if command -v cygpath >/dev/null 2>&1; then cygpath -m "$1"; else printf '%s' "$1"; fi; }

expect() {
  local label=$1 file=$2 sums=$3 rc_want=$4 pattern=$5 out rc
  printf 'include(%s/cmake/wgpu_native_pin.cmake)\nlike_nes_wgpu_verify("%s" "%s")\nmessage(STATUS "verified")\n' \
    "$(native "$ROOT")" "$(native "$file")" "$(native "$sums")" > "$tmp/probe.cmake"
  out=$(cmake -P "$tmp/probe.cmake" 2>&1); rc=$?
  if { [ "$rc_want" = 0 ] && [ "$rc" -ne 0 ]; } || { [ "$rc_want" = 1 ] && [ "$rc" -eq 0 ]; } \
      || ! grep -qF -- "$pattern" <<<"$out"; then
    echo "FAIL ${label}: rc=${rc}, ждали ${rc_want} и «${pattern}»"; echo "$out" | sed 's/^/    /'
    fail=1
  else
    echo "ok   $label"
  fi
}

mkdir -p "$tmp/bin/linux-x86_64"
blob="$tmp/bin/linux-x86_64/libwgpu_native.so"
printf 'wgpu blob fixture\n' > "$blob"
sum=$(cmake -E sha256sum "$blob" | cut -d' ' -f1)
printf '%s  linux-x86_64/libwgpu_native.so\n' "$sum" > "$tmp/sums"
expect "блоб с пиннутой суммой принят" "$blob" "$tmp/sums" 0 "verified"
printf 'wgpu blob fixturE\n' > "$blob"
expect "изменённый байт — отказ" "$blob" "$tmp/sums" 1 "$sum"
printf 'wgpu blob fixture\n' > "$blob"
printf '%s  linux-aarch64/libwgpu_native.so\n' "$sum" > "$tmp/other"
expect "та же сумма под другой тройкой — отказ" "$blob" "$tmp/other" 1 "не пиннут"
printf '%s  x-linux-x86_64/libwgpu_native.so\n' "$sum" > "$tmp/suffix"
expect "ключ сравнивается целиком, не хвостом" "$blob" "$tmp/suffix" 1 "не пиннут"
expect "нет файла — отказ" "$tmp/bin/linux-x86_64/nope.so" "$tmp/sums" 1 "не пиннут"
printf '%s  linux-x86_64/nope.so\n' "$sum" >> "$tmp/sums"
expect "пиннутый, но отсутствующий файл — отказ" "$tmp/bin/linux-x86_64/nope.so" "$tmp/sums" 1 "нет файла"

lib=$(sed -n 's/^WGPU_RUNTIME_LIB:INTERNAL=//p' "$BUILD/CMakeCache.txt" 2>/dev/null)
if [ -n "$lib" ]; then
  expect "настоящий блоб сборки сверен с релизом" "$lib" "$ROOT/cmake/wgpu_native.sha256" 0 "verified"
else
  echo "skip настоящий блоб: $BUILD не сконфигурирован"
fi

[ "$fail" -eq 0 ] && echo "wgpu-pin: PASS" || echo "wgpu-pin: FAIL"
exit "$fail"
