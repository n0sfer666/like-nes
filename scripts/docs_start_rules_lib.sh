# shellcheck shell=bash
# Утверждения о ПРОГОНЕ гейта 5 (спека #19), которым не нужен ни демон, ни сеть: чем прогон
# пользуется, что подменяет и чем судит. Отдельно от утверждений о ТЕКСТЕ документа
# (docs_start_doc_rules_lib.sh) и от разбора блоков (docs_start_lib.sh) по предмету.
#
# Пин базы, выбор движка и его подсказка берутся у release_container_lib.sh: своя копия разъехалась
# бы с релизной, а образ у дерева один. Подключить обязан вызывающий.

# Комментарии снимаются ПЕРЕД всяким грепом по нашим скриптам: слово, найденное в комментарии,
# объявляет меру существующей там, где её нет. Тем и был вакуумно-зелёным
# assert_ci_no_second_packer, нашедший `permissions` в шапке-комментарии workflow.
start_code() {
  sed 's/#.*//' "$1"
}

# База — ГОЛАЯ, и это весь предмет гейта: всё, кроме неё, обязано приехать из документа. Образ
# релизной вертикали (release_linux.Dockerfile) ставит пакеты сам, и прогон на нём был бы зелёным
# при документе, забывшем половину списка. Пин дайджестом — по тому же основанию, что у релиза: тег
# `ubuntu:24.04` переезжает на новый образ молча, и «та же команда» через месяц ставит другую
# систему.
assert_start_bare_base() {
  local root="$1" pin gate="scripts/check_docs_start.sh"
  pin=$(container_base_pin "$root/scripts/release_linux.Dockerfile")
  case "$pin" in
    *@sha256:*) ;;
    *) start_bad "база не пиннута дайджестом: $pin"; return 1 ;;
  esac
  [ -f "$root/$gate" ] || { start_bad "нет $gate — судить не о чем"; return 1; }
  local code
  code=$(start_code "$root/$gate")
  grep -q 'container_base_pin' <<<"$code" || {
    start_bad "$gate не берёт базу из release_linux.Dockerfile — пин дерева у прогона свой"
    return 1
  }
  if grep -qE '(docker|podman|\$ENGINE|"\$ENGINE") build|container_image_tag' <<<"$code"; then
    start_bad "$gate строит образ вместо голой базы — часть системы приехала бы не из документа"
    return 1
  fi
  start_ok "прогон идёт на голой базе, пиннутой дайджестом (${pin#*@})"
}

# Дерево монтируется :ro, а клон подменяется копией. Первое — чтобы прогон не мог править то, что
# судит; второе — чтобы он судил дерево, которое коммитят, а не то, что уже лежит на GitHub.
# Позитивный контроль первым: утверждение, промахнувшееся мимо файла, обязано отличаться от чистого.
assert_start_reads_this_tree() {
  local root="$1" gate="scripts/check_docs_start.sh" runner="scripts/docs_start_run.sh"
  local stub="scripts/docs_start_stub_lib.sh"
  [ -f "$root/$gate" ] && [ -f "$root/$runner" ] && [ -f "$root/$stub" ] || {
    start_bad "нет $gate, $runner или $stub — судить не о чем"
    return 1
  }
  # Раннер обязан ГРУЗИТЬ библиотеку подмен: без неё `start_write_stubs` не определён, прогон умер бы
  # на первой же строке, и «подмена названа вслух» проверялось бы в файле, которого никто не читает.
  local code_runner code_stub code_gate
  code_runner=$(start_code "$root/$runner")
  code_stub=$(start_code "$root/$stub")
  code_gate=$(start_code "$root/$gate")
  grep -q 'docs_start_stub_lib.sh' <<<"$code_runner" || {
    start_bad "$runner не грузит $stub — подменять клон нечем"
    return 1
  }
  grep -q 'start_plan' <<<"$code_runner" || {
    start_bad "$runner не берёт команд из документа — читается не тот файл"
    return 1
  }
  grep -q ':ro' <<<"$code_gate" || {
    start_bad "$gate монтирует дерево на запись — прогон правил бы то, что судит"
    return 1
  }
  # Клон ищется в КОМАНДНОЙ позиции, а не подстрокой: раннер называет ту же пару слов образцом awk,
  # которым выбирает строку документа, и запрет по подстроке отбивал бы его — то есть чинился бы
  # переписыванием образца, а не отказом от сети.
  if grep -qE '(^|[;&|({]|\$\()[[:space:]]*git clone' <<<"$code_runner
$code_stub"; then
    start_bad "прогон клонирует по сети — судил бы дерево с GitHub, а не это"
    return 1
  fi
  # Предмет здесь — ИСПОЛНИМАЯ строка, которую заглушка печатает прогону, а не комментарий рядом с
  # ней: комментарии сняты выше, и слово, оставшееся только в них, объявляло бы подмену названной
  # там, где прогон о ней молчит.
  grep -q 'клон подменён' <<<"$code_stub" || {
    start_bad "$stub не называет подмену клона вслух"
    return 1
  }
  start_ok "прогон читает это дерево :ro, клон подменён копией"
}

# Заглушка клона обязана РАБОТАТЬ, а не только быть упомянутой. Живой прогон 2026-09-06 нашёл ровно
# этот дефект: заглушка звала внутри себя `git ls-files`, PATH вёл в неё же, вызов уходил в ветку
# `exec` с чужими аргументами, каталог оставался пустым — и сборка падала «нет CMakeLists.txt» на
# исправном документе. Греп по раннеру такого не видит, поэтому подмена здесь ЗАПУСКАЕТСЯ.
#
# Дерево ей подставляется своё, крошечное: предмет — механика заглушки (охват git и чистка PATH), а
# не содержимое репозитория. Заодно проверяется сам охват: ненаписанное в индекс приезжает,
# игнорируемое — нет.
#
# Второй кейс — про ИЗБИРАТЕЛЬНОСТЬ подмены, и он тоже находка живого прогона: заглушка ловила ВСЯКИЙ
# клон, поэтому FetchContent получал в `_deps/glfw-src` копию нашего дерева без `.git`, а
# конфигурация умирала «Failed to checkout tag». Чужой адрес обязан уходить настоящему git, и улика
# тому — НАСТОЯЩИЙ клон с `.git` внутри. Клонируется при этом каталог, а не сеть: предмет —
# сравнение адресов, и уводить самопроверку в интернет ради него незачем.
assert_start_stub_runs() {
  local tmp src run out
  tmp=$(mktemp -d "${TMPDIR:-/tmp}/docs-start-stub.XXXXXX") || {
    start_bad "не удалось создать каталог под проверку заглушки"
    return 1
  }
  # Уборка вешается trap'ом: до неё ведёт полдюжины ранних выходов, и каталог, оставшийся от
  # упавшего утверждения, живёт в TMPDIR до перезагрузки машины.
  trap 'rm -rf "$tmp"' RETURN
  src="$tmp/src"
  run="$tmp/run"
  mkdir -p "$src" "$run"
  printf 'cmake_minimum_required(VERSION 3.20)\n' > "$src/CMakeLists.txt"
  printf 'build_junk\n' > "$src/.gitignore"
  printf 'junk\n' > "$src/build_junk"
  printf 'new\n' > "$src/fresh.txt"
  git -C "$src" init -q
  git -C "$src" add CMakeLists.txt .gitignore
  local want=https://example.invalid/acme/widget.git
  start_write_stubs "$tmp/stub" "$src" "$want"
  if ! out=$( cd "$run" && PATH="$tmp/stub:$PATH" git clone "$want" 2>&1 ); then
    start_bad "заглушка клона вернула ненулевой код: $(printf '%s' "$out" | tail -n 1)"
    return 1
  fi
  local bad=0
  [ -f "$run/widget/CMakeLists.txt" ] || { start_bad "заглушка клона не скопировала файла из индекса"; bad=1; }
  [ -f "$run/widget/fresh.txt" ] || { start_bad "заглушка клона потеряла ненаписанный в индекс файл"; bad=1; }
  [ ! -e "$run/widget/build_junk" ] || { start_bad "заглушка клона привезла игнорируемое git"; bad=1; }
  if ! out=$( cd "$run" && PATH="$tmp/stub:$PATH" git clone "$src" dep 2>&1 ); then
    start_bad "чужой клон не дошёл до настоящего git: $(printf '%s' "$out" | tail -n 1)"
    bad=1
  elif [ ! -d "$run/dep/.git" ]; then
    start_bad "чужой адрес подменён копией дерева — зависимости остались бы без своей истории"
    bad=1
  fi
  # Флаг перед адресом: разбор командной строки написан ДВАЖДЫ — функцией start_clone_url (её читает
  # гейт и раннер) и sh-копией внутри заглушки, — а эталона у пары нет. Разъехавшись, они дали бы
  # заглушке WANT, которого та не узнаёт, то есть тихий настоящий клон вместо подмены.
  if ! out=$( cd "$run" && PATH="$tmp/stub:$PATH" git clone -q "$want" flagged 2>&1 ); then
    start_bad "заглушка не поняла клона с флагом: $(printf '%s' "$out" | tail -n 1)"
    bad=1
  elif [ ! -f "$run/flagged/CMakeLists.txt" ]; then
    start_bad "флаг перед адресом сбил разбор — подмена не сработала"
    bad=1
  fi
  # sudo подменяется молча и не проверялся ни одним утверждением: в контейнере его нет вовсе, и
  # команда документа умирала бы «sudo: not found» на фазе установки — то есть подмена без проверки
  # ровно так же неизвестна, как отсутствующая.
  if ! out=$( PATH="$tmp/stub:$PATH" sudo printf 'sudo-ok' 2>&1 ) || [ "$out" != sudo-ok ]; then
    start_bad "заглушка sudo не исполняет команды: $out"
    bad=1
  fi
  [ "$bad" = 0 ] || return 1
  start_ok "заглушка подменяет ТОЛЬКО адрес документа (флаги, охват git, sudo)"
}
