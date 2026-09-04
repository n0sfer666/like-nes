# shellcheck shell=bash
# Утверждения об образе Linux (спека #20, вертикаль 4, шаг B) — отдельно от того, что их запускает,
# по той же причине, что и release_dmg_check_lib.sh: утверждение, живущее только внутри своего
# прогона, невозможно уронить нарочно, а значит неизвестно, умеет ли оно падать вообще.
#
# Предмет здесь — РАСПАКОВАННЫЙ AppDir (`--appimage-extract` кладёт его в squashfs-root), поэтому
# фикстуры под эти правила строятся обычным каталогом и гоняются на любой ОС. Про сам `.AppImage`
# утверждается ровно одно, зато главное — байт-равенство двух прогонов; его место в гейте, где есть
# упаковщик, а не здесь.

# Ожидаемый состав выводится ИЗ ТОГО ЖЕ источника, что и состав tar.gz (expected_files), через ту же
# раскладку, что применил упаковщик: список, написанный здесь второй раз, разъехался бы с
# install_engine.cmake молча. Общая раскладка — известная слабость этого утверждения (ошибись
# appimage_path, и обе половины ошибутся одинаково), и закрывает её СЛЕДУЮЩЕЕ утверждение, которое
# про раскладку не знает ничего. Список стейджа считается ОТДЕЛЬНОЙ строкой, а не подстановкой
# процесса в `done < <(…)`: её код возврата не виден никому, `while` всегда отдаёт ноль, и пропавший
# cmake/licenses.manifest дал бы диагноз «состав разошёлся» вместо «ожидаемое посчитать не из чего».
appimage_expected_files() {
  local root="$1" rel dst src
  src=$(expected_files "$root" Linux) || return 1
  while read -r rel; do
    [ -n "$rel" ] || continue
    dst=$(appimage_path "$rel") || { bad "раскладка не знает файла $rel"; return 1; }
    printf '%s\n' "$dst"
  done <<< "$src"
  printf 'AppRun\n%s\n%s\n.DirIcon\n' "$APPIMAGE_DESKTOP" "$APPIMAGE_ICON"
}

assert_appimage_composition() {
  local root="$1" dir="$2" got exp raw
  got=$( cd "$dir" && find . -type f | sed 's|^\./||' | LC_ALL=C sort )
  # Список считается ДВУМЯ шагами, а не `appimage_expected_files | sort`: код возврата конвейера
  # берётся от `sort`, и отказ раскладки выходил бы диагнозом «состав разошёлся» — ровно тем,
  # которого шапка обещает избежать. Спасал `pipefail`, а он не гарантирован сорсящему библиотеку.
  raw=$(appimage_expected_files "$root") || return 1
  [ -n "$raw" ] || { bad "ожидаемый состав AppImage пуст — сравнивать не с чем"; return 1; }
  exp=$(printf '%s\n' "$raw" | LC_ALL=C sort)
  if [ "$got" != "$exp" ]; then
    bad "состав AppDir разошёлся с ожидаемым"
    diff <(printf '%s\n' "$exp") <(printf '%s\n' "$got") | sed 's/^/       /' >&2 || true
    return 1
  fi
  ok "состав AppImage поимённо ($(printf '%s\n' "$exp" | wc -l | tr -d ' ') файлов)"
}

# Независимый контроль раскладки: МУЛЬТИМНОЖЕСТВО сумм обязано совпадать с суммами стейджа плюс
# ровно четыре строки, которых в стейдже нет и быть не должно, — AppRun, `.desktop` и две копии
# иконки. Про пути это утверждение не знает ничего, поэтому переживает любую ошибку appimage_path и
# ловит ровно то, чего не видит утверждение выше: файл, потерянный раскладкой, задвоенный ею или
# подменённый между установкой и упаковкой.
assert_appimage_mirrors_stage() {
  local dir="$1" stage="$2" got exp extra
  got=$( manifest_of "$dir" | awk '{print $1}' | LC_ALL=C sort )
  exp=$( manifest_of "$stage" | awk '{print $1}' | LC_ALL=C sort )
  if [ "$(comm -23 <(printf '%s\n' "$exp") <(printf '%s\n' "$got") | wc -l | tr -d ' ')" != 0 ]; then
    bad "содержимое стейджа доехало до AppImage не целиком"
    return 1
  fi
  # Четыре — те же четыре строки, что дописывает appimage_expected_files. Связь названа здесь
  # словами намеренно: число и список — два рукописных утверждения об одном факте, и пятый файл
  # AppDir придётся вписать в оба, зато их расхождение падает громко, а не гасит половину контроля.
  extra=$(comm -13 <(printf '%s\n' "$exp") <(printf '%s\n' "$got") | wc -l | tr -d ' ')
  if [ "$extra" != 4 ]; then
    bad "AppImage несёт $extra файлов сверх стейджа, а полагается ровно четыре: AppRun, .desktop и две копии иконки"
    return 1
  fi
  ok "содержимое AppImage совпало со стейджем плюс AppRun, .desktop и иконка"
}

# AppRun — не украшение, а единственный вход: рантайм зовёт именно его, имя точки входа знает только
# он. Утверждается ИСХОД, а не наличие: скрипт, зовущий несуществующий или неисполняемый бинарь,
# выглядит собранным и не запускается ничем — а увидит это владелец, а не гейт.
assert_appimage_apprun() {
  local dir="$1" exe
  [ -f "$dir/AppRun" ] || { bad "в AppDir нет AppRun — рантайму нечего звать"; return 1; }
  [ -x "$dir/AppRun" ] || { bad "AppRun не исполняем"; return 1; }
  exe=$(sed -n 's|^exec "\$here/\(usr/bin/[A-Za-z0-9_.-]*\)".*|\1|p' "$dir/AppRun" | head -1)
  if [ -z "$exe" ]; then
    bad "AppRun не зовёт исполняемый из usr/bin через собственный каталог"; return 1
  fi
  if [ ! -x "$dir/$exe" ]; then
    bad "AppRun зовёт $exe, а исполняемого с таким путём в AppDir нет"; return 1
  fi
  ok "AppRun зовёт $exe через собственный каталог"
}

# `.desktop` читается ПОЛЯМИ секции, а не грепом по файлу: греп одинаково находит ключ в комментарии
# и в чужой секции, а рабочий стол на таком файле просто не покажет ничего. Ключей три несущих —
# точка входа, значок и тип; каждый обязан указывать на то, что в AppDir есть.
appimage_desktop_value() {
  awk -F= -v k="$2" '
    /^\[/ { inside = ($0 == "[Desktop Entry]") ; next }
    inside && $1 == k { sub(/^[^=]*=/, ""); print; exit }
  ' "$1"
}

assert_appimage_desktop() {
  local dir="$1" f type exec icon
  f="$dir/$APPIMAGE_DESKTOP"
  [ -f "$f" ] || { bad "в AppDir нет $APPIMAGE_DESKTOP"; return 1; }
  type=$(appimage_desktop_value "$f" Type)
  exec=$(appimage_desktop_value "$f" Exec)
  icon=$(appimage_desktop_value "$f" Icon)
  if [ "$type" != Application ]; then bad ".desktop не объявляет Type=Application: '$type'"; return 1; fi
  if [ -z "$exec" ] || [ ! -x "$dir/usr/bin/$exec" ]; then
    bad ".desktop называет точкой входа '$exec', а исполняемого usr/bin/$exec в AppDir нет"; return 1
  fi
  # Значок называется БЕЗ расширения — так требует спецификация, и именно на этом appimagetool
  # отказывается собирать образ КОДОМ НОЛЬ, не создав файла: «defined in desktop file but not found».
  if [ -z "$icon" ] || [ ! -f "$dir/$icon.png" ]; then
    bad ".desktop называет значок '$icon', а файла $icon.png в AppDir нет — appimagetool отказал бы кодом 0"
    return 1
  fi
  ok ".desktop называет точку входа $exec и значок $icon"
}

# Значок и `.DirIcon` — один файл, и он же лежит в дереве: копия, разъехавшаяся с packaging/,
# показывала бы вчерашний значок, а `.DirIcon`, разъехавшийся с ней, — разный значок в меню.
assert_appimage_icon() {
  local dir="$1" src="$2" a b c
  [ -f "$src" ] || { bad "иконки $src нет в дереве"; return 1; }
  a=$(sha256_of "$src"); b=$(sha256_of "$dir/$APPIMAGE_ICON" 2>/dev/null); c=$(sha256_of "$dir/.DirIcon" 2>/dev/null)
  if [ "$a" != "$b" ]; then bad "значок в AppDir разошёлся с packaging/$APPIMAGE_ICON"; return 1; fi
  if [ "$a" != "$c" ]; then bad ".DirIcon разошёлся со значком"; return 1; fi
  ok "значок и .DirIcon совпадают с packaging/$APPIMAGE_ICON"
}

# Права едут в squashfs вместе с файлами, то есть В СУММУ пакета: всё в usr/bin обязано быть
# исполняемым, лицензии — нет. Лицензия с битом `+x` не косметика: так она попадает в пакет как
# программа, и это первое, на что смотрит любой упаковщик дистрибутива.
#
# Четыре файла, которые СОЗДАЁТ упаковщик (AppRun, `.desktop` и две копии значка), проверяются
# ЧИСЛОМ, а не битом исполнения, и это не педантизм: перенаправление и `cp` без `-p` берут права у
# umask машины, права едут в сумму образа, и байт-равенство, которое гейт утверждает первым, стало
# бы свойством машины сборки. Два прогона гейта такого не видят по построению — они идут в одном
# шелле с одной umask; ровно тем же доводом обоснован assert_pack_normalized у tar.gz. Файлы
# стейджа сюда не попадают: их права ставит install_engine.cmake и переносит `cp -p`, то есть с
# umask они не связаны вовсе.
appimage_mode_of() {
  # GNU и BSD спрашиваются В ЭТОМ порядке: у GNU `stat -f ФАЙЛ` печатает сведения о ФС и возвращает
  # НОЛЬ, поэтому обратный порядок молча отдавал бы не права, а мусор.
  stat -c '%a' "$1" 2>/dev/null || stat -f '%Lp' "$1"
}

assert_appimage_modes() {
  local dir="$1" f list="" mode want nbin=0 nlic=0 nown=0 ndir=0
  while read -r f; do
    nbin=$((nbin + 1))
    [ -x "$dir/$f" ] || list="$list $f(не исполняем)"
  done < <(cd "$dir" && find usr/bin -type f 2>/dev/null | LC_ALL=C sort)
  while read -r f; do
    nlic=$((nlic + 1))
    [ ! -x "$dir/$f" ] || list="$list $f(исполняем)"
  done < <(cd "$dir" && find usr/share/licenses -type f 2>/dev/null | LC_ALL=C sort)
  for f in AppRun "$APPIMAGE_DESKTOP" "$APPIMAGE_ICON" .DirIcon; do
    [ -f "$dir/$f" ] || { list="$list $f(нет файла)"; continue; }
    nown=$((nown + 1))
    if [ "$f" = AppRun ]; then want=755; else want=644; fi
    mode=$(appimage_mode_of "$dir/$f")
    [ "$mode" = "$want" ] || list="$list $f($mode вместо $want)"
  done
  # Каталоги осматриваются наравне с файлами (находка ревью шага B): их создаёт `mkdir -p` под umask
  # машины, права едут в squashfs, то есть в СУММУ образа, а прежний цикл искал только `-type f` —
  # байт-равенство, утверждаемое первым, оставалось свойством umask. Два прогона того не видят.
  while read -r f; do
    ndir=$((ndir + 1))
    mode=$(appimage_mode_of "$dir/$f")
    [ "$mode" = 755 ] || list="$list $f/($mode вместо 755)"
  done < <(cd "$dir" && find . -type d | sed 's|^\./\{0,1\}||' | grep -v '^$' | LC_ALL=C sort)
  if [ -n "$list" ]; then bad "права в AppDir разъехались:$list"; return 1; fi
  # Осмотренные файлы СЧИТАЮТСЯ: `find` по пропавшему каталогу молчит, цикл не выполняется ни разу,
  # список нарушений пуст — и утверждение печатало бы `ok` про AppDir без точки входа. Тот же класс,
  # что правило `vacuous-gate` в ci_lint.py: «не нашёл» и «нарушений нет» обязаны различаться.
  if [ "$nbin" = 0 ] || [ "$nlic" = 0 ] || [ "$nown" != 4 ] || [ "$ndir" = 0 ]; then
    bad "осмотрено: usr/bin $nbin, licenses $nlic, своих $nown из 4, каталогов $ndir — утверждению о правах не на чем падать"
    return 1
  fi
  ok "права: usr/bin исполняем ($nbin), лицензии — нет ($nlic), свои файлы ровно 755/644 ($nown), каталоги 755 ($ndir)"
}

# Байт-равенство утверждается ПРЯМО, а не через содержимое: squashfs собирается из содержимого и
# выравненного mtime, поэтому два прогона обязаны дать один файл. Это и есть ответ на вопрос, ради
# которого шаг ставился экспериментом до кода: у `.dmg` его пришлось признать недостижимым, у
# AppImage он сбылся — и разница названа вслух, а не сглажена общей формулировкой.
assert_appimage_reproducible() {
  local a="$1" b="$2" sa sb
  sa=$(sha256_of "$a"); sb=$(sha256_of "$b")
  if [ "$sa" != "$sb" ]; then
    bad "сумма .AppImage не воспроизвелась: $sa против $sb"
    return 1
  fi
  # Пустая сумма — не «совпало»: sha256_of на пропавшем файле отдаёт пустую строку, и равенство двух
  # пустых строк напечатало бы `ok` про два несуществующих образа. Сверка размеров, стоявшая здесь
  # прежде, упасть не могла ни на чём: при совпавшей сумме размеры совпадают.
  if [ -z "$sa" ]; then
    bad "сумма .AppImage не посчиталась — образов на месте нет"
    return 1
  fi
  ok "сумма .AppImage воспроизвелась ($sa)"
}
