#!/usr/bin/env bash
# Позитивный контроль утверждений о CRT (спека #20, вертикаль 5). Пакет Windows на машине владельца
# не собирается вовсе, поэтому «утверждение работает» доказывается единственным доступным способом:
# фикстурными пакетами с НАСТОЯЩИМИ PE (scripts/pe_fixture.py), каждый из которых сломан по одному
# месту. Утверждение, у которого нет фикстуры, где оно падает, неотличимо от отсутствующего.
#
# Предмет здесь — ПАКЕТ: кто из его бинарей чего просит и что лежит рядом. Разбор самого формата PE
# ушёл в check_release_crt_reader_selftest.sh: граница между наборами по предмету, а не по длине.
#
# Фикстуры строит ci_make_pkg — ТА ЖЕ фабрика, которой пользуются наборы вертикалей 3 и 4: своя
# копия правил раскладки означала бы, что подмену отбивают на пакете иного устройства, чем тот, на
# котором утверждение проходит.
set -uo pipefail

ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
# shellcheck source=scripts/release_lib.sh
. "$ROOT/scripts/release_lib.sh"
# shellcheck source=scripts/release_check_lib.sh
. "$ROOT/scripts/release_check_lib.sh"
# shellcheck source=scripts/release_ci_check_lib.sh
. "$ROOT/scripts/release_ci_check_lib.sh"
# shellcheck source=scripts/release_ci_fixture_lib.sh
. "$ROOT/scripts/release_ci_fixture_lib.sh"

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
    printf 'crt-selftest: OK   %s (%s)\n' "$name" "$want"
  else
    printf 'crt-selftest: БРАК %s: ожидали %s, получили код %s\n' "$name" "$want" "$rc" >&2
    BAD=1
  fi
}

# Каждое имя, которое набор зовёт, обязано быть ОПРЕДЕЛЕНО. Незагруженная функция даёт код 127, а он
# ненулевой — то есть все ожидающие fail кейсы проезжали бы как «утверждение отбило подмену», не
# позвав утверждения ни разу. Так и вышло на первом прогоне: wiring-lib не был подключён, и три
# фикстуры из четырёх молча проверяли отсутствие функции.
for fn in assert_crt_self_contained assert_static_crt assert_pe_reader_anchor; do
  if ! declare -F "$fn" >/dev/null; then
    printf 'crt-selftest: БРАК утверждение %s не определено — набор проверял бы код 127\n' "$fn" >&2
    exit 1
  fi
done

# Пакет распаковывается СВОИМ упаковщиком (pack_dir внутри ci_make_pkg) и осматривается уже как
# архив: утверждения зовут ровно там, где их зовёт настоящий гейт, а не на стейдже.
unpacked() {
  local tag="$1" damage="${2:-}"
  local base="$FIX/$tag"
  if [ ! -d "$base/out" ]; then
    local pkg
    pkg=$(ci_make_pkg "$ROOT" "$base" "$VER" "$SHA" "$TRIPLE" "$damage") || return 1
    mkdir -p "$base/out"
    tar -xzf "$pkg" -C "$base/out" || return 1
  fi
  printf '%s\n' "$base/out"
}

case_self_contained() { local d; d=$(unpacked "$1" "${2:-}") || return 1; assert_crt_self_contained "$ROOT" "$d"; }
case_static()        { local d; d=$(unpacked "$1" "${2:-}") || return 1; assert_static_crt "$ROOT" "$d"; }

# Опорные прогоны: на честном пакете оба утверждения обязаны ПРОХОДИТЬ. Без них «фикстура отбита»
# выходило бы из утверждения, которое падает на чём угодно.
expect pass "честный пакет · недостающих DLL нет" case_self_contained good
expect pass "честный пакет · наши бинари статичны" case_static good

# Чужой рантайм просит DLL, которой в пакете нет, — ровно тот случай, ради которого вертикаль и
# заведена: на чистой Windows такой пакет не стартует, а состав, лицензии и штамп у него сходятся.
expect fail "рантайм просит DLL, которой в пакете нет" \
    case_self_contained crt "crt:like-nes/bin/wgpu_native.dll"
# Та же нехватка, пришедшая ОТЛОЖЕННОЙ загрузкой (каталог 13). Грепом по строкам она не видна
# вовсе, и читатель, знающий только каталог 1, отдал бы «зависимостей нет».
expect fail "недостающая DLL пришла отложенной загрузкой" \
    case_self_contained delay "delaycrt:like-nes/bin/wgpu_native.dll"
# Файл выпал из пакета: импорт на месте, положить рядом стало нечего.
expect fail "app-local DLL выпала из пакета" \
    case_self_contained dropped "drop:like-nes/bin/vcruntime140.dll"

# Наш бинарь потерял статический CRT. Утверждение о самодостаточности этого НЕ видит и обязано
# проходить: vcruntime140.dll в пакете лежит, и ссылаться на неё может кто угодно. Ровно поэтому
# утверждений два, а не одно, — и вторая строка здесь доказывает, что они не дубликаты.
expect fail "наш exe импортирует CRT динамически" \
    case_static dyn "dyncrt:like-nes/bin/editor_shell.exe"
expect pass "тот же пакет проходит утверждение о самодостаточности" \
    case_self_contained dyn "dyncrt:like-nes/bin/editor_shell.exe"

# Пропавший каталог — не «нарушений нет»: find по нему молчит, цикл не выполняется ни разу, список
# нарушений пуст. Оба утверждения обязаны отказать по счётчику осмотренных (тот же класс, что
# правило vacuous-gate в ci_lint.py).
empty_dir() { mkdir -p "$FIX/empty"; printf '%s\n' "$FIX/empty"; }
case_empty_self() { local d; d=$(empty_dir); assert_crt_self_contained "$ROOT" "$d"; }
case_empty_static() { local d; d=$(empty_dir); assert_static_crt "$ROOT" "$d"; }
expect fail "каталог без единого PE · самодостаточность" case_empty_self
expect fail "каталог без единого PE · статичность" case_empty_static

# Чужое опознаётся ПОИМЁННО, а не по расширению: наша будущая разделяемая библиотека обязана
# попадать под утверждение о статичности, а исключение — доставаться ровно тому файлу, который мы
# не собираем. Пара доказывает обе половины разом: имя решает, а не суффикс.
case_our_dll() {
  local d="$FIX/ourdll"
  mkdir -p "$d"
  python3 "$ROOT/scripts/pe_fixture.py" "$d/editor_shell.exe" --import KERNEL32.dll || return 1
  python3 "$ROOT/scripts/pe_fixture.py" "$d/like_nes_core.dll" --import msvcp140.dll || return 1
  assert_static_crt "$ROOT" "$d"
}
expect fail "наша .dll собрана с динамическим CRT" case_our_dll

case_foreign_dll() {
  local d="$FIX/foreigndll"
  mkdir -p "$d"
  python3 "$ROOT/scripts/pe_fixture.py" "$d/editor_shell.exe" --import KERNEL32.dll || return 1
  python3 "$ROOT/scripts/pe_fixture.py" "$d/wgpu_native.dll" --import VCRUNTIME140.dll || return 1
  assert_static_crt "$ROOT" "$d"
}
expect pass "чужой рантайм с динамическим CRT исключён поимённо" case_foreign_dll

# Файл САМОГО redist, приехавший app-local, тоже не наш: его кладёт cmake из установки MSVC, и
# требовать от vcruntime140.dll статического CRT бессмысленно. Без этой ветки настоящий пакет
# Windows валил бы гейт на файле, который вертикаль в него и положила.
case_redist_file() {
  local d="$FIX/redistfile"
  mkdir -p "$d"
  python3 "$ROOT/scripts/pe_fixture.py" "$d/editor_shell.exe" --import KERNEL32.dll || return 1
  python3 "$ROOT/scripts/pe_fixture.py" "$d/vcruntime140.dll" --import msvcp140.dll || return 1
  assert_static_crt "$ROOT" "$d"
}
expect pass "app-local файл redist не проверяется на статичность" case_redist_file

# App-local поиск идёт по каталогу ПРОЦЕССА. Файл из пакета не выпадал — он переехал этажом выше,
# то есть состав пакета сходится, а на чистой Windows редактор не стартует. Утверждение, искавшее
# DLL «где-нибудь в пакете», этого не видит вовсе.
case_moved_dll() {
  local d f
  d=$(unpacked moved) || return 1
  f=$(find "$d" -type f -name 'vcruntime140.dll' | head -1)
  [ -n "$f" ] || return 1
  mv "$f" "$d/$(basename "$f")" || return 1
  assert_crt_self_contained "$ROOT" "$d"
}
expect fail "app-local DLL уехала из каталога исполняемого" case_moved_dll

# Читатель PE — свой набор: предмет там не пакет, а разбор формата (формы линкеров и места, где
# честного ответа у читателя нет), и якорь на настоящем wgpu_native.dll. Зовётся ВНЕШНЕЙ командой,
# чтобы в логе стояло имя упавшего набора.
bash "$ROOT/scripts/check_release_crt_reader_selftest.sh" || BAD=1

# Проводка дерева — свой набор: предмет там не пакет, а расположение строки в CMakeLists.txt и две
# копии закрытого списка redist-имён. Зовётся ВНЕШНЕЙ командой, чтобы в логе стояло имя упавшего
# набора.
bash "$ROOT/scripts/check_release_crt_wiring_selftest.sh" || BAD=1

if [ "$BAD" != 0 ]; then echo "crt-selftest: FAIL" >&2; exit 1; fi
echo "crt-selftest: PASS"
