#!/usr/bin/env bash
# Гейт: список ручных гейтов в scripts/owner_check.sh и таблица шапки docs/owner-verification.md
# выводятся ИЗ разделов документа, а не пишутся рядом с ними. Ни сборки, ни сети ему не нужно —
# место в checks.json рядом с линтером workflow и бюджетом длины.
#
# Заведён находкой правила 15: owner_check.sh звал гейт 9 спеки #17 открытым четыре дня после того,
# как тот закрылся, и не называл шести открытых вовсе; таблица шапки несла 12 строк на 18 гейтов, а
# вводный абзац — числа, неверные обе. Три рукописных копии одного факта разъехались втроём, и
# заметить это можно было только глазами. Ровно list-drift из ci_lint.py, только эталон у списка
# ЕСТЬ — сам документ.
set -uo pipefail

ROOT=${1:-$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)}
# shellcheck source=scripts/owner_gates_lib.sh
. "$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/owner_gates_lib.sh"

DOC="$ROOT/$OWNER_DOC"
fails=0
note() { fails=$((fails + 1)); }

[ -f "$DOC" ] || { owner_bad "нет $OWNER_DOC — судить не о чем"; exit 1; }

# Форма заголовка и метка статуса. Ноль разделов — ОТКАЗ, а не «нарушений нет»: разбор,
# промахнувшийся мимо документа, обязан отличаться от чистого прогона (тот же класс, что правило
# vacuous-gate в ci_lint.py). Раздел 0 — автоматическая половина, гейтом не является и освобождён.
assert_owner_form() {
  local secs n=0 bad=0 sec spec status date title act
  secs=$(owner_gates_sections "$DOC")
  [ -n "$secs" ] || { owner_bad "в $OWNER_DOC не разобрано ни одного раздела-гейта"; return 1; }
  while IFS="$OWNER_FS" read -r sec spec _ status date title act _; do
    n=$((n + 1))
    [ "$spec" != '?' ] || { owner_bad "раздел $sec: форма заголовка не узнана — «${title}»"; bad=1; }
    case "$status" in
      open|closed) ;;
      -) owner_bad "раздел $sec: нет метки <!-- gate: … --> под заголовком"; bad=1 ;;
      *) owner_bad "раздел $sec: метка статуса не той формы"; bad=1 ;;
    esac
    [ "$status" != closed ] || [ -n "$date" ] || { owner_bad "раздел $sec: закрыт без даты в метке"; bad=1; }
    # Открытый без действия — это перечень, ужавшийся до адреса: владелец получает «§14 руководства»
    # вместо того, что там делать, и правило 18 держит процедуру частью задачи, а не документацией.
    [ "$status" != open ] || [ -n "$act" ] || { owner_bad "раздел $sec: открыт, но метка не называет шага владельца"; bad=1; }
  done <<<"$secs"
  [ "$bad" = 0 ] || return 1
  owner_ok "разделов-гейтов $n, у каждого узнан заголовок и стоит метка статуса"
}

# Метка и БАННЕР закрытия — две записи одного факта, и без сверки они разъезжаются молча: метка open
# над разделом с «Closed …» убирает гейт из перечня владельца, а читатель документа видит обратное.
# Дифф, убравший три рукописных копии, добавил четвёртую — ставить на неё эталон обязан тот же гейт.
assert_owner_banner() {
  local secs bad=0 sec status date banner
  secs=$(owner_gates_sections "$DOC")
  [ -n "$secs" ] || { owner_bad "в $OWNER_DOC не разобрано ни одного раздела-гейта"; return 1; }
  while IFS="$OWNER_FS" read -r sec _ _ status date _ _ banner; do
    if [ "$status" = closed ]; then
      [ -n "$banner" ] || { owner_bad "раздел $sec: метка закрыта, а баннера «> **Closed …**» в разделе нет"; bad=1; continue; }
      [ "$banner" = "$date" ] || { owner_bad "раздел $sec: метка закрыта $date, баннер — $banner"; bad=1; }
    elif [ "$status" = open ] && [ -n "$banner" ]; then
      owner_bad "раздел $sec: метка открыта, а баннер объявляет закрытие $banner"
      bad=1
    fi
  done <<<"$secs"
  [ "$bad" = 0 ] || return 1
  owner_ok "у каждого закрытого раздела баннер стоит и называет ту же дату, что метка"
}

# Нумерация подряд: §-ссылки в owner_check.sh и в прозе документа адресуют раздел ЧИСЛОМ, и пропуск
# в нумерации уводит читателя не туда, ничего не ломая механически.
assert_owner_numbering() {
  local want=1 sec rest
  while IFS="$OWNER_FS" read -r sec rest; do
    [ "$sec" = "$want" ] || { owner_bad "нумерация разделов рвётся: ждали $want, встретили $sec"; return 1; }
    want=$((want + 1))
  done < <(owner_gates_sections "$DOC")
  # Пустой разбор проходит цикл НИ РАЗУ, и без этой строки утверждение печатало бы «пронумерованы
  # подряд (1..0)» на /dev/null — ровно vacuous-gate: соседние свои пустые случаи стерегут, это нет.
  [ "$want" -gt 1 ] || { owner_bad "нумеровать нечего: разбор не дал ни одного раздела"; return 1; }
  owner_ok "разделы пронумерованы подряд (1..$((want - 1)))"
}

# Таблица шапки = разделы, РАВЕНСТВОМ МНОЖЕСТВ по паре «спека, номер» и по дате. Не «всё из таблицы
# нашлось в разделах»: гейт, добавленный разделом и не названный таблицей, обязан валить прогон —
# именно так шесть открытых и пропали из шапки.
assert_owner_table() {
  local want got
  want=$(owner_gates_flat "$DOC" | awk -F"$OWNER_FS" -v OFS="$OWNER_FS" '{ print $1, $2, ($3 == "closed" ? $4 : "\342\200\224") }' | LC_ALL=C sort)
  got=$(owner_gates_table "$DOC" | LC_ALL=C sort)
  [ -n "$want" ] || { owner_bad "разделы не дали ни одного гейта"; return 1; }
  [ -n "$got" ] || { owner_bad "таблица шапки не дала ни одной строки"; return 1; }
  if [ "$want" != "$got" ]; then
    owner_bad "таблица шапки разъехалась с разделами:"
    diff <(printf '%s\n' "$got") <(printf '%s\n' "$want") | sed 's/^/       /' >&2
    return 1
  fi
  owner_ok "таблица шапки совпадает с разделами ($(printf '%s\n' "$want" | wc -l | tr -d ' ') гейтов)"
}

# Числа гейтов в прозе. Проза, которую никто не сверяет, стареет первой: до этого гейта вводный
# абзац обещал «Seven of the twelve», когда гейтов было восемнадцать. Осматривается ВЕСЬ документ, а
# не первые 20 строк: «Beyond the gates» в трёх тысячах строк ниже обещало «7 of the 20 are closed,
# and the open 13» при одиннадцати закрытых и девяти открытых — та же list-drift, только копия лежит
# далеко от эталона и глазами попадается последней. Тело ```-блоков пропускается тем же образцом,
# что в owner_gates_sections, а форма ВВОДНОГО абзаца проверяется отдельно: без неё документ мог бы
# потерять числа в шапке, сохранив их в хвосте, и мера уехала бы вместе с прозой.
assert_owner_intro() {
  local closed total open out
  closed=$(owner_gates_flat "$DOC" | awk -F"$OWNER_FS" '$3 == "closed"' | wc -l | tr -d ' ')
  total=$(owner_gates_flat "$DOC" | wc -l | tr -d ' ')
  open=$((total - closed))
  local head
  head=$(sed -n '1,20p' "$DOC")
  if ! grep -qE '\*\*[0-9]+ of the [0-9]+ gates below are closed\*\*' <<<"$head" \
    || ! grep -qE 'other [0-9]+ stay here' <<<"$head"; then
    owner_bad "вводный абзац не называет чисел в ожидаемой форме — сверять нечего"
    return 1
  fi
  out=$(awk -v C="$closed" -v T="$total" -v O="$open" '
    /^```/ { inb = !inb; next }
    inb { next }
    {
      t = $0
      while (match(t, /[0-9]+ of the [0-9]+ [a-z ]*are closed|other [0-9]+ stay|the open [0-9]+/)) {
        s = substr(t, RSTART, RLENGTH); t = substr(t, RSTART + RLENGTH)
        n = s; gsub(/[^0-9]/, " ", n); split(n, f, " ")
        if (s ~ /are closed/) {
          if (f[1] != C || f[2] != T) printf "строка %d: «%s», а в документе %s из %s\n", FNR, s, C, T
        } else if (f[1] != O) printf "строка %d: «%s», а открытых в документе %s\n", FNR, s, O
      }
    }
  ' "$DOC")
  [ -z "$out" ] || {
    owner_bad "вводный абзац и проза разъехались с разделами:"
    printf '%s\n' "$out" | sed 's/^/       /' >&2
    return 1
  }
  owner_ok "проза называет $closed из $total закрытых и $open открытых, и нигде иначе"
}
# owner_check.sh обязан ЧИТАТЬ документ, а не хранить копию списка. Комментарии снимаются перед
# грепом: слово, оставшееся в комментарии, объявляло бы меру существующей там, где её нет — тем и
# был вакуумно-зелёным assert_ci_no_second_packer.
assert_owner_check_reads_doc() {
  local rel=scripts/owner_check.sh code
  [ -f "$ROOT/$rel" ] || { owner_bad "нет $rel — судить не о чем"; return 1; }
  # Снимаются КОММЕНТАРНЫЕ СТРОКИ, а не хвосты от первого `#`: удалённый перечень начинался каждой
  # строкой с `#<спека>` внутри литерала (`say "  #17 гейт 9 ОТКРЫТ — …"`), и `sed 's/#.*//'` резала
  # его до `say "  ` — слова, которое ищет утверждение, в осмотренном тексте не оставалось ВООБЩЕ.
  # То есть гейт был зелен ровно на том дефекте, ради которого заведён. Верное определение
  # репозиторий уже знает: ci_lint_rules.py — «комментарий это `#` в начале слова».
  code=$(sed '/^[[:space:]]*#/d' "$ROOT/$rel")
  grep -q 'owner_gates_lib.sh' <<<"$code" || {
    owner_bad "$rel не грузит owner_gates_lib.sh — список ручных гейтов у него свой"
    return 1
  }
  grep -q 'owner_gates_open_lines' <<<"$code" || {
    owner_bad "$rel не печатает открытых из разбора документа"
    return 1
  }
  grep -q 'owner_gates_closed_lines' <<<"$code" || {
    owner_bad "$rel не печатает закрытых из разбора документа"
    return 1
  }
  if grep -qE 'ОТКРЫТ|ЗАКРЫТ' <<<"$code"; then
    owner_bad "$rel называет статус гейта строкой — вторая копия списка разъедется с документом"
    return 1
  fi
  owner_ok "$rel печатает ручную половину, читая $OWNER_DOC"
}

assert_owner_form || note
assert_owner_banner || note
assert_owner_numbering || note
assert_owner_table || note
assert_owner_intro || note
assert_owner_check_reads_doc || note

[ "$fails" = 0 ] || { printf 'owner-gates: FAIL нарушений: %d\n' "$fails" >&2; exit 1; }
printf 'owner-gates: PASS список ручных гейтов выведен из %s\n' "$OWNER_DOC"
