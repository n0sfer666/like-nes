#!/usr/bin/env bash
# Позитивный контроль утверждений о ПРОВОДКЕ статического CRT (спека #20, вертикаль 5) —
# release_crt_wiring_lib.sh. Отделён от соседнего набора по ПРЕДМЕТУ, а не по счётчику строк: там
# ломается ПАКЕТ (настоящие PE с заданной таблицей импортов), здесь — само ДЕРЕВО: расположение
# строки в CMakeLists.txt и две копии закрытого списка redist-имён. Пакета этому набору не нужно
# вовсе, а фикстуры его — копии настоящих файлов дерева.
#
# Зовётся ВНЕШНЕЙ командой из check_release_crt_selftest.sh, чтобы в логе стояло имя упавшего набора.
set -uo pipefail

ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
# shellcheck source=scripts/release_crt_lib.sh
. "$ROOT/scripts/release_crt_lib.sh"
# shellcheck source=scripts/release_crt_wiring_lib.sh
. "$ROOT/scripts/release_crt_wiring_lib.sh"

BAD=0
FIX=$(mktemp -d)
trap 'rm -rf "$FIX"' EXIT

expect() {
  local want="$1" name="$2"; shift 2
  local rc=0
  "$@" >/dev/null 2>&1 || rc=$?
  if { [ "$want" = pass ] && [ "$rc" = 0 ]; } || { [ "$want" = fail ] && [ "$rc" != 0 ]; }; then
    printf 'crt-wiring-selftest: OK   %s (%s)\n' "$name" "$want"
  else
    printf 'crt-wiring-selftest: БРАК %s: ожидали %s, получили код %s\n' "$name" "$want" "$rc" >&2
    BAD=1
  fi
}

# Незагруженная функция даёт код 127, а он ненулевой — то есть каждый ожидающий fail кейс проезжал
# бы как «утверждение отбило подмену», не позвав утверждения ни разу. Так и вышло на первом прогоне
# соседнего набора: wiring-lib не был подключён, и три фикстуры молча проверяли отсутствие функции.
for fn in assert_msvc_runtime_wired assert_redist_list_mirrored; do
  if ! declare -F "$fn" >/dev/null; then
    printf 'crt-wiring-selftest: БРАК утверждение %s не определено — набор проверял бы код 127\n' "$fn" >&2
    exit 1
  fi
done

# Порядок в CMakeLists.txt. Фикстуры — копии настоящего файла, потому что предмет утверждения есть
# расположение строки в НЁМ, а не в написанном здесь образце.
cml() {
  local tag="$1"
  local d="$FIX/cml-$tag"
  mkdir -p "$d"
  cp "$ROOT/CMakeLists.txt" "$d/CMakeLists.txt"
  printf '%s\n' "$d"
}
expect pass "настоящее дерево" assert_msvc_runtime_wired "$ROOT"

case_cml_gone() {
  local d; d=$(cml gone)
  grep -v 'cmake/msvc_runtime\.cmake' "$d/CMakeLists.txt" > "$d/x" && mv "$d/x" "$d/CMakeLists.txt"
  cmp -s "$d/CMakeLists.txt" "$ROOT/CMakeLists.txt" && return 0
  assert_msvc_runtime_wired "$d"
}
expect fail "include статического CRT вырезан" case_cml_gone

# Строка на месте, но ПОСЛЕ зависимостей: цели glfw, imgui и flecs объявлены раньше и возьмут
# динамический рантайм — в одном образе окажутся две кучи. Отличить это от честного дерева можно
# только по номеру строки.
case_cml_late() {
  local d; d=$(cml late)
  python3 - "$d/CMakeLists.txt" <<'PY'
import sys
p = sys.argv[1]
lines = open(p, encoding='utf-8').read().splitlines(True)
inc = [i for i, l in enumerate(lines) if 'cmake/msvc_runtime.cmake' in l and not l.lstrip().startswith('#')]
dep = [i for i, l in enumerate(lines) if 'FetchContent_Declare' in l and not l.lstrip().startswith('#')]
moved = lines.pop(inc[0])
lines.insert(dep[-1], moved)
open(p, 'w', encoding='utf-8').write(''.join(lines))
PY
  cmp -s "$d/CMakeLists.txt" "$ROOT/CMakeLists.txt" && return 0
  assert_msvc_runtime_wired "$d"
}
expect fail "include уехал ниже первой зависимости" case_cml_late

# Файл без зависимостей вовсе: сравнивать порядок не с чем, и «нарушений нет» на нём означало бы,
# что гейт разучился читать дерево.
case_cml_nodeps() {
  local d; d=$(cml nodeps)
  grep -v 'FetchContent_Declare' "$d/CMakeLists.txt" > "$d/x" && mv "$d/x" "$d/CMakeLists.txt"
  cmp -s "$d/CMakeLists.txt" "$ROOT/CMakeLists.txt" && return 0
  assert_msvc_runtime_wired "$d"
}
expect fail "в дереве не осталось ни одной зависимости" case_cml_nodeps

# Две копии закрытого списка redist-имён. Фикстуры — копии НАСТОЯЩИХ файлов: предмет утверждения
# есть равенство того, что написано в дереве, а не в написанном здесь образце.
mirror_fix() {
  local d="$FIX/mirror-$1"
  mkdir -p "$d/scripts" "$d/cmake"
  cp "$ROOT/scripts/release_crt_lib.sh" "$d/scripts/release_crt_lib.sh"
  cp "$ROOT/cmake/msvc_redist.cmake" "$d/cmake/msvc_redist.cmake"
  printf '%s\n' "$d"
}
expect pass "настоящее дерево · копии списка" assert_redist_list_mirrored "$ROOT"

case_mirror_shell() {
  local d; d=$(mirror_fix shell)
  sed 's/|concrt\[0-9\]\*\.dll//' "$d/scripts/release_crt_lib.sh" > "$d/x" \
    && mv "$d/x" "$d/scripts/release_crt_lib.sh"
  cmp -s "$d/scripts/release_crt_lib.sh" "$ROOT/scripts/release_crt_lib.sh" && return 0
  assert_redist_list_mirrored "$d"
}
expect fail "имя пропало из копии в шелле" case_mirror_shell

case_mirror_cmake() {
  local d; d=$(mirror_fix cmake)
  sed 's/|concrt|/|/' "$d/cmake/msvc_redist.cmake" > "$d/x" && mv "$d/x" "$d/cmake/msvc_redist.cmake"
  cmp -s "$d/cmake/msvc_redist.cmake" "$ROOT/cmake/msvc_redist.cmake" && return 0
  assert_redist_list_mirrored "$d"
}
expect fail "имя пропало из копии в cmake" case_mirror_cmake

# Разбор, промахнувшийся мимо ОБОИХ файлов, сравнивает пустое с пустым и печатает «совпадают».
case_mirror_blind() {
  local d; d=$(mirror_fix blind)
  sed 's/^crt_is_redist() {/crt_is_redist_renamed() {/' "$d/scripts/release_crt_lib.sh" > "$d/x" \
    && mv "$d/x" "$d/scripts/release_crt_lib.sh"
  sed 's/MATCHES "^(/MATCHES "^X(/' "$d/cmake/msvc_redist.cmake" > "$d/y" \
    && mv "$d/y" "$d/cmake/msvc_redist.cmake"
  # Порча сверяется с оригиналом в ОБОИХ файлах: кейс ломает обе копии, и промах sed по любой из них
  # оставил бы её исправной — «утверждение отбило подмену» вышло бы из подмены, которой не было.
  cmp -s "$d/scripts/release_crt_lib.sh" "$ROOT/scripts/release_crt_lib.sh" && return 0
  cmp -s "$d/cmake/msvc_redist.cmake" "$ROOT/cmake/msvc_redist.cmake" && return 0
  assert_redist_list_mirrored "$d"
}
expect fail "разбор не нашёл ни одной из копий" case_mirror_blind

# Третья пара копий того же списка — app-local состав против импортов рантайма — вынесена своим
# набором: ломать там нечего (утверждение зовёт expected_files как ЗАГРУЖЕННУЮ реализацию), и
# предмет у неё свой. Зовётся ВНЕШНЕЙ командой, чтобы в логе стояло имя упавшего набора.
bash "$ROOT/scripts/check_release_crt_named_selftest.sh" || BAD=1

if [ "$BAD" != 0 ]; then echo "crt-wiring-selftest: FAIL" >&2; exit 1; fi
echo "crt-wiring-selftest: PASS"
