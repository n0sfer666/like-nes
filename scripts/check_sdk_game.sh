#!/usr/bin/env bash
# Гейт SDK (спека #24, В1): игра собирается ПРОТИВ ПОСТАВЛЕННОГО ПРЕФИКСА, а не против дерева.
# Поставить компонент sdk из двух своих каталогов (Release и Debug) в один временный префикс,
# собрать games/neon-rumble в обеих конфигурациях через find_package(like-nes) и прогнать
# `--headless --frames 60`. Сборка внутри дерева этого не доказывает: там видны все заголовки и
# все цели, и забытый в списке поставки заголовок или библиотека проявились бы только у игрока.
#
# Каталоги СВОИ (build-sdk-release, build-sdk-debug), а не build-ci: у Debug другие флаги, а
# одолженный кеш оставил бы чужой этап собирать не ту конфигурацию, которую он заявляет. Оба
# конфигурируются из одного состояния дерева: последняя установка перетирает Config в префиксе.
#
# Сломанные фикстуры (scripts/sdk_game_lib.sh) идут в каждом прогоне — без них зелёный гейт не
# отличить от гейта, который ничего не утверждает.
#
# `--keep` оставляет префикс и сборки игры в build-sdk-work/ для оконного прогона владельцем
# (docs/owner-setup.txt, раздел S): окно на живом железе CI не открыть. Фикстуры и тогда идут во
# временном каталоге, а префикс портят копией: прерванный прогон не оставит владельцу префикс без
# библиотеки.
set -uo pipefail

ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
cd "$ROOT" || exit 1
# shellcheck source=scripts/sdk_game_lib.sh
. "$ROOT/scripts/sdk_game_lib.sh"

GEN=Ninja
WORK=$(mktemp -d)
trap 'rm -rf "$WORK"' EXIT
OUT=$WORK
if [ "${1:-}" = --keep ]; then
    OUT="$ROOT/build-sdk-work"
    mkdir -p "$OUT"
fi
PREFIX="$OUT/prefix"
FLAGS=(-DAUDIO_MINIAUDIO=OFF -DPLUGIN_UI=OFF -DPLUGIN_WASM=OFF -DIDE_POC=OFF)
FAIL=0

sdk_build() {
    local dir=$1 cfg=$2
    shift 2
    cmake -S . -B "$dir" -G "$GEN" -DCMAKE_BUILD_TYPE="$cfg" "${FLAGS[@]}" > "$WORK/$cfg-configure.log" 2>&1 || {
        tail -30 "$WORK/$cfg-configure.log"; sdk_bad "$cfg: SDK configure failed"; return 1; }
    cmake --build "$dir" --target "$@" > "$WORK/$cfg-build.log" 2>&1 || {
        tail -30 "$WORK/$cfg-build.log"; sdk_bad "$cfg: SDK build failed"; return 1; }
    cmake --install "$dir" --config "$cfg" --component sdk --prefix "$(native "$PREFIX")" \
        > "$WORK/$cfg-install.log" 2>&1 || {
        tail -30 "$WORK/$cfg-install.log"; sdk_bad "$cfg: SDK install failed"; return 1; }
    sdk_ok "$cfg: SDK built and installed into the prefix"
}

sdk_build build-sdk-release Release like_nes_sdk assetc || exit 1
sdk_build build-sdk-debug Debug like_nes_sdk || exit 1

for cfg in Release Debug; do
    if game_build "$ROOT/games/neon-rumble" "$OUT/game-$cfg" "$cfg"; then
        game_run "$OUT/game-$cfg" "$cfg" || FAIL=1
    else
        tail -30 "$OUT/game-$cfg.log"
        sdk_bad "$cfg: neon-rumble did not build against the prefix"
        FAIL=1
    fi
done

fixture_internal_header || FAIL=1
fixture_missing_library || FAIL=1
# Фикстура CRT — только у MSVC: у clang и gcc одна стандартная библиотека на обе конфигурации, и
# расхождения, которое она ловит, там нет по построению.
# Судит компилятор, который выбрал CMake, а не окружение: под vcvars с CXX=clang-cl компоновщик
# lld-link, и текста LNK2038 у него нет.
if grep -qs 'set(CMAKE_CXX_COMPILER_ID "MSVC")' "$OUT"/game-Release/CMakeFiles/*/CMakeCXXCompiler.cmake; then
    fixture_msvc_crt || FAIL=1
else
    printf 'sdk-game: SKIP CRT fixture (not an MSVC toolchain)\n'
fi

if [ "$FAIL" -ne 0 ]; then
    echo "sdk-game: FAIL"
    exit 1
fi
echo "sdk-game: PASS (Release + Debug against the prefix, fixtures refused)"
[ "$OUT" = "$WORK" ] || echo "sdk-game: prefix and game builds kept in build-sdk-work/"
