# shellcheck shell=bash
# Каталог сборки оболочки мог остаться от прежней схемы, где оболочка была своим проектом
# (`cmake -S platform/ios`). Такой кэш CMake отвергает («does not match the source»), а сносить
# каталог целиком дорого: в `_deps` лежит собранный из Rust wgpu-native — минуты cargo.
# Печатает `--fresh`, если кэш сконфигурирован из другого корня, иначе ничего.
fresh_flag() {
  local cache="$1/CMakeCache.txt"
  [ -f "$cache" ] || return 0
  grep -qxF "CMAKE_HOME_DIRECTORY:INTERNAL=$2" "$cache" || echo --fresh
}
