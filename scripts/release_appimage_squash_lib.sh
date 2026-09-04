# shellcheck shell=bash
# Нормализация САМОГО ОБРАЗА (спека #20, вертикаль 4, шаг B) — своим файлом, по той же границе, что
# у release_appimage_runpath_lib.sh: предмет здесь не содержимое AppDir, а форма squashfs, и читает
# её единственный в наборе `unsquashfs`.

# Нормализация САМОГО ОБРАЗА — линуксовый близнец assert_pack_normalized, и заведён он по тому же
# доводу: два прогона гейта идут в одном шелле от одного пользователя, поэтому владелец и umask
# совпадают по построению, и выкинь кто-нибудь `-all-root` или chmod каталогов — сумма всё равно
# воспроизвелась бы, а «детерминированный образ» осталось бы утверждать нечем.
#
# Читается ИМЕННО образ, а не распакованный каталог: `--appimage-extract` накладывает на распакованное
# umask распаковщика и теряет владельца, а `appimagetool --list` отвечает «To be implemented». Отсюда
# squashfs-tools в scripts/release_linux.Dockerfile.
#
# Разбор строк вынесен ЧИСТОЙ функцией и проверяется фикстурными строками без единого образа — тот же
# приём, что у ci_picked_verdict в вертикали 3: суждение, достижимое только живым прогоном, не
# проверяет никто.
# Образ — это ELF-обёртка runtime'а, к которой squashfs приклеен СО СДВИГОМ, поэтому `unsquashfs`
# без `-o` читает начало файла, видит ELF и отказывается: живой прогон в контейнере так и сказал —
# «образ не читается unsquashfs» на образе, который прекрасно распаковывался. Сдвиг называет сам
# образ (`--appimage-offset`), и разбор его ответа вынесен ЧИСТОЙ функцией: пустая строка и мусор
# дают одно и то же «не число», а `unsquashfs -o ''` прочитал бы файл с нуля и снова отказал —
# отказ, неотличимый от ненормализованного образа.
appimage_offset_valid() {
  case "${1:-}" in
    ''|*[!0-9]*) return 1 ;;
    0) return 1 ;;
  esac
}

appimage_squash_offset() {
  local off
  off=$(APPIMAGE_EXTRACT_AND_RUN=1 "$1" --appimage-offset 2>/dev/null | tail -1)
  appimage_offset_valid "$off" || return 1
  printf '%s\n' "$off"
}

appimage_listing_violations() {
  awk '
    $1 ~ /^[-dlbcps][-rwxsStT]{9}$/ {
      seen++
      owner = $2
      if (owner != "root/root" && owner != "0/0") { print "владелец " owner " у " $NF; bad++ }
      if (substr($1, 1, 1) == "d" && $1 != "drwxr-xr-x") { print "каталог " $1 " у " $NF; bad++ }
    }
    END { if (seen == 0) print "в листинге образа нет ни одной записи" }
  '
}

assert_appimage_normalized() {
  local img="$1" listing bad_lines n off
  command -v unsquashfs >/dev/null 2>&1 || {
    bad "unsquashfs не найден — владельца и права каталогов внутри образа прочитать нечем (apt install squashfs-tools)"
    return 1
  }
  off=$(appimage_squash_offset "$img") || {
    bad "образ не назвал сдвига squashfs (--appimage-offset): $img"
    return 1
  }
  listing=$(unsquashfs -o "$off" -ll "$img" 2>/dev/null) || { bad "образ не читается unsquashfs: $img"; return 1; }
  bad_lines=$(printf '%s\n' "$listing" | appimage_listing_violations)
  if [ -n "$bad_lines" ]; then
    bad "образ не нормализован:"
    printf '%s\n' "$bad_lines" | sed 's/^/       /' >&2
    return 1
  fi
  n=$(printf '%s\n' "$listing" | grep -cE '^[-dlbcps][-rwxsStT]{9} ')
  ok "образ нормализован: владелец 0, каталоги 755 ($n записей)"
}
