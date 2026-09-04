#!/usr/bin/env bash
# Позитивный контроль гейта релиза: каждое утверждение из release_check_lib.sh обязано УПАСТЬ на
# фикстуре, сломанной ровно под него. Без этого гейт неотличим от набора функций, всегда
# возвращающих ноль, — и узнаётся это в тот день, когда пакет уезжает без лицензий.
#
# Фикстура синтетическая: настоящий пакет требует полной сборки, а предмет здесь — умеют ли
# утверждения падать, а не что лежит в дереве. Совпадение ожидаемого состава с реальным проверяет
# сам гейт, гоняющий release.sh, — здесь оно взято из того же expected_files намеренно.
set -uo pipefail

ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
# shellcheck source=scripts/release_lib.sh
. "$ROOT/scripts/release_lib.sh"
# shellcheck source=scripts/release_check_lib.sh
. "$ROOT/scripts/release_check_lib.sh"

TMP=$(mktemp -d)
trap 'rm -rf "$TMP"' EXIT
VER="v9.9.9-fixture"
BAD=0

expect() {
  local want="$1" name="$2"; shift 2
  local rc=0
  "$@" >/dev/null 2>&1 || rc=$?
  if { [ "$want" = pass ] && [ "$rc" = 0 ]; } || { [ "$want" = fail ] && [ "$rc" != 0 ]; }; then
    printf 'selftest: OK   %s (%s)\n' "$name" "$want"
  else
    printf 'selftest: БРАК %s: ожидали %s, получили код %s\n' "$name" "$want" "$rc" >&2
    BAD=1
  fi
}

make_good() {
  local dir="$1" f
  mkdir -p "$dir"
  expected_files "$ROOT" | while read -r f; do
    mkdir -p "$dir/$(dirname "$f")"
    printf 'fixture %s\n' "$f" > "$dir/$f"
  done
  printf 'like-nes engine %s\ncommit 0123abc\ntarget Fixture-x86_64\n' "$VER" > "$dir/like-nes/version.txt"
}

GOOD="$TMP/good"
make_good "$GOOD"

# Опорный прогон: фикстура, ничем не сломанная, обязана пройти ВСЁ. Иначе «падает всегда» читалось
# бы как «ловит всё», и ни одна находка ниже ничего не значила бы.
expect pass "целая фикстура · состав" assert_composition "$ROOT" "$GOOD"
expect pass "целая фикстура · лицензии" assert_licenses "$ROOT" "$GOOD"
expect pass "целая фикстура · штамп" assert_stamp "$GOOD" "$VER" Fixture-x86_64

LIC1=$(head -1 "$ROOT/cmake/licenses.manifest")
NO_LIC="$TMP/no-license"; cp -R "$GOOD" "$NO_LIC"; rm -f "$NO_LIC/like-nes/licenses/$(basename "$LIC1")"
expect fail "лицензии нет · состав" assert_composition "$ROOT" "$NO_LIC"

# Лицензия на нуль байт — фикстура, РАЗДЕЛЯЮЩАЯ два утверждения: по именам состав цел, и без
# отдельной проверки содержимого пакет уехал бы с пустым файлом лицензии.
EMPTY_LIC="$TMP/empty-license"; cp -R "$GOOD" "$EMPTY_LIC"; : > "$EMPTY_LIC/like-nes/licenses/$(basename "$LIC1")"
expect pass "лицензия пуста · состав молчит" assert_composition "$ROOT" "$EMPTY_LIC"
expect fail "лицензия пуста · лицензии" assert_licenses "$ROOT" "$EMPTY_LIC"

EXTRA="$TMP/extra"; cp -R "$GOOD" "$EXTRA"; printf 'stray\n' > "$EXTRA/like-nes/bin/stray.txt"
expect fail "лишний файл · состав" assert_composition "$ROOT" "$EXTRA"

WRONG_VER="$TMP/wrong-version"; cp -R "$GOOD" "$WRONG_VER"
printf 'like-nes engine v0.0.1\ncommit 0123abc\ntarget Fixture-x86_64\n' > "$WRONG_VER/like-nes/version.txt"
expect fail "штамп называет чужую версию" assert_stamp "$WRONG_VER" "$VER" Fixture-x86_64

NO_COMMIT="$TMP/no-commit"; cp -R "$GOOD" "$NO_COMMIT"
printf 'like-nes engine %s\ntarget Fixture-x86_64\n' "$VER" > "$NO_COMMIT/like-nes/version.txt"
expect fail "штамп без коммита" assert_stamp "$NO_COMMIT" "$VER" Fixture-x86_64

# Тройка штампа против имени пакета: до этой фикстуры утверждение принимало ЛЮБУЮ строку `target …`,
# то есть архив с именем macos спокойно нёс внутри Darwin, а на Linux — чужую платформу.
expect fail "штамп называет чужую тройку" assert_stamp "$GOOD" "$VER" linux-x86_64

# Лишняя строка: греп по строкам был к ней равнодушен, и штамп из полусотни строк мусора с тремя
# правильными среди них проходил как свой.
EXTRA_LINE="$TMP/extra-line"; cp -R "$GOOD" "$EXTRA_LINE"
printf 'like-nes engine %s\ncommit 0123abc\ntarget Fixture-x86_64\nmusor\n' "$VER" \
  > "$EXTRA_LINE/like-nes/version.txt"
expect fail "в штампе лишняя строка" assert_stamp "$EXTRA_LINE" "$VER" Fixture-x86_64

# CRLF — тот же класс, что стоил прогона 192ed67 на `*.wgsl`: шаблон, выехавший из checkout
# Windows-раннера, даёт `\r` в конце строк, и поведение обязано быть определённым (отказ), а не
# «как повезёт с якорем грепа». Закрыто с двух сторон: .gitattributes и NEWLINE_STYLE LF.
CRLF="$TMP/crlf"; cp -R "$GOOD" "$CRLF"
printf 'like-nes engine %s\r\ncommit 0123abc\r\ntarget Fixture-x86_64\r\n' "$VER" \
  > "$CRLF/like-nes/version.txt"
expect fail "штамп с CRLF" assert_stamp "$CRLF" "$VER" Fixture-x86_64

# Имя рантайма — единственное утверждение с НЕЗАВИСИМЫМ источником: остальные сверяют пакет со
# списком, из которого построена и сама фикстура. Здесь источник — кеш CMake, поэтому опечатка в
# expected_files ловится на той ОС, где гейт гоняется по-настоящему.
RT=$(expected_files "$ROOT" | grep wgpu | head -1)
CACHE_OK="$TMP/cache-ok"; printf 'WGPU_RUNTIME_LIB:INTERNAL=/build/%s\n' "$(basename "$RT")" > "$CACHE_OK"
CACHE_ALIEN="$TMP/cache-alien"; printf 'WGPU_RUNTIME_LIB:INTERNAL=/build/libwgpu_alien.so\n' > "$CACHE_ALIEN"
expect pass "рантайм назван кешем CMake" assert_runtime_named "$CACHE_OK" "$GOOD"
expect fail "кеш называет другой рантайм" assert_runtime_named "$CACHE_ALIEN" "$GOOD"
expect fail "кеша CMake нет" assert_runtime_named "$TMP/absent-cache" "$GOOD"

MAN_A="$TMP/a.manifest"; MAN_B="$TMP/b.manifest"
manifest_of "$GOOD" > "$MAN_A"
FLIPPED="$TMP/flipped"; cp -R "$GOOD" "$FLIPPED"; printf 'x' >> "$FLIPPED/like-nes/bin/assetc"
manifest_of "$FLIPPED" > "$MAN_B"
expect pass "манифест сам себе равен" assert_same_manifest "$MAN_A" "$MAN_A"
expect fail "подменён байт · манифест" assert_same_manifest "$MAN_A" "$MAN_B"
expect fail "суммы разошлись" assert_same_sum aaa bbb

STAMP=$(release_stamp "$ROOT")
PKG="$TMP/good.tar.gz"; pack_dir "$GOOD" "$PKG" "$STAMP"
expect pass "архив совпал с манифестом" assert_pkg_matches "$PKG" "$MAN_A"
: > "$TMP/empty.tar.gz"
expect fail "пустой архив" assert_pkg_matches "$TMP/empty.tar.gz" "$MAN_A"
PKG_FLIP="$TMP/flipped.tar.gz"; pack_dir "$FLIPPED" "$PKG_FLIP" "$STAMP"
expect fail "архив не тот, что в манифесте" assert_pkg_matches "$PKG_FLIP" "$MAN_A"

SUMS="$TMP/SHA256SUMS"
PKG_SUM=$(sha256_of "$PKG")
printf '%s  %s\n' "$PKG_SUM" "good.tar.gz" > "$SUMS"
expect pass "суммы названы" assert_sums_listed "$SUMS" "$PKG" "$PKG_SUM"
printf '%s  %s\n' "0000000000000000000000000000000000000000000000000000000000000000" "good.tar.gz" > "$SUMS"
expect fail "в файле сумм чужая сумма" assert_sums_listed "$SUMS" "$PKG" "$PKG_SUM"
: > "$SUMS"
expect fail "файл сумм пуст" assert_sums_listed "$SUMS" "$PKG" "$PKG_SUM"

# Второй контроль — на реализацию, а не на фикстуру: правдоподобно НЕПРАВИЛЬНАЯ проверка состава
# («всё ожидаемое на месте», без равенства множеств) обязана быть отбита набором. Набор, который
# её пропускает, выглядит ровно как честный и молчит про лишний файл в пакете.
subset_composition() {
  local root="$1" pkg_root="$2" f
  expected_files "$root" | while read -r f; do [ -e "$pkg_root/$f" ] || exit 1; done
}
rc=0; subset_composition "$ROOT" "$EXTRA" >/dev/null 2>&1 || rc=$?
if [ "$rc" = 0 ]; then
  printf 'selftest: OK   подменная проверка состава (подмножество) пропускает лишний файл — набор её ловит\n'
else
  printf 'selftest: БРАК подменная проверка состава упала сама, контроль ничего не доказал\n' >&2
  BAD=1
fi

# Тот же контроль с другой стороны: штамп, проверяемый одним лишь наличием файла, обязан
# пропустить фикстуру с чужой версией — а настоящий assert_stamp её отбивает (выше).
if [ -f "$WRONG_VER/like-nes/version.txt" ]; then
  printf 'selftest: OK   подменная проверка штампа (файл существует) слепа к чужой версии\n'
else
  printf 'selftest: БРАК фикстура чужой версии осталась без version.txt\n' >&2
  BAD=1
fi

# Форма архива и гигиена прогона — отдельным файлом и ВНЕШНЕЙ командой: в логе обязано стоять имя
# упавшего набора, а не «что-то из двух разошлось».
bash "$ROOT/scripts/check_release_pack_selftest.sh" || BAD=1

if [ "$BAD" = 0 ]; then
  echo "release-check-selftest: PASS"
else
  echo "release-check-selftest: FAIL" >&2
fi
exit "$BAD"
