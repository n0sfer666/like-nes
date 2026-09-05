# shellcheck shell=bash
# Раскладка установщика Windows (спека #20, вертикаль 4, шаг C): что из стейджа в какой каталог
# установки едет и какими устойчивыми идентификаторами это названо. Отдельный файл по той же
# границе, что release_dmg_lib.sh и release_appimage_lib.sh: предмет здесь — соответствие «файл
# стейджа → каталог установки», а не сборка и не упаковка.
#
# Списка файлов тут нет: он ВЫВОДИТСЯ обходом уже установленного стейджа, а раскладка отвечает
# ровно на вопрос «куда». Путь, которого раскладка не знает, есть ОТКАЗ, а не пропуск: файл,
# добавленный в cmake/install_engine.cmake и не попавший в пакет, не отличим от успеха.

# UpgradeCode один на всю линейку продукта и не меняется НИКОГДА: по нему установщик узнаёт свою же
# прежнюю версию, чтобы снести её перед установкой новой. Переставь его — и в списке программ
# начнут копиться версии, а деинсталляция (гейт 7 спеки) станет снимать одну из копий.
# SC2034: имя шаблона читает упаковщик (release_extra_lib.sh), а shellcheck видит один файл.
# shellcheck disable=SC2034
MSI_TEMPLATE="like-nes.wxs.in"
MSI_UPGRADE_CODE="6F3A9C21-5B84-4E17-9D02-A7C6E4B18D53"

# GUID выводится ИЗ КЛЮЧА, а не берётся случайным: компонент, сменивший GUID между версиями,
# Windows Installer считает другим компонентом — установка поверх оставляет старые файлы. Случайный
# GUID вдобавок ломал бы сверку двух прогонов, которой держится весь релизный гейт.
msi_guid() {
  local key="$1" h tmp
  tmp="$(mktemp)" || return 1
  printf 'like-nes:%s' "$key" > "$tmp"
  h="$(sha256_of "$tmp")"
  rm -f "$tmp"
  [ -n "$h" ] || return 1
  # Форма UUID соблюдается буквой в букву (версия 4, вариант 8): Windows Installer разбирает
  # строку, а не доверяет ей, и «похожий на GUID» набор шестнадцатеричных цифр он отвергает.
  printf '%s-%s-4%s-8%s-%s\n' "${h:0:8}" "${h:8:4}" "${h:13:3}" "${h:17:3}" "${h:20:12}" \
    | tr 'a-f' 'A-F'
}

# Каталог установки для файла стейджа. Вложенности глубже одного уровня раскладка не знает и
# говорит об этом ОТКАЗОМ: `like-nes/bin/sub/x.dll` иначе лёг бы прямо в bin, потеряв подкаталог.
msi_dir_id() {
  local rel="$1" tail
  case "$rel" in
    like-nes/bin/*)      tail="${rel#like-nes/bin/}";      [ "$tail" = "${tail%/*}" ] || return 1
                         printf 'BINFOLDER\n' ;;
    like-nes/licenses/*) tail="${rel#like-nes/licenses/}"; [ "$tail" = "${tail%/*}" ] || return 1
                         printf 'LICENSEFOLDER\n' ;;
    like-nes/*)          tail="${rel#like-nes/}";          [ "$tail" = "${tail%/*}" ] || return 1
                         printf 'INSTALLFOLDER\n' ;;
    *) return 1 ;;
  esac
}

# Идентификатор таблицы File: буквы, цифры, точка и подчёркивание — всё прочее Windows Installer
# в первичном ключе не принимает, а дефис в именах лицензий есть.
msi_id() {
  printf 'f_%s\n' "$1" | tr -c 'A-Za-z0-9._\n' '_'
}

# ProductVersion — ЧИСЛОВОЕ поле из трёх частей с потолками 255.255.65535, и суффикс предрелиза в
# него не влезает: `v0.1.0-rc1` даёт `0.1.0`. Полная версия остаётся в имени пакета, в описании и в
# version.txt. Перебор потолка — отказ: Windows Installer молча обрежет число по модулю.
msi_product_version() {
  local v="${1#v}"
  local a b c rest
  v="${v%%-*}"
  # Разбор идёт ПОСЛЕ проверки формы, а не наоборот: `1..2` при разборе даёт пустое среднее поле,
  # которое дефолт превратил бы в `1.0.2` — то есть кривая версия уехала бы в пакет молча. Формы
  # `1`, `1.2` и `1.2.3` принимаются, недостающие поля дополняются нулями; четвёртое поле у нас
  # отказ, а не отбрасывание: Windows Installer его игнорирует, и разница осталась бы невидимой.
  case "$v" in
    ''|*[!0-9.]*|*..*|.*|*.|*.*.*.*)
      printf 'release: версия %s не даёт числового ProductVersion\n' "$1" >&2
      return 1 ;;
  esac
  a="${v%%.*}"; rest="${v#*.}"; [ "$rest" != "$v" ] || rest=""
  b="${rest%%.*}"; c="${rest#*.}"; [ "$c" != "$rest" ] || c=""
  [ -n "$b" ] || b=0
  [ -n "$c" ] || c=0
  if [ "$a" -gt 255 ] || [ "$b" -gt 255 ] || [ "$c" -gt 65535 ]; then
    printf 'release: версия %s выходит за потолки ProductVersion (255.255.65535)\n' "$1" >&2
    return 1
  fi
  printf '%s.%s.%s\n' "$a" "$b" "$c"
}

# Ключ компонента — запись в HKCU, а не файл: установка пользовательская, и файловый ключ у неё
# ловит ICE38. RemoveFolder на каждом каталоге закрывает гейт 7 спеки — после удаления не остаётся
# ни файлов, ни пустых каталогов установки.
msi_add_dir() {
  local dirid="$1" cid="$2" body="$3" guid
  [ -n "$body" ] || return 0
  guid="$(msi_guid "component:$dirid")" || return 1
  MSI_COMPONENTS="$MSI_COMPONENTS    <DirectoryRef Id=\"$dirid\">
      <Component Id=\"$cid\" Guid=\"$guid\">
        <RegistryValue Root=\"HKCU\" Key=\"Software\\like-nes\\engine\" Name=\"$cid\"
                       Type=\"string\" Value=\"installed\" KeyPath=\"yes\" />
        <RemoveFolder Id=\"rm_$cid\" On=\"uninstall\" />
$body      </Component>
    </DirectoryRef>
"
  MSI_REFS="$MSI_REFS      <ComponentRef Id=\"$cid\" />
"
}

# Обход стейджа идёт heredoc'ом, а не пайпом: отказ раскладки обязан ронять генерацию, а в пайпе
# `return 1` умирает в сабшелле и читается как «нарушений нет».
msi_source_blocks() {
  local stage="$1" all rel dir name line n=0 b_root="" b_bin="" b_lic=""
  all="$( cd "$stage" && find . -type f | sed 's|^\./||' | LC_ALL=C sort )"
  while IFS= read -r rel; do
    [ -n "$rel" ] || continue
    if ! dir="$(msi_dir_id "$rel")"; then
      printf 'release: раскладка MSI не знает пути %s — файл выпал бы из пакета молча\n' "$rel" >&2
      return 1
    fi
    name="${rel##*/}"
    line="        <File Id=\"$(msi_id "$rel")\" Name=\"$name\" Source=\"$rel\" />"
    case "$dir" in
      INSTALLFOLDER) b_root="$b_root$line
" ;;
      BINFOLDER)     b_bin="$b_bin$line
" ;;
      LICENSEFOLDER) b_lic="$b_lic$line
" ;;
    esac
    n=$((n + 1))
  done <<EOF
$all
EOF
  if [ "$n" -eq 0 ]; then
    printf 'release: стейдж %s пуст — MSI собрался бы без единого файла\n' "$stage" >&2
    return 1
  fi
  MSI_COMPONENTS=""
  MSI_REFS=""
  msi_add_dir INSTALLFOLDER c_root "$b_root" || return 1
  msi_add_dir BINFOLDER     c_bin  "$b_bin"  || return 1
  msi_add_dir LICENSEFOLDER c_lic  "$b_lic"  || return 1
}

# Подстановка блоков идёт awk'ом построчно, а не sed'ом: блок многострочный, и в нём есть обратные
# слэши ключа реестра — sed прочитал бы их как экранирование. Скалярные поля (GUID, версии) — уже
# sed'ом: там алфавит из цифр, точек и дефисов, подменять в нём нечего.
msi_make_source() {
  local stage="$1" tpl="$2" out="$3" version="$4" pv comp refs pc sc rc=0
  msi_source_blocks "$stage" || return 1
  pv="$(msi_product_version "$version")" || return 1
  # GUID считаются В ПЕРЕМЕННЫЕ до конвейера, а не подстановкой внутри sed: там их отказ теряется
  # целиком — пустая замена уезжает в Product Id="", неподставленных полей не остаётся, и функция
  # возвращает ноль. Находка ревью; по той же причине слушается статус самого конвейера.
  pc="$(msi_guid "product:$version")" || return 1
  sc="$(msi_guid component:shortcut)" || return 1
  if [ -z "$pc" ] || [ -z "$sc" ]; then
    printf 'release: не вычислен GUID для %s — пакет остался бы без Product Id\n' "$version" >&2
    return 1
  fi
  comp="$(mktemp)" && refs="$(mktemp)" || return 1
  printf '%s' "$MSI_COMPONENTS" > "$comp"
  printf '%s' "$MSI_REFS" > "$refs"
  { awk -v c="$comp" -v r="$refs" '
    $0 == "@COMPONENTS@"     { while ((getline l < c) > 0) print l; next }
    $0 == "@COMPONENT_REFS@" { while ((getline l < r) > 0) print l; next }
    { print }' "$tpl" \
  | sed -e "s/@PRODUCT_CODE@/$pc/" \
        -e "s/@UPGRADE_CODE@/$MSI_UPGRADE_CODE/" \
        -e "s/@GUID_SHORTCUT@/$sc/" \
        -e "s/@PRODUCT_VERSION@/$pv/g" \
        -e "s/@FULL_VERSION@/$version/g" > "$out"; } || rc=$?
  rm -f "$comp" "$refs"
  if [ "$rc" != 0 ]; then
    printf 'release: сборка исходника %s оборвалась (код %s)\n' "$out" "$rc" >&2
    return 1
  fi
  if [ ! -s "$out" ]; then
    printf 'release: исходник %s вышел пустым\n' "$out" >&2
    return 1
  fi
  if grep -q '@[A-Z_]*@' "$out"; then
    printf 'release: в %s остались неподставленные поля:\n%s\n' "$out" "$(grep -n '@[A-Z_]*@' "$out")" >&2
    return 1
  fi
}
