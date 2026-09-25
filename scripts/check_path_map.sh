#!/usr/bin/env bash
# Абсолютный путь дерева не попадает в исполняемые файлы (аудит #21 A·3·7). Шапка
# cmake/determinism.cmake обещает канонизацию на всех ОС, а до A·3·7 на MSVC её не было: путь
# каталога — с именем учётной записи — уезжал в пакет, и проверка на macOS этого не видела.
#
# Судит пробу path_map_probe (она вшивает свой __FILE__) и продукты пакета assetc и editor_shell.
# Позитивный контроль: строка, которую проба печатает, обязана находиться тем же поиском в её
# бинаре — иначе поиск, не видящий строк в исполняемом файле, был бы зелёным всегда.
# Негативный контроль (`--selftest`): та же проба, собранная компилятором этого каталога БЕЗ
# карты, обязана нести путь дерева — иначе зелёный прогон значил бы «компилятор не вшивает
# абсолютных путей», а не «карта работает» (на MSVC это допущение, что Ninja отдаёт cl полный путь).
#
#   bash scripts/check_path_map.sh <каталог сборки>
#   bash scripts/check_path_map.sh --selftest <каталог сборки>
set -uo pipefail

selftest=0
[ "${1:-}" = "--selftest" ] && { selftest=1; shift; }
BUILD=${1:?каталог сборки}
CACHE="$BUILD/CMakeCache.txt"
src=$(sed -n 's/^CMAKE_HOME_DIRECTORY:INTERNAL=//p' "$CACHE")
bin=$(sed -n 's/^CMAKE_CACHEFILE_DIR:INTERNAL=//p' "$CACHE")
[ -n "$src" ] && [ -n "$bin" ] || { echo "path-map: FAIL — в $CACHE нет каталогов дерева"; exit 1; }

exe() { if [ -f "$1/$2.exe" ]; then echo "$1/$2.exe"; else echo "$1/$2"; fi; }
hits() { grep -a -i -F -c -e "$2" "$1" 2>/dev/null || true; }

if [ "$selftest" -eq 1 ]; then
  cxx=$(sed -n 's/^CMAKE_CXX_COMPILER:[A-Z]*=//p' "$CACHE")
  gen=$(sed -n 's/^CMAKE_GENERATOR:INTERNAL=//p' "$CACHE")
  tmp=$(mktemp -d)
  trap 'rm -rf "$tmp"' EXIT
  printf 'cmake_minimum_required(VERSION 3.20)\nproject(nomap CXX)\nadd_executable(path_map_probe "%s/engine/core/path_map_probe.cpp")\n' \
    "$src" > "$tmp/CMakeLists.txt"
  if ! cmake -S "$tmp" -B "$tmp/b" -G "$gen" -DCMAKE_CXX_COMPILER="$cxx" -DCMAKE_BUILD_TYPE=Release >"$tmp/log" 2>&1 \
      || ! cmake --build "$tmp/b" >>"$tmp/log" 2>&1; then
    sed 's/^/    /' "$tmp/log"; echo "path-map selftest: FAIL — проба без карты не собралась ($cxx)"; exit 1
  fi
  probe=$(exe "$tmp/b" path_map_probe)
  n=$(( $(hits "$probe" "$src") + $(hits "$probe" "${src//\//\\}") ))
  if [ "$n" -ge 1 ]; then
    echo "path-map selftest: PASS — без карты путь дерева в пробе находится ($n), компилятор $(basename "$cxx")"
    exit 0
  fi
  echo "path-map selftest: FAIL — проба без карты не несёт «${src}»: поиск слеп или компилятор не вшивает путь"
  exit 1
fi

probe=$(exe "$BUILD" path_map_probe)
[ -f "$probe" ] || { echo "path-map: FAIL — нет $probe"; exit 1; }
printed=$("$probe" | tr -d '\r')
fail=0
[ -n "$printed" ] && [ "$(hits "$probe" "$printed")" -ge 1 ] \
  || { echo "path-map: FAIL — строка пробы «${printed}» не находится в её бинаре, поиск слеп"; fail=1; }

targets=("$probe" "$(exe "$BUILD" assetc)")
[ -f "$(exe "$BUILD" editor_shell)" ] && targets+=("$(exe "$BUILD" editor_shell)")
for f in "${targets[@]}"; do
  [ -f "$f" ] || { echo "path-map: FAIL — нет $f"; fail=1; continue; }
  for dir in "$src" "$bin"; do
    for form in "$dir" "${dir//\//\\}"; do
      n=$(hits "$f" "$form")
      [ "$n" -eq 0 ] || { echo "path-map: FAIL — $f несёт «${form}» ($n)"; fail=1; }
    done
  done
done

[ "$fail" -eq 0 ] && echo "path-map: PASS — ${#targets[@]} файлов, проба печатает «${printed}»"
exit "$fail"
