# shellcheck shell=bash
# Утверждения об образе macOS (спека #20, вертикаль 4, шаг A) — отдельно от того, что их запускает,
# по той же причине, что и release_check_lib.sh: утверждение, живущее только внутри своего прогона,
# невозможно уронить нарочно, а значит неизвестно, умеет ли оно падать вообще.
#
# Про САМ образ здесь не утверждается ничего: два `hdiutil create` из одного каталога дают разные
# суммы (в UDIF едут время и UUID), и обещать байт-равенство значило бы обещать то, чего формат не
# даёт. Утверждается СОДЕРЖИМОЕ: имена, суммы, права и Info.plist — граница названа вслух в ADR.

# Монтирование только для чтения и без окна Finder: гейт не должен ничего показывать владельцу и
# тем более менять образ. Размонтирование терпит второй заход с `-force`: том, занятый чужим
# процессом (Spotlight успевает заглянуть), иначе оставил бы за прогоном смонтированный образ.
dmg_mount() {
  local dmg="$1" mnt="$2"
  mkdir -p "$mnt"
  hdiutil attach "$dmg" -nobrowse -readonly -mountpoint "$mnt" -quiet
}

dmg_umount() {
  [ -d "$1" ] || return 0
  hdiutil detach "$1" -quiet 2>/dev/null || hdiutil detach "$1" -force -quiet 2>/dev/null || true
}

# Ожидаемый состав тома выводится ИЗ ТОГО ЖЕ источника, что и состав tar.gz (expected_files), через
# ту же раскладку, что применил упаковщик. Общий источник — намеренно: список, написанный здесь
# второй раз, разъехался бы с install_engine.cmake молча. Общая раскладка — известная слабость
# этого утверждения (ошибись dmg_app_path, и обе половины ошибутся одинаково), и закрывает её
# СЛЕДУЮЩЕЕ утверждение, которое про раскладку ничего не знает.
dmg_expected_files() {
  local root="$1" rel dst
  while read -r rel; do
    [ -n "$rel" ] || continue
    dst=$(dmg_app_path "$rel") || { bad "раскладка не знает файла $rel"; return 1; }
    printf '%s/%s\n' "$DMG_APP_NAME" "$dst"
  done < <(expected_files "$root" Darwin) || return 1
  printf '%s/Contents/Info.plist\n' "$DMG_APP_NAME"
}

assert_dmg_composition() {
  local root="$1" mnt="$2" got exp
  got=$( cd "$mnt" && find . -type f | sed 's|^\./||' | LC_ALL=C sort )
  exp=$( dmg_expected_files "$root" | LC_ALL=C sort ) || return 1
  if [ "$got" != "$exp" ]; then
    bad "состав образа разошёлся с ожидаемым"
    diff <(printf '%s\n' "$exp") <(printf '%s\n' "$got") | sed 's/^/       /' >&2 || true
    return 1
  fi
  ok "состав образа поимённо ($(printf '%s\n' "$exp" | wc -l | tr -d ' ') файлов)"
}

# Независимый контроль раскладки: МУЛЬТИМНОЖЕСТВО сумм файлов бандла обязано совпадать с суммами
# стейджа плюс ровно одна лишняя строка — Info.plist. Про пути это утверждение не знает ничего,
# поэтому переживает любую ошибку dmg_app_path и ловит ровно то, чего не видит утверждение выше:
# файл, потерянный раскладкой, задвоенный ею или подменённый между установкой и упаковкой.
assert_dmg_mirrors_stage() {
  local mnt="$1" stage="$2" got exp extra
  got=$( manifest_of "$mnt" | awk '{print $1}' | LC_ALL=C sort )
  exp=$( manifest_of "$stage" | awk '{print $1}' | LC_ALL=C sort )
  extra=$(comm -13 <(printf '%s\n' "$exp") <(printf '%s\n' "$got") | wc -l | tr -d ' ')
  if [ "$(comm -23 <(printf '%s\n' "$exp") <(printf '%s\n' "$got") | wc -l | tr -d ' ')" != 0 ]; then
    bad "содержимое стейджа доехало до образа не целиком"
    return 1
  fi
  if [ "$extra" != 1 ]; then
    bad "образ несёт $extra файлов сверх стейджа, а сверх него полагается ровно один — Info.plist"
    return 1
  fi
  ok "содержимое образа совпало со стейджем плюс Info.plist"
}

# Симлинк — не украшение, а способ установки: без него `.dmg` остаётся архивом с ручным
# копированием. `find -type f` его не видит, манифест — тоже, поэтому он утверждается отдельно, и
# утверждается ЦЕЛЬ, а не наличие: ссылка на соседний каталог тома выглядит так же и никуда не ведёт.
assert_dmg_applications_link() {
  local mnt="$1" target
  [ -L "$mnt/Applications" ] || { bad "в образе нет симлинка Applications — ставить бандл некуда"; return 1; }
  target=$(readlink "$mnt/Applications")
  if [ "$target" != /Applications ]; then
    bad "симлинк Applications ведёт не туда: '$target'"; return 1
  fi
  ok "симлинк на /Applications на месте"
}

# Info.plist проверяется ЧИТАТЕЛЕМ системы (PlistBuddy), а не грепом: греп одинаково находит ключ в
# комментарии и в чужой секции, а не-plist, названный этим именем, он бы вовсе не заметил — тогда как
# LaunchServices на таком бандле просто откажется его открывать.
#
# Названный исполняемый обязан СУЩЕСТВОВАТЬ и быть исполняемым. Бандл, потерявший бит `+x` (его
# держит `cp -p` в раскладчике), выглядит собранным и не запускается ничем.
assert_dmg_plist() {
  local mnt="$1" version="$2" commit="$3" plist exe short ver
  plist="$mnt/$DMG_APP_NAME/Contents/Info.plist"
  [ -f "$plist" ] || { bad "в бандле нет Info.plist"; return 1; }
  exe=$(/usr/libexec/PlistBuddy -c 'Print :CFBundleExecutable' "$plist" 2>/dev/null) \
    || { bad "Info.plist не читается как plist или не называет CFBundleExecutable"; return 1; }
  short=$(/usr/libexec/PlistBuddy -c 'Print :CFBundleShortVersionString' "$plist" 2>/dev/null) || short=""
  ver=$(/usr/libexec/PlistBuddy -c 'Print :CFBundleVersion' "$plist" 2>/dev/null) || ver=""
  if [ ! -x "$mnt/$DMG_APP_NAME/Contents/MacOS/$exe" ]; then
    bad "Info.plist называет точкой входа $exe, а исполняемого с таким именем в бандле нет"; return 1
  fi
  if [ "$short" != "$version" ]; then
    bad "CFBundleShortVersionString не совпал с версией пакета: '$short' против '$version'"; return 1
  fi
  # Коммит сверяется с ШТАМПОМ пакета, а не с `git rev-parse`: Info.plist и version.txt внутри
  # одного бандла обязаны называть одно дерево, и разойтись они могут только между собой.
  if [ -z "$ver" ] || [ "$ver" != "$commit" ]; then
    bad "CFBundleVersion не совпал с коммитом штампа: '$ver' против '$commit'"; return 1
  fi
  ok "Info.plist называет точку входа $exe, версию $short и коммит $ver"
}

# Права едут в образ вместе с файлами, и утверждение о них независимо от раскладки: всё в MacOS/
# обязано быть исполняемым, всё в Resources/ — нет. Ресурс с битом `+x` — не косметика: так
# лицензия и штамп попадают в бандл как «программы», и это первое, на что смотрит нотаризация.
assert_dmg_modes() {
  local mnt="$1" f bad_list="" nbin=0 nres=0
  while read -r f; do
    nbin=$((nbin + 1))
    [ -x "$mnt/$f" ] || bad_list="$bad_list $f(не исполняем)"
  done < <(cd "$mnt" && find "$DMG_APP_NAME/Contents/MacOS" -type f 2>/dev/null | LC_ALL=C sort)
  while read -r f; do
    nres=$((nres + 1))
    [ ! -x "$mnt/$f" ] || bad_list="$bad_list $f(исполняем)"
  done < <(cd "$mnt" && find "$DMG_APP_NAME/Contents/Resources" -type f 2>/dev/null | LC_ALL=C sort)
  if [ -n "$bad_list" ]; then bad "права в бандле разъехались:$bad_list"; return 1; fi
  # Осмотренные файлы СЧИТАЮТСЯ: пропавший каталог не даёт `find` ни одной строки, цикл не
  # выполняется ни разу, список нарушений остаётся пустым — и утверждение печатало бы `ok` про
  # бандл, у которого нет ни точки входа, ни ресурсов. Тот же класс, что правило `vacuous-gate`
  # в ci_lint.py: «не нашёл» и «нарушений нет» обязаны различаться.
  if [ "$nbin" = 0 ] || [ "$nres" = 0 ]; then
    bad "осмотрено файлов: MacOS $nbin, Resources $nres — утверждению о правах не на чем падать"
    return 1
  fi
  ok "права: MacOS исполняемы ($nbin), Resources — нет ($nres)"
}

# Смонтированный том, оставшийся после прогона, — след, который `git status` не видит, а владелец
# видит иконкой на рабочем столе. Проверяется не каталогом (его мы и удалим), а списком самой
# системы: `hdiutil info` знает про том, чей mountpoint уже убран.
dmg_physical_path() {
  local p="$1" tail="" base
  while [ ! -d "$p" ] && [ "$p" != / ] && [ "$p" != . ]; do
    tail="/$(basename "$p")$tail"
    p=$(dirname "$p")
  done
  base=$(cd "$p" 2>/dev/null && pwd -P) || { printf '%s\n' "$1"; return 0; }
  case "$base" in /) base="" ;; esac
  if [ -z "$tail" ]; then printf '%s\n' "${base:-/}"; else printf '%s%s\n' "$base" "$tail"; fi
}

# Сравнивается ПОЛЕ точки монтирования целиком, а не вхождение подстроки: `grep -F` по «…/mnt»
# находит его внутри «…/mnt1» и обвиняет прогон в следе, которого тот не оставлял. Путь при этом
# приводится к ФИЗИЧЕСКОМУ: систему наш /var/folders/… знает как /private/var/folders/…, и точное
# сравнение без этого не сошлось бы никогда, то есть утверждение стало бы вечно зелёным.
assert_no_mount_left() {
  local mnt="$1" phys
  phys=$(dmg_physical_path "$mnt")
  if hdiutil info | awk -F'\t' -v a="$mnt" -v b="$phys" \
       'NF >= 3 && ($3 == a || $3 == b) { found = 1 } END { exit !found }'; then
    bad "образ остался смонтированным: $mnt"; return 1
  fi
  ok "следов монтирования не осталось"
}
