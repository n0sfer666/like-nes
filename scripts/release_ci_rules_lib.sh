# shellcheck shell=bash
# Утверждения о КОНФИГУРАЦИИ пути через CI (спека #20, вертикаль 3): что собирает прогон, чем он
# это делает, что он НЕ делает и как оркестратор выбирает прогон. Отдельно от утверждений о самом
# приехавшем пакете (release_ci_check_lib.sh) — граница по предмету, та же, что делит
# check_release_selftest.sh и check_release_pack_selftest.sh.

# Тот же инвариант, что у контейнера: CI даёт Windows-ХОСТ, а не второй упаковщик. Утверждение
# СНАЧАЛА доказывает, что видит в workflow вызов release.sh, и только потом ищет чужую упаковку:
# греп-гейт, у которого путь с опечаткой, зелен вакуумно — ровно тот класс, ради которого в
# ci_lint.py заведено правило vacuous-gate.
assert_ci_no_second_packer() {
  local wf="$1" own
  if ! grep -q 'bash scripts/release\.sh' "$wf"; then
    bad "в $(basename "$wf") не найден вызов scripts/release.sh — утверждать про упаковку нечего"
    return 1
  fi
  # Комментарии выбрасываются ДО поиска: строка «ни одного собственного tar в этом workflow» —
  # не упаковка, а ложное срабатывание на самой себе, и гейт, который врёт, перестают читать
  # раньше, чем он поймает настоящую находку.
  # Компиляторы MSI здесь по тому же основанию, что tar: установщик Windows собирает release.sh
  # через extra_build, и шаг, зовущий wixl или candle/light сам, был бы вторым упаковщиком — с
  # собственным списком файлов, который разъедется с install_engine.cmake молча. Ловится ФОРМА
  # ВЫЗОВА (имя, за ним флаг), иначе проверка наличия candle.exe в шаге установки сама срабатывала бы.
  own=$(grep -vE '^[[:space:]]*#' "$wf" \
    | grep -nE '(^|[^_a-z])(tar |Compress-Archive|cmake --install|gzip |(wixl|candle|light)(\.exe)? -)' || true)
  if [ -n "$own" ]; then
    bad "workflow пакует сам, а обязан делегировать release.sh:"
    printf '%s\n' "$own" | sed 's/^/       /' >&2
    return 1
  fi
  ok "прогон CI даёт Windows-хост, а не второй упаковщик"
}

# Имя артефакта живёт в ОДНОМ месте — в workflow, — и оркестратор читает его оттуда. Утверждение
# про то, что чтение работает: молчаливый провал `ci_artifact_name` дал бы пустое имя, а `gh run
# download -n ''` скачивает ВСЕ артефакты прогона, то есть отказ выглядел бы как успех.
assert_ci_artifact_named() {
  local wf="$1" name
  # Сверять прочитанное имя с этим же файлом бессмысленно — оно оттуда и взято, и такое
  # утверждение зелено при любой реализации чтения. Проверяется ровно то, что проверяемо:
  # чтение вообще состоялось и вернуло непустое имя.
  name=$(ci_artifact_name "$wf") || { bad "имя артефакта из $(basename "$wf") не читается"; return 1; }
  # Непустота проверяется ЗДЕСЬ, а не доверяется коду возврата чтения. Молчаливый провал — это
  # реализация, вернувшая ноль и пустую строку; утверждение, смотревшее только на код, её
  # пропускало, и подмена из impl-набора проходила гейт зелёным.
  if [ -z "$(printf '%s' "$name" | tr -d '[:space:]')" ]; then
    bad "чтение имени артефакта вернуло пустое — скачается весь прогон вместо одного артефакта"
    return 1
  fi
  ok "имя артефакта читается из workflow ($name)"
}

# Прогон обязан быть запускаемым руками: без workflow_dispatch с входом `version` оркестратор мог
# бы ждать только тегового прогона, то есть проверить путь можно было бы лишь настоящим релизом.
assert_ci_dispatchable() {
  local wf="$1"
  # Ищется ОБЪЯВЛЕНИЕ триггера (два пробела отступа, уровень `on:`), а не подстрока где угодно:
  # тело шага ветвится по `github.event_name = workflow_dispatch`, и утверждение находило это
  # упоминание в копии, у которой сам триггер вырезан, — то есть было зелено вакуумно.
  if ! grep -qE '^[[:space:]]{2}workflow_dispatch:' "$wf"; then
    bad "$(basename "$wf") не запускается вручную — путь нечем проверить без тега"; return 1
  fi
  if ! grep -q '^      version:' "$wf"; then
    bad "$(basename "$wf") не принимает вход version"; return 1
  fi
  ok "прогон запускается вручную и принимает версию"
}

# Прогон НЕ публикует ничего наружу. Это и есть причина, по которой пакет движка живёт отдельным
# workflow, а не job'ом в release.yml: тот создаёт публичный GitHub Release, и каждая проверка
# оркестратора публиковала бы релиз. Утверждение стережёт именно это свойство — оно легко теряется
# при следующей правке, а замечено будет уже по странице релизов.
assert_ci_publishes_nothing() {
  local wf="$1" body bad_lines
  # Комментарии выбрасываются ДО поиска — по той же причине, что в assert_ci_no_second_packer, и
  # найдено это тем же способом: шапка workflow объясняет словами «здесь permissions: contents:
  # read», и утверждение читало собственное объяснение. Обе половины были зелены вакуумно —
  # копия без permissions и копия с `contents: write` проходили гейт.
  body=$(grep -vE '^[[:space:]]*#' "$wf")
  if ! printf '%s\n' "$body" | grep -q 'permissions:'; then
    bad "$(basename "$wf") не объявляет permissions — прогон получит права по умолчанию"; return 1
  fi
  if ! printf '%s\n' "$body" | grep -q 'contents: read'; then
    bad "$(basename "$wf") не ограничивает contents до read"; return 1
  fi
  # `contents: read` СВЕРХУ и `contents: write` в job'е — обе строки на месте, и утверждение,
  # смотревшее только на наличие, было зелено: permissions job'а перекрывают верхние, а не
  # складываются с ними. Поэтому проверяется не присутствие ограничения, а ОТСУТСТВИЕ права записи
  # где бы то ни было — иначе прогон получает токен, которым можно опубликовать релиз, и заметят
  # это по странице релизов.
  bad_lines=$(printf '%s\n' "$body" | grep -nE 'contents:[[:space:]]*write' || true)
  if [ -n "$bad_lines" ]; then
    bad "workflow пакета движка просит права на запись:"
    printf '%s\n' "$bad_lines" | sed 's/^/       /' >&2
    return 1
  fi
  bad_lines=$(printf '%s\n' "$body" | grep -nE 'gh release|softprops/action-gh-release|steamcmd' || true)
  if [ -n "$bad_lines" ]; then
    bad "workflow пакета движка публикует наружу:"
    printf '%s\n' "$bad_lines" | sed 's/^/       /' >&2
    return 1
  fi
  ok "прогон не публикует ничего, кроме артефакта"
}

# Делегирование доказывается ИСХОДОМ делегата, а не грепом по диспатчу: код 4 приходит из
# release_ci.sh (машина не готова), код 3 — из самого диспатча (эта платформа собирается не здесь).
# Пока windows отказывал третьим, вертикаль была закрыта наполовину, и разница видна только так.
assert_windows_delegated() {
  local release="$1" fake rc=0
  # Хост делается ЧУЖИМ платформе windows заглушкой uname — тем же приёмом, что в
  # assert_foreign_platform_refused. На macOS и Linux это ничего не меняет, а на WINDOWS-раннере
  # меняет всё: там `--only windows` совпадает с хостом, диспатча не касается вовсе и уходит в
  # НАСТОЯЩУЮ сборку — то есть утверждение о делегировании проверяло бы сборку, а полчаса спустя
  # отдавало бы код 0 или 1 по причинам, к вертикали не относящимся. Гейт зовётся в preflight на
  # всех трёх ОС, так что «на macOS работает» тут не аргумент.
  fake=$(mktemp -d)
  printf '#!/bin/sh\ncase "$1" in -m) echo x86_64 ;; *) echo Darwin ;; esac\n' > "$fake/uname"
  chmod +x "$fake/uname"
  if [ "$(PATH="$fake:$PATH" uname -s)" != Darwin ]; then
    bad "заглушка uname не подхватилась — делегирование проверять не на чем"; rm -rf "$fake"; return 1
  fi
  LIKE_NES_GH=likenes-no-such-gh PATH="$fake:$PATH" \
    bash "$release" --only windows --version v0.0.0-check >/dev/null 2>&1 || rc=$?
  rm -rf "$fake"
  if [ "$rc" = 3 ]; then
    bad "release.sh всё ещё отказывает по windows кодом 3 — вертикаль 3 не подключена"; return 1
  fi
  if [ "$rc" != 4 ]; then
    bad "--only windows без gh: ждали код 4 от пути через CI, получили $rc"; return 1
  fi
  ok "--only windows уходит в CI, а не в отказ"
}

# Две причины «машина не готова» разведены СЛОВАМИ при общем коде: «поставь gh» и «войди в gh» —
# разные действия, и одно сообщение на оба заставляло бы переустанавливать работающий клиент.
assert_gh_hint_split() {
  local no_gh no_auth
  no_gh=$(ci_gh_hint 1 2>&1)
  no_auth=$(ci_gh_hint 2 2>&1)
  case "$no_gh" in *"gh на машине нет"*) ;; *) bad "подсказка без gh не называет причину"; return 1 ;; esac
  case "$no_auth" in *"не авторизован"*) ;; *) bad "подсказка без авторизации не называет причину"; return 1 ;; esac
  if [ "$no_gh" = "$no_auth" ]; then bad "обе причины дают одну подсказку"; return 1; fi
  ok "нет gh и нет авторизации разведены словами"
}

# Прогон выбирается по КОММИТУ и берётся свежайший. Фикстура строится здесь же: суждение, которое
# проверяется только живым GitHub, проверяется ровно тогда, когда до него дошли руки.
assert_run_picked_by_commit() {
  local dir picked
  dir=$(mktemp -d)
  cat > "$dir/runs.json" <<'JSON'
[{"databaseId":1,"headSha":"aaaa1111","status":"completed","conclusion":"success","createdAt":"2026-09-01T00:00:00Z"},
 {"databaseId":2,"headSha":"bbbb2222","status":"completed","conclusion":"failure","createdAt":"2026-09-02T00:00:00Z"},
 {"databaseId":3,"headSha":"aaaa1111","status":"in_progress","conclusion":"","createdAt":"2026-09-03T00:00:00Z"}]
JSON
  picked=$(ci_pick_run "$dir/runs.json" aaaa1111 || true)
  if [ "${picked% *}" != "3 in_progress" ]; then
    bad "по коммиту aaaa1111 ждали свежайший прогон 3, получили '$picked'"; rm -rf "$dir"; return 1
  fi
  if ci_pick_run "$dir/runs.json" cccc3333 >/dev/null 2>&1; then
    bad "прогон нашёлся по коммиту, которого в списке нет"; rm -rf "$dir"; return 1
  fi
  rm -rf "$dir"
  ok "прогон выбирается по коммиту, свежайший из своих"
}

# «Идёт» и «упал» — разные исходы. Слипшись в один булев, они дали бы оркестратору ровно ту ошибку,
# ради которой в ci_watch.sh разведены коды 1 и 2: красный прогон читался бы как незавершённый и
# ждал бы своего часа до дедлайна.
assert_verdict_split() {
  local a b c
  a=$(ci_run_verdict completed success)
  b=$(ci_run_verdict completed failure)
  c=$(ci_run_verdict in_progress "")
  if [ "$a" != ok ] || [ "$b" != fail ] || [ "$c" != pending ]; then
    bad "вердикт прогона слипся: success=$a failure=$b in_progress=$c"; return 1
  fi
  ok "вердикт разводит зелёный, красный и незавершённый"
}

# Разбор строки выбора — суждение об ИДУЩЕМ прогоне, и до вертикали 3 его не проверял никто: цикл
# ожидания гоняет только `--live`, а у идущего прогона `conclusion` пуст, отчего разбор позиционными
# параметрами убивал оркестратор на `$3: unbound variable`. Утверждение существует ровно затем, чтобы
# пустое поле перестало быть тем, что проверяется живым GitHub.
assert_picked_parsed() {
  local a b c
  a=$(ci_picked_verdict "17 in_progress ") || { bad "разбор строки идущего прогона провалился"; return 1; }
  b=$(ci_picked_verdict "18 completed failure") || { bad "разбор строки красного прогона провалился"; return 1; }
  c=$(ci_picked_verdict "19 completed success") || { bad "разбор строки зелёного прогона провалился"; return 1; }
  if [ "$a" != "17 pending" ] || [ "$b" != "18 fail" ] || [ "$c" != "19 ok" ]; then
    bad "разбор выбора разошёлся: '$a' / '$b' / '$c'"; return 1
  fi
  if ci_picked_verdict "" >/dev/null 2>&1; then
    bad "пустая строка выбора разобралась как прогон"; return 1
  fi
  ok "разбор выбора переживает пустой conclusion и отбивает пустую строку"
}
