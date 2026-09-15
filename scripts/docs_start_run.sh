#!/usr/bin/env bash
# Гейт 5 спеки #19, ПРОГОН: пройти getting-started командами, взятыми из самого документа, и
# осмотреть то, что он обещает получить. Запускается на чистой машине — в контейнере голой базы
# (check_docs_start.sh --live) или на линукс-хосте, которого не жалко.
#
#   bash scripts/docs_start_run.sh <дерево :ro> <каталог прогона>
#
# Утверждения о САМОМ документе живут отдельно (docs_start_lib.sh, docs_start_rules_lib.sh): они не
# требуют ни машины, ни четверти часа, и уронить их нарочно можно фикстурой.
set -euo pipefail

SRC=${1:?первым аргументом — дерево, которое судим}
WORK=${2:?вторым аргументом — каталог прогона}

. "$SRC/scripts/docs_start_lib.sh"
. "$SRC/scripts/docs_start_stub_lib.sh"

mkdir -p "$WORK"
STUB="$WORK/stub"

# Адрес подменяемого клона берётся ИЗ ДОКУМЕНТА, а не из origin машины: подменять надо ровно ту
# команду, которую исполняет читатель, и второй источник адреса разъехался бы с ней молча. Что этот
# адрес ведёт в наш репозиторий, держит assert_start_clone_url. Пусто — отказ: заглушка, которой
# нечего сверять, подменяла бы всякий клон, включая клоны зависимостей.
# Команды забираются в ПЕРЕМЕННУЮ, а не текут в awk с `exit`: ранний выход читателя рвёт пайп
# писателю, и под pipefail код пайплайна ненулевой РОВНО НА СОВПАДЕНИИ. Тот же класс, что уже чинили
# в поиске обещания (assert_start_expect_in_text).
CMDS=$(start_plan "$SRC" | start_cmds)
CLONE_LINE=$(awk '/^git clone /{ print; exit }' <<<"$CMDS")
CLONE_URL=$(start_clone_url "$CLONE_LINE")
[ -n "$CLONE_URL" ] || {
  printf 'docs-start: FAIL в документе нет команды клонирования — подменять нечего\n' >&2
  exit 1
}
start_write_stubs "$STUB" "$SRC" "$CLONE_URL"

# Подготовка МАШИНЫ, а не команда документа, и печатается она отдельно: индекс пакетов у голой базы
# пуст, а диалог настройки на неинтерактивном прогоне ждал бы ответа до дедлайна. Требовать этих
# двух строк от документа было бы неверно — читатель ставит систему не с голого образа.
echo "docs-start: подготовка машины (не из документа): apt-get update, DEBIAN_FRONTEND=noninteractive"
export DEBIAN_FRONTEND=noninteractive
apt-get update -qq

PATH="$STUB:$PATH"
export PATH

# Фаза A: исполняется всё, кроме запусков (`./…`). Запуск — предмет фазы B: окна на чистой машине
# нет, и `./build/editor_shell` там открыть нечем, а собрать его — можно и нужно.
PLAY="$WORK/play.sh"
{
  echo '#!/usr/bin/env bash'
  echo 'set -eux'
  while IFS= read -r cmdline; do
    [ -z "$(start_run_cmd "$cmdline")" ] || continue
    printf '%s\n' "$cmdline"
  done <<<"$CMDS"
  echo 'pwd'
} > "$PLAY"

echo "docs-start: фаза A — команды документа, дословно"
sed 's/^/     | /' "$PLAY"
( cd "$WORK" && bash "$PLAY" ) | tee "$WORK/play.log"
TREE_DIR=$(tail -n 1 "$WORK/play.log")
[ -d "$TREE_DIR" ] || { printf 'docs-start: FAIL фаза A не оставила каталога дерева\n' >&2; exit 1; }
# Каталог ПУСТОЙ — тоже отказ: заглушка клона, промахнувшаяся мимо дерева, оставляет ровно его, и
# без этой строки прогон уходил бы дальше, чтобы упасть на сборке чужой причиной.
[ -n "$(ls -A "$TREE_DIR")" ] || {
  printf 'docs-start: FAIL каталог дерева пуст — клон не скопировал ничего\n' >&2
  exit 1
}
# Улика ПОДМЕНЫ, а не просто успешного клона: сеть в контейнере есть, и настоящий `git clone` с
# GitHub оставил бы такой же непустой каталог — то есть прогон судил бы дерево прошлого push, молча
# и с тем же выводом. Копия делается охватом git и `.git` не несёт; настоящий клон несёт всегда.
[ ! -e "$TREE_DIR/.git" ] || {
  printf 'docs-start: FAIL в каталоге дерева есть .git — клон пришёл из сети, а не подменён\n' >&2
  exit 1
}

# Фаза B: о продуктах сборки. Существование и исполнимость — обо всех; запуск и обещанная строка —
# о тех, кому документ вывод обещал. Роль `check` от `run` отличается ровно этим.
EXPECT="$WORK/expect.tsv"
: > "$EXPECT"
while read -r rel; do
  [ -n "$rel" ] || continue
  start_expect "$SRC" "$rel" >> "$EXPECT"
done <<EOF2
$(start_docs "$SRC")
EOF2

fails=0
seen=0
# Блоки ролей run/check, где запуска не нашлось: покрытие обязано быть отказом, а не тишиной.
covered="$WORK/covered.txt"
: > "$covered"
while IFS=$'\t' read -r role rel blk line; do
  case "$role" in run|check) ;; *) continue ;; esac
  cmd=$(start_run_cmd "$line") || true
  [ -n "$cmd" ] || continue
  printf '%s\t%s\n' "$rel" "$blk" >> "$covered"
  bin=${cmd%% *}
  seen=$((seen + 1))
  if [ ! -x "$TREE_DIR/$bin" ]; then
    printf 'docs-start: FAIL %s обещает %s, а его нет или он не исполняем\n' "$rel" "$bin" >&2
    fails=$((fails + 1))
    continue
  fi
  if [ "$role" != check ]; then
    printf 'docs-start: OK   %s собран (%s)\n' "$bin" "$rel"
    continue
  fi
  want=$(awk -F'\t' -v r="$rel" -v b="$blk" '$1 == r && $2 == b {print $3}' "$EXPECT")
  if [ -z "$want" ]; then
    printf 'docs-start: FAIL блок %s роли check в %s не назвал ожидаемой строки\n' "$blk" "$rel" >&2
    fails=$((fails + 1))
    continue
  fi
  out="$WORK/out.$seen"
  # Команда исполняется ЦЕЛИКОМ, вместе с аргументами документа, — отсюда `bash -c`: строка пришла
  # из нашего же дерева, которое гейт и судит. Таймаут отдельным сообщением: продукт, ждущий окна
  # или ввода, держал бы --live до дедлайна прогона, а висящий гейт хуже падающего — его читают как
  # «ещё считает».
  #
  # Код забирается через `|| rc=$?`, а НЕ из `$?` внутри `if ! …`: там он равен нулю, потому что
  # последней исполненной командой условия была сама инверсия, — и ветка таймаута не сработала бы
  # никогда, печатая «вернул ненулевой код» на зависшем продукте.
  rc=0
  ( cd "$TREE_DIR" && timeout 300 bash -c "$cmd" ) > "$out" 2>&1 || rc=$?
  if [ "$rc" != 0 ]; then
    if [ "$rc" = 124 ]; then
      printf 'docs-start: FAIL %s не ответил за 300 с — прогон завис бы до дедлайна\n' "$cmd" >&2
    else
      printf 'docs-start: FAIL %s вернул ненулевой код\n' "$cmd" >&2
    fi
    sed 's/^/       /' "$out" >&2
    fails=$((fails + 1))
    continue
  fi
  if ! grep -qF -- "$want" "$out"; then
    printf 'docs-start: FAIL %s не напечатал обещанного «%s»\n' "$cmd" "$want" >&2
    sed 's/^/       /' "$out" >&2
    fails=$((fails + 1))
    continue
  fi
  printf 'docs-start: OK   %s напечатал обещанное «%s»\n' "$cmd" "$want"
done <<EOF2
$(start_plan "$SRC")
EOF2

# Блок роли run/check, в котором не нашлось ни одного запуска ОТСЮДА, — отказ, а не пропуск:
# документ, написавший `cd build && ./editor_shell`, иначе выпадал бы из фазы B молча, и продукт,
# который он обещает, не осматривал бы никто.
while IFS=$'\t' read -r role rel blk _; do
  case "$role" in run|check) ;; *) continue ;; esac
  grep -qxF "$(printf '%s\t%s' "$rel" "$blk")" "$covered" && continue
  printf 'docs-start: FAIL блок %s роли %s в %s не запускает продукта из дерева (`./…`)\n' \
    "$blk" "$role" "$rel" >&2
  fails=$((fails + 1))
done <<EOF2
$(start_plan "$SRC" | LC_ALL=C sort -u -t"$(printf '\t')" -k1,3)
EOF2

# Ноль осмотренных продуктов — отказ: фаза B выродилась бы в пустой цикл, а прогон печатал бы
# PASS, ничего не запустив. Тот же класс, что правило vacuous-gate в ci_lint.py.
[ "$seen" != 0 ] || { printf 'docs-start: FAIL документ не велит запустить ни одного бинаря\n' >&2; exit 1; }
[ "$fails" = 0 ] || { printf 'docs-start: FAIL продуктов сборки не хватило: %d\n' "$fails" >&2; exit 1; }
printf 'docs-start: PASS чистая машина прошла getting-started (%d продуктов)\n' "$seen"
