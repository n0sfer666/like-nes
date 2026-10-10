#!/usr/bin/env bash
# Печатает исходники .cpp цели из её add_library — путями от корня репозитория, по одному в строке.
#
# Запуск:
#   bash scripts/cmake_target_sources.sh engine/input/CMakeLists.txt input_core
#
# Рукописный список TU для сборки мимо CMake (ASan-шаги CI) расходится с целью на первом же новом
# файле, и ошибкой это становится у компоновщика. Пустой результат — код 1: шаг, собравший тест без
# ядра, упал бы по непонятной причине или, хуже, собрал бы не то. Источник, которого нет на диске,
# — тоже код 1 со своим именем, а не «no such file» у компилятора.
set -euo pipefail

if [ "$#" -ne 2 ]; then
    echo "usage: bash scripts/cmake_target_sources.sh <CMakeLists.txt> <target>" >&2
    exit 2
fi
lists=$1
target=$2
dir=$(dirname "$lists")

# shellcheck disable=SC2016
sources=$(awk -v t="$target" '
    { sub(/#.*/, "") }
    !f && index($0, "add_library(" t " ") { f = 1 }
    f { print }
    f && /\)/ { exit }
' "$lists" | sed 's|\${CMAKE_CURRENT_SOURCE_DIR}/||g' | grep -oE '[A-Za-z0-9_./-]+\.cpp' || true)

if [ -z "$sources" ]; then
    echo "cmake_target_sources: no .cpp sources of '$target' in $lists" >&2
    exit 1
fi
for src in $sources; do
    if [ ! -f "$dir/$src" ]; then
        echo "cmake_target_sources: '$target' lists $src, but $dir/$src does not exist" >&2
        exit 1
    fi
done
printf '%s\n' "$sources" | sed "s|^|$dir/|"
