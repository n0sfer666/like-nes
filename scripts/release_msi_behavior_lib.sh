# shellcheck shell=bash
# Утверждения о ПОВЕДЕНИИ установщика Windows (спека #20, вертикаль 4, шаг C): где он ставит, что
# сносит при удалении, как узнаёт свою же прежнюю версию и куда ведёт ярлык. Отделено от
# release_msi_check_lib.sh по предмету, а не по счётчику строк: там пакет читается и сверяется как
# СОДЕРЖИМОЕ (двумя прогонами, зеркалом стейджа), здесь по тем же таблицам судится то, что
# случится на машине владельца при двойном щелчке и при удалении.
#
# Читалки таблиц (msi_table, msi_prop) живут в соседнем файле, и он обязан быть загружен раньше:
# граница здесь по предмету, а не по независимости.

# Инвариант 3 спеки: не требовать администратора там, где можно без него. Проверяется это по
# готовому пакету, а не по намерению: корень установки обязан висеть на LocalAppDataFolder, а
# свойства ALLUSERS не должно быть вовсе — заданное в единицу, оно делает установку машинной.
assert_msi_per_user() {
  local msi="$1" parent all
  parent=$(msi_table "$msi" Directory | awk -F'\t' '$1 == "INSTALLFOLDER" { print $2 }')
  if [ -z "$parent" ]; then bad "в таблице Directory нет INSTALLFOLDER"; return 1; fi
  if [ "$parent" != LocalAppDataFolder ]; then
    bad "корень установки висит на $parent, а не на LocalAppDataFolder — это машинная установка"
    return 1
  fi
  all=$(msi_prop "$msi" ALLUSERS)
  if [ -n "$all" ]; then bad "задано ALLUSERS='$all' — установка перестала быть пользовательской"; return 1; fi
  ok "установка пользовательская (корень в LocalAppDataFolder, ALLUSERS не задано)"
}

# Гейт 7 спеки: после удаления не остаётся файлов установки. У MSI это даётся не обещанием, а
# двумя записями — RemoveFolder на КАЖДОМ нашем каталоге (иначе останутся пустые каталоги) и
# принадлежностью каждого файла компоненту, который вообще входит в устанавливаемую функцию:
# компонент вне Feature не ставится и не удаляется. Судится это по ТАБЛИЦЕ пакета: у wixl такой
# компонент выпадает из пакета целиком и виден заодно по составу, но компиляторов у нашего
# исходника два, и полагаться на поведение одного из них значило бы проверять компилятор.
# Наши каталоги выводятся ОБХОДОМ таблицы Directory вверх до корня установки, а не исключением
# `ProgramMenuFolder` по имени: имя каталога ярлыка живёт в шаблоне, и вторая его копия здесь
# разъехалась бы с ним молча (тот же класс, что правило `list-drift` в ci_lint.py). Системный
# каталог нашим при этом не становится ни при каком переименовании — RemoveFolder на нём снёс бы
# чужое.
msi_our_dirs() {
  msi_table "$1" Directory | awk -F'\t' '
    { parent[$1] = $2 }
    END {
      for (d in parent) {
        cur = d
        for (i = 0; i < 32 && cur != ""; i++) {
          if (cur == "INSTALLFOLDER") { print d; break }
          cur = parent[cur]
        }
      }
    }'
}

assert_msi_uninstall() {
  local msi="$1" dirs d files f comp infeat ours n=0 m=0
  ours=$(msi_our_dirs "$msi" | LC_ALL=C sort -u)
  [ -n "$ours" ] || { bad "в таблице Directory нет ни одного каталога под корнем установки"; return 1; }
  dirs=$(msi_table "$msi" Component | awk -F'\t' 'NR == FNR { our[$0] = 1; next } our[$3] { print $3 }' \
    <(printf '%s\n' "$ours") - | LC_ALL=C sort -u)
  [ -n "$dirs" ] || { bad "в пакете нет ни одного компонента с каталогом установки"; return 1; }
  while IFS= read -r d; do
    [ -n "$d" ] || continue
    if ! msi_table "$msi" RemoveFile | awk -F'\t' -v d="$d" '$4 == d && $5 == 2 { f = 1 } END { exit !f }'; then
      bad "каталог $d не удаляется при деинсталляции — останется пустым"
      return 1
    fi
    n=$((n + 1))
  done <<EOF
$dirs
EOF
  infeat=$(msi_table "$msi" FeatureComponents | awk -F'\t' '{ print $2 }' | LC_ALL=C sort -u)
  files=$(msi_table "$msi" File)
  [ -n "$files" ] || { bad "в таблице File ни одной строки"; return 1; }
  while IFS= read -r f; do
    [ -n "$f" ] || continue
    comp=$(printf '%s' "$f" | awk -F'\t' '{ print $2 }')
    if ! printf '%s\n' "$infeat" | grep -Fqx "$comp"; then
      bad "файл $(printf '%s' "$f" | awk -F'\t' '{ print $3 }') лежит в компоненте $comp вне Feature"
      return 1
    fi
    m=$((m + 1))
  done <<EOF
$files
EOF
  ok "деинсталляция полна: каталогов с удалением $n, файлов в устанавливаемых компонентах $m"
}

# Линия обновления: UpgradeCode один на продукт и совпадает с записанным в библиотеке раскладки,
# ProductCode выводится из версии (то есть у двух версий он разный), а таблица Upgrade говорит
# установщику снести свою же прежнюю. Без последнего версии копились бы в списке программ.
#
# Проверяется не СУЩЕСТВОВАНИЕ строки, а ГРАНИЦЫ диапазонов, и это находка ревью: у сносящей строки
# без верхней границы «своя линейка» включает и более новые версии, то есть установка старой поверх
# новой молча сносит новую. Строка с нашим UpgradeCode при этом на месте, и утверждение о её наличии
# проходило и на исправном пакете, и на пакете с даунгрейдом. Отсюда две записи: сносящая обязана
# кончаться нашей версией, детектирующая — начинаться с неё и уходить вверх без потолка.
#
# Скобки нормализуются У ОБЕИХ сторон: свойства msiinfo отдаёт в фигурных, таблицу Upgrade — тоже,
# но компилятор, пишущий UpgradeCode без них, ронял бы утверждение по причине, не имеющей отношения
# к предмету, а компиляторов у нашего исходника два.
msi_upgrade_rows() { msi_table "$1" Upgrade | awk -F'\t' -v u="$2" '{ g = $1; gsub(/[{}]/, "", g) } g == u'; }

assert_msi_upgrade() {
  local msi="$1" version="$2" uc pc want rows pv
  uc=$(msi_prop "$msi" UpgradeCode | tr -d '{}')
  if [ "$uc" != "$MSI_UPGRADE_CODE" ]; then
    bad "UpgradeCode пакета '$uc' не тот, что в раскладке ($MSI_UPGRADE_CODE)"; return 1
  fi
  pc=$(msi_prop "$msi" ProductCode | tr -d '{}')
  want=$(msi_guid "product:$version") || return 1
  if [ "$pc" != "$want" ]; then
    bad "ProductCode '$pc' не выводится из версии $version (ждали $want)"; return 1
  fi
  pv=$(msi_product_version "$version") || return 1
  rows=$(msi_upgrade_rows "$msi" "$MSI_UPGRADE_CODE")
  if [ -z "$rows" ]; then bad "в таблице Upgrade нет своей же линии — версии будут копиться"; return 1; fi
  # Разряд 2 в Attributes — OnlyDetect: строка без него сносит найденное, строка с ним лишь помечает.
  if ! printf '%s\n' "$rows" | awk -F'\t' -v v="$pv" '$5 % 4 < 2 && $3 == v { f = 1 } END { exit !f }'; then
    bad "сносящая строка Upgrade не кончается версией $pv — установка старой поверх новой снесёт новую"
    printf '%s\n' "$rows" | sed 's/^/       /' >&2
    return 1
  fi
  if ! printf '%s\n' "$rows" | awk -F'\t' -v v="$pv" '$5 % 4 >= 2 && $2 == v && $3 == "" { f = 1 } END { exit !f }'; then
    bad "в таблице Upgrade нет строки, обнаруживающей более новую версию — даунгрейд пройдёт молча"
    printf '%s\n' "$rows" | sed 's/^/       /' >&2
    return 1
  fi
  ok "линия обновления на месте (сносится не новее $pv, более новое обнаруживается и отбивается)"
}

# Версия и ярлык. ProductVersion — числовое поле из трёх частей, и суффикс предрелиза в него не
# влезает: сверяется он с тем, что даёт msi_product_version, а не с исходной строкой.
assert_msi_version() {
  local msi="$1" version="$2" got want
  want=$(msi_product_version "$version") || return 1
  got=$(msi_prop "$msi" ProductVersion)
  if [ "$got" != "$want" ]; then
    bad "ProductVersion '$got' не отвечает версии $version (ждали $want)"; return 1
  fi
  ok "ProductVersion отвечает версии пакета ($want из $version)"
}

assert_msi_shortcut() {
  local msi="$1" row target
  row=$(msi_table "$msi" Shortcut | head -1)
  [ -n "$row" ] || { bad "в пакете нет ярлыка — движок придётся искать в каталоге установки"; return 1; }
  target=$(printf '%s' "$row" | awk -F'\t' '{ print $5 }')
  case "$target" in
    *editor_shell.exe) : ;;
    *) bad "ярлык ведёт не в редактор: '$target'"; return 1 ;;
  esac
  ok "ярлык ведёт в редактор ($target)"
}

# Тихая установка (`msiexec /i … /qn`) — вторая половина инварианта 3, и от предыдущего утверждения
# она отличается предметом: там КУДА ставим, здесь СПРОСЯТ ЛИ по дороге. Корень в
# LocalAppDataFolder этого не решает — решает бит 8 сводного потока («elevated privileges are not
# required»), который msiexec читает раньше всех таблиц. Без него пакет ставится в пользовательский
# каталог и всё равно поднимает UAC, а под `/qn` показать запрос некому: установка обрывается.
assert_msi_silent() {
  local msi="$1" wc n cond
  wc=$(msi_wordcount "$msi") || { bad "сводный поток пакета не читается — о правах судить не по чему"; return 1; }
  if [ "$((wc & 8))" -ne 8 ]; then
    bad "в сводном потоке нет бита 8 (Source=$wc): msiexec спросит права, а под /qn спрашивать некому"
    return 1
  fi
  # Отсутствие строк и есть ожидаемое, поэтому у утверждения СВОЙ позитивный контроль: сперва
  # читается таблица, которая обязана быть. Без него сломанный msiinfo отвечал бы «строк нет», и
  # утверждение было бы зелёным на любом файле.
  msi_table "$msi" Directory >/dev/null || { bad "таблицы пакета не читаются — о тихом режиме судить не по чему"; return 1; }
  n=$(msi_table "$msi" CustomAction | grep -c '[^[:space:]]') || n=0
  if [ "$n" -gt 0 ]; then
    bad "в пакете $n CustomAction: чужой код посреди установки и единственный способ втащить в неё повышение прав"
    return 1
  fi
  # Сама по себе LaunchCondition законна — ею отбивается даунгрейд. Незаконно условие ПРО ПРАВА: под
  # /qn оно обрывает установку молча, и владелец видит «ничего не произошло».
  cond=$(msi_table "$msi" LaunchCondition | grep -Ei 'privileged|adminuser') || cond=""
  if [ -n "$cond" ]; then
    bad "условие запуска требует администратора: $cond"
    return 1
  fi
  ok "тихая установка: прав не требует (Source=$wc), CustomAction нет, условия запуска не про права"
}
