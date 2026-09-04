# shellcheck shell=bash
# Механика пути через CI (спека #20, вертикаль 3): найти прогон, вынести о нём вердикт, вытащить
# штамп из скачанного пакета. Отдельно от самого прогона, потому что потребителей три — сам
# release_ci.sh, гейт check_release_ci.sh и его самопроверка; повтори их у каждого, и «гейт
# проверил выбор прогона» означало бы «гейт проверил СВОЮ копию выбора прогона».

# Имя артефакта читается ИЗ САМОГО workflow, а не пишется здесь второй раз. Имя, записанное в двух
# местах, разъезжается молча: workflow переименовал артефакт, оркестратор ищет прежний и печатает
# «артефакта нет» на успешном прогоне — то есть отказ, неотличимый от красной сборки. Тот же приём,
# что у container_base_pin, и тот же класс, ради которого в ci_lint.py заведено правило list-drift.
ci_artifact_name() {
  local n
  n=$(awk '
    /uses: actions\/upload-artifact/ { seen = 1; next }
    seen && $1 == "name:" { print $2; exit }
  ' "$1")
  [ -n "$n" ] || return 1
  printf '%s\n' "$n"
}

# `gh` в PATH ещё не значит «готов»: неавторизованный клиент отвечает на каждый запрос отказом
# сети, и оркестратор читал бы это как «прогонов нет». Проверяются оба условия, и порознь — чтобы
# сообщение называло ту причину, которая есть.
# Имя клиента берётся из окружения по образцу LIKE_NES_CONTAINER_ENGINE: самопроверке нужен способ
# показать машину БЕЗ gh, не трогая PATH целиком — обнулив его, она отобрала бы у скрипта и git.
ci_gh_ready() {
  local bin=${LIKE_NES_GH:-gh}
  command -v "$bin" >/dev/null 2>&1 || return 1
  "$bin" auth status >/dev/null 2>&1 || return 2
}

ci_gh_hint() {
  local rc="$1"
  if [ "$rc" = 1 ]; then
    printf 'release: пакет Windows забирается из CI, а gh на машине нет\n' >&2
    printf '        brew install gh && gh auth login, затем повтори: scripts/release.sh --only windows\n' >&2
  else
    printf 'release: gh есть, но не авторизован — запросы к GitHub уйдут в отказ\n' >&2
    printf '        gh auth login, затем повтори: scripts/release.sh --only windows\n' >&2
  fi
}

# Прогон выбирается по КОММИТУ, а не по имени тега: тег переставляется одной командой, и прогон,
# найденный по нему, мог быть собран из другого дерева. Коммит — то единственное, что штамп внутри
# пакета называет независимо, и вся цепочка вертикали сходится именно на нём.
#
# Разбор идёт python3 (он и так зависимость гейтов), а не `gh --jq`: суждение, отданное внутрь gh,
# нечем проверить фикстурой, а фикстура здесь — единственный способ узнать, умеет ли выбор падать.
ci_pick_run() {
  python3 - "$1" "$2" <<'PY'
import json, sys
runs = json.load(open(sys.argv[1]))
sha = sys.argv[2]
mine = [r for r in runs if r.get("headSha") == sha]
if not mine:
    sys.exit(1)
# Свежайший, а не первый попавшийся: повторный dispatch по тому же коммиту — штатное действие,
# и старый прогон с истёкшим артефактом отдавал бы отказ там, где рядом лежит новый.
r = sorted(mine, key=lambda r: r.get("createdAt", ""))[-1]
print(r.get("databaseId", ""), r.get("status", ""), r.get("conclusion", ""))
PY
}

# Вердикт отделён от ожидания: «идёт» и «упал» — разные исходы, и слипшись в один булев они дали бы
# оркестратору ровно ту ошибку, ради которой в ci_watch.sh разведены коды 1 и 2.
ci_run_verdict() {
  local status="$1" conclusion="$2"
  if [ "$status" != completed ]; then printf 'pending\n'; return 0; fi
  if [ "$conclusion" = success ]; then printf 'ok\n'; else printf 'fail\n'; fi
}

# Штамп читается ИЗ АРХИВА, а не из распакованного каталога: распаковывает пакет уже пользователь,
# и утверждение о том, что доехало, обязано смотреть на то, что доехало.
#
# Архив распаковывается ЦЕЛИКОМ, как везде в дереве (check_release.sh, release_check_lib.sh), а не
# извлечением по имени члена: члены лежат как `./like-nes/version.txt`, и совпадение с запрошенным
# `like-nes/version.txt` держалось на нормализации `./` конкретным tar'ом — bsdtar с macOS её
# делает, про GNU из git-bash никто не проверял. Промах ЧИТАТЕЛЯ выходил при этом обвинением
# ПАКЕТА («в пакете нет version.txt»), потому что stderr tar'а уходил в /dev/null.
ci_stamp_of() {
  local tmp rc=0
  tmp=$(mktemp -d)
  tar -xzf "$1" -C "$tmp" || rc=$?
  if [ "$rc" != 0 ]; then
    printf 'release: архив не распаковывается: %s\n' "$1" >&2
    rm -rf "$tmp"; return 1
  fi
  if [ ! -f "$tmp/like-nes/version.txt" ]; then rm -rf "$tmp"; return 1; fi
  cat "$tmp/like-nes/version.txt"
  rm -rf "$tmp"
}

# Разбор строки выбора живёт ЗДЕСЬ, а не в цикле ожидания, и это НАХОДКА, а не вкус: у идущего
# прогона `conclusion` пуст, `set -- $PICKED` давал два позиционных параметра, и `"$3"` под
# `set -u` убивал оркестратор строкой `$3: unbound variable` — на главном же пути вертикали,
# «дождаться прогон». Ветка `pending` не достигалась никогда, и не видело этого ни одно
# утверждение: сам ЦИКЛ не гоняет никто, кроме `--live`. Вынесенный разбор проверяется фикстурой
# с пустым conclusion, то есть суждение об идущем прогоне стало проверяемым без сети.
ci_picked_verdict() {
  local id status conclusion
  read -r id status conclusion <<EOF
$1
EOF
  [ -n "$id" ] || return 1
  printf '%s %s\n' "$id" "$(ci_run_verdict "$status" "$conclusion")"
}
