# shellcheck shell=bash
# Образ macOS (спека #20, вертикаль 4, шаг A): `.app` и `.dmg` собираются из УЖЕ УСТАНОВЛЕННОГО
# стейджа, а не вторым набором install-правил. Причина та же, по которой контейнер вертикали 2 и
# прогон CI вертикали 3 не являются вторыми упаковщиками: список файлов, написанный дважды,
# разъезжается молча — ровно тот класс, ради которого в ci_lint.py заведено правило `list-drift`.
# Состав бандла здесь ВЫВОДИТСЯ из состава стейджа, и добавляется к нему ровно один файл — Info.plist.
#
# Иконки у бандла нет, и это не забывчивость: исходного изображения в дереве не существует, а
# пустой .icns дал бы бандл, который выглядит настроенным и показывает системную заглушку. Иконка
# приедет тем раундом, который принесёт саму картинку.

# SC2034: имя бандла читают потребители библиотеки (упаковщик, гейт, оба набора самопроверки), и
# внутри самого файла оно не употребляется — shellcheck видит один файл, а не связку.
# shellcheck disable=SC2034
DMG_APP_NAME="like-nes.app"
# Точка входа названа ЗДЕСЬ и один раз: её пишет Info.plist, и гейт утверждает, что файл с этим
# именем в `Contents/MacOS` есть и исполняем. Второго списка исполняемых не нужно — остальные файлы
# приезжают из стейджа как есть.
DMG_APP_EXE="editor_shell"

# Раскладка «путь в стейдже → путь внутри бандла». Незнакомый путь — ОТКАЗ, а не молчаливый
# пропуск: файл, добавленный в install_engine.cmake и не названный здесь, иначе просто не доехал бы
# до `.app`, и пакет macOS тихо отличался бы составом от tar.gz с тем же именем версии.
dmg_app_path() {
  case "$1" in
    like-nes/bin/*) printf 'Contents/MacOS/%s\n' "${1#like-nes/bin/}" ;;
    like-nes/licenses/*) printf 'Contents/Resources/licenses/%s\n' "${1#like-nes/licenses/}" ;;
    like-nes/version.txt) printf 'Contents/Resources/version.txt\n' ;;
    *) return 1 ;;
  esac
}

# Коммит для CFBundleVersion читается ИЗ ШТАМПА, а не считается заново `git rev-parse`: штамп уже
# лежит в стейдже, и второй счёт означал бы, что бандл и version.txt внутри него могут назвать
# разные коммиты — при пересборке между двумя вызовами именно так и вышло бы.
dmg_commit_of() {
  local stamp="$1/like-nes/version.txt" line
  [ -f "$stamp" ] || { echo "release: в стейдже нет version.txt — коммит для Info.plist читать неоткуда" >&2; return 1; }
  line=$(sed -n 2p "$stamp")
  case "$line" in
    commit\ *) printf '%s\n' "${line#commit }" ;;
    *) echo "release: вторая строка штампа не называет коммит: '$line'" >&2; return 1 ;;
  esac
}

dmg_plist() {
  local version="$1" commit="$2"
  cat <<PLIST
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
  <key>CFBundleName</key>              <string>like-nes</string>
  <key>CFBundleDisplayName</key>       <string>like-nes engine</string>
  <key>CFBundleIdentifier</key>        <string>com.likenes.engine</string>
  <key>CFBundleExecutable</key>        <string>$DMG_APP_EXE</string>
  <key>CFBundlePackageType</key>       <string>APPL</string>
  <key>CFBundleShortVersionString</key><string>$version</string>
  <key>CFBundleVersion</key>           <string>$commit</string>
  <key>LSMinimumSystemVersion</key>    <string>11.0</string>
  <key>NSHighResolutionCapable</key>   <true/>
</dict>
</plist>
PLIST
}

# `cp -p` сохраняет права: `install(PROGRAMS)` ставит 0755, и бандл, потерявший бит исполнения,
# запускается ровно никак. Права едут в образ, то есть в его содержимое, — «переставим потом» тут
# значит другой пакет.
dmg_make_app() {
  local stage="$1" app="$2" version="$3" commit rel dst
  commit=$(dmg_commit_of "$stage") || return 1
  rm -rf "$app"
  mkdir -p "$app/Contents"
  while read -r rel; do
    [ -n "$rel" ] || continue
    dst=$(dmg_app_path "$rel") || {
      printf 'release: файл стейджа %s не назван раскладкой бандла — .app вышел бы неполным\n' "$rel" >&2
      return 1
    }
    mkdir -p "$app/$(dirname "$dst")"
    cp -p "$stage/$rel" "$app/$dst" || return 1
  done < <(cd "$stage" && find . -type f | sed 's|^\./||' | LC_ALL=C sort)
  dmg_plist "$version" "$commit" > "$app/Contents/Info.plist"
  if [ ! -x "$app/Contents/MacOS/$DMG_APP_EXE" ]; then
    printf 'release: в бандле нет исполняемого %s, который называет Info.plist\n' "$DMG_APP_EXE" >&2
    return 1
  fi
}

# Том несёт бандл И симлинк на `/Applications`: это принятый в macOS способ установки — перетащить
# бандл в соседнюю папку окна образа. Без симлинка `.dmg` остаётся архивом, который пользователь
# копирует руками, то есть форматом ради формата.
#
# Время файлов выставляется штампом коммита тем же приёмом, что в pack_dir. Сумма САМОГО образа этим
# не чинится и не заявляется: hdiutil пишет в UDIF своё время и UUID — два прогона из одного
# каталога дали разные суммы, проверено. Воспроизводится СОДЕРЖИМОЕ, и сверяет его гейт.
dmg_seal_volume() {
  local vol="$1" stamp="$2"
  ln -s /Applications "$vol/Applications" || return 1
  TZ=UTC0 find "$vol" -exec touch -h -t "$stamp" {} +
}

# Упаковщик отделён от раскладки тома ровно затем, чтобы у образа был СВОЙ манифест: он считается с
# тома (manifest_of, та же функция, что у tar.gz), а гейт сверяет с ним содержимое смонтированного
# образа. Утверждай про том без этого шага — и подмена между раскладкой и hdiutil прошла бы мимо,
# ровно как её ловит assert_pkg_matches у архива.
dmg_pack() {
  hdiutil create -srcfolder "$1" -volname "$3" -format UDZO -fs HFS+ -ov -quiet "$2"
}
