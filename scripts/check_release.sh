#!/usr/bin/env bash
# Гейт релизного пакета (спека #20, гейты 1, 2, 9, 10): упаковать ДВАЖДЫ и утверждать про пакет
# состав, лицензии, штамп, нормализацию архива и воспроизводимость. Правила живут в
# scripts/release_check_lib.sh и scripts/release_check_hygiene.sh, здесь — только прогон; сломанные
# фикстуры гоняет scripts/check_release_selftest.sh.
#
# Два прогона, а не один с последующим сравнением с записанным числом: сумма релиза зависит от
# компилятора и хоста, поэтому голден пришлось бы обновлять каждым обновлением тулчейна, и он
# перестал бы что-либо ловить в первый же раз, когда его обновили не глядя.
set -uo pipefail

ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
# shellcheck source=scripts/release_lib.sh
. "$ROOT/scripts/release_lib.sh"
# shellcheck source=scripts/release_check_lib.sh
. "$ROOT/scripts/release_check_lib.sh"
# shellcheck source=scripts/release_check_hygiene.sh
. "$ROOT/scripts/release_check_hygiene.sh"

VER=${LIKE_NES_RELEASE_VERSION:-v0.0.0-check}
# Каталог сборки — СВОЙ у релиза, а не общий с preflight. Конфигурация релиза несёт
# LIKE_NES_RELEASE=ON и версию пакета, и кеш CMake её переживает: одолжив у гейта build-full, мы
# оставили бы этап «Сборка полного набора опций» навсегда собирать не ту конфигурацию, которую он
# заявляет, и штамповать игру-образец версией v0.0.0-check. Минута сборки этого не стоит.
BUILD=${LIKE_NES_RELEASE_BUILD:-$ROOT/build-release}
TMP=$(mktemp -d)
OUT1="$ROOT/release"
OUT2="$TMP/second"
NAME="like-nes-engine-$VER"
FAIL=0
# Каталог своего прогона гейт убирает за собой САМ: он лежит в release/, то есть в .gitignore, и
# `git status` его не видит — утверждение «следов не осталось» без уборки было бы слепо ровно к
# тому единственному следу, который прогон и оставляет (3 МБ архив с версией v0.0.0-check рядом с
# настоящими релизами).
BEFORE=$(git -C "$ROOT" status --porcelain)
# Утверждение идёт ДО установки trap, и это несущий порядок: первый прогон пишет прямо в `release/`
# дерева, а убирает за собой `rm -rf`. Версия берётся из окружения, поэтому прогон с версией
# настоящего релиза затирал бы готовый пакет и удалял его вместе с SHA256SUMS — то есть проверка
# ломала бы предмет проверки. Находка ревью шага B вертикали 4.
assert_run_dir_absent "$OUT1/$VER" || exit 1
trap 'rm -rf "$TMP" "$OUT1/$VER"; rmdir "$OUT1" 2>/dev/null || true' EXIT

# Первый прогон идёт в НАСТОЯЩИЙ release/, а не в /tmp: утверждение «каталог релиза игнорируется
# git» проверяется только тем каталогом, который релиз и создаёт.
echo "=== прогон 1: $OUT1"
bash "$ROOT/scripts/release.sh" --version "$VER" --out "$OUT1" --build "$BUILD" >/dev/null || {
  bad "первый прогон release.sh упал"; exit 1; }
echo "=== прогон 2: $OUT2"
bash "$ROOT/scripts/release.sh" --version "$VER" --out "$OUT2" --build "$BUILD" >/dev/null || {
  bad "второй прогон release.sh упал"; exit 1; }

PKG1=$(ls "$OUT1/$VER/$NAME"-*.tar.gz 2>/dev/null | head -1)
PKG2=$(ls "$OUT2/$VER/$NAME"-*.tar.gz 2>/dev/null | head -1)
if [ -z "$PKG1" ] || [ -z "$PKG2" ]; then
  bad "пакет не найден после прогона: '$PKG1' / '$PKG2'"; exit 1
fi
MAN1="${PKG1%.tar.gz}.manifest"
MAN2="${PKG2%.tar.gz}.manifest"
# Тройка берётся ИЗ ИМЕНИ ПАКЕТА и уходит в утверждение о штампе: имя и штамп обязаны называть одну
# платформу, иначе архив, названный linux, спокойно уезжает с macOS-штампом внутри.
TRIPLE=$(basename "$PKG1" .tar.gz); TRIPLE=${TRIPLE#"like-nes-engine-$VER-"}

UNPACKED="$TMP/unpacked"
mkdir -p "$UNPACKED"
tar -xzf "$PKG1" -C "$UNPACKED" || { bad "архив не распаковывается"; exit 1; }

assert_composition "$ROOT" "$UNPACKED" || FAIL=1
assert_licenses "$ROOT" "$UNPACKED" || FAIL=1
assert_runtime_named "$BUILD/CMakeCache.txt" "$UNPACKED" || FAIL=1
assert_stamp "$UNPACKED" "$VER" "$TRIPLE" || FAIL=1
assert_pkg_matches "$PKG1" "$MAN1" || FAIL=1
assert_pack_normalized "$PKG1" || FAIL=1
assert_same_manifest "$MAN1" "$MAN2" || FAIL=1
SUM1=$(sha256_of "$PKG1")
assert_same_sum "$SUM1" "$(sha256_of "$PKG2")" || FAIL=1
assert_sums_listed "$(dirname "$PKG1")/SHA256SUMS" "$PKG1" "$SUM1" || FAIL=1
assert_ignored "$ROOT" release || FAIL=1

rm -rf "${OUT1:?}/$VER"
if [ -e "$OUT1/$VER" ]; then
  bad "каталог прогона $OUT1/$VER остался в дереве"
  FAIL=1
else
  ok "каталог прогона убран за собой"
fi
assert_tree_unchanged "$BEFORE" "$(git -C "$ROOT" status --porcelain)" || FAIL=1

if [ "$FAIL" = 0 ]; then
  echo "release-check: PASS"
else
  echo "release-check: FAIL" >&2
fi
exit "$FAIL"
