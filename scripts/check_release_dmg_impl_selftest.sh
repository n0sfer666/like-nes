#!/usr/bin/env bash
# Позитивный контроль ПОДМЕНАМИ РЕАЛИЗАЦИЙ (спека #20, вертикаль 4, шаг A). Предмет здесь другой,
# чем у check_release_dmg_selftest.sh: тот ломает СОДЕРЖИМОЕ готового тома, а этот ломает то, чем
# том делается, — раскладку бандла, сборку plist, запечатывание тома и размонтирование.
#
# Так проверяется единственное, чего не даёт сломанная фикстура: утверждение о составе выводит
# ожидаемое ТОЙ ЖЕ функцией dmg_app_path, которой раскладывал упаковщик, и ошибись она — обе
# половины ошибутся одинаково и останутся зелёными. Каждая такая подмена ниже обязана ПРОЙТИ
# утверждение о составе и быть отбитой утверждением, которое про раскладку ничего не знает.
set -uo pipefail

ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
# shellcheck source=scripts/release_lib.sh
. "$ROOT/scripts/release_lib.sh"
# shellcheck source=scripts/release_dmg_lib.sh
. "$ROOT/scripts/release_dmg_lib.sh"
# shellcheck source=scripts/release_check_lib.sh
. "$ROOT/scripts/release_check_lib.sh"
# shellcheck source=scripts/release_dmg_check_lib.sh
. "$ROOT/scripts/release_dmg_check_lib.sh"
# shellcheck source=scripts/release_dmg_rpath_lib.sh
. "$ROOT/scripts/release_dmg_rpath_lib.sh"
# shellcheck source=scripts/release_dmg_fixture_lib.sh
. "$ROOT/scripts/release_dmg_fixture_lib.sh"
# Гигиена прогона подключается ровно потому, что её утверждения зовёт гейт: проверка «каждое
# assert_* определено» иначе называла бы браком набор, а не гейт.
# shellcheck source=scripts/release_check_hygiene.sh
. "$ROOT/scripts/release_check_hygiene.sh"
# shellcheck source=scripts/gate_wiring_lib.sh
. "$ROOT/scripts/gate_wiring_lib.sh"
# shellcheck source=scripts/selftest_sub_lib.sh
. "$ROOT/scripts/selftest_sub_lib.sh"

if [ "$(uname -s)" != Darwin ]; then
  echo "dmg-impl-selftest: ПРОПУСК — предмет набора есть бандл и образ macOS (здесь $(uname -s))"
  exit 0
fi

VER=v0.0.0-check
SHA=abc1234
TRIPLE=macos-arm64
LICS="$DMG_APP_NAME/Contents/Resources/licenses"
BINS="$DMG_APP_NAME/Contents/MacOS"
BAD=0
FIX=$(mktemp -d)
trap 'rm -rf "$FIX"' EXIT

ORIG_APP_PATH=$(declare -f dmg_app_path)
ORIG_PLIST=$(declare -f dmg_plist)
ORIG_SEAL=$(declare -f dmg_seal_volume)
ORIG_MAKE_APP=$(declare -f dmg_make_app)
eval "dmg_make_app_real() $(declare -f dmg_make_app | tail -n +2)"

expect() {
  local want="$1" name="$2"; shift 2
  local rc=0
  "$@" >/dev/null 2>&1 || rc=$?
  if { [ "$want" = pass ] && [ "$rc" = 0 ]; } || { [ "$want" = fail ] && [ "$rc" != 0 ]; }; then
    printf 'dmg-impl-selftest: OK   %s (%s)\n' "$name" "$want"
  else
    printf 'dmg-impl-selftest: БРАК %s: ожидали %s, получили код %s\n' "$name" "$want" "$rc" >&2
    BAD=1
  fi
}

# Фикстура строится ВНУТРИ подмены и один раз на сценарий: утверждение о составе обязано считать
# ожидаемое той же испорченной раскладкой, что применил раскладчик, — иначе доказывать нечего.
case_app_path() {
  local base="$FIX/$1" arm="$2" what="$3"
  (
    case "$arm" in
      licenses) dmg_app_path() {
        case "$1" in
          like-nes/bin/*) printf 'Contents/MacOS/%s\n' "${1#like-nes/bin/}" ;;
          like-nes/licenses/*) printf 'Contents/licenses/%s\n' "${1#like-nes/licenses/}" ;;
          like-nes/version.txt) printf 'Contents/Resources/version.txt\n' ;;
          *) return 1 ;;
        esac
      } ;;
      runtime) dmg_app_path() {
        case "$1" in
          like-nes/bin/*wgpu*) printf 'Contents/Frameworks/%s\n' "${1#like-nes/bin/}" ;;
          like-nes/bin/*) printf 'Contents/MacOS/%s\n' "${1#like-nes/bin/}" ;;
          like-nes/licenses/*) printf 'Contents/Resources/licenses/%s\n' "${1#like-nes/licenses/}" ;;
          like-nes/version.txt) printf 'Contents/Resources/version.txt\n' ;;
          *) return 1 ;;
        esac
      } ;;
      flat) dmg_app_path() { printf 'Contents/MacOS/%s\n' "$(basename "$1")"; } ;;
      collide) dmg_app_path() {
        case "$1" in
          like-nes/bin/*) printf 'Contents/MacOS/%s\n' "$DMG_APP_EXE" ;;
          like-nes/licenses/*) printf 'Contents/Resources/licenses/%s\n' "${1#like-nes/licenses/}" ;;
          like-nes/version.txt) printf 'Contents/Resources/version.txt\n' ;;
          *) return 1 ;;
        esac
      } ;;
    esac
    subbed dmg_app_path "$ORIG_APP_PATH" || exit 1
    [ -d "$base/vol" ] || dmg_fixture_vol "$ROOT" "$base" "$VER" "$SHA" "$TRIPLE" || exit 1
    case "$what" in
      composition) assert_dmg_composition "$ROOT" "$base/vol" ;;
      mirrors) assert_dmg_mirrors_stage "$base/vol" "$base/stage" ;;
      licenses) assert_licenses "$ROOT" "$base/vol" "$LICS" ;;
      runtime) assert_runtime_named "$FIX/CMakeCache.txt" "$base/vol" "$BINS" ;;
      modes) assert_dmg_modes "$base/vol" ;;
    esac
  )
}

printf 'WGPU_RUNTIME_LIB:FILEPATH=/nowhere/libwgpu_native.dylib\n' > "$FIX/CMakeCache.txt"

# Лицензии мимо Resources: обе половины утверждения о составе ошиблись одинаково — и это ровно тот
# случай, ради которого подкорень лицензий у assert_licenses задан ЯВНО, а не выведен из раскладки.
expect pass "лицензии мимо Resources · состав слеп (общая ошибка половин)" case_app_path lic licenses composition
expect pass "лицензии мимо Resources · зеркало стейджа слепо" case_app_path lic licenses mirrors
expect fail "лицензии мимо Resources · утверждение о лицензиях" case_app_path lic licenses licenses

# Рантайм в Frameworks/: бандл выглядит собранным и не стартует — `reproducible_rpath` ставит
# `@executable_path`, то есть dylib обязан лежать РЯДОМ с exe. Состав этого не видит, кеш CMake видит.
expect pass "рантайм в Frameworks · состав слеп" case_app_path rt runtime composition
expect fail "рантайм в Frameworks · имя рантайма из кеша" case_app_path rt runtime runtime

# Плоская раскладка: всё в MacOS/. Лицензии и штамп приезжают туда с правами 644, и утверждение о
# правах — единственное, что об этом знает: оно ветвится по каталогам, а не по раскладке.
expect pass "плоская раскладка · состав слеп" case_app_path flat flat composition
expect fail "плоская раскладка · права" case_app_path flat flat modes

# Коллизия имён теряет файл, и её обязаны отбить ОБА: у состава ожидаемое несёт строку дважды,
# у зеркала не сходится мультимножество сумм.
expect fail "коллизия раскладки · состав" case_app_path col collide composition
expect fail "коллизия раскладки · зеркало стейджа" case_app_path col collide mirrors

# Подмена СОДЕРЖИМОГО при верных именах — обратный случай: состав слеп по построению, и независимость
# зеркала стейджа доказывается здесь, на уровне реализации, а не только сломанной фикстурой.
case_swapped() {
  local base="$FIX/swap" what="$1"
  (
    dmg_make_app() {
      dmg_make_app_real "$@" || return 1
      printf 'чужое содержимое\n' > "$2/Contents/MacOS/$DMG_APP_EXE"
      chmod 755 "$2/Contents/MacOS/$DMG_APP_EXE"
    }
    subbed dmg_make_app "$ORIG_MAKE_APP" || exit 1
    [ -d "$base/vol" ] || dmg_fixture_vol "$ROOT" "$base" "$VER" "$SHA" "$TRIPLE" || exit 1
    case "$what" in
      composition) assert_dmg_composition "$ROOT" "$base/vol" ;;
      mirrors) assert_dmg_mirrors_stage "$base/vol" "$base/stage" ;;
    esac
  )
}
expect pass "подменённое содержимое · состав слеп" case_swapped composition
expect fail "подменённое содержимое · зеркало стейджа" case_swapped mirrors

# Info.plist без точки входа: бандл со всеми файлами на местах, который LaunchServices не откроет.
case_plist() {
  local base="$FIX/plist"
  (
    dmg_plist() { printf '<?xml version="1.0"?>\n<plist version="1.0"><dict/></plist>\n'; }
    subbed dmg_plist "$ORIG_PLIST" || exit 1
    [ -d "$base/vol" ] || dmg_fixture_vol "$ROOT" "$base" "$VER" "$SHA" "$TRIPLE" || exit 1
    assert_dmg_plist "$base/vol" "$VER" "$SHA"
  )
}
expect fail "plist без CFBundleExecutable" case_plist

# Запечатывание без симлинка: образ, из которого бандл переносят руками. `find -type f` и манифест
# симлинка не видят вовсе, поэтому утверждение о нём — единственный контроль этой строки.
case_seal() {
  local base="$FIX/seal"
  (
    dmg_seal_volume() { TZ=UTC0 find "$1" -exec touch -h -t "$2" {} +; }
    subbed dmg_seal_volume "$ORIG_SEAL" || exit 1
    [ -d "$base/vol" ] || dmg_fixture_vol "$ROOT" "$base" "$VER" "$SHA" "$TRIPLE" || exit 1
    assert_dmg_applications_link "$base/vol"
  )
}
expect fail "запечатывание без симлинка Applications" case_seal

# Живой цикл образа (hdiutil: упаковать, смонтировать, снять) — свой файл, зовётся ВНЕШНЕЙ
# командой, чтобы в логе стояло имя упавшего набора: предмет там другой (поведение системы, а не
# файлы), и он единственный оставляет за собой то, что нужно снимать.
bash "$ROOT/scripts/check_release_dmg_live_selftest.sh" || BAD=1

expect pass "имена утверждений гейта определены" \
  assert_gate_asserts_defined "$ROOT/scripts/check_release_dmg.sh"

if [ "$BAD" != 0 ]; then echo "dmg-impl-selftest: FAIL" >&2; exit 1; fi
echo "dmg-impl-selftest: PASS"
