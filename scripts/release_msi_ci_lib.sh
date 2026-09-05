# shellcheck shell=bash
# Утверждения о пути УСТАНОВЩИКА Windows через CI (спека #20, вертикаль 4, шаг C). Файл отдельный от
# release_ci_rules_lib.sh и release_ci_check_lib.sh: там предмет — архив и цепочка «коммит → прогон →
# штамп», здесь — .msi, который собирает тот же прогон, и компилятор, которым он его собирает.

# WiX пиннут ВЕРСИЕЙ и СУММОЙ файла — по тому же основанию, что appimagetool в
# scripts/release_linux.Dockerfile: ассеты релизов на GitHub переписываются, и «та же ссылка» через
# месяц отдаёт другой инструмент, а образ раннера несёт свой WiX, версия которого меняется без
# нашего ведома. Утверждение СНАЧАЛА доказывает, что видит установку, и только потом судит о пине:
# греп по пропавшему шагу молчит ровно так же, как по честному. Комментарии выброшены — ровно тот
# класс вакуумной зелени, который уже чинили в assert_ci_no_second_packer: строка, найденная в
# собственном пояснении шага, ничего не утверждает.
assert_ci_wix_pinned() {
  local wf="$1" body url step sum var cmp
  body=$(grep -vE '^[[:space:]]*#' "$wf")
  # Ссылка ищется по КОРНЮ релизов, а не по `/releases/download/`: сузив её до формы, которая одна
  # и считается пиннутой, ветка «качается по latest» становится недостижимой — то есть проверяет
  # сама себя. Находка ревью: фикстура с `latest` падала через «установки WiX не видно».
  url=$(printf '%s\n' "$body" \
    | grep -oE 'https://github\.com/wixtoolset/wix3/releases/[^ "'"'"']+' | head -1)
  if [ -z "$url" ]; then
    bad "в $(basename "$wf") не видно установки WiX v3 — .msi собирался бы неизвестно чем"
    return 1
  fi
  case "$url" in
    */latest/*|*/latest)
      bad "WiX качается по 'latest' — инструмент меняется без нашего ведома: $url"
      return 1 ;;
  esac
  # Сумма ищется в ТЕЛЕ ТОГО ЖЕ ШАГА, а не по всему файлу: любой чужой шестнадцатеричный хеш в
  # workflow (пин образа, ключ кеша) выглядел бы для греп-гейта пином WiX.
  step=$(printf '%s\n' "$body" | awk -v u="$url" '
    /^[[:space:]]*-[[:space:]]*name:/ { if (want) { print blk; exit } blk = "" }
    { blk = blk $0 "\n"; if (index($0, u)) want = 1 }
    END { if (want) print blk }')
  sum=$(printf '%s\n' "$step" | grep -oE '\b[0-9a-f]{64}\b' | head -1)
  if [ -z "$sum" ]; then
    bad "скачанный WiX не сверяется суммой — ассет по той же ссылке переписывается молча"
    return 1
  fi
  # И обязана УЧАСТВОВАТЬ В СРАВНЕНИИ: лежащая в шаге строка ничего не проверяет, а шаг с ней
  # выглядит пиннутым. Имя переменной берётся из присваивания — диалект шага (pwsh или bash) при
  # этом безразличен.
  var=$(printf '%s\n' "$step" | awk -v s="$sum" '
    index($0, s) && match($0, /^[[:space:]]*\$?[A-Za-z_][A-Za-z0-9_]*[[:space:]]*=/) {
      t = substr($0, RSTART, RLENGTH); gsub(/[^A-Za-z0-9_]/, "", t); print t; exit }')
  cmp=$(printf '%s\n' "$step" | awk -v v="$var" -v s="$sum" '
    /-ne|-eq|!=|==/ {
      if ((v != "" && index($0, v)) || (v == "" && index($0, s))) { print; exit }
    }')
  if [ -z "$cmp" ]; then
    bad "сумма WiX в шаге лежит, но ни с чем не сравнивается — скачанное принимается любым"
    return 1
  fi
  ok "WiX v3 пиннут версией и суммой (${url##*/}, ${sum:0:12}…)"
}

# О приехавшем установщике утверждается ТО ЖЕ, что о собранном на машине владельца: цепочка
# «коммит → прогон → штамп» о содержимом не говорит ничего, а прогон CI не утверждает о пакете
# вовсе — там release.sh и upload. Пакет без editor_shell.exe, без рантайма или с лицензией в нуль
# байт сходится по версии и коммиту и доезжает как успех.
#
# Осмотр требует msitools, которых на Windows нет вовсе, — поэтому отсутствие ИНСТРУМЕНТА пропуск
# ВСЛУХ, а отсутствие самого ПАКЕТА отказ: молчаливая пропажа половины продукта Windows читается
# ровно как успех. Функции — те же, что зовут вертикали 1-3 (assert_composition/licenses/stamp) плюс
# таблицы установки: копия утверждений разъехалась бы с оригиналом в первый же месяц.
assert_ci_msi() {
  local root="$1" pkg="$2" version="$3"
  local msi="${pkg%.tar.gz}.msi"
  local base triple os tmp rc=0
  if [ ! -s "$msi" ]; then
    bad "рядом с архивом нет установщика $(basename "$msi") — половина пакета Windows не доехала"
    return 1
  fi
  base=$(basename "$msi" .msi)
  triple=${base#"like-nes-engine-$version-"}
  if [ "$triple" = "$base" ] || [ -z "$triple" ]; then
    bad "имя установщика не называет тройку: $(basename "$msi")"; return 1
  fi
  os=$(ci_os_of_triple "$triple") || {
    bad "тройка $triple не называет известную ОС — состав установщика неизвестен"; return 1
  }
  if ! command -v msiinfo >/dev/null 2>&1 || ! command -v msiextract >/dev/null 2>&1; then
    ok "установщик доехал: $(basename "$msi")"
    printf 'ПРОПУСК осмотра .msi: msitools не установлены (brew install msitools)\n'
    return 0
  fi
  tmp=$(mktemp -d)
  if ! msi_extract_to "$msi" "$tmp"; then
    bad "установщик не разворачивается: $(basename "$msi")"; rm -rf "$tmp"; return 1
  fi
  assert_composition "$root" "$tmp" "$os" || rc=1
  assert_licenses "$root" "$tmp" || rc=1
  assert_stamp "$tmp" "$version" "$triple" || rc=1
  rm -rf "$tmp"
  assert_msi_per_user "$msi" || rc=1
  assert_msi_uninstall "$msi" || rc=1
  assert_msi_upgrade "$msi" "$version" || rc=1
  assert_msi_version "$msi" "$version" || rc=1
  assert_msi_shortcut "$msi" || rc=1
  return "$rc"
}
