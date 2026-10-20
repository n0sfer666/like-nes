#!/usr/bin/env bash
# Печатает первый вход цели, который новее бинаря, — по графу ninja, а не по mtime каталога: файл
# чужой цели (golden_main.cpp, пакетный .cmake) сборку не трогает, и предупреждение о нём висело бы
# после любой пересборки. `ninja -n` тут слеп: проверка глобов CMake в сухом прогоне всегда
# «перезапускает CMake» и дальше не смотрит. Входы — `-t inputs`, заголовки — `-t deps` объектов.
# Свежий бинарь — пустой вывод; нет каталога сборки — печатается он сам.
#
#   bash scripts/stale_bin.sh <бинарь> <каталог>:<цель>[,<цель>]...
set -euo pipefail

[ "$#" -ge 2 ] || { echo "usage: bash scripts/stale_bin.sh <bin> <dir>:<target>[,<target>]..." >&2; exit 2; }
BIN=$1
shift
for spec in "$@"; do
  dir=${spec%%:*}
  IFS=, read -r -a targets <<<"${spec#*:}"
  [ -f "$dir/build.ninja" ] || { echo "$dir"; exit 0; }
  inputs=$(ninja -C "$dir" -t inputs "${targets[@]}")
  objects=()
  while IFS= read -r o; do objects+=("$o"); done < <(grep '\.o$' <<<"$inputs" || true)
  headers=$([ "${#objects[@]}" -eq 0 ] || ninja -C "$dir" -t deps "${objects[@]}" | awk '/^    /{print $1}')
  while IFS= read -r f; do
    [ -n "$f" ] || continue
    case "$f" in /*) p=$f ;; *) p=$dir/$f ;; esac
    [ -f "$p" ] && [ "$p" -nt "$BIN" ] && { echo "$p"; exit 0; }
  done <<<"$inputs"$'\n'"$headers"
done
