#!/usr/bin/env bash
# Позитивный контроль check_owner_gates.sh: утверждение, у которого нет фикстуры, где оно падает,
# неотличимо от отсутствующего. Порчи ломают ровно те три копии одного факта, что уже разъезжались
# на дереве, — метку раздела, строку таблицы шапки и рукописный перечень в owner_check.sh.
set -uo pipefail

ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
NAME=owner-gates-selftest
pass=0; mut=0; fail=0
FILES="docs/owner-verification.md scripts/owner_check.sh scripts/owner_gates_lib.sh scripts/check_owner_gates.sh"

tree_for() {
  local d rel
  d=$(mktemp -d "${TMPDIR:-/tmp}/owner-gates.XXXXXX")
  for rel in $FILES; do
    mkdir -p "$d/$(dirname "$rel")"
    cp "$ROOT/$rel" "$d/$rel"
  done
  printf '%s\n' "$d"
}

# Порча обязана РЕАЛЬНО менять файл: подмена, ничего не подменившая, читается как «утверждение её
# отбило» — так уже вышло в refusal-наборе, где sed промахнулся мимо уехавшей строки.
mutate() {
  local d="$1" rel="$2"; shift 2
  local before after
  before=$(cksum < "$d/$rel")
  "$@"
  after=$(cksum < "$d/$rel")
  [ "$before" != "$after" ] || {
    printf '%s: FAIL порча не изменила %s — проверяли бы собственный промах\n' "$NAME" "$rel" >&2
    fail=$((fail + 1))
  }
}

sed_in() { local f="$1" e="$2"; sed -e "$e" "$f" > "$f.new" && mv "$f.new" "$f"; }

# Правки, где образец sed сам полон вертикальных черт и скобок (строка markdown-таблицы), делает
# python: разделитель `s|…|…|` ломается о `|` внутри строки, а BRE-группа — о неё же.
py_in() {
  python3 -c '
import pathlib, sys
p = pathlib.Path(sys.argv[1])
lines = p.read_text(encoding="utf-8").splitlines()
mode, anchor, text = sys.argv[2], sys.argv[3], sys.argv[4]
for i, l in enumerate(lines):
    if l.startswith(anchor):
        if mode == "after":
            lines.insert(i + 1, text)
        else:
            lines[i] = l.replace(mode, text)
        break
else:
    sys.exit("anchor not found: " + anchor)
p.write_text("\n".join(lines) + "\n", encoding="utf-8")
' "$@"
}

# Показанная читателю разметка: раздел и метка ВНУТРИ ```-блока. Разбор обязан их не заметить.
fenced_tail() {
  {
    printf '\n'
    printf '```text\n'
    printf '## 3. Gate 9 of #99 — how a mark looks\n'
    printf '<!-- gate: open -->\n'
    printf '```\n'
  } >> "$1"
}

expect() {
  local want="$1" needle="$2" what="$3" d="$4" out rc=0
  out=$(bash "$d/scripts/check_owner_gates.sh" "$d" 2>&1) || rc=$?
  rm -rf "$d"
  if [ "$want" = pass ] && [ "$rc" != 0 ]; then
    printf '%s: FAIL %s — ждали PASS, код %s\n%s\n' "$NAME" "$what" "$rc" "$out" >&2
    fail=$((fail + 1)); return
  fi
  if [ "$want" = fail ]; then
    if [ "$rc" = 0 ]; then
      printf '%s: FAIL %s — порча проехала зелёной\n' "$NAME" "$what" >&2
      fail=$((fail + 1)); return
    fi
    if ! grep -qF -- "$needle" <<<"$out"; then
      printf '%s: FAIL %s — отказ не назвал «%s»:\n%s\n' "$NAME" "$what" "$needle" "$out" >&2
      fail=$((fail + 1)); return
    fi
  fi
  printf '%s: OK   %s\n' "$NAME" "$what"
  # Счётчика ДВА: «отбито порч N», посчитанное вместе с опорными, называет число, которого набор не
  # проверял, — ровно та копия факта, ради которой заведён и сам гейт.
  if [ "$want" = pass ]; then pass=$((pass + 1)); else mut=$((mut + 1)); fi
}

DOC=docs/owner-verification.md
CHK=scripts/owner_check.sh

d=$(tree_for); expect pass '' 'исправное дерево' "$d"

d=$(tree_for)
mutate "$d" "$DOC" sed_in "$d/$DOC" 's|^<!-- gate: closed 2026-09-02 -->$|<!-- gate: open -->|'
expect fail 'таблица шапки разъехалась' 'закрытый гейт назван открытым' "$d"

d=$(tree_for)
mutate "$d" "$DOC" sed_in "$d/$DOC" '/^<!-- gate: closed 2026-08-05 -->$/d'
expect fail 'нет метки' 'у раздела нет метки статуса' "$d"

d=$(tree_for)
mutate "$d" "$DOC" sed_in "$d/$DOC" 's|^<!-- gate: closed 2026-08-07 -->$|<!-- gate: closed -->|'
expect fail 'закрыт без даты в метке' 'закрытый без даты в метке' "$d"

# Форма, которую разбор не узнал, — ОТДЕЛЬНЫЙ статус `!`: до этого кейса ветку не проверял никто, а
# «не разобрал» и «нарушений нет» обязаны различаться. Дата словом проходит любой ленивый образец.
d=$(tree_for)
mutate "$d" "$DOC" sed_in "$d/$DOC" 's|^<!-- gate: closed 2026-08-05 -->$|<!-- gate: closed tomorrow -->|'
expect fail 'метка статуса не той формы' 'дата в метке словом' "$d"

# Открытый без ДЕЙСТВИЯ — то, чем перечень и стал в первой версии этого гейта: владелец получал
# «§14 руководства», адрес вместо инструкции, и правило 18 держит процедуру частью задачи.
d=$(tree_for)
mutate "$d" "$DOC" sed_in "$d/$DOC" 's%^<!-- gate: open | .* -->$%<!-- gate: open -->%'
expect fail 'не называет шага владельца' 'открытый гейт без действия' "$d"

d=$(tree_for)
mutate "$d" "$DOC" sed_in "$d/$DOC" 's|^## 8\. Gate 2 of #17 |## 8. Gate two of #17 |'
expect fail 'форма заголовка не узнана' 'номер гейта словом в заголовке' "$d"

d=$(tree_for)
mutate "$d" "$DOC" sed_in "$d/$DOC" '/^| Five lights out of a table/d'
expect fail 'таблица шапки разъехалась' 'строка таблицы пропала' "$d"

d=$(tree_for)
mutate "$d" "$DOC" py_in "$d/$DOC" after '| Five lights out of a table' \
  '| Ghost gate | [#99](../.context/specs/2026-07-26-materials-shaders.md) 4 | any | — | nothing |'
expect fail 'таблица шапки разъехалась' 'в таблице лишний гейт' "$d"

d=$(tree_for)
mutate "$d" "$DOC" py_in "$d/$DOC" '2026-08-22' '| Physics frame cost' '2026-08-21'
expect fail 'таблица шапки разъехалась' 'в таблице чужая дата закрытия' "$d"

d=$(tree_for)
mutate "$d" "$DOC" sed_in "$d/$DOC" 's|\*\*7 of the 18 gates below are closed\*\*|**9 of the 18 gates below are closed**|'
expect fail 'вводный абзац' 'вводный абзац называет чужое число' "$d"

d=$(tree_for)
mutate "$d" "$DOC" sed_in "$d/$DOC" 's|\*\*7 of the 18 gates below are closed\*\*|**most of the gates below are closed**|'
expect fail 'не называет чисел в ожидаемой форме' 'вводный абзац без чисел вовсе' "$d"

d=$(tree_for)
mutate "$d" "$CHK" py_in "$d/$CHK" after 'owner_gates_open_lines' \
  'say "  #17 гейт 9 ОТКРЫТ — образцы после переезда на графику (§7 руководства)"'
expect fail 'называет статус гейта строкой' 'перечень снова написан руками' "$d"

d=$(tree_for)
mutate "$d" "$CHK" sed_in "$d/$CHK" 's|owner_gates_closed_lines|say "  остальные закрыты"; : |'
expect fail 'не печатает закрытых' 'закрытые снова написаны руками' "$d"

d=$(tree_for)
mutate "$d" "$CHK" sed_in "$d/$CHK" 's|owner_gates_lib.sh|owner_gates_none.sh|g'
expect fail 'не грузит owner_gates_lib.sh' 'owner_check.sh потерял разбор' "$d"

d=$(tree_for)
mutate "$d" "$DOC" sed_in "$d/$DOC" 's|^## 11\. Gate 3 of #18 |## 12. Gate 3 of #18 |'
expect fail 'нумерация разделов рвётся' 'номер раздела повторён' "$d"

d=$(tree_for)
mutate "$d" "$DOC" sed_in "$d/$DOC" 's|^## \([0-9][0-9]*\)\. |### \1. |'
expect fail 'не разобрано ни одного раздела-гейта' 'разбор промахнулся мимо документа' "$d"

# Тот же промах, но needle НУМЕРАЦИИ: пустой разбор проходит её цикл ни разу, и без счётчика
# утверждение печатало бы «пронумерованы подряд (1..0)» — vacuous-gate на собственном предмете.
d=$(tree_for)
mutate "$d" "$DOC" sed_in "$d/$DOC" 's|^## \([0-9][0-9]*\)\. |### \1. |'
expect fail 'нумеровать нечего' 'нумерация на пустом разборе' "$d"

# Метка и баннер — новая пара копий, которую этот же дифф и завёл, убрав три старые. Обе стороны
# различия держат свои кейсы: метка открыта над закрытым разделом — и наоборот, чужая дата.
d=$(tree_for)
mutate "$d" "$DOC" sed_in "$d/$DOC" 's%^<!-- gate: closed 2026-09-02 -->$%<!-- gate: open | что-нибудь -->%'
expect fail 'баннер объявляет закрытие' 'метка открыта над закрытым разделом' "$d"

d=$(tree_for)
mutate "$d" "$DOC" sed_in "$d/$DOC" 's|^> \*\*Closed 2026-08-22\*\*|> **Closed 2026-08-21**|'
expect fail 'баннер — 2026-08-21' 'баннер называет чужую дату' "$d"

d=$(tree_for)
mutate "$d" "$DOC" sed_in "$d/$DOC" 's|^> \*\*Closed 2026-08-05\*\* on Nobara|> Closed 2026-08-05 on Nobara|'
expect fail 'а баннера' 'у закрытого пропал баннер' "$d"

# Опорный pass: документ на две с половиной тысячи строк САМ показывает читателю форму метки, и
# заголовок внутри ```-блока структурой документа не является. Ту же границу проводит check_docs.sh.
d=$(tree_for)
mutate "$d" "$DOC" fenced_tail "$d/$DOC"
expect pass '' 'разметка внутри ```-блока разделом не считается' "$d"

d=$(tree_for); rm -f "$d/$DOC"
expect fail 'судить не о чем' 'документа нет вовсе' "$d"

printf '%s: %s — опорных pass %d, отбито порч %d\n' "$NAME" \
  "$( [ "$fail" = 0 ] && echo PASS || echo FAIL )" "$pass" "$mut"
[ "$fail" = 0 ]
