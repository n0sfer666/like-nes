#!/usr/bin/env bash
# Позитивный контроль ФОРМЫ архива и гигиены прогона (release_check_hygiene.sh, pack_dir). Отдельный
# файл, а не хвост check_release_selftest.sh: там предмет — утверждения о СОДЕРЖИМОМ пакета, и они
# проверяются синтетическим каталогом, здесь — детерминизм упаковщика, ради которого приходится
# ПОДМЕНЯТЬ его реализацию. Граница по предмету, как у ci_watch_wait_selftest.sh, а не по счётчику
# строк; зовётся внешней командой из check_release_selftest.sh, чтобы в логе стояло имя упавшего.
set -uo pipefail

ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
# shellcheck source=scripts/release_lib.sh
. "$ROOT/scripts/release_lib.sh"
# shellcheck source=scripts/release_check_lib.sh
. "$ROOT/scripts/release_check_lib.sh"
# shellcheck source=scripts/release_check_hygiene.sh
. "$ROOT/scripts/release_check_hygiene.sh"

TMP=$(mktemp -d)
trap 'rm -rf "$TMP"' EXIT
BAD=0

expect() {
  local want="$1" name="$2"; shift 2
  local rc=0
  "$@" >/dev/null 2>&1 || rc=$?
  if { [ "$want" = pass ] && [ "$rc" = 0 ]; } || { [ "$want" = fail ] && [ "$rc" != 0 ]; }; then
    printf 'pack-selftest: OK   %s (%s)\n' "$name" "$want"
  else
    printf 'pack-selftest: БРАК %s: ожидали %s, получили код %s\n' "$name" "$want" "$rc" >&2
    BAD=1
  fi
}

SRC="$TMP/stage"
mkdir -p "$SRC/bin" "$SRC/licenses"
printf 'binary\n' > "$SRC/bin/tool"
printf 'license\n' > "$SRC/licenses/MIT"
printf 'stamp\n' > "$SRC/version.txt"
STAMP=$(release_stamp "$ROOT")

PKG="$TMP/good.tar.gz"
pack_dir "$SRC" "$PKG" "$STAMP"
expect pass "нормализованный архив" assert_pack_normalized "$PKG"
expect fail "архива нет" assert_pack_normalized "$TMP/absent.tar.gz"

# Детерминизм проверяется ВРЕМЕНЕМ ФАЙЛОВ, а не вторым вызовом того же кода: между прогонами
# настоящего гейта mtime стейджа тот же по построению, поэтому нормализация времени там ничего не
# доказывает. Здесь он нарочно другой — и сумма обязана совпасть до байта.
find "$SRC" -type f -exec touch -t 200001010101.01 {} +
PKG_LATER="$TMP/later.tar.gz"
pack_dir "$SRC" "$PKG_LATER" "$STAMP"
expect pass "иное время файлов — та же сумма" \
  assert_same_sum "$(sha256_of "$PKG")" "$(sha256_of "$PKG_LATER")"

# Три ПОДМЕННЫХ упаковщика: каждый отличается от настоящего ровно одной снятой мерой, и набор обязан
# отбить всех троих. Без этого «детерминированный архив» держалось бы на чтении кода: два прогона на
# одной машине из одного стейджа дают ту же сумму и с невыключенными мерами, и с выключенными.
pack_owner_left() {
  ( cd "$1" && find . -type f | LC_ALL=C sort | tar --format=ustar -cf - -T - ) | gzip -n -9 > "$2"
}
pack_unsorted() {
  tar_pack_flags
  ( cd "$1" && find . -type f | LC_ALL=C sort -r | tar "${TAR_PACK[@]}" -cf - -T - ) | gzip -n -9 > "$2"
}
pack_gzip_dated() {
  tar_pack_flags
  ( cd "$1" && find . -type f | LC_ALL=C sort | tar "${TAR_PACK[@]}" -cf - -T - ) | gzip -9 > "$2"
}
pack_owner_left "$SRC" "$TMP/owner.tar.gz"
pack_unsorted "$SRC" "$TMP/unsorted.tar.gz"
pack_gzip_dated "$SRC" "$TMP/dated.tar.gz"
# Подмена обязана СОБРАТЬ архив: отбитая по его отсутствию неотличима от отбитой по дефекту, а
# git-bash показал, что это не гипотеза — нераспознанный флаг оставлял пустой файл, и набор считал
# его находкой. Утверждение отдельное, потому что вердикт `assert_pack_normalized` тут молчит.
for decoy in owner unsorted dated; do
  if [ ! -s "$TMP/$decoy.tar.gz" ]; then
    printf 'pack-selftest: БРАК подменный упаковщик %s не собрал архив — отбивать нечего\n' "$decoy" >&2
    BAD=1
  fi
done

expect fail "упаковщик без зануления владельца" assert_pack_normalized "$TMP/owner.tar.gz"
expect fail "упаковщик без сортировки имён" assert_pack_normalized "$TMP/unsorted.tar.gz"
expect fail "упаковщик без gzip -n" assert_pack_normalized "$TMP/dated.tar.gz"

# Симлинк и пустой каталог `find -type f` не видит ОДИНАКОВО у упаковщика и у манифеста, поэтому
# сверка архива с манифестом на них слепа: единственная защита — отказ до упаковки.
LINKED="$TMP/linked"; cp -R "$SRC" "$LINKED"; ln -s bin/tool "$LINKED/alias" 2>/dev/null
if [ -L "$LINKED/alias" ]; then
  expect fail "в стейдже симлинк" pack_dir "$LINKED" "$TMP/linked.tar.gz" "$STAMP"
else
  # git-bash без прав на симлинки делает КОПИЮ, и фикстура перестаёт быть собой: «отбито» вышло бы
  # из обычного файла, которого утверждение и не обязано отбивать. Пропуск назван вслух — молчаливый
  # неотличим от пройденного, а на macOS и Linux эта ветка не берётся вовсе.
  printf 'pack-selftest: ПРОПУЩЕНО в стейдже симлинк — ln -s дал не симлинк (нет прав на этой ФС)\n'
  rm -f "$LINKED/alias"
fi
HOLLOW="$TMP/hollow"; cp -R "$SRC" "$HOLLOW"; mkdir -p "$HOLLOW/empty"
expect fail "в стейдже пустой каталог" pack_dir "$HOLLOW" "$TMP/hollow.tar.gz" "$STAMP"

# Фикстура — СВОЙ репозиторий без единого каталога на диске: спрошенное у живого дерева зеленело
# от того, что `release/` там лежал, и первый же прогон, убравший его за собой, показал, что
# утверждение читало диск вместо .gitignore.
IGN="$TMP/ignrepo"; mkdir -p "$IGN"; git init -q "$IGN"
printf 'release/\n' > "$IGN/.gitignore"
expect pass "release/ игнорируется" assert_ignored "$IGN" release
expect fail "scripts/ не игнорируется" assert_ignored "$IGN" scripts

expect pass "снимки дерева совпали" assert_tree_unchanged " M a.txt" " M a.txt"
expect fail "снимки дерева разошлись" assert_tree_unchanged " M a.txt" " M a.txt
?? b.txt"

# Незнакомая ОС обязана быть отказом, а не Linux-именованием по умолчанию: подменяем `uname`
# функцией — единственный способ пройти веткой, которой на этой машине не бывает.
uname() { if [ "${1:-}" = -s ]; then echo FreeBSD; else command uname "$@"; fi; }
expect fail "незнакомая ОС · ожидаемый состав" expected_files "$ROOT"
unset -f uname
expect pass "своя ОС · ожидаемый состав" expected_files "$ROOT"

if [ "$BAD" = 0 ]; then
  echo "release-pack-selftest: PASS"
else
  echo "release-pack-selftest: FAIL" >&2
fi
exit "$BAD"
