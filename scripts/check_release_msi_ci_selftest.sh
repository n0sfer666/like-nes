#!/usr/bin/env bash
# Позитивный контроль утверждения о ПРИЕХАВШЕМ установщике (спека #20, вертикаль 4, шаг C). Предмет
# отличается от соседей по обе стороны: check_release_ci_selftest.sh ломает ФАЙЛЫ конфигурации пути
# через CI (копии workflow), а здесь ломается сам ПАКЕТ, который оркестратор скачал с чужой машины.
# Цепочка «коммит → прогон → штамп» о содержимом не говорит ничего, поэтому пакет без редактора, без
# рантайма и с лицензией в нуль байт обязан быть отбит здесь — иначе он доедет как успех.
set -uo pipefail

ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
# shellcheck source=scripts/release_lib.sh
. "$ROOT/scripts/release_lib.sh"
# shellcheck source=scripts/release_msi_lib.sh
. "$ROOT/scripts/release_msi_lib.sh"
# shellcheck source=scripts/release_msi_pack_lib.sh
. "$ROOT/scripts/release_msi_pack_lib.sh"
# shellcheck source=scripts/release_dmg_lib.sh
. "$ROOT/scripts/release_dmg_lib.sh"
# shellcheck source=scripts/release_appimage_lib.sh
. "$ROOT/scripts/release_appimage_lib.sh"
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
# shellcheck source=scripts/release_msi_ci_lib.sh
. "$ROOT/scripts/release_msi_ci_lib.sh"
# shellcheck source=scripts/selftest_sub_lib.sh
. "$ROOT/scripts/selftest_sub_lib.sh"

# Пропуск ВСЛУХ, а не молчаливый ноль: собрать фикстурный установщик нечем на машине без компилятора,
# а осмотреть — без msitools, и «набор не гонялся» обязано отличаться от «находок нет».
if ! msi_tool_name >/dev/null 2>&1; then
  echo "msi-ci-selftest: ПРОПУСК — компилятора MSI нет (нужен wixl из msitools либо candle/light из WiX)"
  exit 0
fi
if ! command -v msiinfo >/dev/null 2>&1 || ! command -v msiextract >/dev/null 2>&1; then
  echo "msi-ci-selftest: ПРОПУСК — msitools не установлены, осматривать пакет нечем (brew install msitools)"
  exit 0
fi

VER=v0.0.0-check
SHA=abc1234def5678
BAD=0
ORIG_SOURCE=$(declare -f msi_make_source)
eval "msi_make_source_real() $(declare -f msi_make_source | tail -n +2)"
FIX=$(mktemp -d)
trap 'rm -rf "$FIX"' EXIT

expect() {
  local want="$1" name="$2"; shift 2
  local rc=0
  "$@" >/dev/null 2>&1 || rc=$?
  if { [ "$want" = pass ] && [ "$rc" = 0 ]; } || { [ "$want" = fail ] && [ "$rc" != 0 ]; }; then
    printf 'msi-ci-selftest: OK   %s (%s)\n' "$name" "$want"
  else
    printf 'msi-ci-selftest: БРАК %s: ожидали %s, получили код %s\n' "$name" "$want" "$rc" >&2
    BAD=1
  fi
}

# Раскладка «скачанного»: рядом с архивом лежит установщик того же имени — ровно то, что оркестратор
# получает из артефакта прогона. Тот же ci_make_pkg строит и стейдж, поэтому состав фикстуры выведен
# из expected_files, а не написан здесь второй раз.
arrived() {
  local tag="$1"
  local damage="${2:-}"
  local stamp_triple="${3:-$MSI_FIXTURE_TRIPLE}"
  local base="$FIX/$tag"
  local pkg="$base/like-nes-engine-$VER-$MSI_FIXTURE_TRIPLE.tar.gz"
  if [ ! -f "$pkg" ]; then
    msi_fixture_stage "$ROOT" "$base" "$VER" "$SHA" "$damage" "$stamp_triple" >/dev/null || return 1
    msi_fixture_pack "$ROOT" "$base" "$VER" || return 1
    cp "$base/dest/pkg.msi" "${pkg%.tar.gz}.msi" || return 1
  fi
  printf '%s\n' "$pkg"
}

case_msi() {
  local tag="$1"
  local damage="${2:-}"
  local stamp_triple="${3:-$MSI_FIXTURE_TRIPLE}"
  local pkg
  pkg=$(arrived "$tag" "$damage" "$stamp_triple") || return 1
  assert_ci_msi "$ROOT" "$pkg" "$VER"
}

# Опорный прогон: на честном пакете утверждение обязано ПРОХОДИТЬ. Без него «фикстура отбита»
# выходило бы из утверждения, которое падает на чём угодно.
expect pass "честный пакет доехал целиком" case_msi good

# Пропажа самого установщика — отказ, а не пропуск: архив на месте, цепочка сходится, и половина
# продукта Windows отсутствует молча.
case_no_msi() {
  local pkg
  pkg=$(arrived good) || return 1
  cp "${pkg%.tar.gz}.msi" "$FIX/hidden.msi" && rm -f "${pkg%.tar.gz}.msi"
  assert_ci_msi "$ROOT" "$pkg" "$VER"
  local rc=$?
  mv "$FIX/hidden.msi" "${pkg%.tar.gz}.msi"
  return "$rc"
}
expect fail "установщика рядом с архивом нет" case_no_msi

# Имя обязано называть тройку: из неё выводится ожидаемый состав, и пакет с чужим именем
# осматривался бы по чужому списку файлов.
case_name() {
  local pkg alt
  pkg=$(arrived good) || return 1
  alt="$FIX/good/engine.tar.gz"
  cp "${pkg%.tar.gz}.msi" "${alt%.tar.gz}.msi"
  assert_ci_msi "$ROOT" "$alt" "$VER"
}
expect fail "имя установщика не называет тройку" case_name

# Три порчи содержимого, каждая — своим утверждением: состав, лицензии, штамп. Прогон CI о них не
# говорит ничего, цепочка сходится по версии и коммиту у всех трёх.
expect fail "нет редактора" case_msi noeditor drop:like-nes/bin/editor_shell.exe
expect fail "нет рантайма" case_msi nortl drop:like-nes/bin/wgpu_native.dll
expect fail "лицензия в нуль байт" case_msi zerolic zero:like-nes/licenses/LICENSE
expect fail "чужая тройка в штампе" case_msi badtriple "" linux-x86_64

# Тихая установка — единственное утверждение набора, чей предмет ставит КОМПИЛЯТОР, а не раскладка:
# бит 8 сводного потока пишет он сам по атрибуту исходника. Компиляторов у одного исходника два
# (wixl здесь, WiX v3 на раннере), их совпадение по этому биту ничем не доказано, и приехавший
# пакет — единственное место, где выход настоящего WiX вообще осматривается. Порча делается тем же
# приёмом, что в impl-наборе: настоящий генератор, затем строка из готового .wxs.
case_no_scope() {
  local base="$FIX/noscope"
  local pkg="$base/like-nes-engine-$VER-$MSI_FIXTURE_TRIPLE.tar.gz"
  (
    msi_make_source() {
      msi_make_source_real "$@" || return 1
      sed '/InstallScope="perUser"/d' "$3" > "$3.t" && mv "$3.t" "$3"
    }
    subbed msi_make_source "$ORIG_SOURCE" || exit 1
    if [ ! -f "$pkg" ]; then
      msi_fixture_stage "$ROOT" "$base" "$VER" "$SHA" >/dev/null || exit 1
      msi_fixture_pack "$ROOT" "$base" "$VER" || exit 1
      cp "$base/dest/pkg.msi" "${pkg%.tar.gz}.msi" || exit 1
    fi
    assert_ci_msi "$ROOT" "$pkg" "$VER"
  )
}
expect fail "приехавший пакет спросит права под /qn" case_no_scope

if [ "$BAD" != 0 ]; then echo "msi-ci-selftest: FAIL" >&2; exit 1; fi
echo "msi-ci-selftest: PASS"
