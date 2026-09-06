#!/usr/bin/env bash
# Позитивный контроль гейта 5 (спека #19), половина про ПРОГОН: голая база, монтирование дерева,
# раннер и заглушка клона. Граница с документным набором по ПРЕДМЕТУ, а не по счётчику строк: там
# утверждения читают текст, здесь — то, чем этот текст исполняется, и порчи здесь ломают наши
# скрипты, а не документацию.
set -uo pipefail

ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
# shellcheck source=scripts/docs_start_fixture_lib.sh
. "$ROOT/scripts/docs_start_fixture_lib.sh"
# Читает её общая оснастка (docs_start_selftest_lib.sh) — отсюда директива: имя набора обязано
# стоять в КАЖДОЙ строке вывода, иначе «упало что-то из сорока» не говорит, в каком файле искать.
# shellcheck disable=SC2034
SELFTEST_NAME=docs-start-run-selftest
# shellcheck source=scripts/docs_start_selftest_lib.sh
. "$ROOT/scripts/docs_start_selftest_lib.sh"

# Опорный pass свой: без него все порчи ниже доказывали бы лишь то, что гейт умеет падать, — на
# дереве, о котором никто не утверждал, что он на нём зелен.
d=$(tree_for); expect pass '' 'исправная фикстура' "$d"

d=$(tree_for)
start_fix_mutate "$(file_of "$d" release_linux.Dockerfile)" \
  sed -i.bak -e 's|@sha256:.*|:24.04|' "$(file_of "$d" release_linux.Dockerfile)"
rm -f "$(file_of "$d" release_linux.Dockerfile).bak"
expect fail 'не пиннута дайджестом' 'база пиннута тегом' "$d"

d=$(tree_for); script_mut "$d" check_docs_start.sh 's|container_base_pin|base_pin_of|g'
expect fail 'не берёт базу из release_linux.Dockerfile' 'у прогона свой пин базы' "$d"

d=$(tree_for); script_mut "$d" check_docs_start.sh 's|^ENGINE=|docker build -t x .\nENGINE=|'
expect fail 'строит образ вместо голой базы' 'прогон собирает свой образ' "$d"

d=$(tree_for); script_mut "$d" check_docs_start.sh 's|:/src:ro|:/src|'
expect fail 'монтирует дерево на запись' 'дерево примонтировано на запись' "$d"

d=$(tree_for); script_mut "$d" docs_start_run.sh 's|start_plan|plan_of_doc|g'
expect fail 'не берёт команд из документа' 'раннер пишет команды сам' "$d"

d=$(tree_for); script_mut "$d" docs_start_run.sh 's|^PLAY=|git clone https://example.invalid/x\nPLAY=|'
expect fail 'клонирует по сети' 'раннер судит дерево с GitHub' "$d"

d=$(tree_for); script_mut "$d" docs_start_run.sh 's|docs_start_stub_lib.sh|docs_start_lib.sh|'
expect fail 'не грузит' 'раннер не грузит библиотеку подмен' "$d"

d=$(tree_for); script_mut "$d" docs_start_stub_lib.sh 's|подмен|тихо|g'
expect fail 'не называет подмену клона вслух' 'подмена клона молчалива' "$d"

# Порча ТЕКСТОВАЯ нарочно: строка стоит в теле функции, которую никто не зовёт. Впиши её исполнимо —
# и кейс, проверяющий греп, уходил бы в сеть на каждом прогоне самопроверки.
d=$(tree_for)
script_mut "$d" docs_start_stub_lib.sh \
  's|^start_write_stubs()|start_unused_clone() { git clone https://example.invalid/x; }\n&|'
expect fail 'клонирует по сети' 'библиотека подмен зовёт настоящий клон' "$d"

# Дальше — порчи САМОЙ заглушки: греп по файлу их не видит, и до вертикали 2 гейт 5 о них молчал.
# Ровно эта слепота стоила первого живого прогона.
d=$(tree_for); script_mut "$d" docs_start_stub_lib.sh 's|tar -cf - --null -T -|true|'
expect fail 'не скопировала файла из индекса' 'заглушка не разворачивает дерева' "$d"

d=$(tree_for); script_mut "$d" docs_start_stub_lib.sh 's|--others ||'
expect fail 'потеряла ненаписанный в индекс файл' 'охват заглушки без ненаписанного' "$d"

d=$(tree_for); script_mut "$d" docs_start_stub_lib.sh 's| --exclude-standard||'
expect fail 'привезла игнорируемое' 'охват заглушки тянет игнорируемое' "$d"

# Подмена БЕЗ сверки адреса — то, чем заглушка и была до живого прогона 2026-09-06: FetchContent
# получал копию нашего дерева вместо glfw, и конфигурация умирала «Failed to checkout tag».
d=$(tree_for); script_mut "$d" docs_start_stub_lib.sh 's|\[ "$url" = "$WANT" \]|[ -n "$url" ]|'
expect fail 'чужой адрес подменён' 'заглушка подменяет всякий клон' "$d"

# Мера вырезана, а слово осталось в комментарии рядом: без снятия комментариев греп объявлял бы
# монтирование :ro существующим там, где прогон правит то, что судит.
d=$(tree_for); script_mut "$d" check_docs_start.sh 's|-v "$ROOT:/src:ro" \\|# было :ro\n  -v "$ROOT:/src" \\|'
expect fail 'монтирует дерево на запись' 'мера :ro осталась только в комментарии' "$d"

# Чистка PATH вырезана. До защиты от самонахождения этот кейс гейт не ронял, а ВЕШАЛ: заглушка
# звала саму себя без конца — и это на каждом коммите, потому что гейт стоит в checks.json.
d=$(tree_for); script_mut "$d" docs_start_stub_lib.sh 's|^PATH=$clean|PATH=$PATH|'
expect fail 'ведёт в саму заглушку' 'чистка PATH у заглушки вырезана' "$d"

d=$(tree_for); script_mut "$d" docs_start_stub_lib.sh 's|^      -\*) ;;||'
expect fail 'клона с флагом' 'разбор заглушки не пропускает флагов' "$d"

d=$(tree_for); script_mut "$d" docs_start_stub_lib.sh 's|^exec "$@"$|exit 3|'
expect fail 'заглушка sudo не исполняет' 'подмена sudo не исполняет команды' "$d"

# Возврат к тому, чем раннер и был до 2026-09-06. На macOS такая порча ничего не ломает — awk BSD
# интервалы понимает, — поэтому доказать её вредность прогоном гейта на хосте невозможно, и держит
# её только это утверждение.
d=$(tree_for); script_mut "$d" docs_start_lib.sh \
  's|for (i = 0; i < 3; i++) sub(/\^\[\^\\t\]\*\\t/, "", line)|sub(/^([^\\t]*\\t){3}/, "", line)|'
expect fail 'интервальный квантификатор' 'раннер режет поля интервалом' "$d"

# Позитивный контроль ОБХОДА подключений, а не только образца интервала. Счётчик найденных
# библиотек заведён отдельно от общего именно поэтому: раннер считается всегда, и обход,
# промахнувшийся мимо ВСЕХ подключений, объявлял бы дерево чистым — то есть восстановленный
# интервал в любой библиотеке проезжал бы зелёным.
d=$(tree_for); script_mut "$d" docs_start_awk_lib.sh 's|scripts\\/\[A-Za-z0-9_.-\]|zzzz\\/[A-Za-z0-9_.-]|'
expect fail 'не грузит ни одной библиотеки' 'обход подключений промахнулся мимо всех' "$d"

# Интервал в ПОДКЛЮЧЁННОЙ библиотеке, а не в самом раннере: без обхода утверждение видело бы один
# файл из трёх, и mawk спотыкался бы ровно там, где никто не смотрит.
d=$(tree_for)
script_mut "$d" docs_start_stub_lib.sh \
  's|^start_write_stubs()|start_probe() { grep -qE "a{2}" /dev/null; }\n&|'
expect fail 'интервальный квантификатор' 'интервал в подключённой библиотеке' "$d"

# Форма подключения предметом утверждения не является: `source` исполняется ровно как `.`, и обход,
# знающий одну форму, тихо терял бы библиотеку вместе с её интервалами.
d=$(tree_for)
script_mut "$d" docs_start_run.sh 's|^\. "\$SRC/scripts/docs_start_stub_lib.sh"|source "$SRC/scripts/docs_start_stub_lib.sh"|'
script_mut "$d" docs_start_stub_lib.sh \
  's|^start_write_stubs()|start_probe() { grep -qE "a{2}" /dev/null; }\n&|'
expect fail 'интервальный квантификатор' 'библиотека подключена формой source' "$d"

start_selftest_verdict
