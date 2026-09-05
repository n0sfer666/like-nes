#!/usr/bin/env bash
# Релиз одной командой (спека #20, вертикаль 1): собрать, установить компонент `engine`, упаковать
# детерминированно и напечатать таблицу, по которой пакет опознаётся без распаковки.
#
# Оркестратор живёт на машине владельца (macOS), поэтому «собрать» здесь значит «собрать ДЛЯ ХОСТА».
# Linux и Windows названы кодом и внятным отказом, а не молчаливым пропуском: релиз, тихо собравший
# одну треть заявленного, выглядит ровно как успешный, и узнают об этом уже по пустой странице
# загрузок.
set -euo pipefail

ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
# shellcheck source=scripts/release_lib.sh
. "$ROOT/scripts/release_lib.sh"
# shellcheck source=scripts/release_dmg_lib.sh
. "$ROOT/scripts/release_dmg_lib.sh"
# shellcheck source=scripts/release_appimage_lib.sh
. "$ROOT/scripts/release_appimage_lib.sh"
# shellcheck source=scripts/release_msi_lib.sh
. "$ROOT/scripts/release_msi_lib.sh"
# shellcheck source=scripts/release_msi_pack_lib.sh
. "$ROOT/scripts/release_msi_pack_lib.sh"
# shellcheck source=scripts/release_extra_lib.sh
. "$ROOT/scripts/release_extra_lib.sh"

VERSION=""
ONLY=""
OUT="$ROOT/release"
BUILD="$ROOT/build-release"
BUILD_SET=""
PLATFORM=""

usage() {
  cat <<'USAGE'
usage: scripts/release.sh [--version vX.Y.Z] [--only host|linux|windows] [--out DIR] [--build DIR]
  --version  версия пакета; без неё берётся тег НА HEAD, и его отсутствие — отказ, а не "0.0.0"
  --only     что паковать; по умолчанию хост. linux уезжает в контейнер, windows — в задачу CI
  --out      куда класть пакеты (умолчание release/, он в .gitignore)
  --build    каталог сборки, СВОЙ у релиза (умолчание build-release/): конфигурация здесь
             релизная, и чужой каталог она бы переписала
  --platform платформа контейнера для --only linux с НЕ-линуксовой машины (умолчание —
             архитектура хоста); чужая архитектура идёт через эмуляцию и стоит часы, поэтому
             называется явно. Прогон без контейнера флаг не принимает — это отказ, а не тишина
USAGE
}

# Значение флага требуется ЯВНО. `${2:-}` с последующим `shift 2` на последнем аргументе даёт
# ненулевой код shift, и под `set -e` скрипт умирал молча, без единого слова про опечатку: отказ,
# неотличимый от успеха всем, кроме кода возврата.
need_value() {
  [ "$1" -ge 2 ] || { echo "release: $2 требует значения" >&2; usage >&2; exit 2; }
}

while [ $# -gt 0 ]; do
  case "$1" in
    --version) need_value $# "$1"; VERSION="$2"; shift 2 ;;
    --only) need_value $# "$1"; ONLY="$2"; shift 2 ;;
    --out) need_value $# "$1"; OUT="$2"; shift 2 ;;
    --build) need_value $# "$1"; BUILD="$2"; BUILD_SET=1; shift 2 ;;
    --platform) need_value $# "$1"; PLATFORM="$2"; shift 2 ;;
    -h|--help) usage; exit 0 ;;
    *) echo "release: непонятый аргумент: $1" >&2; usage >&2; exit 2 ;;
  esac
done

case "$(uname -s)" in
  Darwin) HOST=macos; TRIPLE="macos-$(uname -m)" ;;
  Linux) HOST=linux; TRIPLE="linux-$(uname -m)" ;;
  MINGW*|MSYS*|CYGWIN*) HOST=windows; TRIPLE="windows-$(uname -m)" ;;
  *) echo "release: незнакомая ОС $(uname -s) — паковать нечем" >&2; exit 2 ;;
esac
# `host` — документированное значение флага (usage выше, docs/owner-setup.txt), поэтому оно обязано
# разворачиваться в имя хоста ДО сравнения: иначе `--only host` на macOS означает «не macos» и
# отказывает, а работает только недокументированное `--only macos`.
if [ -z "$ONLY" ] || [ "$ONLY" = host ]; then
  ONLY="$HOST"
fi

case "$ONLY" in
  linux|windows|macos) ;;
  *) echo "release: --only принимает host|linux|windows, получено: $ONLY" >&2; exit 2 ;;
esac

# Чужая платформа не собирается хостом, но и не отвергается одинаково: Linux уезжает в контейнер на
# этой же машине (вертикаль 2), Windows — в задачу CI (вертикаль 3). Код 3 — «эта платформа
# собирается не здесь», отдельный от кода 2 («ошибка употребления»): первый ждёт оркестратор, чтобы
# отличить незакрытую вертикаль от опечатки в своей же команде. Обеих закрытых вертикалей он
# лишился, но ветка `*)` остаётся живой: пакет macOS с линукс-машины собрать нечем и делегировать
# некому, и молчаливый пропуск такой трети выглядел бы ровно как успешный релиз.
# --platform относится к КОНТЕЙНЕРУ и больше ни к чему: ни хостовая сборка, ни прогон CI на него не
# смотрят. Молча съеденный флаг читается как выполненный — «собрал под linux/amd64» вышло бы из
# прогона, который собрал под хост или вовсе скачал чужой пакет. Проверка стоит ВЫШЕ диспатча,
# потому что съедала его как раз делегируемая ветка.
#
# Условие проверяет НЕ имя платформы, а то, дойдёт ли прогон до контейнера: тот берётся ровно тогда,
# когда просят linux с не-линуксовой машины. Сравнения `$ONLY != linux` не хватало ровно на одной
# ОС — `host` разворачивается в имя хоста строкой выше, поэтому на линукс-машине `--only host`
# становился неотличим от `--only linux`, флаг чужого этажа съедался молча, а гейт, гонявшийся на
# macOS, этого не видел (найдено первым прогоном самопроверки на настоящем Linux).
if [ -n "$PLATFORM" ] && { [ "$ONLY" != linux ] || [ "$HOST" = linux ]; }; then
  echo "release: --platform относится к сборке linux в контейнере (--only linux с не-линуксовой машины), а этот прогон собирает $ONLY на хосте $HOST" >&2
  exit 2
fi

if [ "$ONLY" != "$HOST" ]; then
  case "$ONLY" in
    linux)
      # Версия резолвится в КАЖДОЙ ветке, а не разом над `case`. Общая точка однажды уже стоила
      # ветке `*)` её кода: пока `resolve_version` жил выше диспатча, платформа, собираемая не
      # здесь, на HEAD без тега умирала кодом 2 («укажи --version») вместо кода 3, и оркестратор
      # читал незакрытую вертикаль как свою же опечатку. Делегатам ранняя резолюция нужна: сборка
      # образа стоит четверть часа, прогон CI — до часа, и узнавать про отсутствующий тег после
      # них поздно.
      VERSION=$(resolve_version "$ROOT" "$VERSION") || exit 2
      # Делегирование, а не вторая реализация: контейнер даёт линукс-ХОСТ, внутри которого работает
      # ЭТОТ ЖЕ скрипт. Пакет, собранный вторым упаковщиком, отличался бы от хостового составом или
      # нормализацией архива, и узнали бы об этом по расхождению сумм у того, кто его скачал.
      set -- --version "$VERSION" --out "$OUT"
      # `[ … ] && set --` здесь был бы миной: под `set -e` ложное условие само становится
      # ненулевым кодом строки, и прогон БЕЗ --platform умирал бы молча, ровно на главном пути.
      if [ -n "$PLATFORM" ]; then set -- "$@" --platform "$PLATFORM"; fi
      if [ -n "$BUILD_SET" ]; then set -- "$@" --build "$BUILD"; fi
      exec bash "$ROOT/scripts/release_container.sh" "$@"
      ;;
    windows)
      # Windows тоже делегируется, но не контейнеру: нативный пакет требует MSVC, и честного
      # способа получить его на macOS нет. Делегат ждёт прогон release_engine.yml, скачивает
      # артефакт и сверяет цепочку — прогон на нашем коммите, тот же коммит в штампе пакета.
      # Версия ему нужна до всякого запроса: по ней он ищет тег и по ней сверяет штамп.
      VERSION=$(resolve_version "$ROOT" "$VERSION") || exit 2
      set -- --version "$VERSION" --out "$OUT"
      exec bash "$ROOT/scripts/release_ci.sh" "$@"
      ;;
    *) echo "release: пакет $ONLY собирается не хостом $HOST" >&2 ;;
  esac
  exit 3
fi
VERSION=$(resolve_version "$ROOT" "$VERSION") || exit 2


NAME="like-nes-engine-$VERSION-$TRIPLE"
STAGE="$BUILD/stage/$NAME"
# Каталог релиза — `release/<версия>/`, как записано в требованиях спеки #20: пакеты двух версий,
# лежащие вперемешку, делят один файл сумм, и первая же вторая сборка стирает суммы первой.
DEST="$OUT/$VERSION"
PKG="$DEST/$NAME.tar.gz"
mkdir -p "$DEST"
rm -rf "$STAGE"

# Каталог сборки принадлежит РЕЛИЗУ. Конфигурация ниже несёт LIKE_NES_RELEASE=ON и версию пакета,
# и попади она в общий каталог (build-full преflight'а) — тот навсегда остался бы релизным с чужой
# версией в штампе: кеш CMake переживает следующий прогон, который те же флаги уже не передаёт.
# Поэтому чужой каталог — отказ, а не молчаливая перенастройка. Это тот же класс, что чинит
# LIKE_NES_BUILD_TYPE в build_check.sh, только пойманный с другой стороны.
if [ -f "$BUILD/CMakeCache.txt" ] && ! grep -q '^LIKE_NES_RELEASE:BOOL=ON$' "$BUILD/CMakeCache.txt"; then
  echo "release: каталог $BUILD сконфигурирован не релизом (или недоконфигурирован упавшим прогоном) — назови свой через --build или удали его" >&2
  exit 2
fi

echo "release: $NAME"
cmake -S "$ROOT" -B "$BUILD" -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DLIKE_NES_RELEASE=ON -DPLUGIN_UI=ON -DGAME_VERSION="$VERSION" \
  -DLIKE_NES_TARGET_TRIPLE="$TRIPLE" >/dev/null
cmake --build "$BUILD" --target assetc editor_shell >/dev/null
cmake --install "$BUILD" --component engine --prefix "$STAGE" >/dev/null

STAMP=$(release_stamp "$ROOT")
manifest_of "$STAGE" > "$DEST/$NAME.manifest"
pack_dir "$STAGE" "$PKG" "$STAMP"

# Родной установщик ВДОБАВОК к архиву (решение 2 спеки #20): образ на macOS, AppImage на Linux,
# MSI на Windows.
# Какой именно и чем он делается — знает release_extra_lib.sh; здесь только место в порядке
# действий: строго ПОСЛЕ установки стейджа, из которого он и раскладывается.
extra_build "$HOST" "$STAGE" "$DEST" "$NAME" "$VERSION" "$STAMP" "$BUILD" "$ROOT/packaging"

SUM=$(sha256_of "$PKG")
# Пересчёт по фактическому содержимому каталога, а не дописывание строки этого прогона: правило
# живёт в release_lib.sh, потому что тот же каталог наполняет и оркестратор CI (release_ci.sh).
# Пересчёт заодно чинит протухшее: удалённый пакет исчезает из файла сам.
write_sums "$DEST"

FILES=$(wc -l < "$DEST/$NAME.manifest" | tr -d ' ')
SIZE=$(wc -c < "$PKG" | tr -d ' ')
# Заголовки латиницей не из вкуса: `printf %-46s` считает БАЙТЫ, а кириллица в UTF-8 занимает по
# два на букву — таблица разъезжалась ровно относительно того образца, что записан в
# docs/owner-setup.txt как ожидаемый вывод.
printf '\n%-46s %10s %6s  %s\n' "package" "size" "files" "sha256"
printf '%-46s %10s %6s  %s\n' "$(basename "$PKG")" "$SIZE" "$FILES" "$SUM"
# Число файлов у образа считается по ЕГО манифесту, а не арифметикой от архива: «стейдж плюс один
# Info.plist» верно ровно до первого файла, который бандл добавит от себя (иконка), и с этого дня
# напечатанное число врало бы молча — а владелец сверяет прогоны именно по этой таблице.
if [ -n "$EXTRA" ]; then
  printf '%-46s %10s %6s  %s\n' "$(basename "$EXTRA")" "$(wc -c < "$EXTRA" | tr -d ' ')" \
    "$(wc -l < "$EXTRA_MANIFEST" | tr -d ' ')" "$(sha256_of "$EXTRA")"
fi
printf '\nвнутри: %s\n' "$(cd "$STAGE" && find . -type f | LC_ALL=C sort | sed 's|^\./||' | tr '\n' ' ')"
printf 'манифест: %s\nсуммы:    %s\n' "$DEST/$NAME.manifest" "$DEST/SHA256SUMS"
# Сказано ВСЛУХ и там, где стоит меняющееся число: владелец сверяет два прогона по этой самой
# таблице, и строка установщика — разная у образа, одинаковая у AppImage — без объяснения читается
# как находка. Что именно обещано на этой ОС, знает та же библиотека, что установщик и делает.
if [ -n "$EXTRA" ]; then
  extra_note "$HOST"
elif [ -n "$EXTRA_SKIP" ]; then
  printf '%s\n' "$EXTRA_SKIP"
fi
