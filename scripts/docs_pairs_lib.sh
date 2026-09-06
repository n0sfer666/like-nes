# shellcheck shell=bash
# Утверждения о ПАРЕ ЯЗЫКОВ публичной документации (спека #19, вертикаль 1) — отдельно от
# утверждений о содержимом одного документа (docs_content_lib.sh). Предмет здесь один: en — источник
# истины, ru — перевод, и расхождение между ними обязано быть находкой, а не поводом для
# рассуждений (решение 1 спеки). Утверждения живут отдельно от того, что их запускает: их зовёт
# гейт (check_docs.sh) и самопроверка на сломанных фикстурах — утверждение, существующее только
# внутри своего прогона, невозможно уронить нарочно, а значит неизвестно, умеет ли оно падать.
#
# Сумму берёт sha256_of из release_lib.sh; подключить обязан вызывающий. Своя копия разъехалась бы
# с релизной в первый же переезд между `shasum` и `sha256sum`, а обе они уже разведены там по ОС.

docs_ok() { printf 'docs-check: OK   %s\n' "$1"; }
docs_bad() { printf 'docs-check: FAIL %s\n' "$1" >&2; }
docs_warn() { printf 'docs-check: ВНИМАНИЕ %s\n' "$1"; }

# Пары «источник → перевод» ОДНИМ местом: их читают все три утверждения этого файла плюс
# штамповщик, и написанный у каждого свой обход разъехался бы молча — ровно list-drift. README
# лежит в корне и зеркалом каталогов не описывается, поэтому он назван здесь явно, а не особым
# случаем внутри каждого читателя.
docs_pairs() {
  local root="$1" rel
  [ -f "$root/README.md" ] && printf 'README.md README.ru.md\n'
  [ -d "$root/docs/en" ] || return 0
  find "$root/docs/en" -type f -name '*.md' | LC_ALL=C sort | while read -r f; do
    rel=${f#"$root/docs/en/"}
    printf 'docs/en/%s docs/ru/%s\n' "$rel" "$rel"
  done
}

# Перевод, который не имеет права отставать: по нему человек ставит движок, и устаревшая инструкция
# установки не «немного не та» — она не работает. Спека называет эти два раздела ошибкой, а прочие
# предупреждением, и разница здесь именно такая.
docs_pair_critical() {
  case "$1" in
    README.ru.md|docs/ru/getting-started/*) return 0 ;;
  esac
  return 1
}

# Штамп версии исходника: перевод несёт сумму того en-файла, с которого сделан. Комментарием HTML,
# а не frontmatter'ом, потому что читаться документ обязан прямо на хостинге репозитория (решение 2
# спеки), а frontmatter там показывается таблицей поверх текста.
# Читается РОВНО СТРОКА 1, а не первое совпадение в файле: иначе документ, который объясняет сам
# механизм и цитирует образец штампа, читался бы как проштампованный, ни разу его не неся, — а
# снятие «везде, где встретится» вдобавок молча съедало бы у него этот пример.
docs_stamp_of() {
  sed -n '1s/^<!-- en-sha256: \([0-9a-f]\{64\}\) -->$/\1/p' "$1"
}

# Множества файлов обеих сторон совпадают. Перевод, у которого нет источника, — документ, которого
# на главном языке не существует; источник без перевода — тихая дыра в ru-ветке, и заметил бы её
# только читатель.
assert_docs_mirrored() {
  local root="$1" en ru miss=0 n=0
  en=$(cd "$root" && find docs/en -type f -name '*.md' 2>/dev/null | sed 's|^docs/en/||' | LC_ALL=C sort)
  ru=$(cd "$root" && find docs/ru -type f -name '*.md' 2>/dev/null | sed 's|^docs/ru/||' | LC_ALL=C sort)
  # Пустое равно пустому: обход, промахнувшийся мимо обоих каталогов, иначе печатал бы
  # «структура зеркальна» — тот же класс, что vacuous-gate в ci_lint.py.
  if [ -z "$en" ] || [ -z "$ru" ]; then
    docs_bad "документов не найдено: en=$(printf '%s' "$en" | wc -w | tr -d ' '), ru=$(printf '%s' "$ru" | wc -w | tr -d ' ')"
    return 1
  fi
  if [ "$en" != "$ru" ]; then
    docs_bad "структура docs/en и docs/ru разошлась"
    diff <(printf '%s\n' "$en") <(printf '%s\n' "$ru") | sed 's/^/       /' >&2 || true
    miss=1
  fi
  n=$(printf '%s\n' "$en" | wc -l | tr -d ' ')
  [ "$miss" = 0 ] || return 1
  docs_ok "структура docs/en и docs/ru зеркальна ($n документов)"
}

# Перевод свежий: штамп совпадает с суммой его en-исходника. Отсутствие штампа есть ошибка ВСЕГДА,
# и это не строгость ради строгости: файл без штампа неотличим от файла, переведённого когда
# угодно, то есть проверять в нём нечего, а гейт печатал бы про него «ok».
assert_docs_translations_fresh() {
  local root="$1" en ru have want stale=0 warned=0 n=0
  while read -r en ru; do
    [ -n "$en" ] || continue
    [ -f "$root/$en" ] || { docs_bad "нет источника $en для перевода $ru"; stale=1; continue; }
    [ -f "$root/$ru" ] || { docs_bad "нет перевода $ru"; stale=1; continue; }
    n=$((n + 1))
    have=$(docs_stamp_of "$root/$ru")
    want=$(sha256_of "$root/$en")
    if [ -z "$have" ]; then
      docs_bad "$ru: нет штампа <!-- en-sha256: … --> — от какой версии $en он переведён, неизвестно"
      stale=1
      continue
    fi
    [ "$have" = "$want" ] && continue
    if docs_pair_critical "$ru"; then
      docs_bad "$ru отстал от $en (штамп $(printf '%.8s' "$have")…, источник $(printf '%.8s' "$want")…)"
      stale=1
    else
      docs_warn "$ru отстал от $en — перевести и проштамповать scripts/docs_stamp.sh"
      warned=$((warned + 1))
    fi
  done <<EOF2
$(docs_pairs "$root")
EOF2
  # Ноль пар — отказ по тому же основанию, что пустое зеркало: сверять было нечего, а вывод
  # выглядел бы как проверенная документация.
  [ "$n" != 0 ] || { docs_bad "пар «источник → перевод» не найдено — сверять свежесть не с чем"; return 1; }
  [ "$stale" = 0 ] || return 1
  docs_ok "переводы соответствуют своим en-исходникам ($n пар, отставших некритичных: $warned)"
}

# Переключатель языка: каждый документ ссылается на свою пару, и ссылка ведёт в существующий файл.
# Требование спеки, и оно же единственное, что связывает две ветки для ЧИТАТЕЛЯ: зеркальность
# каталогов видна гейту, а человеку — нет.
assert_docs_language_switch() {
  local root="$1" en ru bad=0 n=0
  while read -r en ru; do
    [ -n "$en" ] || continue
    docs_switch_points_at "$root" "$en" "$ru" || bad=1
    docs_switch_points_at "$root" "$ru" "$en" || bad=1
    n=$((n + 1))
  done <<EOF2
$(docs_pairs "$root")
EOF2
  [ "$n" != 0 ] || { docs_bad "пар не найдено — переключатель языка проверять не на чем"; return 1; }
  [ "$bad" = 0 ] || return 1
  docs_ok "у каждого документа есть ссылка на его пару ($n пар)"
}

# Ссылка ищется РАЗРЕШЁННЫМ путём, а не подстрокой имени: `../../ru/guide/input.md` и
# `../ru/guide/input.md` отличаются одним сегментом, оба содержат имя пары, и только один из них
# открывается у читателя.
docs_switch_points_at() {
  local root="$1" from="$2" to="$3" dir target
  [ -f "$root/$from" ] || { docs_bad "нет файла $from"; return 1; }
  dir=$(dirname "$from")
  while read -r target; do
    [ -n "$target" ] || continue
    [ "$(docs_normalize "$dir/$target")" = "$to" ] && return 0
  done <<EOF2
$(sed -n '1,12p' "$root/$from" | grep -o '](\([^)]*\.md\))' | sed 's/^](//; s/)$//')
EOF2
  docs_bad "$from не ссылается в шапке на свою пару $to"
  return 1
}

# Свой нормализатор пути, потому что realpath на macOS без coreutils не умеет несуществующих путей,
# а битую ссылку надо именно НАЗВАТЬ, а не упасть на ней.
docs_normalize() {
  printf '%s\n' "$1" | awk -F/ '{
    n = 0
    for (i = 1; i <= NF; i++) {
      if ($i == "." || $i == "") continue
      if ($i == ".." && n > 0 && p[n] != "..") { n--; continue }
      p[++n] = $i
    }
    s = ""
    for (i = 1; i <= n; i++) s = s (i > 1 ? "/" : "") p[i]
    print s
  }'
}
