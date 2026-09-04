# shellcheck shell=bash
# Установщик «родного» вида ВДОБАВОК к архиву (спека #20, вертикаль 4): `.dmg` на macOS, `.AppImage`
# на Linux. Именно вдобавок — «переносимый вариант есть всегда» есть то же решение спеки, и `.tar.gz`
# остаётся способом поставить движок без монтирования, перетаскивания и бита исполнения.
#
# Ветвление живёт здесь, а не в release.sh: тот стоит на границе бюджета длины, и главное — предмет
# у него другой (собрать, установить, упаковать, напечатать таблицу), а «чем на этой ОС ставят
# софт» есть отдельная ответственность с двумя реализациями, у каждой свой гейт и свой набор
# самопроверок. Windows-инсталлятор (WiX/MSI) станет третьей веткой этого же файла.

extra_none() {
  EXTRA=""
  EXTRA_MANIFEST=""
  # Пропуск, о котором никто не сказал, читается ровно как «образ собран»: в таблице release.sh
  # строки установщика просто нет, а нет её и у отказа. Поэтому у пропуска есть СВОЙ вывод, и
  # печатает его release.sh там же, где печатал бы строку про воспроизводимость.
  EXTRA_SKIP=""
}

# Оба формата раскладываются из УЖЕ УСТАНОВЛЕННОГО стейджа, поэтому вторым списком файлов состав не
# задаётся ни там, ни там: разъезжаться нечему. Манифест пишется рядом с пакетом и у образа, и у
# AppImage — по нему гейт сверяет прогоны, когда байт-равенства формат не даёт.
extra_build() {
  local host="$1" stage="$2" dest="$3" name="$4" version="$5" stamp="$6" build="$7" icon="$8"
  extra_none
  case "$host" in
    macos)
      EXTRA="$dest/$name.dmg"
      EXTRA_MANIFEST="$dest/$name.dmg.manifest"
      local vol="$build/stage/$name.vol"
      rm -rf "$vol"
      mkdir -p "$vol"
      dmg_make_app "$stage" "$vol/$DMG_APP_NAME" "$version" || return 1
      dmg_seal_volume "$vol" "$stamp" || return 1
      manifest_of "$vol" > "$EXTRA_MANIFEST" || return 1
      dmg_pack "$vol" "$EXTRA" "like-nes $version" || return 1
      ;;
    linux)
      # «Инструмента нет» и «инструмент отказал» — разные исходы, и путать их нельзя: appimagetool
      # живёт в образе контейнера, а не в системе владельца (по той же причине его пропускает
      # preflight), и безусловный вызов ронял бы на Linux ВЕСЬ release.sh, а с ним check_release.sh
      # и релизный этап preflight — то есть отсутствие необязательного инструмента отменяло бы
      # архив, который к нему не имеет отношения. Отказ самого инструмента остаётся отказом:
      # его ловит appimage_pack, слушая ненулевой код.
      if ! command -v appimagetool >/dev/null 2>&1; then
        # SC2034: строку читает release.sh, а shellcheck видит один файл, а не связку.
        # shellcheck disable=SC2034
        EXTRA_SKIP='release: appimagetool не найден — .AppImage НЕ собран, в каталоге только .tar.gz.
       Инструмент пиннут суммой в образе scripts/release_linux.Dockerfile: полный пакет Linux даёт
       "bash scripts/release.sh --only linux" (сборка идёт в контейнере), либо поставь appimagetool.'
        return 0
      fi
      EXTRA="$dest/$name.AppImage"
      EXTRA_MANIFEST="$dest/$name.AppImage.manifest"
      local dir="$build/stage/$name.AppDir"
      appimage_make_appdir "$stage" "$dir" "$icon" || return 1
      appimage_seal_appdir "$dir" "$stamp" || return 1
      manifest_of "$dir" > "$EXTRA_MANIFEST" || return 1
      appimage_pack "$dir" "$EXTRA" "$(uname -m)" || return 1
      ;;
    # Windows пакуется на раннере CI тем же скриптом, и установщика у него пока нет — WiX/MSI стоит
    # следующим шагом вертикали. Пропуск обязан быть НАЗВАН: молчаливый ноль отсюда неотличим от
    # «образ собран», и ровно ради этого различения у пропуска есть свой вывод (см. extra_none).
    windows)
      # shellcheck disable=SC2034
      EXTRA_SKIP='release: установщика Windows ещё нет — в каталоге только .tar.gz.
       WiX/MSI — следующий шаг вертикали 4 спеки #20.'
      ;;
    # Незнакомый хост — ОТКАЗ: `$HOST` выставляет release.sh тремя значениями и незнакомую ОС
    # отбивает сам, поэтому сюда попадает только рассинхрон между ними, и тишина о нём читалась бы
    # как «на этой платформе образа не бывает».
    *)
      printf 'release: extra_build не знает платформы %s — образ не собран и не пропущен вслух\n' "$host" >&2
      return 1
      ;;
  esac
}

# Строка про воспроизводимость печатается ТАМ, где стоит меняющееся число, и говорит про формат
# правду, а не общее место: у `.dmg` сумма И РАЗМЕР разные каждый прогон (hdiutil пишет в UDIF время
# и UUID), у `.AppImage` сумма повторяется — squashfs собирается из содержимого, и выравненный mtime
# делает её равной. Владелец сверяет прогоны именно по этой таблице: строка, разная без объяснения,
# читается как находка, а одинаковая там, где её никто не обещал, — как совпадение.
extra_note() {
  case "$1" in
    macos)
      printf 'манифест образа: %s\n' "$EXTRA_MANIFEST"
      printf 'сумма И РАЗМЕР .dmg от прогона к прогону разные (hdiutil пишет в UDIF время и UUID);\n'
      printf 'воспроизводится СОДЕРЖИМОЕ образа — его сверяет scripts/check_release_dmg.sh\n'
      ;;
    linux)
      printf 'манифест AppDir: %s\n' "$EXTRA_MANIFEST"
      printf 'сумма .AppImage ПОВТОРЯЕТСЯ между прогонами (в squashfs едут содержимое, mtime, владелец\n'
      printf 'и права — упаковщик выравнивает все четыре) — байт-равенство сверяет\n'
      printf 'scripts/check_release_appimage.sh\n'
      ;;
  esac
}
