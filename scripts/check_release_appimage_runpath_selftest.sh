#!/usr/bin/env bash
# Позитивный контроль связи ВНУТРИ ELF (спека #20, вертикаль 4, шаг B). Свой файл по границе
# предмета — той же, что развела check_release_dmg_rpath-утверждение и остальной набор шага A: у
# соседей предмет есть содержимое AppDir (имена, права, суммы), а здесь — то, чем исполняемый
# находит рантайм, и читает это единственный во всём наборе `readelf`.
#
# Фикстуры тут НАСТОЯЩИЕ ELF от `cc`, а не текстовые файлы остальных наборов: о тексте readelf не
# скажет ни DT_NEEDED, ни DT_RUNPATH, и утверждение, проверенное на таком «бинаре», проверяло бы
# собственную ветку отказа, а не то, ради чего написано.
set -uo pipefail

ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
# shellcheck source=scripts/release_lib.sh
. "$ROOT/scripts/release_lib.sh"
# shellcheck source=scripts/release_check_lib.sh
. "$ROOT/scripts/release_check_lib.sh"
# shellcheck source=scripts/release_appimage_lib.sh
. "$ROOT/scripts/release_appimage_lib.sh"
# shellcheck source=scripts/release_appimage_runpath_lib.sh
. "$ROOT/scripts/release_appimage_runpath_lib.sh"
# shellcheck source=scripts/release_appimage_fixture_lib.sh
. "$ROOT/scripts/release_appimage_fixture_lib.sh"

# Пропуск ВСЛУХ и кодом 0: `$ORIGIN` — линуксовое поле, и ни `cc` на macOS, ни git-bash на Windows
# ELF с ним не сделают. Молчаливый пропуск в общем прогоне читался бы как пройденный набор.
if [ "$(uname -s)" != Linux ]; then
  echo "appimage-runpath-selftest: ПРОПУСК — предмет набора есть ELF и \$ORIGIN (здесь $(uname -s))"
  exit 0
fi
for t in cc readelf; do
  command -v "$t" >/dev/null 2>&1 || {
    echo "appimage-runpath-selftest: ПРОПУСК — нет $t, фикстурных ELF не из чего сделать"
    exit 0
  }
done

RUNTIME=libwgpu_native.so
BAD=0
FIX=$(mktemp -d)
trap 'rm -rf "$FIX"' EXIT

expect() {
  local want="$1" name="$2"; shift 2
  local rc=0
  "$@" >/dev/null 2>&1 || rc=$?
  # Код 2 сценарии отдают, когда не собралась сама фикстура. Без этой ветки он читался бы как
  # «утверждение отбило подмену» у каждого ожидания fail, то есть несобравшийся `cc` делал бы
  # половину набора зелёной, ничего не проверив.
  if [ "$rc" = 2 ]; then
    printf 'appimage-runpath-selftest: БРАК %s: фикстурный ELF не построился\n' "$name" >&2
    BAD=1
  elif { [ "$want" = pass ] && [ "$rc" = 0 ]; } || { [ "$want" = fail ] && [ "$rc" != 0 ]; }; then
    printf 'appimage-runpath-selftest: OK   %s (%s)\n' "$name" "$want"
  else
    printf 'appimage-runpath-selftest: БРАК %s: ожидали %s, получили код %s\n' "$name" "$want" "$rc" >&2
    BAD=1
  fi
}

# Каждый сценарий строит свой usr/bin: SONAME, под которым библиотека попадает в исполняемый, и
# путь поиска — ровно те две величины, о которых утверждение и говорит.
case_elf() {
  local d="$FIX/$1" soname="$2" runpath="$3"
  [ -d "$d/usr/bin" ] || appimage_fixture_elf "$d/usr/bin" "$RUNTIME" "$soname" "$runpath" || return 2
  assert_appimage_runpath "$d" "$RUNTIME"
}

# Опора набора: так выглядит то, что собирает наш CMake. Без неё каждая порча ниже проходила бы
# «отбитой» и на утверждении, которое отбивает вообще всё.
expect pass "\$ORIGIN рядом с исполняемым" case_elf origin "$RUNTIME" '$ORIGIN'
expect pass "\$ORIGIN с подкаталогом" case_elf originsub "$RUNTIME" '$ORIGIN/../bin'

# Абсолютный путь поиска ведёт в каталог сборки: у владельца образ стартует, у пользователя нет.
# Именно от этого reproducible_rpath переопределяет rpath после копирующего помощника CMake.
# `$ORIGIN` без разделителя — не `$ORIGIN`: шаблон-префикс принимал бы соседний каталог с похожим
# именем, к образу отношения не имеющий. Находка ревью шага B вертикали 4.
expect fail "имя, начинающееся с \$ORIGIN" case_elf originfoo "$RUNTIME" '$ORIGINFOO/lib'
# Перевес `..` над глубиной usr/bin выводит поиск НА МАШИНУ, где образ запускают. Пара с опорной
# `$ORIGIN/../bin` выше: сам по себе `..` законен, и отбивать его значило бы валить честную раскладку.
expect fail "runpath выходит за пределы AppDir" case_elf escape "$RUNTIME" '$ORIGIN/../../../etc'
expect fail "абсолютный runpath" case_elf abs "$RUNTIME" /build/stage/like-nes/bin
expect fail "runpath отсутствует вовсе" case_elf none "$RUNTIME" ''

# Вторая половина связки: путь поиска на месте, а имени библиотеки в DT_NEEDED нет — искать нечего.
expect fail "чужой SONAME в DT_NEEDED" case_elf soname libother.so '$ORIGIN'

# Имя рантайма пустое — утверждать не о чем, и это отказ, а не «нарушений нет»: цикл сравнивает с
# ним каждое имя, и с пустым он объявил бы годным любой каталог.
expect fail "имя рантайма не названо" assert_appimage_runpath "$FIX/origin" ''

# Каталог без единого ELF — та же ловушка, что ловит правило vacuous-gate в ci_lint.py: список
# нарушений пуст, потому что осматривать было нечего, а не потому что всё в порядке.
case_empty() {
  local d="$FIX/empty"
  mkdir -p "$d/usr/bin"
  cp "$FIX/origin/usr/bin/$RUNTIME" "$d/usr/bin/$RUNTIME"
  assert_appimage_runpath "$d" "$RUNTIME"
}
expect fail "в usr/bin только рантайм — осматривать нечего" case_empty

# Не-ELF обязан быть отбит СВОЕЙ причиной: на части сборок binutils readelf отвечает о текстовом
# файле кодом НОЛЬ, поэтому «пропустили молча» и «нарушений нет» без этой проверки неразличимы.
case_nonelf() {
  local d="$FIX/nonelf" out
  [ -d "$d/usr/bin" ] || appimage_fixture_elf "$d/usr/bin" "$RUNTIME" "$RUNTIME" '$ORIGIN' || return 2
  printf '#!/bin/sh\necho я не ELF\n' > "$d/usr/bin/editor_shell"
  chmod 755 "$d/usr/bin/editor_shell"
  out=$(assert_appimage_runpath "$d" "$RUNTIME" 2>&1) && return 1
  printf '%s' "$out" | grep -q 'editor_shell(не ELF)'
}
expect pass "не-ELF отбит и назван поимённо" case_nonelf

if [ "$BAD" != 0 ]; then echo "appimage-runpath-selftest: FAIL" >&2; exit 1; fi
echo "appimage-runpath-selftest: PASS"
