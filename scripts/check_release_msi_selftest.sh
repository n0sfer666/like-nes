#!/usr/bin/env bash
# Позитивный контроль СОДЕРЖИМЫМ готового пакета (спека #20, вертикаль 4, шаг C). Утверждение, у
# которого нет фикстуры, где оно падает, неотличимо от отсутствующего, поэтому здесь каждое правило
# гейта сначала проходит на честном пакете, а потом обязано быть отбито на сломанном.
#
# Предмет — ЧТО ВНУТРИ пакета: состав, лицензии, штамп, зеркало стейджа, таблицы установки. То, чем
# пакет делается, забрал ..._impl_selftest.sh, а ВЫЗОВ компилятора — ..._pack_selftest.sh.
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
# shellcheck source=scripts/gate_wiring_lib.sh
. "$ROOT/scripts/gate_wiring_lib.sh"

# Пропуски те же и по той же причине, что у самого гейта: компилятор MSI и msitools живут на разных
# машинах, и молчаливый пропуск в общем прогоне preflight читался бы как пройденный набор.
if ! msi_tool_name >/dev/null 2>&1; then
  echo "msi-selftest: ПРОПУСК — компилятора MSI нет (нужен wixl из msitools либо candle/light из WiX)"
  exit 0
fi
if ! command -v msiinfo >/dev/null 2>&1 || ! command -v msiextract >/dev/null 2>&1; then
  echo "msi-selftest: ПРОПУСК — msitools не установлены, осматривать пакет нечем (brew install msitools)"
  exit 0
fi

VER=v0.0.0-check
SHA=abc1234def5678
TRIPLE=windows-x86_64
BAD=0
FIX=$(mktemp -d)
trap 'rm -rf "$FIX"' EXIT

expect() {
  local want="$1" name="$2"; shift 2
  local rc=0
  "$@" >/dev/null 2>&1 || rc=$?
  if { [ "$want" = pass ] && [ "$rc" = 0 ]; } || { [ "$want" = fail ] && [ "$rc" != 0 ]; }; then
    printf 'msi-selftest: OK   %s (%s)\n' "$name" "$want"
  else
    printf 'msi-selftest: БРАК %s: ожидали %s, получили код %s\n' "$name" "$want" "$rc" >&2
    BAD=1
  fi
}

# Фикстура на сценарий строится ОДИН раз: сборка зовёт компилятор, и пересборка ради каждого
# утверждения растянула бы набор на минуты, ничего не добавив.
fixture() {
  local tag="$1"
  local base="$FIX/$tag"
  if [ ! -d "$base/ex" ]; then
    case "$tag" in
      good|run2) msi_fixture_pkg "$ROOT" "$base" "$VER" "$SHA" ;;
      # Пропавшая лицензия видна и составу, и утверждению о лицензиях; лицензия на НУЛЬ БАЙТ —
      # только второму, ради чего оно от состава и отделено (регресс спеки #9).
      nolic)   msi_fixture_pkg "$ROOT" "$base" "$VER" "$SHA" drop:like-nes/licenses/THIRD-PARTY.md ;;
      zerolic) msi_fixture_pkg "$ROOT" "$base" "$VER" "$SHA" zero:like-nes/licenses/LICENSE-MIT ;;
      noruntime) msi_fixture_pkg "$ROOT" "$base" "$VER" "$SHA" drop:like-nes/bin/wgpu_native.dll ;;
      nostamp) msi_fixture_pkg "$ROOT" "$base" "$VER" - ;;
      badver)  msi_fixture_pkg "$ROOT" "$base" v9.9.9 "$SHA" ;;
      badtriple) msi_fixture_pkg "$ROOT" "$base" "$VER" "$SHA" "" linux-x86_64 ;;
      # Лишний файл по ИЗВЕСТНОМУ раскладке пути: пакет собирается, зеркало стейджа его не видит
      # (он в обеих половинах), и стоит между ним и людьми только состав по expected_files.
      extra)
        msi_fixture_stage "$ROOT" "$base" "$VER" "$SHA" \
          && printf 'чужое\n' > "$base/stage/like-nes/bin/leftover.dll" \
          && msi_fixture_pack "$ROOT" "$base" "$VER" \
          && msi_fixture_extract "$base" ;;
      # Подменённое СОДЕРЖИМОЕ при верных именах — обратный случай: состав слеп по построению, и
      # независимость зеркала стейджа доказывается только здесь.
      swapped)
        msi_fixture_pkg "$ROOT" "$base" "$VER" "$SHA" \
          && printf 'чужое содержимое\n' > "$base/ex/like-nes/bin/assetc.exe" ;;
    esac || return 1
  fi
  printf '%s\n' "$base"
}

case_assert() {
  local tag="$1" what="$2" base
  base=$(fixture "$tag") || return 1
  case "$what" in
    composition) assert_composition "$ROOT" "$base/ex" MINGW ;;
    licenses)    assert_licenses "$ROOT" "$base/ex" ;;
    stamp)       assert_stamp "$base/ex" "$VER" "$TRIPLE" ;;
    mirrors)     assert_msi_mirrors_stage "$base/ex" "$base/stage" ;;
    per_user)    assert_msi_per_user "$base/dest/pkg.msi" ;;
    uninstall)   assert_msi_uninstall "$base/dest/pkg.msi" ;;
    upgrade)     assert_msi_upgrade "$base/dest/pkg.msi" "$VER" ;;
    version)     assert_msi_version "$base/dest/pkg.msi" "$VER" ;;
    shortcut)    assert_msi_shortcut "$base/dest/pkg.msi" ;;
    manifest)    assert_dir_matches "$base/ex" "$base/dest/pkg.msi.manifest" ;;
    *) return 1 ;;
  esac
}

# Опорные прогоны: на честном пакете обязано проходить КАЖДОЕ правило гейта. Без этой половины
# набор доказывал бы только то, что утверждения умеют падать.
for what in composition licenses stamp mirrors per_user uninstall upgrade version shortcut manifest; do
  expect pass "честный пакет · $what" case_assert good "$what"
done

expect fail "нет лицензии · состав"                 case_assert nolic composition
expect fail "нет лицензии · утверждение о лицензиях" case_assert nolic licenses
expect pass "лицензия в нуль байт · состав слеп"     case_assert zerolic composition
expect fail "лицензия в нуль байт · утверждение о лицензиях" case_assert zerolic licenses
expect fail "нет рантайма wgpu · состав"             case_assert noruntime composition
expect fail "нет штампа · состав"                    case_assert nostamp composition
expect fail "нет штампа · утверждение о штампе"      case_assert nostamp stamp
expect fail "чужая версия в штампе"                  case_assert badver stamp
expect fail "чужая тройка в штампе"                  case_assert badtriple stamp
expect fail "лишний файл в пакете · состав"          case_assert extra composition
expect pass "лишний файл в пакете · зеркало стейджа слепо" case_assert extra mirrors
expect pass "подменённое содержимое · состав слеп"   case_assert swapped composition
expect fail "подменённое содержимое · зеркало стейджа" case_assert swapped mirrors

# Файл по пути, которого раскладка не знает, обязан валить СБОРКУ: молча выпав из пакета, он
# ничем не отличался бы от успеха — состав считается по expected_files, а туда он и не попадал.
case_unknown_path() {
  local base="$FIX/deep"
  rm -rf "$base"
  msi_fixture_stage "$ROOT" "$base" "$VER" "$SHA" || return 1
  mkdir -p "$base/stage/like-nes/bin/plugins"
  printf 'чужое\n' > "$base/stage/like-nes/bin/plugins/x.dll"
  msi_fixture_pack "$ROOT" "$base" "$VER"
}
expect fail "файл по незнакомому пути валит сборку пакета" case_unknown_path

# Два прогона и то, чем они отличаются от копии. Сравнение пакета с самим собой зелено вакуумно,
# и единственная улика, отделяющая копию от второй сборки, — случайный package code.
case_two() {
  local what="$1" a b
  a=$(fixture good) || return 1
  b=$(fixture run2) || return 1
  case "$what" in
    distinct)     assert_msi_runs_distinct "$a/dest/pkg.msi" "$b/dest/pkg.msi" ;;
    reproducible) assert_msi_content_reproducible "$a/dest/pkg.msi" "$b/dest/pkg.msi" ;;
  esac
}
expect pass "два прогона · package code разный" case_two distinct
expect pass "два прогона · содержимое совпало"  case_two reproducible

case_copy() {
  local what="$1" a
  a=$(fixture good) || return 1
  cp "$a/dest/pkg.msi" "$FIX/copy.msi" || return 1
  case "$what" in
    distinct)     assert_msi_runs_distinct "$a/dest/pkg.msi" "$FIX/copy.msi" ;;
    reproducible) assert_msi_content_reproducible "$a/dest/pkg.msi" "$FIX/copy.msi" ;;
  esac
}
expect fail "копия вместо второго прогона · утверждение о двух прогонах" case_copy distinct
expect pass "копия вместо второго прогона · сравнение содержимого слепо" case_copy reproducible

# Обратная сторона: сравнение содержимого обязано ВИДЕТЬ разницу — иначе оно проходило бы всегда.
case_content_differs() {
  local a b
  a=$(fixture good) || return 1
  b=$(fixture nolic) || return 1
  assert_msi_content_reproducible "$a/dest/pkg.msi" "$b/dest/pkg.msi"
}
expect fail "пакеты из разных стейджей · сравнение содержимого" case_content_differs

# Манифест, лежащий рядом с пакетом: по нему владелец опознаёт содержимое, не разворачивая.
case_manifest_mangled() {
  local base
  base=$(fixture good) || return 1
  sed '$d' "$base/dest/pkg.msi.manifest" > "$FIX/mangled.manifest" || return 1
  cmp -s "$base/dest/pkg.msi.manifest" "$FIX/mangled.manifest" && return 1
  assert_dir_matches "$base/ex" "$FIX/mangled.manifest"
}
expect fail "манифест разъехался с пакетом" case_manifest_mangled

expect pass "имена утверждений гейта определены" \
  assert_gate_asserts_defined "$ROOT/scripts/check_release_msi.sh"

bash "$ROOT/scripts/check_release_msi_impl_selftest.sh" || BAD=1

if [ "$BAD" != 0 ]; then echo "msi-selftest: FAIL" >&2; exit 1; fi
echo "msi-selftest: PASS"
