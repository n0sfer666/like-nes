#!/usr/bin/env bash
# Позитивный контроль утверждений о ПАРЕ ЯЗЫКОВ (спека #19, вертикаль 1): зеркальность каталогов,
# свежесть переводов по штампу и переключатель языка. Утверждение, у которого нет фикстуры, где оно
# падает, неотличимо от отсутствующего — а гейт документации зелен на дереве, которое сам же и
# описывает, то есть без порчи о нём не известно ровно ничего.
#
# Граница с check_docs_content_selftest.sh — по ПРЕДМЕТУ, ровно как у пары
# check_release_selftest.sh / check_release_pack_selftest.sh: здесь ломается СВЯЗЬ двух языков, там —
# содержимое одного документа. Сосед зовётся ВНЕШНЕЙ командой, чтобы в логе стояло имя упавшего
# набора.
set -uo pipefail

ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
export DOCS_FIXTURE_SELF="$ROOT"
# shellcheck source=scripts/release_lib.sh
. "$ROOT/scripts/release_lib.sh"
# shellcheck source=scripts/docs_pairs_lib.sh
. "$ROOT/scripts/docs_pairs_lib.sh"
# shellcheck source=scripts/docs_fixture_lib.sh
. "$ROOT/scripts/docs_fixture_lib.sh"

BAD=0
FIX=$(mktemp -d)
trap 'rm -rf "$FIX"' EXIT

expect() {
  local want="$1" name="$2"; shift 2
  local rc=0
  "$@" >/dev/null 2>&1 || rc=$?
  if { [ "$want" = pass ] && [ "$rc" = 0 ]; } || { [ "$want" = fail ] && [ "$rc" != 0 ]; }; then
    printf 'docs-selftest: OK   %s (%s)\n' "$name" "$want"
  else
    printf 'docs-selftest: БРАК %s: ожидали %s, получили код %s\n' "$name" "$want" "$rc" >&2
    BAD=1
  fi
}

# Незагруженная функция даёт код 127, а он ненулевой — то есть каждый ожидающий fail кейс проезжал
# бы как «утверждение отбило порчу», не позвав утверждения ни разу.
# Помощники сюда входят наравне с утверждениями: печать и разбор штампа живут в той же библиотеке,
# и незагруженная функция даёт тот же код 127 — то есть кейс проезжал бы как «порча отбита».
for fn in assert_docs_mirrored assert_docs_translations_fresh assert_docs_language_switch \
          docs_pairs docs_pair_critical docs_stamp_of docs_normalize docs_ok docs_bad docs_warn; do
  declare -F "$fn" >/dev/null || {
    printf 'docs-selftest: БРАК утверждение %s не определено\n' "$fn" >&2
    exit 1
  }
done

# Каждой порче — своё дерево: гейт читает файлы, и порча, оставшаяся от прошлого кейса, судила бы
# следующий.
tree_of() {
  local d="$FIX/$1"
  docs_fixture_tree "$d" >/dev/null || return 1
  printf '%s\n' "$d"
}

case_on() {
  local tag="$1" fn="$2" d
  shift 2
  d=$(tree_of "$tag") || return 1
  "$@" "$d" || return 1
  "$fn" "$d"
}

noop() { :; }

expect pass "исправная фикстура · зеркальность" case_on ok-mirror assert_docs_mirrored noop
expect pass "исправная фикстура · свежесть переводов" case_on ok-fresh assert_docs_translations_fresh noop
expect pass "исправная фикстура · переключатель языка" case_on ok-switch assert_docs_language_switch noop

# Якорь на НАСТОЯЩЕМ дереве: фикстура игрушечная по построению, и утверждение, случайно заточенное
# под неё, выглядело бы здоровым ровно до первого прогона гейта.
anchor_real() {
  assert_docs_mirrored "$ROOT" && assert_docs_translations_fresh "$ROOT" && assert_docs_language_switch "$ROOT"
}
expect pass "настоящее дерево репозитория · все три утверждения" anchor_real

# --- зеркальность -------------------------------------------------------------------------------
drop_ru() { rm -f "$1/docs/ru/getting-started/build.md"; }
expect fail "перевод пропал из docs/ru" case_on m-drop assert_docs_mirrored drop_ru

add_orphan() { printf 'x\n' > "$1/docs/ru/orphan.md"; }
expect fail "в docs/ru лежит документ без en-источника" case_on m-orphan assert_docs_mirrored add_orphan

# Пустое равно пустому: обход, промахнувшийся мимо обоих каталогов, обязан быть отказом, а не
# «структура зеркальна».
empty_both() { rm -rf "${1:?}/docs"; mkdir -p "$1/docs/en" "$1/docs/ru"; }
expect fail "оба каталога документации пусты" case_on m-empty assert_docs_mirrored empty_both

# --- свежесть перевода --------------------------------------------------------------------------
# Критичный раздел: по нему человек ставит движок, и устаревшая инструкция не «немного не та».
stale_critical() { printf '\nnew paragraph\n' >> "$1/docs/en/getting-started/build.md"; }
expect fail "критичный перевод отстал от источника" case_on f-crit assert_docs_translations_fresh stale_critical

# Обратная половина того же различия, и она опорная: некритичное отставание обязано ПРОХОДИТЬ с
# предупреждением. Без этого кейса «ошибка на всём подряд» выглядела бы ровно так же.
stale_minor() { printf '\nnew paragraph\n' >> "$1/docs/en/index.md"; }
expect pass "некритичный перевод отстал — предупреждение, не ошибка" case_on f-minor assert_docs_translations_fresh stale_minor

no_stamp() { sed -i.bak '/^<!-- en-sha256: /d' "$1/README.ru.md" && rm -f "$1/README.ru.md.bak"; }
expect fail "у перевода снят штамп источника" case_on f-nostamp assert_docs_translations_fresh no_stamp

# Отсутствие штампа есть ошибка ВСЕГДА, в том числе у некритичного перевода: файл без штампа
# неотличим от переведённого когда угодно, и различие «ошибка против предупреждения» относится к
# отставанию, а не к тому, что сверять нечем. Кейс выше снимает штамп с КРИТИЧНОГО файла, то есть
# сам по себе эту ветку не отделяет от общего правила.
no_stamp_minor() { sed -i.bak '/^<!-- en-sha256: /d' "$1/docs/ru/index.md" && rm -f "$1/docs/ru/index.md.bak"; }
expect fail "штамп снят с НЕкритичного перевода" case_on f-nostamp-minor assert_docs_translations_fresh no_stamp_minor

# Штамп читается РОВНО СО СТРОКИ 1: документ, объясняющий сам механизм и цитирующий образец штампа
# в теле, иначе читался бы как проштампованный, ни разу его не неся.
stamp_quoted() {
  local f="$1/docs/ru/getting-started/build.md" stamp
  stamp=$(sed -n '1p' "$f")
  sed -i.bak '1d' "$f" && rm -f "$f.bak"
  printf '\n%s\n' "$stamp" >> "$f"
  grep -q '^<!-- en-sha256: ' "$f" || return 1
}
expect fail "штамп процитирован в теле, а не стоит первой строкой" case_on f-quoted assert_docs_translations_fresh stamp_quoted

# Чужой штамп — не то же самое, что снятый: ветка разбора здесь другая, а вывод обеих порч у
# читателя одинаков.
alien_stamp() {
  sed -i.bak "s/^<!-- en-sha256: .* -->$/<!-- en-sha256: $(printf '0%.0s' $(seq 64)) -->/" "$1/README.ru.md" \
    && rm -f "$1/README.ru.md.bak"
}
expect fail "штамп перевода указывает на чужую версию" case_on f-alien assert_docs_translations_fresh alien_stamp

drop_translation() { rm -f "$1/README.ru.md"; }
expect fail "перевода README нет вовсе" case_on f-noru assert_docs_translations_fresh drop_translation

no_pairs() { rm -rf "${1:?}/docs/en"; rm -f "$1/README.md"; }
expect fail "пар «источник → перевод» не найдено" case_on f-nopairs assert_docs_translations_fresh no_pairs

# --- переключатель языка ------------------------------------------------------------------------
drop_switch() {
  sed -i.bak '1s|.*|# build|' "$1/docs/en/getting-started/build.md" && rm -f "$1/docs/en/getting-started/build.md.bak"
}
expect fail "документ не ссылается на свою пару" case_on s-drop assert_docs_language_switch drop_switch

# Ссылка отличается ОДНИМ сегментом, имя пары в ней есть — открывается она в никуда. Кейс
# доказывает, что ссылка разрешается путём, а не поиском подстроки.
wrong_depth() {
  sed -i.bak 's|\.\./\.\./ru/getting-started/build\.md|../ru/getting-started/build.md|' \
    "$1/docs/en/getting-started/build.md" && rm -f "$1/docs/en/getting-started/build.md.bak"
}
expect fail "ссылка на пару неверной глубины" case_on s-depth assert_docs_language_switch wrong_depth

# Ссылка есть, но не в шапке: переключатель, до которого читатель доскроллит на двадцатой строке,
# переключателем не является.
# Порча обязана СОХРАНИТЬ ссылку и лишь опустить её ниже шапки: прежняя версия брала тело со
# второй строки, то есть выбрасывала переключатель вовсе и падала по причине соседнего кейса
# «документ не ссылается на свою пару», ничего не доказывая о границе шапки. Строка переносится, а
# её присутствие в испорченном файле утверждается — иначе тот же промах вернулся бы молча.
switch_buried() {
  local f="$1/docs/en/getting-started/build.md" head_line body
  head_line=$(sed -n '1p' "$f")
  body=$(sed -n '2,$p' "$f")
  { printf '# build\n'; printf 'filler\n%.0s' $(seq 20); printf '%s\n' "$head_line" "$body"; } > "$f"
  grep -q '\.\./\.\./ru/getting-started/build\.md' "$f" || return 1
  [ "$(grep -c '\.\./\.\./ru/getting-started/build\.md' "$f")" = 1 ] || return 1
  sed -n '1,12p' "$f" | grep -q '\.\./\.\./ru/getting-started/build\.md' && return 1
  return 0
}
expect fail "ссылка на пару утоплена ниже шапки" case_on s-buried assert_docs_language_switch switch_buried

expect fail "пар нет — переключатель проверять не на чем" case_on s-nopairs assert_docs_language_switch no_pairs

bash "$ROOT/scripts/check_docs_content_selftest.sh" || BAD=1

if [ "$BAD" != 0 ]; then echo "docs-selftest: FAIL" >&2; exit 1; fi
echo "docs-selftest: PASS"
