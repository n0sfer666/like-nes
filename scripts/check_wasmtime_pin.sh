#!/usr/bin/env bash
# Пин wasmtime C-API (аудит #21 A·3·9): конфигурация берёт из deps/ только пиннутый каталог со
# сверенным штампом, чужой отбивает, а fetch_wasmtime.sh не распаковывает архив с чужой суммой.
# Каждый кейс — своя фикстура deps/ во временном каталоге; сеть нужна только `--live`, который
# качает настоящий архив для этой машины (позитивный контроль fetch, вне preflight).
set -uo pipefail

ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
cd "$ROOT" || exit 1
tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT
fail=0
# Путь внутрь .cmake-файла: MSYS переводит в нативную форму аргументы, но не содержимое файлов.
native() { if command -v cygpath >/dev/null 2>&1; then cygpath -m "$1"; else printf '%s' "$1"; fi; }

probe() {
  printf 'include(%s/cmake/wasmtime_pin.cmake)\n%s\nmessage(STATUS "out=[${out}]")\n' \
    "$(native "$ROOT")" "$1" > "$tmp/probe.cmake"
  cmake -P "$tmp/probe.cmake" 2>&1
}

expect() {
  local label=$1 call=$2 rc_want=$3 pattern=$4 out rc
  out=$(probe "$call"); rc=$?
  if { [ "$rc_want" = 0 ] && [ "$rc" -ne 0 ]; } || { [ "$rc_want" = 1 ] && [ "$rc" -eq 0 ]; } \
      || ! grep -qF -- "$pattern" <<<"$out"; then
    echo "FAIL ${label}: rc=${rc}, ждали ${rc_want} и «${pattern}»"; echo "$out" | sed 's/^/    /'
    fail=1
  else
    echo "ok   $label"
  fi
}
dir_case() { expect "$1" "like_nes_wasmtime_dir(\"$(native "$2")\" \"$3\" out)" "$4" "$5"; }

pin_line=$(grep -- '-aarch64-linux-c-api\.tar\.xz$' cmake/wasmtime.sha256)
sum=${pin_line%%  *}
pin=${pin_line#*  }; pin=${pin%.tar.xz}
version=${pin#wasmtime-}; version=${version%%-*}
mk() { mkdir -p "$1/include" && : > "$1/include/wasmtime.h" && echo "${2:-$sum}" > "$1/.sha256"; }

expect "тройка Darwin/arm64" 'like_nes_wasmtime_triple(Darwin arm64 out)' 0 "out=[aarch64-macos]"
expect "тройка Linux/AMD64" 'like_nes_wasmtime_triple(Linux AMD64 out)' 0 "out=[x86_64-linux]"
expect "тройка Windows/AMD64" 'like_nes_wasmtime_triple(Windows AMD64 out)' 0 "out=[x86_64-Windows]"

mkdir -p "$tmp/empty"
dir_case "пустой deps/ — гейт пропущен с подсказкой" "$tmp/empty" aarch64-linux 0 "не найден в deps/"
mk "$tmp/pinned/$pin"
dir_case "пиннутый каталог со штампом найден" "$tmp/pinned" aarch64-linux 0 "out=[$(native "$tmp/pinned/$pin")]"
mk "$tmp/nostamp/$pin"; rm "$tmp/nostamp/$pin/.sha256"
dir_case "пиннутый каталог без штампа — отказ" "$tmp/nostamp" aarch64-linux 1 "не сверен"
mk "$tmp/badstamp/$pin" 0000
dir_case "штамп чужой суммы — отказ" "$tmp/badstamp" aarch64-linux 1 "не сверен"
mk "$tmp/v99/wasmtime-v99.0.0-aarch64-linux-c-api"
dir_case "чужая версия — отказ" "$tmp/v99" aarch64-linux 1 "wasmtime-v99.0.0-aarch64-linux-c-api"
mk "$tmp/both/$pin"; mk "$tmp/both/wasmtime-v99.0.0-aarch64-linux-c-api"
dir_case "чужая версия рядом с пином — отказ" "$tmp/both" aarch64-linux 1 "wasmtime-v99.0.0"
mk "$tmp/triple/wasmtime-$version-x86_64-linux-c-api" "$(grep -- '-x86_64-linux-c-api' cmake/wasmtime.sha256 | cut -d' ' -f1)"
dir_case "сверенная соседняя тройка не мешает" "$tmp/triple" aarch64-linux 0 "не найден в deps/"
mk "$tmp/triple/$pin"
dir_case "соседняя тройка рядом с пином — берётся пин" "$tmp/triple" aarch64-linux 0 "out=[$(native "$tmp/triple/$pin")]"
mk "$tmp/rawtriple/wasmtime-$version-x86_64-linux-c-api" 0000
dir_case "несверенная соседняя тройка — отказ" "$tmp/rawtriple" aarch64-linux 1 "не сверен"
mk "$tmp/nohdr/$pin"; rm "$tmp/nohdr/$pin/include/wasmtime.h"
dir_case "пин без заголовка — гейт пропущен" "$tmp/nohdr" aarch64-linux 0 "out=[]"
mkdir -p "$tmp/archive"; : > "$tmp/archive/$pin.tar.xz"
dir_case "архив рядом — не каталог, не отказ" "$tmp/archive" aarch64-linux 0 "out=[]"
mk "$tmp/unpinned/wasmtime-v99.0.0-x86_64-windows-c-api"
dir_case "непиннутая платформа — пропуск, не отказ" "$tmp/unpinned" x86_64-Windows 0 "не пиннут"
if probe "like_nes_wasmtime_dir(\"$(native "$tmp/unpinned")\" x86_64-Windows out)" | grep -q fetch_wasmtime; then
  echo "FAIL непиннутая платформа: подсказка про fetch_wasmtime.sh, который на ней откажет"; fail=1
else
  echo "ok   непиннутая платформа — без подсказки про fetch"
fi

fetch() { WASMTIME_TRIPLE=aarch64-linux WASMTIME_DEPS_DIR="$1" bash scripts/fetch_wasmtime.sh "${@:2}" 2>&1; }
fetch_case() {
  local label=$1 pattern=$2 deps=$3 out
  if out=$(fetch "$deps" "${@:4}") || ! grep -qF -- "$pattern" <<<"$out" || [ -e "$deps/$pin/include" ]; then
    echo "FAIL ${label}: ${out}"; fail=1
  else
    echo "ok   $label"
  fi
}
printf 'not wasmtime' > "$tmp/fake.tar.xz"
fetch_case "архив с чужой суммой не распакован" "sha256" "$tmp/f1" --from "$tmp/fake.tar.xz"
fetch_case "--from без файла — отказ" "нет файла" "$tmp/f2" --from "$tmp/nope.tar.xz"
mkdir -p "$tmp/f3/$pin"
fetch_case "несверенный каталог — отказ, не «уже в deps/»" "не сверен" "$tmp/f3"
if out=$(WASMTIME_TRIPLE=x86_64-plan9 bash scripts/fetch_wasmtime.sh 2>&1) || ! grep -q "не пиннут" <<<"$out"; then
  echo "FAIL незнакомая тройка: $out"; fail=1
else
  echo "ok   незнакомая тройка — отказ"
fi

if [ "${1:-}" = "--live" ]; then
  if out=$(WASMTIME_DEPS_DIR="$tmp/live" bash scripts/fetch_wasmtime.sh 2>&1) \
      && again=$(WASMTIME_DEPS_DIR="$tmp/live" bash scripts/fetch_wasmtime.sh 2>&1) \
      && grep -q "sha256 сверена" <<<"$again"; then
    echo "ok   --live: $out"
    live=$(grep -o 'wasmtime-v[0-9.]*-[a-z0-9_]*-[a-z]*-c-api' <<<"$out" | head -1)
    triple=${live#wasmtime-v*-}; triple=${triple%-c-api}
    dir_case "--live: конфигурация принимает штамп fetch" "$tmp/live" "$triple" 0 "out=[$(native "$tmp/live/$live")]"
  else
    echo "FAIL --live: $out ${again:-}"; fail=1
  fi
fi

[ "$fail" -eq 0 ] && echo "wasmtime-pin: PASS" || echo "wasmtime-pin: FAIL"
exit "$fail"
