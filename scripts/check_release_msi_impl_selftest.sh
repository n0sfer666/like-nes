#!/usr/bin/env bash
# Позитивный контроль ПОДМЕНАМИ РЕАЛИЗАЦИЙ (спека #20, вертикаль 4, шаг C). Предмет другой, чем у
# check_release_msi_selftest.sh: тот ломает СОДЕРЖИМОЕ готового пакета, а этот — то, чем пакет
# делается: раскладку по каталогам и исходник, который читает компилятор. Раскладка едет в имена
# развёрнутого дерева, поэтому её ошибку видит и состав по expected_files — это утверждается, а не
# предполагается; независимость зеркала стейджа доказывает подмена СОДЕРЖИМОГО при верных именах.
set -uo pipefail

ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
# shellcheck source=scripts/release_lib.sh
. "$ROOT/scripts/release_lib.sh"
# shellcheck source=scripts/release_dmg_lib.sh
. "$ROOT/scripts/release_dmg_lib.sh"
# shellcheck source=scripts/release_appimage_lib.sh
. "$ROOT/scripts/release_appimage_lib.sh"
# shellcheck source=scripts/release_msi_lib.sh
. "$ROOT/scripts/release_msi_lib.sh"
# shellcheck source=scripts/release_msi_pack_lib.sh
. "$ROOT/scripts/release_msi_pack_lib.sh"
# shellcheck source=scripts/release_extra_lib.sh
. "$ROOT/scripts/release_extra_lib.sh"
# shellcheck source=scripts/release_check_lib.sh
. "$ROOT/scripts/release_check_lib.sh"
# shellcheck source=scripts/release_ci_check_lib.sh
. "$ROOT/scripts/release_ci_check_lib.sh"
# shellcheck source=scripts/release_ci_fixture_lib.sh
. "$ROOT/scripts/release_ci_fixture_lib.sh"
# shellcheck source=scripts/release_msi_check_lib.sh
. "$ROOT/scripts/release_msi_check_lib.sh"
# shellcheck source=scripts/release_msi_behavior_lib.sh
. "$ROOT/scripts/release_msi_behavior_lib.sh"
# shellcheck source=scripts/release_msi_fixture_lib.sh
. "$ROOT/scripts/release_msi_fixture_lib.sh"
# shellcheck source=scripts/selftest_sub_lib.sh
. "$ROOT/scripts/selftest_sub_lib.sh"

if ! msi_tool_name >/dev/null 2>&1; then
  echo "msi-impl-selftest: ПРОПУСК — компилятора MSI нет (нужен wixl из msitools либо candle/light из WiX)"
  exit 0
fi
if ! command -v msiinfo >/dev/null 2>&1 || ! command -v msiextract >/dev/null 2>&1; then
  echo "msi-impl-selftest: ПРОПУСК — msitools не установлены, осматривать пакет нечем (brew install msitools)"
  exit 0
fi

VER=v0.0.0-check
SHA=abc1234def5678
TRIPLE=windows-x86_64
BAD=0
FIX=$(mktemp -d)
trap 'rm -rf "$FIX"' EXIT

ORIG_DIR_ID=$(declare -f msi_dir_id)
ORIG_ADD_DIR=$(declare -f msi_add_dir)
ORIG_GUID=$(declare -f msi_guid)
ORIG_SOURCE=$(declare -f msi_make_source)
eval "msi_make_source_real() $(declare -f msi_make_source | tail -n +2)"
eval "msi_add_dir_real() $(declare -f msi_add_dir | tail -n +2)"

expect() {
  local want="$1" name="$2"; shift 2
  local rc=0
  "$@" >/dev/null 2>&1 || rc=$?
  if { [ "$want" = pass ] && [ "$rc" = 0 ]; } || { [ "$want" = fail ] && [ "$rc" != 0 ]; }; then
    printf 'msi-impl-selftest: OK   %s (%s)\n' "$name" "$want"
  else
    printf 'msi-impl-selftest: БРАК %s: ожидали %s, получили код %s\n' "$name" "$want" "$rc" >&2
    BAD=1
  fi
}

# Правка исходника ПОСЛЕ настоящего генератора: тот же компилятор и тот же шаблон, отличается строка.
wxs_edit() {
  local expr="$1" f="$2"
  sed "$expr" "$f" > "$f.t" && mv "$f.t" "$f"
}

# Утверждения над фикстурой, построенной ВНУТРИ подмены: ожидаемое обязано считаться той же
# испорченной реализацией, что применил упаковщик, — иначе доказывать нечего.
run_assert() {
  local base="$1" what="$2"
  case "$what" in
    composition) assert_composition "$ROOT" "$base/ex" MINGW ;;
    mirrors)     assert_msi_mirrors_stage "$base/ex" "$base/stage" ;;
    stamp)       assert_stamp "$base/ex" "$VER" "$TRIPLE" ;;
    uninstall)   assert_msi_uninstall "$base/dest/pkg.msi" ;;
    per_user)    assert_msi_per_user "$base/dest/pkg.msi" ;;
    upgrade)     assert_msi_upgrade "$base/dest/pkg.msi" "$VER" ;;
    version)     assert_msi_version "$base/dest/pkg.msi" "$VER" ;;
    shortcut)    assert_msi_shortcut "$base/dest/pkg.msi" ;;
    *) return 1 ;;
  esac
}

# Подмены исходника: каждая — строка в готовом .wxs, отбитая тем утверждением, ради которого сделана.
case_wxs() {
  local arm="$1"
  local what="$2"
  local base="$FIX/wxs.$arm"
  (
    case "$arm" in
      swapped)  EXPR='s|Source="like-nes/version.txt"|Source="like-nes/licenses/LICENSE"|' ;;
      machine)  EXPR='s/LocalAppDataFolder/ProgramFiles64Folder/' ;;
      allusers) EXPR='s|<Feature |<Property Id="ALLUSERS" Value="1" /><Feature |' ;;
      noupgrade) EXPR='/<Upgrade /,/<\/Upgrade>/d' ;;
      openupgrade) EXPR='s|Maximum="[0-9.]*" IncludeMaximum="yes"||' ;;
      nodetect) EXPR='/IncludeMinimum="no"/{N;d;}' ;;
      version)  EXPR='s/Version="0.0.0"/Version="9.9.9"/' ;;
      shortcut) EXPR='s|\[BINFOLDER\]editor_shell.exe|[BINFOLDER]assetc.exe|' ;;
      *) exit 1 ;;
    esac
    msi_make_source() {
      msi_make_source_real "$@" || return 1
      wxs_edit "$EXPR" "$3"
    }
    subbed msi_make_source "$ORIG_SOURCE" || exit 1
    [ -d "$base/ex" ] || msi_fixture_pkg "$ROOT" "$base" "$VER" "$SHA" || exit 1
    run_assert "$base" "$what"
  )
}

# Подменённое содержимое при верных именах: состав слеп по построению, и независимость зеркала
# стейджа доказывается только здесь. Штамп ловит ту же подмену своей причиной — он читает файл.
expect pass "чужой источник у version.txt · состав слеп" case_wxs swapped composition
expect fail "чужой источник у version.txt · зеркало стейджа" case_wxs swapped mirrors
expect fail "чужой источник у version.txt · штамп" case_wxs swapped stamp

# Инвариант 3 спеки: не требовать администратора. Обе подмены — машинная установка, и обе
# оставляют состав нетронутым: по развёрнутому дереву они неотличимы от честного пакета.
expect pass "корень в ProgramFiles · состав слеп" case_wxs machine composition
expect fail "корень в ProgramFiles · пользовательская установка" case_wxs machine per_user
expect fail "задано ALLUSERS=1 · пользовательская установка" case_wxs allusers per_user

expect fail "нет линии обновления · версии будут копиться" case_wxs noupgrade upgrade
# Находка ревью, а не гипотеза: у сносящей строки без верхней границы «своя линейка» включает и
# более новые версии, и установка старого пакета поверх нового молча сносит новый. Строка с нашим
# UpgradeCode при этом на месте — утверждение о её НАЛИЧИИ обе подмены ниже проходит.
expect fail "диапазон без верхней границы · старая версия снесёт новую" case_wxs openupgrade upgrade
expect fail "более новая версия не обнаруживается · даунгрейд молча" case_wxs nodetect upgrade
expect fail "чужая ProductVersion в исходнике" case_wxs version version
expect fail "ярлык ведёт не в редактор" case_wxs shortcut shortcut

# Гейт 7 спеки даётся не обещанием, а записью RemoveFolder на каждом каталоге. Без неё пакет
# ставится и удаляется, оставляя пустые каталоги, — и состав такой пакет устраивает целиком.
case_add_dir() {
  local arm="$1"
  local what="$2"
  local base="$FIX/add.$arm"
  (
    case "$arm" in
      norm) msi_add_dir() {
        msi_add_dir_real "$@" || return 1
        MSI_COMPONENTS="$(printf '%s' "$MSI_COMPONENTS" | grep -v '<RemoveFolder ')
"
      } ;;
      nofeat) msi_add_dir() {
        local refs="$MSI_REFS"
        msi_add_dir_real "$@" || return 1
        [ "$1" != LICENSEFOLDER ] || MSI_REFS="$refs"
      } ;;
    esac
    subbed msi_add_dir "$ORIG_ADD_DIR" || exit 1
    [ -d "$base/ex" ] || msi_fixture_pkg "$ROOT" "$base" "$VER" "$SHA" || exit 1
    run_assert "$base" "$what"
  )
}
expect pass "нет RemoveFolder · состав слеп" case_add_dir norm composition
expect fail "нет RemoveFolder · деинсталляция" case_add_dir norm uninstall
# Компонент вне Feature у wixl выпадает из пакета ЦЕЛИКОМ (прогон показал дерево без единой
# лицензии), поэтому порчу ловят ОБА утверждения. Проверка по таблице всё равно нужна: она не
# зависит от поведения компилятора, а компиляторов у одного исходника два — WiX v3 и wixl.
expect fail "лицензии вне Feature · состав" case_add_dir nofeat composition
expect fail "лицензии вне Feature · деинсталляция" case_add_dir nofeat uninstall

# Раскладка: плоская теряет подкаталоги, и у MSI это видно ОБОИМ утверждениям — имена развёрнутого
# дерева задаёт она же. Утверждается это прогоном, а не предположением о формате.
case_dir_id() {
  local what="$1" base="$FIX/flat"
  (
    msi_dir_id() { case "$1" in like-nes/*) printf 'INSTALLFOLDER\n' ;; *) return 1 ;; esac; }
    subbed msi_dir_id "$ORIG_DIR_ID" || exit 1
    [ -d "$base/ex" ] || msi_fixture_pkg "$ROOT" "$base" "$VER" "$SHA" || exit 1
    run_assert "$base" "$what"
  )
}
expect fail "плоская раскладка · состав" case_dir_id composition
expect fail "плоская раскладка · зеркало стейджа" case_dir_id mirrors

# GUID выводится из ключа, а не берётся случайным: случайный ломает сверку двух прогонов, которой
# держится весь релизный гейт, — и это единственное, что о нём говорит.
case_guid_random() {
  (
    msi_guid() {
      printf '%08X-%04X-4%03X-8%03X-%012X\n' "$RANDOM" "$RANDOM" "$((RANDOM % 4096))" \
        "$((RANDOM % 4096))" "$((RANDOM * 32768 + RANDOM))"
    }
    subbed msi_guid "$ORIG_GUID" || exit 1
    msi_fixture_pkg "$ROOT" "$FIX/rnd1" "$VER" "$SHA" || exit 1
    msi_fixture_pkg "$ROOT" "$FIX/rnd2" "$VER" "$SHA" || exit 1
    assert_msi_content_reproducible "$FIX/rnd1/dest/pkg.msi" "$FIX/rnd2/dest/pkg.msi"
  )
}
expect fail "случайный GUID · содержимое двух прогонов" case_guid_random

# Отказ вычисления GUID: пока он считался подстановкой ВНУТРИ sed, его код возврата терялся
# целиком — в исходник уезжало `Product Id=""`, неподставленных полей не оставалось, и упаковщик
# возвращал ноль. Находка ревью.
case_guid_fail() {
  (
    msi_guid() { return 1; }
    subbed msi_guid "$ORIG_GUID" || exit 1
    msi_fixture_pkg "$ROOT" "$FIX/guidfail" "$VER" "$SHA"
  )
}
expect fail "GUID не вычислился · пакет не собирается" case_guid_fail

bash "$ROOT/scripts/check_release_msi_pack_selftest.sh" || BAD=1

if [ "$BAD" != 0 ]; then echo "msi-impl-selftest: FAIL" >&2; exit 1; fi
echo "msi-impl-selftest: PASS"
