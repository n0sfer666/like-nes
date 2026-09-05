#!/usr/bin/env bash
# Позитивный контроль ЧИТАТЕЛЯ таблиц импортов (спека #20, вертикаль 5). Граница с соседним набором
# проходит по предмету, а не по счётчику строк: check_release_crt_selftest.sh ломает ПАКЕТ (кто чего
# просит и что рядом лежит), а здесь ломается сам разбор PE — формы, которые пишут разные линкеры, и
# места, где честного ответа у читателя нет. Молчаливое «имён больше нет» в таком месте неотличимо
# от статически слинкованного бинаря, поэтому каждая порча обязана давать ОТКАЗ, а не пустой список.
#
# Второе утверждение набора — якорь: синтетические фикстуры доказывают, что читатель понимает ЭТОТ
# генератор, и ничего не говорят о том, понимает ли он формат Microsoft.
set -uo pipefail

ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
# shellcheck source=scripts/release_crt_lib.sh
. "$ROOT/scripts/release_crt_lib.sh"

BAD=0
FIX=$(mktemp -d)
trap 'rm -rf "$FIX"' EXIT

expect() {
  local want="$1" name="$2"; shift 2
  local rc=0
  "$@" >/dev/null 2>&1 || rc=$?
  if { [ "$want" = pass ] && [ "$rc" = 0 ]; } || { [ "$want" = fail ] && [ "$rc" != 0 ]; }; then
    printf 'crt-reader-selftest: OK   %s (%s)\n' "$name" "$want"
  else
    printf 'crt-reader-selftest: БРАК %s: ожидали %s, получили код %s\n' "$name" "$want" "$rc" >&2
    BAD=1
  fi
}

# Каждое имя, которое набор зовёт, обязано быть ОПРЕДЕЛЕНО: незагруженная функция даёт код 127, а он
# ненулевой — то есть все ожидающие fail кейсы проезжали бы как «утверждение отбило подмену».
for fn in assert_static_crt assert_pe_reader_anchor; do
  if ! declare -F "$fn" >/dev/null; then
    printf 'crt-reader-selftest: БРАК утверждение %s не определено — набор проверял бы код 127\n' "$fn" >&2
    exit 1
  fi
done

# Фикстура кладётся одна в каталог и осматривается assert_static_crt: он зовёт читателя на КАЖДОМ
# нашем бинаре и обязан свалиться, если тот отказал.
fixture() {
  local tag="$1"; shift
  local d="$FIX/$tag"
  mkdir -p "$d"
  python3 "$ROOT/scripts/pe_fixture.py" "$d/editor_shell.exe" "$@" || return 1
  assert_static_crt "$ROOT" "$d"
}

# Опорный прогон: честная фикстура с одним системным импортом обязана ПРОХОДИТЬ. Без него «порча
# отбита» выходило бы из утверждения, которое падает на чём угодно.
expect pass "честная фикстура разобрана" fixture good --import KERNEL32.dll

# Бинарь, который читатель не разобрал, обязан валить утверждение, а не проезжать как «импортов
# нет»: до починки отказ python внутри `done < <(…)` не был виден коду возврата цикла вовсе.
case_not_pe() {
  local d="$FIX/notpe"
  mkdir -p "$d"
  printf 'MZ this is not a PE\n' > "$d/editor_shell.exe"
  assert_static_crt "$ROOT" "$d"
}
expect fail "файл с расширением .exe, но не PE" case_not_pe

# PE без единого импорта: читатель обязан отличать «таблица пуста» от «прочитано ничего», иначе
# любой битый образ проезжал бы как бинарь без зависимостей.
expect fail "PE без таблицы импортов" fixture noimp

# Отложенный импорт СТАРОГО формата (абсолютные адреса вместо RVA) и 32-разрядный образ: обе формы
# живут в природе, и читатель, знающий по одной ветке каждой развилки, отдал бы «зависимостей нет»
# на бинаре чужого линкера. Проверяется здесь, а не якорем: настоящий wgpu_native.dll — PE32+ и
# отложенных импортов не имеет вовсе.
expect fail "отложенный импорт абсолютными адресами" \
    fixture va --import KERNEL32.dll --delay msvcp140.dll --delay-va
expect fail "32-разрядный образ" fixture pe32 --import KERNEL32.dll --import msvcp140.dll --pe32

# Заниженный (но ненулевой) Size в каталоге данных — форма, которую пишут реальные линкеры и которую
# загрузчик Windows не читает вовсе, идя до нулевого дескриптора. Читатель, принявший Size за
# границу, вернул бы только первое имя и промолчал бы об этом.
expect fail "заниженный Size в каталоге импортов" \
    fixture short-plain --short-size --import KERNEL32.dll --import msvcp140.dll
expect fail "заниженный Size в каталоге отложенных импортов" \
    fixture short-delay --short-size --delay KERNEL32.dll --delay msvcp140.dll

# Три места, где у читателя нет честного ответа и где он до починки МОЛЧАЛ. Каждое даёт пустой
# список имён, то есть выглядит ровно как статически слинкованный бинарь — и пакет с динамическим
# CRT проезжал бы через оба утверждения вертикали.
# 1. RVA каталога уведён в НЕинициализированный хвост секции: файловых байтов там нет.
expect fail "каталог импортов не отображается в файл" fixture unmapped --import KERNEL32.dll --unmapped-dir
# 2. Дескрипторы кончились концом секции, нулевого среди них нет: обход упёрся в границу.
expect fail "таблица дескрипторов без нулевого терминатора" \
    fixture noterm --import KERNEL32.dll --import msvcp140.dll --no-terminator
# 3. NumberOfRvaAndSizes занижено: каталог 13 объявлен вне таблицы, и читатель обязан сказать это
#    вслух, а не молча пропустить отложенные импорты.
expect fail "каталог отложенных импортов вне таблицы каталогов" \
    fixture nrva --import KERNEL32.dll --delay msvcp140.dll --nrva 10

# Якорь. Настоящий wgpu_native.dll из выкачанной дистрибуции — единственное в наборе, что собрано
# ЧУЖИМ тулчейном; на нём утверждение обязано проходить, а с вырезанным ожидаемым именем — падать.
# Без второй половины якорь не имеет ни одной фикстуры, где он падает, и неотличим от отсутствующего.

# Корень БЕЗ дистрибуции: якорь обязан вернуть ноль и сказать про пропуск вслух. Молчаливый ноль
# здесь неотличим от пройденного якоря, поэтому проверяется само слово в выводе.
case_anchor_skip() {
  local d="$FIX/noroot" out
  mkdir -p "$d"
  out=$(assert_pe_reader_anchor "$d") || return 1
  printf '%s' "$out" | grep -q 'ПРОПУСК' || return 1
}
expect pass "дистрибуции нет — якорь пропущен ВСЛУХ" case_anchor_skip

if ! REAL=$(crt_real_dll "$ROOT"); then
  echo "crt-reader-selftest: ПРОПУСК — настоящего wgpu_native.dll нет в дереве, якорь не проверен"
else
  # Дерево-фикстура повторяет ОБА пути, по которым якорь ходит от корня: и дистрибуцию, и самого
  # читателя. Корень у него один на то и другое, поэтому подменить дистрибуцию, оставив корнем
  # репозиторий, нечем — а править файл в дереве значило бы ломать рабочую копию.
  anchor_tree() {
    # Объявления РАЗДЕЛЕНЫ: в bash 3.2 (macOS) вторая переменная одного `local` не видит первую, и
    # каталог фикстуры молча становился пустым именем — кейс «имени в импортах нет» проходил тогда
    # не потому, что якорь отбил подмену.
    local tag="$1"
    local d="$FIX/$tag"
    local sub
    sub="$d/build-fix/$(crt_dist_subdir)"
    mkdir -p "$sub" "$d/scripts"
    cp "$ROOT/scripts/pe_imports.py" "$d/scripts/" || return 1
    cp "$REAL" "$sub/wgpu_native.dll" || return 1
    printf '%s\n' "$d"
  }
  case_anchor_ok() { local d; d=$(anchor_tree okreal) || return 1; assert_pe_reader_anchor "$d"; }
  expect pass "настоящий wgpu_native.dll разобран" case_anchor_ok

  # Имя вырезается ПОДМЕНОЙ РАВНОЙ ДЛИНЫ: обрезать файл значило бы сломать заголовки и проверять
  # отказ читателя, а не сверку имён.
  case_anchor_missing() {
    local d
    d=$(anchor_tree cut) || return 1
    python3 - "$d/build-fix/_deps/webgpu-backend-wgpu-src/bin/windows-x86_64/wgpu_native.dll" <<'PY' || return 1
import sys
p = sys.argv[1]
b = open(p, 'rb').read()
if b.count(b'VCRUNTIME140.dll') != 1:
    sys.exit('фикстура: имя VCRUNTIME140.dll встречается не один раз')
open(p, 'wb').write(b.replace(b'VCRUNTIME140.dll', b'VCRUNTIME141.dll'))
PY
    assert_pe_reader_anchor "$d"
  }
  expect fail "ожидаемого имени в импортах нет" case_anchor_missing
fi

if [ "$BAD" != 0 ]; then echo "crt-reader-selftest: FAIL" >&2; exit 1; fi
echo "crt-reader-selftest: PASS"
