#!/usr/bin/env bash
# Позитивный контроль гейта 5 (спека #19), половина про ТЕКСТ документа: разметка исполнимых блоков,
# согласие языков и обещанный читателю вывод. Утверждение, у которого нет фикстуры, где оно падает,
# неотличимо от отсутствующего: гейт зелен на дереве, которое сам же и описывает.
#
# Половина про ПРОГОН (база, монтирование, заглушка) живёт в check_docs_start_run_selftest.sh и
# зовётся отсюда ВНЕШНЕЙ командой — чтобы в логе стояло имя упавшего набора.
set -uo pipefail

ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
# shellcheck source=scripts/docs_start_fixture_lib.sh
. "$ROOT/scripts/docs_start_fixture_lib.sh"
SELFTEST_NAME=docs-start-selftest
# shellcheck source=scripts/docs_start_selftest_lib.sh
. "$ROOT/scripts/docs_start_selftest_lib.sh"

# Каждое имя assert_*, которое гейт зовёт, обязано быть ОПРЕДЕЛЕНО: незагруженная функция даёт код
# 127, он ненулевой, и все ждущие fail кейсы проезжали бы как «утверждение отбило порчу».
missing=$(
  # Библиотеки выводятся ИЗ ГЕЙТА, а не перечисляются здесь: список, написанный вторым местом,
  # разъезжается молча — половина утверждений осталась бы незагруженной, и проверка определённости
  # объявляла бы их отсутствующими. Ровно list-drift из ci_lint.py.
  while read -r libname; do
    # shellcheck source=/dev/null
    . "$ROOT/scripts/$libname"
  done <<EOF2
$(sed 's/#.*//' "$ROOT/scripts/check_docs_start.sh" | awk '/^\. scripts\//{ sub(/^\. scripts\//, ""); print }')
EOF2
  grep -o 'assert_start_[a-z_]*' "$ROOT/scripts/check_docs_start.sh" | sort -u | while read -r fn; do
    declare -F "$fn" > /dev/null || printf '%s ' "$fn"
  done
)
if [ -n "$missing" ]; then
  printf '%s: БРАК гейт зовёт неопределённые утверждения: %s\n' "$SELFTEST_NAME" "$missing" >&2
  BAD=1
else
  printf '%s: OK   все утверждения гейта определены\n' "$SELFTEST_NAME"
fi

d=$(tree_for); expect pass '' 'исправная фикстура' "$d"
expect pass '' 'настоящее дерево репозитория' "$ROOT"

d=$(tree_for); doc_both "$d" first-run.md 's|^<!-- container: run -->|&\n\ntext|'
expect fail 'не открывает фенс' 'маркер без фенса' "$d"

d=$(tree_for); doc_both "$d" build.md 's|container: build|container: assemble|'
expect fail 'незнакомая роль' 'роль вне закрытого списка' "$d"

d=$(tree_for); doc_both "$d" first-run.md "s|^<!-- container: check .*|<!-- container: run -->|"
expect fail 'ни одного блока роли check' 'фаза проверки исчезла' "$d"

d=$(tree_for); doc_both "$d" prerequisites.md 's|^<!-- container: install -->||'
expect fail 'ни одного блока роли install' 'фаза установки исчезла' "$d"

d=$(tree_for); doc_one "$d" ru build.md 's|mkdir -p build|mkdir build|'
expect fail 'исполнимые блоки разошлись' 'команда перевода разошлась' "$d"

d=$(tree_for); rm -f "$d/docs/ru/getting-started/build.md"
expect fail 'нет перевода' 'перевода документа нет вовсе' "$d"

d=$(tree_for); doc_one "$d" ru first-run.md "s|container: check .*|container: check other-line -->|"
expect fail 'ожидаемый вывод в маркерах разошёлся' 'хвост маркера в переводе другой' "$d"

d=$(tree_for); doc_one "$d" en first-run.md "s|^It prints.*||"
expect fail 'текст документа этого не обещает' 'обещания нет в тексте источника' "$d"

d=$(tree_for); doc_one "$d" ru first-run.md "s|^It prints.*||"
expect fail 'перевод не обещает' 'обещания нет в тексте перевода' "$d"

d=$(tree_for); doc_both "$d" first-run.md "s|^<!-- container: check .*|<!-- container: check -->|"
expect fail 'ни один блок не ждёт вывода' 'ни одному блоку вывод не обещан' "$d"

d=$(tree_for); doc_both "$d" build.md 's|acme/widget|acme/other|'
expect fail 'клон ведёт в' 'документ зовёт в чужой репозиторий' "$d"

d=$(tree_for); doc_both "$d" build.md '/^git clone /d'
expect fail 'нет команды клонирования' 'документ не велит клонировать' "$d"

d=$(tree_for); git -C "$d" remote remove origin
expect fail 'нет remote origin' 'дереву не с чем сверять адрес' "$d"

d=$(tree_for); doc_both "$d" first-run.md 's|^\./build/|build/|'
expect fail 'не называет бинаря путём' 'запуск не назван путём' "$d"

# Опорный pass с ДЛИННЫМ документом: поиск обещания шёл пайпом в `grep -q`, а тот выходит по первому
# совпадению, рвёт пайп писателю и под pipefail отдаёт ненулевой код РОВНО НА СОВПАДЕНИИ. На коротком
# документе всё влезает в буфер и дефект не виден — то есть блокирующий гейт объявлял бы нарушение
# на исправном дереве, и никакая порча об этом не сказала бы.
d=$(tree_for)
for l in en ru; do
  p="$d/docs/$l/getting-started/first-run.md"
  start_fix_mutate "$p" bash -c 'yes "filler line for a long document" | head -20000 >> "$1"' _ "$p"
done
expect pass '' 'обещание в начале длинного документа' "$d"

# Та же фикстура со СТАРОЙ реализацией: без неё опорный pass выше подтверждает лишь то, что гейт
# зелен на длинном документе, и о причине длины не говорит ничего.
d=$(tree_for)
for l in en ru; do
  p="$d/docs/$l/getting-started/first-run.md"
  start_fix_mutate "$p" bash -c 'yes "filler line for a long document" | head -20000 >> "$1"' _ "$p"
done
script_mut "$d" docs_start_doc_rules_lib.sh \
  's|grep -qF -- "$want" <<<"$text"|grep -vF "<!-- container:" "$root/$rel" \| grep -qF -- "$want"|g'
expect fail 'текст документа этого не обещает' 'поиск обещания течёт в grep пайпом' "$d"

d=$(tree_for); doc_both "$d" first-run.md 's|^<!-- container: run -->|<!--  container: run -->|'
expect fail 'похожа на маркер' 'лишний пробел в маркере' "$d"

d=$(tree_for); doc_both "$d" first-run.md 's|^<!-- container: run -->|<!-- container: Run -->|'
expect fail 'похожа на маркер' 'заглавная буква в роли' "$d"

# Блок роли check БЕЗ ожидания: соседнее утверждение довольствуется одним обещанием на дерево, и без
# этого кейса блок узнавал бы о своей беде в --live, после установки системы и полной сборки.
d=$(tree_for)
doc_both "$d" first-run.md 's|^<!-- container: check .*|<!-- container: check -->\n```sh\n./build/widget_check\n```\n\n&|'
expect fail 'не назвал ожидаемой строки' 'блок check без ожидания' "$d"

bash "$ROOT/scripts/check_docs_start_run_selftest.sh" || BAD=1

start_selftest_verdict
