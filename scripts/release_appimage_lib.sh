# shellcheck shell=bash
# Установщик Linux (спека #20, вертикаль 4, шаг B): AppDir и `.AppImage` собираются из УЖЕ
# УСТАНОВЛЕННОГО стейджа, а не вторым набором install-правил. Причина та же, по которой контейнер
# вертикали 2 и прогон CI вертикали 3 не являются вторыми упаковщиками, а `.app` шага A выводится
# из состава стейджа: список файлов, написанный дважды, разъезжается молча — тот класс, ради
# которого в ci_lint.py заведено правило `list-drift`.
#
# Иконка здесь ОБЯЗАТЕЛЬНА, в отличие от шага A, и это не смена вкуса: без неё appimagetool
# отказывается собирать образ — и, что хуже, отказывается КОДОМ НОЛЬ, оставив на диске ничего
# («like-nes{.png,.svg,.xpm} defined in desktop file but not found», код 0, файла нет — проверено).
# Поэтому в дереве лежит заглушка packaging/like-nes.png, а упаковщик утверждает ИСХОД (файл на
# месте), а не код возврата инструмента.

# SC2034: имена читают потребители библиотеки (упаковщик, гейт, оба набора самопроверки), внутри
# самого файла они не употребляются — shellcheck видит один файл, а не связку.
# shellcheck disable=SC2034
APPIMAGE_DESKTOP="like-nes.desktop"
# shellcheck disable=SC2034
APPIMAGE_ICON="like-nes.png"
# Точка входа названа ЗДЕСЬ и один раз: её зовёт AppRun и её же называет `.desktop`. Второго списка
# исполняемых не нужно — остальные файлы приезжают из стейджа как есть.
APPIMAGE_EXE="editor_shell"

# Раскладка «путь в стейдже → путь внутри AppDir». Незнакомый путь — ОТКАЗ, а не молчаливый
# пропуск: файл, добавленный в install_engine.cmake и не названный здесь, иначе просто не доехал бы
# до образа, и пакет Linux тихо отличался бы составом от tar.gz с тем же именем версии.
appimage_path() {
  case "$1" in
    like-nes/bin/*) printf 'usr/bin/%s\n' "${1#like-nes/bin/}" ;;
    like-nes/licenses/*) printf 'usr/share/licenses/like-nes/%s\n' "${1#like-nes/licenses/}" ;;
    like-nes/version.txt) printf 'usr/share/like-nes/version.txt\n' ;;
    *) return 1 ;;
  esac
}

# AppRun зовёт точку входа через собственный каталог, а не по имени в PATH: образ монтируется в
# случайный /tmp/.mount_*, и `exec editor_shell` нашёл бы чужой бинарь или не нашёл ничего.
# `$APPDIR` задаёт runtime, но подстраховка через $0 нужна для запуска распакованного AppDir —
# именно так его осматривает гейт.
appimage_apprun() {
  cat <<'RUN'
#!/bin/sh
here=${APPDIR:-$(dirname "$(readlink -f "$0")")}
exec "$here/usr/bin/editor_shell" "$@"
RUN
}

appimage_desktop_entry() {
  cat <<DESKTOP
[Desktop Entry]
Type=Application
Name=like-nes
GenericName=like-nes engine
Comment=2D engine editor and asset baker
Exec=$APPIMAGE_EXE
Icon=like-nes
Categories=Development;IDE;
Terminal=false
DESKTOP
}

# `cp -p` сохраняет права: `install(PROGRAMS)` ставит 0755, и образ, потерявший бит исполнения,
# запускается ровно никак. Права едут в squashfs, то есть в сумму пакета, — «переставим потом» тут
# значит другой пакет.
appimage_make_appdir() {
  local stage="$1" dir="$2" icon="$3" rel dst
  [ -f "$icon" ] || {
    printf 'release: иконки %s нет — appimagetool отказал бы кодом НОЛЬ, не создав образа\n' "$icon" >&2
    return 1
  }
  rm -rf "$dir"
  mkdir -p "$dir"
  while read -r rel; do
    [ -n "$rel" ] || continue
    dst=$(appimage_path "$rel") || {
      printf 'release: файл стейджа %s не назван раскладкой AppDir — образ вышел бы неполным\n' "$rel" >&2
      return 1
    }
    mkdir -p "$dir/$(dirname "$dst")"
    cp -p "$stage/$rel" "$dir/$dst" || return 1
  done < <(cd "$stage" && find . -type f | sed 's|^\./||' | LC_ALL=C sort)
  appimage_apprun > "$dir/AppRun"
  chmod 0755 "$dir/AppRun"
  appimage_desktop_entry > "$dir/$APPIMAGE_DESKTOP"
  cp "$icon" "$dir/$APPIMAGE_ICON"
  # `.DirIcon` — тот же файл, а не ссылка: символьная ссылка внутри squashfs читается не всяким
  # окружением, а лишний килобайт стоит дешевле, чем образ без значка у половины пользователей.
  cp "$icon" "$dir/.DirIcon"
  # Права проставляются ЯВНО, а не достаются от umask машины: перенаправление и `cp` без `-p`
  # накладывают её на новый файл, права едут в squashfs, то есть в СУММУ образа, — и байт-равенство,
  # которое гейт утверждает первым, стало бы свойством машины. Два прогона гейта этого не видят по
  # построению: они идут в одном шелле с одной umask — ровно тот довод, ради которого для tar.gz
  # заведён assert_pack_normalized.
  chmod 0644 "$dir/$APPIMAGE_DESKTOP" "$dir/$APPIMAGE_ICON" "$dir/.DirIcon"
  # Права КАТАЛОГОВ выравниваются тем же доводом и той же строгостью, что права файлов: каталоги
  # создаёт `mkdir -p` под umask машины, в squashfs они едут наравне с файлами, и образ, собранный
  # при umask 077, отличался бы от собранного при 022 байтами при том же содержимом. Ревью шага B:
  # два прогона гейта этого не видят по построению — они идут в одном шелле с одной umask.
  find "$dir" -type d -exec chmod 0755 {} + || return 1
  if [ ! -x "$dir/usr/bin/$APPIMAGE_EXE" ]; then
    printf 'release: в AppDir нет исполняемого %s, который зовёт AppRun\n' "$APPIMAGE_EXE" >&2
    return 1
  fi
}

# Время файлов выставляется штампом коммита тем же приёмом, что в pack_dir. Здесь это НЕСУЩАЯ мера,
# а не гигиена: сумма образа зависит от mtime (тронутый файл дал другую сумму — проверено), и без
# выравнивания байт-детерминизм, который утверждает гейт, не сбылся бы.
appimage_seal_appdir() {
  TZ=UTC0 find "$1" -exec touch -h -t "$2" {} +
}

# Архитектура задаётся ЯВНО, а не угадывается: appimagetool читает её из ELF первого попавшегося
# файла, и на AppDir, где точка входа оказалась скриптом, он отказывается — «Unable to guess the
# architecture», снова кодом ноль. Собственный отказ упаковщика лучше чужого молчания.
#
# Проверок ДВЕ, и они про разное. Ненулевой код достоверен — его и слушаем первым, вместе со всем,
# что инструмент сказал: иначе отказ, оставивший на диске обрубок, проезжал бы как успех (`-f` такой
# файл принимает, и `release_container.sh` тоже), а владелец не видел бы ни строки причины. Ноль же
# о создании образа не говорит НИЧЕГО — поэтому исход утверждается файлом, вторым.
#
# `-all-root` — вторая половина того же ответа, что и chmod каталогов выше: mksquashfs пишет в образ
# uid/gid ВЛАДЕЛЬЦА файлов, поэтому без него сумма пакета зависела бы от того, под кем шла сборка, —
# в контейнере это root, на линукс-машине владельца нет. Опция названа ЗДЕСЬ, одной строкой, и её же
# читает гейт: список аргументов, переписанный вторым местом, разъехался бы молча.
appimage_mksquashfs_opts() {
  printf '%s\n' -all-root
}

appimage_pack() {
  local dir="$1" out="$2" arch="$3" log opt rc=0 args=()
  rm -f "$out"
  while read -r opt; do
    [ -n "$opt" ] || continue
    args+=(--mksquashfs-opt "$opt")
  done < <(appimage_mksquashfs_opts)
  log=$(mktemp)
  ARCH="$arch" APPIMAGE_EXTRACT_AND_RUN=1 \
    appimagetool --no-appstream "${args[@]}" "$dir" "$out" > "$log" 2>&1 || rc=$?
  if [ "$rc" != 0 ]; then
    printf 'release: appimagetool отказал кодом %s:\n' "$rc" >&2
    sed 's/^/       /' "$log" >&2
    rm -f "$out"
  fi
  # Лог убирается ОДНОЙ строкой на всех путях: две точки уборки уже значит третий путь, на котором
  # файл переживает прогон, — ровно тот класс, ради которого `pack_dir` перехватывает код возврата.
  rm -f "$log"
  [ "$rc" = 0 ] || return 1
  [ -f "$out" ] || {
    printf 'release: appimagetool не создал %s (он сообщает об этом кодом НОЛЬ)\n' "$out" >&2
    return 1
  }
}
