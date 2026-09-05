#!/usr/bin/env bash
# Позитивный контроль ВЫЗОВА КОМПИЛЯТОРА (спека #20, вертикаль 4, шаг C). Предмет третий, отличный
# от соседних наборов: check_release_msi_selftest.sh ломает СОДЕРЖИМОЕ готового пакета,
# check_release_msi_impl_selftest.sh — то, чем пакет делается, а здесь — выбор инструмента, разбор
# его ИСХОДА и ветка «компилятора на машине нет».
#
# Настоящий компилятор набору не нужен и намеренно не зовётся: он подменяется заглушкой в PATH,
# потому что предмет — реакция упаковщика на чужой исход, а не MSI. По той же причине набор
# работает и там, где msitools не стоят вовсе: осматривать здесь нечего.
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

VER=v0.0.0-check
SHA=abc1234def5678
BAD=0
FIX=$(mktemp -d)
trap 'rm -rf "$FIX"' EXIT

expect() {
  local want="$1" name="$2"; shift 2
  local rc=0
  "$@" >/dev/null 2>&1 || rc=$?
  if { [ "$want" = pass ] && [ "$rc" = 0 ]; } || { [ "$want" = fail ] && [ "$rc" != 0 ]; }; then
    printf 'msi-pack-selftest: OK   %s (%s)\n' "$name" "$want"
  else
    printf 'msi-pack-selftest: БРАК %s: ожидали %s, получили код %s\n' "$name" "$want" "$rc" >&2
    BAD=1
  fi
}

# Заглушка берёт имя выходного файла из тех же аргументов, что и настоящий инструмент (`-o` у wixl,
# `-out` у candle/light), — иначе она проверяла бы себя. Слово в файле называет ВЕТКУ: без него
# «собрано wix» и «собрано wixl» с одного взгляда неразличимы.
stub_tool() {
  local dir="$1" name="$2" arm="$3"
  mkdir -p "$dir"
  {
    printf '#!/bin/sh\nout=""; prev=""\nfor a in "$@"; do\n'
    printf '  case "$prev" in -o|-out) out=$a ;; esac\n  prev=$a\ndone\n'
    case "$arm" in
      ok)       printf 'printf %s > "$out"\n' "$name" ;;
      silent)   printf 'exit 0\n' ;;
      leftover) printf 'printf half > "$out"\necho "исходник не разобран" >&2\nexit 3\n' ;;
    esac
  } > "$dir/$name"
  chmod 755 "$dir/$name"
}

# PATH чистится ПЕРЕБОРОМ каталогов, а не вырезанием одного: на машине владельца wixl стоит в
# системе, и ветка «компилятора нет» проверяла бы там не то, что заявляет.
path_without_msi_tools() {
  local out="" d
  local IFS=:
  for d in $PATH; do
    [ -n "$d" ] || continue
    if [ -x "$d/wixl" ] || [ -x "$d/candle" ] || [ -x "$d/light" ]; then continue; fi
    out="$out${out:+:}$d"
  done
  printf '%s\n' "$out"
}

# Выбор инструмента: WiX (candle+light) — родной компилятор раннера Windows и потому первый; wixl
# из msitools — единственный, который есть у владельца. Половина WiX инструментом не считается:
# candle без light даёт объектный файл и ни одного пакета.
case_tool() {
  # Раздельные `local`: bash 3.2 на macOS объявляет все имена строки разом и лишь потом
  # присваивает, поэтому ссылка на соседнее имя под `set -u` убивает набор МОЛЧА — вывод кейса
  # проглочен `expect`, и в логе не остаётся ни строки. Уже было в соседнем наборе про AppImage.
  local arm="$1"
  local want="$2"
  local dir="$FIX/tool.$arm"
  local got
  rm -rf "$dir"; mkdir -p "$dir"
  case "$arm" in
    wix)  stub_tool "$dir" candle ok; stub_tool "$dir" light ok; stub_tool "$dir" wixl ok ;;
    half) stub_tool "$dir" candle ok; stub_tool "$dir" wixl ok ;;
    wixl) stub_tool "$dir" wixl ok ;;
    none) : ;;
  esac
  (
    PATH="$dir:$(path_without_msi_tools)"
    got=$(msi_tool_name) || got=нет
    [ "$got" = "$want" ]
  )
}
expect pass "есть WiX и wixl · берётся WiX"        case_tool wix wix
expect pass "есть candle без light · берётся wixl" case_tool half wixl
expect pass "есть только wixl · берётся wixl"      case_tool wixl wixl
expect pass "нет ни одного · инструмент не назван" case_tool none нет

# Исход, а не код возврата: пакет считается собранным по ФАЙЛУ. Ненулевой код при этом слушается
# первым — он достовернее и называет причину, а оставленный обрубок обязан быть УБРАН: `[ -s ]`
# принимает его наравне с готовым пакетом, и отказ доехал бы до владельца как собранный пакет.
case_pack() {
  local arm="$1"
  local dir="$FIX/bin.$arm"
  local out="$FIX/pack.$arm.msi"
  local rc=0
  rm -rf "$dir"; mkdir -p "$dir"
  case "$arm" in
    wix) stub_tool "$dir" candle ok; stub_tool "$dir" light ok ;;
    *)   stub_tool "$dir" wixl "$arm" ;;
  esac
  printf '<Wix/>\n' > "$FIX/fake.wxs"
  (
    PATH="$dir:$(path_without_msi_tools)"
    # Заглушка проверяется ПЕРВОЙ строкой: не подхватилась — проверять нечего, и это находка.
    case "$arm" in
      wix) [ "$(command -v candle)" = "$dir/candle" ] || exit 1 ;;
      *)   [ "$(command -v wixl)" = "$dir/wixl" ] || exit 1 ;;
    esac
    rm -f "$out"
    msi_pack "$FIX" "$FIX/fake.wxs" "$out" || rc=$?
    case "$arm" in
      ok)       [ "$rc" = 0 ] && [ "$(cat "$out")" = wixl ] ;;
      wix)      [ "$rc" = 0 ] && [ "$(cat "$out")" = light ] ;;
      silent)   [ "$rc" != 0 ] && [ ! -e "$out" ] ;;
      leftover) [ "$rc" != 0 ] && [ ! -e "$out" ] ;;
    esac
  )
}
expect pass "wixl отработал · пакет на месте"          case_pack ok
expect pass "WiX отработал · пакет собран веткой WiX"  case_pack wix
expect pass "код ноль без файла · отказ упаковщика"    case_pack silent
expect pass "ненулевой код · отказ и обрубок убран"    case_pack leftover

# Код 2 — «инструмента нет», и он ОТДЕЛЬНЫЙ от отказа сборки: msitools не стоят на раннере Windows,
# WiX не существует на macOS, и ни то, ни другое не есть поломка. Слитые в один код, они дали бы
# либо падающий на ровном месте релиз, либо молчаливый пропуск вместо отказа.
case_pack_missing() {
  local rc=0
  printf '<Wix/>\n' > "$FIX/fake.wxs"
  (
    PATH=$(path_without_msi_tools)
    command -v wixl >/dev/null 2>&1 && exit 1
    msi_pack "$FIX" "$FIX/fake.wxs" "$FIX/none.msi" || rc=$?
    [ "$rc" = 2 ] && [ ! -e "$FIX/none.msi" ]
  )
}
expect pass "компилятора нет · код 2, а не отказ сборки" case_pack_missing

# Ветка «компилятора на машине нет» в самом релизе: безусловный вызов ронял бы `release.sh` на
# Windows-раннере без WiX вместе с архивом, который там собрался. Пропуск обязан быть СЛЫШЕН —
# молчаливый читается ровно как «пакет собран».
case_extra() {
  local arm="$1"
  local base="$FIX/extra.$arm"
  local dir="$FIX/extrabin"
  local rc=0
  rm -rf "$base"
  msi_fixture_stage "$ROOT" "$base" "$VER" "$SHA" || return 1
  rm -rf "$dir"; stub_tool "$dir" wixl ok
  (
    if [ "$arm" = present ]; then
      PATH="$dir:$(path_without_msi_tools)"
      [ "$(command -v wixl)" = "$dir/wixl" ] || exit 1
    else
      PATH=$(path_without_msi_tools)
      command -v wixl >/dev/null 2>&1 && exit 1
    fi
    extra_build windows "$base/stage" "$base/dest" pkg "$VER" 202601010000.00 "$base/build" \
      "$ROOT/packaging" || rc=$?
    [ "$rc" = 0 ] || exit 1
    case "$arm" in
      present) [ -n "$EXTRA" ] && [ -s "$EXTRA" ] && [ -z "$EXTRA_SKIP" ] ;;
      missing) [ -z "$EXTRA" ] && [ -n "$EXTRA_SKIP" ] && [ ! -e "$base/dest/pkg.msi" ] ;;
    esac
  )
}
expect pass "компилятор есть · пакет собран"                case_extra present
expect pass "компилятора нет · архив цел, пропуск назван вслух" case_extra missing

if [ "$BAD" != 0 ]; then echo "msi-pack-selftest: FAIL" >&2; exit 1; fi
echo "msi-pack-selftest: PASS"
