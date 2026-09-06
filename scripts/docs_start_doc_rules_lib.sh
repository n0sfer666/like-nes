# shellcheck shell=bash
# Утверждения о ТЕКСТЕ getting-started (гейт 5 спеки #19): что документ велит делать и что обещает
# читателю. Отдельно от утверждений о ПРОГОНЕ (docs_start_rules_lib.sh) по предмету: там — чем
# команды исполняются и что при этом подменяется, здесь — сами команды и обещания. Разрез сделан
# находкой бюджета длины: общий файл упёрся в жёсткий порог, а он причиной не выкупается ни для кого.
#
# Разбор блоков (start_plan, start_expect, start_docs) живёт в docs_start_lib.sh; подключить обязан
# вызывающий.

# Ожидание из маркера обязано быть тем, что документ обещает ЧИТАТЕЛЮ. Без этого разметка вправе
# требовать любой строки, и «гейт проверил обещание документа» держалось бы только тем, что писавший
# маркер не ошибся. Ищется в ОБОИХ языках: русский читатель видит ту же строку вывода.
assert_start_expect_in_text() {
  local root="$1" rel blk want bad=0 n=0 ru text
  while IFS=$'\t' read -r rel blk want; do
    [ -n "$rel" ] || continue
    n=$((n + 1))
    ru=docs/ru/${rel#docs/en/}
    # Строки самой РАЗМЕТКИ из поиска выброшены: маркер несёт ожидание сам, и поиск по файлу
    # целиком находил бы его всегда — то есть утверждение было бы вакуумным. Доказано порчей
    # «обещания нет в тексте источника»: до этой строки она проезжала гейт зелёной.
    # Текст забирается в ПЕРЕМЕННУЮ, а не течёт в `grep -q` пайпом: под pipefail ранний выход grep'а
    # по первому совпадению рвёт пайп, писатель слева получает SIGPIPE, и код пайплайна ненулевой
    # РОВНО НА СОВПАДЕНИИ — то есть блокирующий гейт объявлял бы нарушение на исправном дереве. Тот
    # же класс уже чинили в check_docs.sh («заголовки цели забираются в переменную»).
    text=$(grep -vF '<!-- container:' "$root/$rel") || text=
    grep -qF -- "$want" <<<"$text" || {
      start_bad "$rel: блок $blk ждёт «${want}», а текст документа этого не обещает"
      bad=1
    }
    text=$(grep -vF '<!-- container:' "$root/$ru" 2>/dev/null) || text=
    grep -qF -- "$want" <<<"$text" || {
      start_bad "$ru: перевод не обещает «${want}», которого ждёт блок $blk"
      bad=1
    }
  done <<EOF2
$(while read -r rel; do [ -n "$rel" ] && start_expect "$root" "$rel"; done <<EOF3
$(start_docs "$root")
EOF3
)
EOF2
  # Ноль ожиданий — отказ: прогон, у которого никто ничего не печатает, проверяет существование
  # файлов и ничего не говорит о том, что они работают. Тот же класс, что vacuous-gate в ci_lint.py.
  [ "$n" != 0 ] || { start_bad "ни один блок не ждёт вывода — запускать бинари незачем"; return 1; }
  [ "$bad" = 0 ] || return 1
  start_ok "обещанный вывод назван и в документе, и в его переводе ($n блоков)"
}

# Адрес клона из документа ведёт в ЭТОТ репозиторий. Прогон клон подменяет копией дерева (иначе он
# судил бы то, что уже на GitHub, а не то, что коммитят), и без этого утверждения документ мог бы
# годами звать читателя в чужой репозиторий, оставаясь зелёным. Сравниваются host и path: origin
# здесь ssh-формы, документ — https, и побайтное равенство отбивало бы исправную пару.
assert_start_clone_url() {
  local root="$1" role rel blk line url origin n=0 bad=0
  origin=$(git -C "$root" remote get-url origin 2>/dev/null | start_url_key)
  [ -n "$origin" ] || { start_bad "у дерева нет remote origin — сверять адрес клона не с чем"; return 1; }
  while IFS=$'\t' read -r role rel blk line; do
    case "$line" in
      "git clone "*) ;;
      *) continue ;;
    esac
    n=$((n + 1))
    url=$(start_clone_url "$line" | start_url_key)
    [ "$url" = "$origin" ] && continue
    start_bad "$rel: клон ведёт в $url, а origin дерева — $origin"
    bad=1
  done <<EOF2
$(start_plan "$root")
EOF2
  [ "$n" != 0 ] || { start_bad "в документе нет команды клонирования — прогону нечего подменять"; return 1; }
  [ "$bad" = 0 ] || return 1
  start_ok "адрес клона в документе — этот репозиторий ($origin)"
}

# Ключ адреса: host/path без схемы, пользователя и .git. ssh-форма пишет двоеточие вместо слэша.
start_url_key() {
  sed -e 's|^[a-z+]*://||' -e 's|^[^@/]*@||' -e 's|:|/|' -e 's|\.git$||' -e 's|/$||'
}

# Бинари, которые документ велит запустить, названы явными путями: только по ним прогон и судит,
# собралось ли обещанное. Ноль таких строк — отказ: фаза запуска выродилась бы в пустой цикл.
assert_start_binaries_named() {
  local root="$1" role rel blk line n=0
  while IFS=$'\t' read -r role rel blk line; do
    case "$role" in run|check) ;; *) continue ;; esac
    case "$line" in ./*) n=$((n + 1)) ;; esac
  done <<EOF2
$(start_plan "$root")
EOF2
  [ "$n" != 0 ] || { start_bad "ни одна run/check-строка не называет бинаря путём ./…"; return 1; }
  start_ok "документ называет запускаемые бинари путями ($n строк)"
}

# У КАЖДОГО блока роли check есть ожидание. Роль `check` от `run` отличается ровно им, и блок без
# ожидания проходил бы статический гейт молча, а падал бы в --live — после установки системы и
# полной сборки, то есть через четверть часа и на чужой машине.
assert_start_check_expected() {
  local root="$1" rel blk want exp n=0 bad=0
  exp=$(
    while read -r rel; do
      [ -n "$rel" ] || continue
      start_expect "$root" "$rel"
    done <<EOF3
$(start_docs "$root")
EOF3
  )
  while IFS=$'\t' read -r rel blk; do
    [ -n "$rel" ] || continue
    n=$((n + 1))
    want=$(awk -F'\t' -v r="$rel" -v b="$blk" '$1 == r && $2 == b {print $3}' <<<"$exp")
    [ -n "$want" ] && continue
    start_bad "$rel: блок $blk роли check не назвал ожидаемой строки"
    bad=1
  done <<EOF2
$(start_plan "$root" | awk -F'\t' '$1 == "check" {print $2 "\t" $3}' | LC_ALL=C sort -u)
EOF2
  [ "$n" != 0 ] || { start_bad "ни одного блока роли check — обещанный вывод не сверяется ни с чем"; return 1; }
  [ "$bad" = 0 ] || return 1
  start_ok "у каждого блока роли check названо ожидание ($n блоков)"
}
