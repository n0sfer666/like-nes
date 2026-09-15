# shellcheck shell=bash
# Утверждения о РАЗМЕТКЕ getting-started: блоки на месте, роли знакомы, у каждого маркера есть тело,
# все три фазы представлены, а команды en и ru совпадают побайтно. Отдельно от разбора
# (docs_start_lib.sh), которым они пользуются: разбор читает документ, эти — судят прочитанное.
# Подключить обязан вызывающий; ему же принадлежат start_ok/start_bad.

# Разметка на месте: блоки есть, роли знакомы, у каждого маркера есть тело, и КАЖДАЯ из трёх фаз
# представлена. Последнее — не педантизм: прогон, потерявший фазу установки, собирал бы дерево на
# машине, которую подготовил кто-то другой, и утверждал бы про документ то, чего в нём нет.
assert_start_marked() {
  local root="$1" all bad=0 role n
  all=$(
    while read -r rel; do
      [ -n "$rel" ] || continue
      start_blocks "$root" "$rel"
    done <<EOF2
$(start_docs "$root")
EOF2
  )
  if [ -z "$all" ]; then
    start_bad "в docs/en/getting-started нет ни одного блока <!-- container: … --> — исполнять нечего"
    return 1
  fi
  while IFS=$'\t' read -r role rel n _; do
    [ "$role" = '!' ] || continue
    start_bad "$rel: маркер блока $n не открывает фенс — размечено, а показывать нечего"
    bad=1
  done <<EOF2
$all
EOF2
  while IFS=$'\t' read -r role rel n line; do
    [ "$role" = '?' ] || continue
    start_bad "$rel: строка похожа на маркер, но не той формы: $line"
    bad=1
  done <<EOF2
$all
EOF2
  while IFS=$'\t' read -r role rel n _; do
    case "$role" in '!'|'?') continue ;; esac
    case " $START_ROLES " in
      *" $role "*) continue ;;
    esac
    start_bad "$rel: незнакомая роль блока $n: $role"
    bad=1
  done <<EOF2
$all
EOF2
  for role in $START_ROLES; do
    n=$(printf '%s\n' "$all" | awk -F'\t' -v r="$role" '$1 == r' | wc -l | tr -d ' ')
    [ "$n" != 0 ] && continue
    start_bad "ни одного блока роли $role — эта фаза прогона не проверяется вовсе"
    bad=1
  done
  [ "$bad" = 0 ] || return 1
  n=$(printf '%s\n' "$all" | awk -F'\t' '{print $2 "\t" $3}' | LC_ALL=C sort -u | wc -l | tr -d ' ')
  start_ok "исполнимых блоков размечено: $n"
}

# Команды en и ru совпадают ПОБАЙТНО. Штамп свежести (docs_pairs_lib.sh) держит перевод целиком, но
# перевод, разошедшийся с источником ИМЕННО в командах, — это инструкция, которая у русского
# читателя не работает, и она обязана быть ошибкой, а не строкой diff'а в предупреждении.
assert_start_langs_agree() {
  local root="$1" rel ru bad=0 n=0
  while read -r rel; do
    [ -n "$rel" ] || continue
    ru=docs/ru/${rel#docs/en/}
    if [ ! -f "$root/$ru" ]; then
      start_bad "нет перевода $ru — сверять команды не с чем"
      bad=1
      continue
    fi
    n=$((n + 1))
    # Ожидания сверяются вместе с телом: строку вывода прогон берёт у en, и разошедшийся хвост
    # маркера в ru — это перевод, обещающий читателю не то, что печатает бинарь.
    if ! diff <(start_expect "$root" "$rel") <(start_expect "$root" "$ru" | sed "s|^$ru\t|$rel\t|") \
        > /dev/null; then
      start_bad "$ru: ожидаемый вывод в маркерах разошёлся с $rel"
      bad=1
    fi
    if ! diff <(start_blocks "$root" "$rel") <(start_blocks "$root" "$ru" | sed "s|\t$ru\t|\t$rel\t|") \
        > /dev/null; then
      start_bad "$ru: исполнимые блоки разошлись с $rel"
      diff <(start_blocks "$root" "$rel" | cut -f1,4) <(start_blocks "$root" "$ru" | cut -f1,4) \
        | sed 's/^/       /' >&2 || true
      bad=1
    fi
  done <<EOF2
$(start_docs "$root")
EOF2
  [ "$n" != 0 ] || { start_bad "документов getting-started не найдено"; return 1; }
  [ "$bad" = 0 ] || return 1
  start_ok "команды en и ru совпадают побайтно ($n документов)"
}
