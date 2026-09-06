# shellcheck shell=bash
# Фабрика фикстурных деревьев для самопроверки гейта 5 (спека #19). ОДНА на весь набор: собери
# каждый кейс дерево своей копией правил — и «утверждение отбило порчу» проверялось бы на дереве
# иного устройства, чем то, на котором утверждение проходит.
#
# Дерево игрушечное нарочно: предмет набора в том, что утверждения умеют падать. Что они верны на
# настоящем дереве, проверяет якорный кейс, гоняющий гейт по репозиторию.

# Адрес фикстуры намеренно НЕ совпадает по форме с адресом в её документе: origin в ssh-форме,
# документ в https. Побайтное равенство отбивало бы исправную пару, и нормализация ключа адреса
# без этого не проверялась бы ничем.
START_FIX_ORIGIN='git@github.com:acme/widget.git'
START_FIX_CLONE='https://github.com/acme/widget.git'
START_FIX_WANT='fixture-ok'

start_fix_docs() {
  local root="$1" lang="$2" d="$1/docs/$2/getting-started"
  mkdir -p "$d"
  cat > "$d/prerequisites.md" <<EOD
# Prerequisites ($lang)

<!-- container: install -->
\`\`\`sh
sudo apt-get install -y coreutils
\`\`\`
EOD
  cat > "$d/build.md" <<EOD
# Build ($lang)

<!-- container: build -->
\`\`\`sh
git clone $START_FIX_CLONE
cd widget
mkdir -p build
\`\`\`
EOD
  cat > "$d/first-run.md" <<EOD
# First run ($lang)

<!-- container: run -->
\`\`\`sh
./build/widget
\`\`\`

It prints \`$START_FIX_WANT\` when the machine is ready.

<!-- container: check $START_FIX_WANT -->
\`\`\`sh
./build/widget_check
\`\`\`
EOD
}

# Скрипты фикстуры — КОПИИ настоящих: набор ломает их поимённо, и копия, отставшая от оригинала,
# проверяла бы вчерашний гейт. Список выводится из того, что гейт грузит, а не пишется заново.
start_fix_scripts() {
  local root="$1" src="$2" f list
  mkdir -p "$root/scripts"
  # Библиотеки берутся ИЗ САМОГО гейта: рукописный список отстаёт от него молча, и набор ломал бы
  # копию, которой гейт фикстуры не грузит. Пусто — отказ: разбор, промахнувшийся мимо файла,
  # оставил бы дерево без единого скрипта, и все кейсы падали бы чужой причиной.
  list=$(sed 's/#.*//' "$src/scripts/check_docs_start.sh" \
    | awk '/^\. scripts\//{ sub(/^\. scripts\//, ""); print }')
  [ -n "$list" ] || {
    printf 'selftest: из гейта не выведено ни одной библиотеки\n' >&2
    return 1
  }
  # Гейт и раннер дописываются: первый грузит библиотеки, а не себя, второй грузится прогоном в
  # контейнере. Промах по ним не молчалив — гейт отказывает «судить не о чем», если файла нет.
  for f in $list check_docs_start.sh docs_start_run.sh; do
    cp "$src/scripts/$f" "$root/scripts/$f"
  done
  grep -m1 '^FROM ' "$src/scripts/release_linux.Dockerfile" > "$root/scripts/release_linux.Dockerfile"
}

start_fix_tree() {
  local root="$1" src="$2"
  mkdir -p "$root"
  start_fix_docs "$root" en
  start_fix_docs "$root" ru
  start_fix_scripts "$root" "$src"
  git -C "$root" init -q
  git -C "$root" remote add origin "$START_FIX_ORIGIN"
}

# Порча обязана РЕАЛЬНО менять файл: подмена, ничего не подменившая, читается как «утверждение её
# отбило» — тот же класс, что правило vacuous-gate в ci_lint.py. Так уже было в refusal-наборе
# вертикали 3: sed промахнулся мимо уехавшей строки, и фикстура полгейта проверяла свой промах.
start_fix_mutate() {
  local file="$1"
  shift
  local before after
  before=$(cksum < "$file")
  "$@"
  after=$(cksum < "$file")
  [ "$before" != "$after" ] || {
    printf 'selftest: порча ничего не изменила в %s\n' "$file" >&2
    return 1
  }
}
