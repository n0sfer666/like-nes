#!/usr/bin/env bash
# Позитивный контроль ВЫЗОВА ИНСТРУМЕНТА (спека #20, вертикаль 4, шаг B). Предмет
# третий, отличный от соседних наборов: check_release_appimage_selftest.sh ломает СОДЕРЖИМОЕ
# готового AppDir, check_release_appimage_impl_selftest.sh — раскладку, которой AppDir делается, а
# здесь — ВЫЗОВ ИНСТРУМЕНТА: разбор его исхода и ветка «инструмента на машине нет». То, что уезжает
# в сумму образа (время, права, владелец), забрал себе check_release_appimage_norm_selftest.sh —
# он зовётся отсюда внешней командой, чтобы в логе стояло имя упавшего набора. Настоящий
# appimagetool не нужен и намеренно не зовётся: он подменяется заглушкой в PATH, потому что предмет
# — реакция упаковщика на его исход, а не squashfs.
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
# shellcheck source=scripts/release_appimage_fixture_lib.sh
. "$ROOT/scripts/release_appimage_fixture_lib.sh"
# shellcheck source=scripts/release_extra_lib.sh
. "$ROOT/scripts/release_extra_lib.sh"
# shellcheck source=scripts/release_check_hygiene.sh
. "$ROOT/scripts/release_check_hygiene.sh"

case "$(uname -s)" in
  Linux|Darwin) : ;;
  *) echo "appimage-pack-selftest: ПРОПУСК — git-bash не моделирует бит исполнения (здесь $(uname -s))"; exit 0 ;;
esac

VER=v0.0.0-check
SHA=abc1234
TRIPLE=linux-x86_64
BAD=0
FIX=$(mktemp -d)
trap 'rm -rf "$FIX"' EXIT


expect() {
  local want="$1" name="$2"; shift 2
  local rc=0
  "$@" >/dev/null 2>&1 || rc=$?
  if { [ "$want" = pass ] && [ "$rc" = 0 ]; } || { [ "$want" = fail ] && [ "$rc" != 0 ]; }; then
    printf 'appimage-pack-selftest: OK   %s (%s)\n' "$name" "$want"
  else
    printf 'appimage-pack-selftest: БРАК %s: ожидали %s, получили код %s\n' "$name" "$want" "$rc" >&2
    BAD=1
  fi
}


# Заглушка вместо инструмента: аргументы у appimagetool позиционные и образ он пишет ПОСЛЕДНИМ, так
# что имя выходного файла заглушка берёт оттуда же, откуда настоящий, — иначе она проверяла бы себя.
stub_tool() {
  local dir="$FIX/bin.$1"
  local arm="$2"
  mkdir -p "$dir"
  {
    printf '#!/bin/sh\nfor a in "$@"; do out=$a; done\n'
    case "$arm" in
      ok) printf 'printf image > "$out"\n' ;;
      silent) printf 'exit 0\n' ;;
      leftover) printf 'printf half > "$out"\necho "нет иконки" >&2\nexit 3\n' ;;
    esac
  } > "$dir/appimagetool"
  chmod 755 "$dir/appimagetool"
  printf '%s\n' "$dir"
}

# PATH чистится ПЕРЕБОРОМ каталогов, а не вырезанием одного: в контейнере вертикали 2 настоящий
# appimagetool лежит в системе, и ветка «инструмента нет» проверяла бы там не то, что заявляет.
path_without_appimagetool() {
  local out="" d
  local IFS=:
  for d in $PATH; do
    [ -n "$d" ] || continue
    [ -x "$d/appimagetool" ] && continue
    out="$out${out:+:}$d"
  done
  printf '%s\n' "$out"
}

case_pack() {
  # Раздельные `local`: bash 3.2 на macOS объявляет все имена строки разом и лишь потом присваивает,
  # поэтому ссылка на соседнее имя под `set -u` убивает набор молча — уже было в соседнем файле.
  local arm="$1"
  local base="$FIX/pack.$arm"
  local out="$FIX/pack.$arm.AppImage"
  local bin rc=0
  bin=$(stub_tool "$arm" "$arm")
  (
    PATH="$bin:$(path_without_appimagetool)"
    # Заглушка проверяется ПЕРВОЙ строкой: не подхватилась — проверять нечего, и это находка.
    [ "$(command -v appimagetool)" = "$bin/appimagetool" ] || exit 1
    rm -rf "$base"; mkdir -p "$base"
    printf 'AppDir\n' > "$base/marker"
    rm -f "$out"
    appimage_pack "$base" "$out" x86_64 || rc=$?
    case "$arm" in
      ok) [ "$rc" = 0 ] && [ -s "$out" ] ;;
      silent) [ "$rc" != 0 ] && [ ! -e "$out" ] ;;
      # Обрубок обязан быть УБРАН, а не только назван: `[ -f ]` его принимает, и release_container.sh
      # принял бы тоже — то есть отказ инструмента доехал бы до владельца как собранный образ.
      leftover) [ "$rc" != 0 ] && [ ! -e "$out" ] ;;
    esac
  )
}
expect pass "инструмент отработал · образ на месте" case_pack ok
expect pass "код ноль без файла · отказ упаковщика" case_pack silent
expect pass "ненулевой код · отказ и обрубок убран" case_pack leftover

# Ветка «инструмента на машине нет» — не украшение: appimagetool живёт в образе контейнера, а не в
# системе владельца, и безусловный вызов ронял бы на Linux весь release.sh, а с ним check_release.sh
# и релизный этап preflight. Пропуск обязан быть СЛЫШЕН: молчаливый читается как «образ собран».
case_extra() {
  local arm="$1"
  local base="$FIX/extra.$arm"
  local bin rc=0
  rm -rf "$base"; mkdir -p "$base/dest" "$base/build"
  appimage_fixture_stage "$ROOT" "$base/stage" "$VER" "$SHA" "$TRIPLE" || return 1
  bin=$(stub_tool "extra" ok)
  (
    if [ "$arm" = present ]; then
      PATH="$bin:$(path_without_appimagetool)"
      [ "$(command -v appimagetool)" = "$bin/appimagetool" ] || exit 1
    else
      PATH=$(path_without_appimagetool)
      command -v appimagetool >/dev/null 2>&1 && exit 1
    fi
    extra_build linux "$base/stage" "$base/dest" like-nes "$VER" 202601010000.00 "$base/build" \
      "$ROOT/packaging" || rc=$?
    [ "$rc" = 0 ] || exit 1
    case "$arm" in
      present) [ -n "$EXTRA" ] && [ -s "$EXTRA" ] && [ -z "$EXTRA_SKIP" ] ;;
      missing) [ -z "$EXTRA" ] && [ -n "$EXTRA_SKIP" ] && [ ! -e "$base/dest/like-nes.AppImage" ] ;;
    esac
  )
}
expect pass "инструмент есть · образ собран" case_extra present
expect pass "инструмента нет · архив цел, пропуск назван вслух" case_extra missing


bash "$ROOT/scripts/check_release_appimage_norm_selftest.sh" || BAD=1

if [ "$BAD" != 0 ]; then echo "appimage-pack-selftest: FAIL" >&2; exit 1; fi
echo "appimage-pack-selftest: PASS"
