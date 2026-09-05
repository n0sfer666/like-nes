#!/usr/bin/env bash
# Гейт: пакет Windows не требует установленного VC++ Redistributable (спека #20, вертикаль 5).
#
# Утверждений шесть, и предмет у каждого свой. Порядок в CMakeLists.txt («статический CRT задаётся
# ДО зависимостей») читается из дерева и не требует ни сборки, ни Windows. Якорь читателя PE
# доказывает, что импорты вообще умеют читаться на НАСТОЯЩЕМ чужом бинаре, а не только на нашей
# фикстуре. Сверка двух копий закрытого списка redist-имён держит вместе шелл и cmake, которые о
# нём договариваются молча, а сверка ожидаемого состава с импортами рантайма — рукописную строку
# `vcruntime140.dll` в expected_files с тем набором, что кладёт в пакет cmake. Осмотр пакета
# отвечает на главный вопрос — чего не хватит на чистой машине.
#
# Пакет Windows на машине владельца не собирается (`--only windows` уходит в CI, см. вертикаль 3),
# поэтому его отсутствие есть ПРОПУСК ВСЛУХ, а не отказ: молчаливый ноль в общем прогоне preflight
# читался бы как пройденный гейт. Приехавший из CI пакет осматривают те же два утверждения — их
# зовёт assert_ci_package в release_ci_check_lib.sh.
set -uo pipefail

ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
# shellcheck source=scripts/release_crt_lib.sh
. "$ROOT/scripts/release_crt_lib.sh"
# shellcheck source=scripts/release_crt_wiring_lib.sh
. "$ROOT/scripts/release_crt_wiring_lib.sh"
# shellcheck source=scripts/release_check_lib.sh
. "$ROOT/scripts/release_check_lib.sh"

RC=0
assert_msvc_runtime_wired "$ROOT" || RC=1
assert_redist_list_mirrored "$ROOT" || RC=1
assert_pe_reader_anchor "$ROOT" || RC=1
assert_redist_expected_named "$ROOT" || RC=1

# Пакетов в release/ после релиза лежит по одному на версию, и «последний по глобу» осматривал бы
# один из них, не говоря какой. Больше одного — отказ с перечислением: гейт, молча выбравший себе
# предмет, отвечает не про тот пакет, который читает владелец.
PKG=""
NPKG=0
for p in "$ROOT"/release/*/like-nes-engine-*-windows-*.tar.gz; do
  [ -f "$p" ] || continue
  PKG="$p"
  NPKG=$((NPKG + 1))
done
if [ "$NPKG" -gt 1 ]; then
  echo "crt-check: FAIL в release/ несколько пакетов Windows — осматривать нужно один:" >&2
  ls -1 "$ROOT"/release/*/like-nes-engine-*-windows-*.tar.gz >&2
  exit 1
fi
if [ -z "$PKG" ]; then
  echo "crt-check: ПРОПУСК — пакета Windows в release/ нет (собирается в CI), осматривать нечего"
  [ "$RC" = 0 ] || { echo "crt-check: FAIL" >&2; exit 1; }
  echo "crt-check: PASS"
  exit 0
fi

echo "crt-check: осматривается $(basename "$PKG")"
TMP=$(mktemp -d)
trap 'rm -rf "$TMP"' EXIT
if ! tar -xzf "$PKG" -C "$TMP"; then
  echo "crt-check: FAIL архив не распаковывается: $(basename "$PKG")" >&2
  exit 1
fi
assert_crt_self_contained "$ROOT" "$TMP" || RC=1
assert_static_crt "$ROOT" "$TMP" || RC=1

[ "$RC" = 0 ] || { echo "crt-check: FAIL" >&2; exit 1; }
echo "crt-check: PASS"
