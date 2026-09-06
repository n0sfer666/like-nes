#!/usr/bin/env bash
# Позитивный контроль гейта примеров (гейт 2 спеки #19). Утверждение, у которого нет фикстуры, где
# оно падает, неотличимо от отсутствующего, поэтому каждое ломается своим деревом: примера нет,
# голдена нет, голден пережил пример, цель не собрана, пример упал кодом, вывод разошёлся.
#
# Фикстурные деревья строит ОДНА фабрика, и бинарь в них — скрипт-заглушка: предмет набора — что
# утверждения умеют падать, а не компилятор. Настоящую сборку проверяет сам гейт на дереве.
#
#   bash scripts/check_docs_examples_selftest.sh [каталог-сборки]   # по умолчанию build-full
set -uo pipefail

ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
# shellcheck source=scripts/docs_examples_lib.sh
. "$ROOT/scripts/docs_examples_lib.sh"
# shellcheck source=scripts/selftest_sub_lib.sh
. "$ROOT/scripts/selftest_sub_lib.sh"

bad() { printf 'docs-examples-selftest: БРАК %s\n' "$1" >&2; }

BAD=0
FIX=$(mktemp -d)
trap 'rm -rf "$FIX"' EXIT

expect() {
  local want="$1" name="$2"; shift 2
  local rc=0
  "$@" >/dev/null 2>&1 || rc=$?
  if { [ "$want" = pass ] && [ "$rc" = 0 ]; } || { [ "$want" = fail ] && [ "$rc" != 0 ]; }; then
    printf 'docs-examples-selftest: OK   %s (%s)\n' "$name" "$want"
  else
    printf 'docs-examples-selftest: БРАК %s: ожидали %s, получили код %s\n' "$name" "$want" "$rc" >&2
    BAD=1
  fi
}

# Незагруженная функция даёт код 127, а он ненулевой — то есть каждый ожидающий fail кейс проезжал
# бы как «утверждение отбило порчу», не позвав утверждения ни разу.
for fn in assert_examples_present assert_examples_paired assert_example_runs assert_examples_run_all; do
  declare -F "$fn" >/dev/null || { bad "утверждение $fn не определено"; exit 1; }
done

# Фабрика: дерево с одним примером, его голденом и каталогом сборки, где лежит заглушка. Голден и
# заглушка согласованы по построению — порчу вносит вызывающий кейс, а не фабрика.
ex_tree() {
  local tag="$1"
  # Второе присваивание в ОДНОМ `local` читает первое до его установки: bash 3.2 на macOS отвечает
  # `unbound variable`, фабрика умирает, а кейс, ожидающий отказа утверждения, читает этот код как
  # «утверждение отбило порчу» — не позвав его ни разу. Тот же капкан уже ловили в наборе вертикали 3.
  local d="$FIX/$tag"
  mkdir -p "$d/$(docs_examples_dir)" "$d/build"
  printf 'int main() { return 0; }\n' > "$d/$(docs_examples_dir)/sample.cpp"
  printf 'first line\nsecond line\n' > "$d/$(docs_examples_dir)/sample.out"
  printf '#!/bin/sh\nprintf "first line\\nsecond line\\n"\n' > "$d/build/doc_example_sample"
  chmod +x "$d/build/doc_example_sample"
  printf '%s\n' "$d"
}

case_ok() {
  local d; d=$(ex_tree ok) || return 1
  assert_examples_present "$d" && assert_examples_paired "$d" && assert_examples_run_all "$d" "$d/build"
}
expect pass "исправное дерево проходит все три утверждения" case_ok

# 10. Ноль примеров: каталог описывает сам себя, и промах пути читался бы как чистый прогон.
case_none() {
  local d; d=$(ex_tree none) || return 1
  rm -f "$d/$(docs_examples_dir)/sample.cpp"
  assert_examples_present "$d"
}
expect fail "в каталоге нет ни одного примера" case_none

# 11. Пример без голдена: сверять его вывод не с чем, а гейт молчал бы про самый интересный файл.
case_no_golden() {
  local d; d=$(ex_tree nogold) || return 1
  rm -f "$d/$(docs_examples_dir)/sample.out"
  assert_examples_paired "$d"
}
expect fail "у примера нет .out" case_no_golden

# 15. Голден пережил свой пример: его больше не читает никто, и он молча описывает вчерашний код.
case_stale_golden() {
  local d; d=$(ex_tree stale) || return 1
  printf 'nothing prints this\n' > "$d/$(docs_examples_dir)/gone.out"
  assert_examples_paired "$d"
}
expect fail "голден пережил свой пример" case_stale_golden

# 12. Цель не собрана: каталог сборки гейт не создаёт, поэтому промах каталога обязан быть слышен
# отказом, а не пропуском.
case_not_built() {
  local d; d=$(ex_tree nobin) || return 1
  rm -f "$d/build/doc_example_sample"
  assert_examples_run_all "$d" "$d/build"
}
expect fail "цель doc_example_sample не собрана" case_not_built

# 13. Пример упал: код возврата спрашивается НАРАВНЕ с выводом — программа, печатающая правильный
# текст и падающая на выходе, работающей не является.
case_nonzero() {
  local d; d=$(ex_tree rc) || return 1
  printf '#!/bin/sh\nprintf "first line\\nsecond line\\n"\nexit 3\n' > "$d/build/doc_example_sample"
  chmod +x "$d/build/doc_example_sample"
  assert_examples_run_all "$d" "$d/build"
}
expect fail "пример упал ненулевым кодом" case_nonzero

# 14. Вывод разошёлся с голденом — то, ради чего гейт и заведён: текст руководства показывает этот
# вывод врезкой, и расхождение здесь есть расхождение документации с движком.
case_diff() {
  local d; d=$(ex_tree diff) || return 1
  printf '#!/bin/sh\nprintf "first line\\nOTHER\\n"\n' > "$d/build/doc_example_sample"
  chmod +x "$d/build/doc_example_sample"
  assert_examples_run_all "$d" "$d/build"
}
expect fail "вывод разошёлся с голденом" case_diff

# Та же находка в форме, к которой слепа подстановка $( … ), — и порча здесь именно ХВОСТОВАЯ: обе
# строки на месте, нет только финального перевода строки. Выброшенная целиком строка (первая версия
# этого кейса) ловится и подстановкой, то есть решение «сравнение ФАЙЛАМИ» не пришпиливала ничем:
# замени diff на `[ "$(cat …)" = "$(…)" ]` — и набор остался бы зелёным (находка ревью).
case_lost_tail() {
  local d; d=$(ex_tree tail) || return 1
  printf '#!/bin/sh\nprintf "first line\\nsecond line"\n' > "$d/build/doc_example_sample"
  chmod +x "$d/build/doc_example_sample"
  assert_examples_run_all "$d" "$d/build"
}
expect fail "вывод примера потерял хвостовой перевод строки" case_lost_tail

# Снятие CR — мера, а не порча: на Windows stdout идёт текстовым режимом, и голден с окончаниями LF
# расходился бы с исправным примером ровно на одной ОС. Опорный `pass` и порча реализации стоят
# парой — без второй половины мера могла бы исчезнуть незамеченной.
ORIG_STRIP=$(declare -f docs_example_strip_cr)
crlf_tree() {
  local tag="$1" d
  d=$(ex_tree "$tag") || return 1
  printf '#!/bin/sh\nprintf "first line\\r\\nsecond line\\r\\n"\n' > "$d/build/doc_example_sample"
  chmod +x "$d/build/doc_example_sample"
  printf '%s\n' "$d"
}
case_crlf_ok() {
  local d; d=$(crlf_tree crlf) || return 1
  assert_examples_run_all "$d" "$d/build"
}
expect pass "вывод с CRLF сходится с голденом на LF" case_crlf_ok

case_crlf_unstripped() {
  (
    local d; d=$(crlf_tree crlfbad) || exit 1
    docs_example_strip_cr() { cat; }
    subbed docs_example_strip_cr "$ORIG_STRIP" || exit 1
    assert_examples_run_all "$d" "$d/build"
  )
}
expect fail "возврат каретки не снят с вывода — голден разошёлся бы на Windows" case_crlf_unstripped

# Вторая половина той же меры, и без неё гейт краснел бы на windows-раннере на КАЖДОЙ строке:
# `core.autocrlf=true` выводит из checkout с CRLF сам ГОЛДЕН, а CR снимался только с вывода
# программы. Порча живёт на стороне, которую первая пара кейсов воспроизвести не может по
# построению (там голден всегда LF), — находка ревью раунда #19.
crlf_golden_tree() {
  local tag="$1" d
  d=$(ex_tree "$tag") || return 1
  printf 'first line\r\nsecond line\r\n' > "$d/$(docs_examples_dir)/sample.out"
  printf '%s\n' "$d"
}
case_crlf_golden_ok() {
  local d; d=$(crlf_golden_tree crlfgold) || return 1
  assert_examples_run_all "$d" "$d/build"
}
expect pass "голден с CRLF сходится с выводом на LF" case_crlf_golden_ok

case_crlf_golden_unstripped() {
  (
    local d; d=$(crlf_golden_tree crlfgoldbad) || exit 1
    docs_example_strip_cr() { cat; }
    subbed docs_example_strip_cr "$ORIG_STRIP" || exit 1
    assert_examples_run_all "$d" "$d/build"
  )
}
expect fail "возврат каретки не снят с голдена — гейт краснел бы на Windows" case_crlf_golden_unstripped

# Утверждения на НАСТОЯЩЕМ дереве: фикстура игрушечная по построению, и утверждение, случайно
# заточенное под неё, выглядело бы здоровым ровно до первого прогона гейта. Каталога сборки на
# свежем клоне нет (`build*` в .gitignore) — тогда пропуск ВСЛУХ, иначе набор падал бы по причине
# ОКРУЖЕНИЯ, неотличимо от честно сломанного утверждения.
case_real_paired() { assert_examples_present "$ROOT" && assert_examples_paired "$ROOT"; }
expect pass "настоящее дерево · примеры и голдены парны" case_real_paired

# Каталог сборки принимается аргументом с тем же умолчанием, что у гейта: у CI он называется
# `build`, и прибитый build-full молча пропускал бы там единственный кейс, который смотрит на
# настоящие бинари, — то есть на трёх ОС набор проверял бы только фикстуры.
REAL_BUILD="${1:-build-full}"
case "$REAL_BUILD" in /*) ;; *) REAL_BUILD="$ROOT/$REAL_BUILD" ;; esac
if [ -d "$REAL_BUILD" ]; then
  expect pass "настоящее дерево · примеры собраны и сходятся с голденами" \
    assert_examples_run_all "$ROOT" "$REAL_BUILD"
else
  echo "docs-examples-selftest: ПРОПУСК — каталога сборки $REAL_BUILD нет, запуск примеров на дереве не проверен"
fi

if [ "$BAD" != 0 ]; then echo "docs-examples-selftest: FAIL" >&2; exit 1; fi
echo "docs-examples-selftest: PASS"
