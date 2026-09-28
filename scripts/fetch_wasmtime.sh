#!/usr/bin/env bash
# wasmtime C-API для WASM-гейта (спека #6, gate #5) в deps/ — пиннутая версия, сверка sha256.
#
# Сумма фиксирует файл, а не обещание того, кто его выложил: тег релиза переставляется, а архив,
# линкуемый в песочницу untrusted-плагинов, судит всё, что эта песочница обещает (аудит #21 A·3·9).
# Версия и суммы — в cmake/wasmtime.sha256, их же читает конфигурация. После сверенной распаковки
# в каталог пишется штамп `.sha256`: без него конфигурация каталог не примет. Незнакомая тройка —
# отказ: непроверенный архив хуже отсутствующего, гейт без него просто пропускается.
#
#   bash scripts/fetch_wasmtime.sh                   # скачать для этой машины
#   bash scripts/fetch_wasmtime.sh --from <архив>    # сверить и распаковать уже скачанный
#
# WASMTIME_TRIPLE=<cpu>-<os> подменяет тройку хоста (Rosetta, кросс-сборка), WASMTIME_DEPS_DIR —
# каталог deps/.
set -euo pipefail

ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
DEPS=${WASMTIME_DEPS_DIR:-$ROOT/deps}
from=""
if [ "${1:-}" = "--from" ]; then
  from=${2:?--from требует путь к архиву}
  [ -f "$from" ] || { echo "fetch_wasmtime: нет файла $from" >&2; exit 1; }
fi

case "$(uname -m)" in arm64|aarch64) cpu=aarch64 ;; amd64) cpu=x86_64 ;; *) cpu=$(uname -m) ;; esac
case "$(uname -s)" in Darwin) os=macos ;; Linux) os=linux ;; *) os=$(uname -s) ;; esac
triple=${WASMTIME_TRIPLE:-$cpu-$os}
pin=$(awk -v t="$triple" '{ n = $2; sub(/^wasmtime-v[0-9.]+-/, "", n); sub(/-c-api\.tar\.xz$/, "", n)
                            if (n == t) print }' "$ROOT/cmake/wasmtime.sha256")
[ -n "$pin" ] || { echo "fetch_wasmtime: wasmtime не пиннут для тройки $triple" >&2; exit 1; }
sum=${pin%%  *}
file=${pin#*  }
name=${file%.tar.xz}
version=${name#wasmtime-}
version=${version%%-*}

if [ -d "$DEPS/$name" ]; then
  if [ "$(cat "$DEPS/$name/.sha256" 2>/dev/null || true)" = "$sum" ]; then
    echo "fetch_wasmtime: $name уже в deps/ (sha256 сверена)"
    exit 0
  fi
  echo "fetch_wasmtime: deps/$name есть, но не сверен с пином — удалите его и запустите снова" >&2
  exit 1
fi

tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT
archive="$tmp/$file"
if [ -n "$from" ]; then
  cp "$from" "$archive"
else
  curl -fsSL "https://github.com/bytecodealliance/wasmtime/releases/download/$version/$file" \
    -o "$archive"
fi

if command -v sha256sum >/dev/null 2>&1; then
  got=$(sha256sum "$archive" | cut -d' ' -f1)
else
  got=$(shasum -a 256 "$archive" | cut -d' ' -f1)
fi
if [ "$got" != "$sum" ]; then
  echo "fetch_wasmtime: sha256 архива $file — $got, пиннут $sum; ничего не распаковано" >&2
  exit 1
fi

mkdir -p "$DEPS"
tar -xf "$archive" -C "$DEPS"
echo "$sum" > "$DEPS/$name/.sha256"
echo "fetch_wasmtime: $name распакован в deps/ (sha256 $sum)"
