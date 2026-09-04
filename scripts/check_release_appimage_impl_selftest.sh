#!/usr/bin/env bash
# Позитивный контроль ПОДМЕНАМИ РЕАЛИЗАЦИЙ (спека #20, вертикаль 4, шаг B). Предмет здесь другой,
# чем у check_release_appimage_selftest.sh: тот ломает СОДЕРЖИМОЕ готового AppDir, а этот ломает то,
# чем AppDir делается, — раскладку, AppRun и `.desktop`. Каждая подмена ниже обязана ПРОЙТИ
# утверждение о составе и быть отбитой тем, которое о раскладке ничего не знает. Исход appimagetool и
# ветка «инструмента нет» — в check_release_appimage_pack_selftest.sh, а то, что уезжает в СУММУ
# образа (время, владелец, права каталогов), — в check_release_appimage_norm_selftest.sh.
set -uo pipefail

ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
# shellcheck source=scripts/release_lib.sh
. "$ROOT/scripts/release_lib.sh"
# shellcheck source=scripts/release_appimage_lib.sh
. "$ROOT/scripts/release_appimage_lib.sh"
# shellcheck source=scripts/release_check_lib.sh
. "$ROOT/scripts/release_check_lib.sh"
# shellcheck source=scripts/release_appimage_check_lib.sh
. "$ROOT/scripts/release_appimage_check_lib.sh"
# shellcheck source=scripts/release_appimage_runpath_lib.sh
. "$ROOT/scripts/release_appimage_runpath_lib.sh"
# shellcheck source=scripts/release_appimage_fixture_lib.sh
. "$ROOT/scripts/release_appimage_fixture_lib.sh"
# Гигиена прогона: общий с dmg контроль подмен и проверка, что каждое assert_* гейта определено.
# shellcheck source=scripts/release_check_hygiene.sh
. "$ROOT/scripts/release_check_hygiene.sh"
# shellcheck source=scripts/gate_wiring_lib.sh
. "$ROOT/scripts/gate_wiring_lib.sh"
# shellcheck source=scripts/selftest_sub_lib.sh
. "$ROOT/scripts/selftest_sub_lib.sh"

case "$(uname -s)" in
  Linux|Darwin) : ;;
  *) echo "appimage-impl-selftest: ПРОПУСК — git-bash не моделирует бит исполнения (здесь $(uname -s))"; exit 0 ;;
esac

VER=v0.0.0-check
SHA=abc1234
TRIPLE=linux-x86_64
LICS=usr/share/licenses/like-nes
BAD=0
FIX=$(mktemp -d)
trap 'rm -rf "$FIX"' EXIT

ORIG_PATH=$(declare -f appimage_path)
ORIG_APPRUN=$(declare -f appimage_apprun)
ORIG_DESKTOP=$(declare -f appimage_desktop_entry)
ORIG_MAKE=$(declare -f appimage_make_appdir)
eval "appimage_make_appdir_real() $(declare -f appimage_make_appdir | tail -n +2)"

expect() {
  local want="$1" name="$2"; shift 2
  local rc=0
  "$@" >/dev/null 2>&1 || rc=$?
  if { [ "$want" = pass ] && [ "$rc" = 0 ]; } || { [ "$want" = fail ] && [ "$rc" != 0 ]; }; then
    printf 'appimage-impl-selftest: OK   %s (%s)\n' "$name" "$want"
  else
    printf 'appimage-impl-selftest: БРАК %s: ожидали %s, получили код %s\n' "$name" "$want" "$rc" >&2
    BAD=1
  fi
}

# Фикстура строится ВНУТРИ подмены и один раз на сценарий: утверждение о составе обязано считать
# ожидаемое той же испорченной раскладкой, что применил раскладчик, — иначе доказывать нечего.
case_path() {
  local base="$FIX/$1" arm="$2" what="$3"
  (
    case "$arm" in
      licenses) appimage_path() {
        case "$1" in
          like-nes/bin/*) printf 'usr/bin/%s\n' "${1#like-nes/bin/}" ;;
          like-nes/licenses/*) printf 'usr/licenses/%s\n' "${1#like-nes/licenses/}" ;;
          like-nes/version.txt) printf 'usr/share/like-nes/version.txt\n' ;;
          *) return 1 ;;
        esac
      } ;;
      runtime) appimage_path() {
        case "$1" in
          like-nes/bin/*wgpu*) printf 'usr/lib/%s\n' "${1#like-nes/bin/}" ;;
          like-nes/bin/*) printf 'usr/bin/%s\n' "${1#like-nes/bin/}" ;;
          like-nes/licenses/*) printf 'usr/share/licenses/like-nes/%s\n' "${1#like-nes/licenses/}" ;;
          like-nes/version.txt) printf 'usr/share/like-nes/version.txt\n' ;;
          *) return 1 ;;
        esac
      } ;;
      flat) appimage_path() { printf 'usr/bin/%s\n' "$(basename "$1")"; } ;;
      collide) appimage_path() {
        case "$1" in
          like-nes/bin/*) printf 'usr/bin/%s\n' "$APPIMAGE_EXE" ;;
          like-nes/licenses/*) printf 'usr/share/licenses/like-nes/%s\n' "${1#like-nes/licenses/}" ;;
          like-nes/version.txt) printf 'usr/share/like-nes/version.txt\n' ;;
          *) return 1 ;;
        esac
      } ;;
    esac
    subbed appimage_path "$ORIG_PATH" || exit 1
    [ -d "$base/AppDir" ] || appimage_fixture_appdir "$ROOT" "$base" "$VER" "$SHA" "$TRIPLE" || exit 1
    case "$what" in
      composition) assert_appimage_composition "$ROOT" "$base/AppDir" ;;
      mirrors) assert_appimage_mirrors_stage "$base/AppDir" "$base/stage" ;;
      licenses) assert_licenses "$ROOT" "$base/AppDir" "$LICS" ;;
      runtime) assert_runtime_named "$FIX/CMakeCache.txt" "$base/AppDir" usr/bin ;;
      modes) assert_appimage_modes "$base/AppDir" ;;
    esac
  )
}

printf 'WGPU_RUNTIME_LIB:FILEPATH=/nowhere/libwgpu_native.so\n' > "$FIX/CMakeCache.txt"

# Лицензии мимо usr/share: обе половины утверждения о составе ошиблись одинаково — ровно тот случай,
# ради которого подкорень лицензий у assert_licenses задан ЯВНО, а не выведен из раскладки.
expect pass "лицензии мимо usr/share · состав слеп (общая ошибка половин)" case_path lic licenses composition
expect pass "лицензии мимо usr/share · зеркало стейджа слепо" case_path lic licenses mirrors
expect fail "лицензии мимо usr/share · утверждение о лицензиях" case_path lic licenses licenses

# Рантайм в usr/lib: образ выглядит собранным и не стартует — `reproducible_rpath` ставит `$ORIGIN`,
# то есть библиотека обязана лежать РЯДОМ с исполняемым; состав слеп, кеш CMake — нет.
expect pass "рантайм в usr/lib · состав слеп" case_path rt runtime composition
expect fail "рантайм в usr/lib · имя рантайма из кеша" case_path rt runtime runtime

# Плоская раскладка: всё в usr/bin, и видит это одно утверждение о правах — оно ветвится по
# каталогам, а не по раскладке, а лицензии приезжают в usr/bin с правами 644.
expect pass "плоская раскладка · состав слеп" case_path flat flat composition
expect fail "плоская раскладка · права" case_path flat flat modes

# Коллизия имён теряет файл, и её обязаны отбить ОБА: у состава ожидаемое несёт строку дважды,
# у зеркала не сходится мультимножество сумм.
expect fail "коллизия раскладки · состав" case_path col collide composition
expect fail "коллизия раскладки · зеркало стейджа" case_path col collide mirrors

# Подмена СОДЕРЖИМОГО при верных именах — обратный случай: состав слеп по построению, и
# независимость зеркала стейджа доказывается здесь, на уровне реализации.
case_swapped() {
  local base="$FIX/swap" what="$1"
  (
    appimage_make_appdir() {
      appimage_make_appdir_real "$@" || return 1
      printf 'чужое содержимое\n' > "$2/usr/bin/$APPIMAGE_EXE"
      chmod 755 "$2/usr/bin/$APPIMAGE_EXE"
    }
    subbed appimage_make_appdir "$ORIG_MAKE" || exit 1
    [ -d "$base/AppDir" ] || appimage_fixture_appdir "$ROOT" "$base" "$VER" "$SHA" "$TRIPLE" || exit 1
    case "$what" in
      composition) assert_appimage_composition "$ROOT" "$base/AppDir" ;;
      mirrors) assert_appimage_mirrors_stage "$base/AppDir" "$base/stage" ;;
    esac
  )
}
expect pass "подменённое содержимое · состав слеп" case_swapped composition
expect fail "подменённое содержимое · зеркало стейджа" case_swapped mirrors

# AppRun, зовущий точку входа по PATH: внутри смонтированного образа PATH ведёт куда угодно, только
# не в него, а состав такой AppRun устраивает — он на месте и исполняем.
case_apprun() {
  local base="$FIX/apprun" what="$1"
  (
    appimage_apprun() { printf '#!/bin/sh\nexec %s "$@"\n' "$APPIMAGE_EXE"; }
    subbed appimage_apprun "$ORIG_APPRUN" || exit 1
    [ -d "$base/AppDir" ] || appimage_fixture_appdir "$ROOT" "$base" "$VER" "$SHA" "$TRIPLE" || exit 1
    case "$what" in
      composition) assert_appimage_composition "$ROOT" "$base/AppDir" ;;
      apprun) assert_appimage_apprun "$base/AppDir" ;;
    esac
  )
}
expect pass "AppRun по PATH · состав слеп" case_apprun composition
expect fail "AppRun по PATH · утверждение об AppRun" case_apprun apprun

# `.desktop` без значка: файл на месте и разбирается, а appimagetool на таком AppDir отказывается
# КОДОМ НОЛЬ, не создав образа. Утверждение о значке — единственное, что стоит между этим и релизом.
case_desktop() {
  local base="$FIX/desktop"
  (
    appimage_desktop_entry() {
      printf '[Desktop Entry]\nType=Application\nName=like-nes\nExec=%s\nCategories=Development;\n' "$APPIMAGE_EXE"
    }
    subbed appimage_desktop_entry "$ORIG_DESKTOP" || exit 1
    [ -d "$base/AppDir" ] || appimage_fixture_appdir "$ROOT" "$base" "$VER" "$SHA" "$TRIPLE" || exit 1
    assert_appimage_desktop "$base/AppDir"
  )
}
expect fail ".desktop не называет значка" case_desktop


expect pass "имена утверждений гейта определены" \
  assert_gate_asserts_defined "$ROOT/scripts/check_release_appimage.sh"

if [ "$BAD" != 0 ]; then echo "appimage-impl-selftest: FAIL" >&2; exit 1; fi
echo "appimage-impl-selftest: PASS"
