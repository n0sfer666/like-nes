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

VERSION=""
ONLY=""
OUT="$ROOT/release"
BUILD="$ROOT/build-release"

usage() {
  cat <<'USAGE'
usage: scripts/release.sh [--version vX.Y.Z] [--only host|linux|windows] [--out DIR] [--build DIR]
  --version  версия пакета; без неё берётся тег НА HEAD, и его отсутствие — отказ, а не "0.0.0"
  --only     что паковать; по умолчанию хост. linux и windows пока отказывают со словами
  --out      куда класть пакеты (умолчание release/, он в .gitignore)
  --build    каталог сборки, СВОЙ у релиза (умолчание build-release/): конфигурация здесь
             релизная, и чужой каталог она бы переписала
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
    --build) need_value $# "$1"; BUILD="$2"; shift 2 ;;
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
# Код 3 — «эта платформа собирается не здесь», отдельный от кода 2 («ошибка употребления»): первый
# ждёт оркестратор, чтобы отличить незакрытую вертикаль от опечатки в своей же команде.
if [ "$ONLY" != "$HOST" ]; then
  case "$ONLY" in
    linux) echo "release: пакет Linux собирается в контейнере (вертикаль 2 спеки #20), не хостом $HOST" >&2 ;;
    windows) echo "release: пакет Windows собирается задачей CI (.github/workflows/release.yml) и забирается gh run download, не хостом $HOST" >&2 ;;
    *) echo "release: пакет $ONLY собирается не хостом $HOST" >&2 ;;
  esac
  exit 3
fi

if [ -z "$VERSION" ]; then
  VERSION=$(git -C "$ROOT" describe --tags --exact-match HEAD 2>/dev/null || true)
fi
# Версии нет — отказ. Подстановка "0.0.0" дала бы пакет, чей штамп не отличает релиз от случайного
# состояния дерева, а узнаётся это только у того, кто его уже скачал.
if [ -z "$VERSION" ]; then
  echo "release: на HEAD нет тега и версия не названа — укажи --version vX.Y.Z" >&2
  exit 2
fi
if ! printf '%s' "$VERSION" | grep -Eq '^v[0-9]+\.[0-9]+\.[0-9]+([-.][A-Za-z0-9.]+)?$'; then
  echo "release: версия '$VERSION' не похожа на тег vX.Y.Z" >&2
  exit 2
fi

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
  echo "release: каталог $BUILD сконфигурирован не релизом — назови свой через --build" >&2
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

SUM=$(sha256_of "$PKG")
# SHA256SUMS пересчитывается по ФАКТИЧЕСКОМУ содержимому каталога версии, а не дописывается
# строкой этого прогона: пакеты трёх ОС кладутся в один каталог разными запусками (--only), и файл,
# который перезаписывается одной строкой, оставлял бы релиз с суммой последней собранной трети.
# Пересчёт заодно чинит протухшее: удалённый пакет исчезает из файла сам.
( cd "$DEST" && find . -maxdepth 1 -name '*.tar.gz' | sed 's|^\./||' | LC_ALL=C sort \
  | while read -r f; do printf '%s  %s\n' "$(sha256_of "$f")" "$f"; done > SHA256SUMS )

FILES=$(wc -l < "$DEST/$NAME.manifest" | tr -d ' ')
SIZE=$(wc -c < "$PKG" | tr -d ' ')
# Заголовки латиницей не из вкуса: `printf %-46s` считает БАЙТЫ, а кириллица в UTF-8 занимает по
# два на букву — таблица разъезжалась ровно относительно того образца, что записан в
# docs/owner-setup.txt как ожидаемый вывод.
printf '\n%-46s %10s %6s  %s\n' "package" "size" "files" "sha256"
printf '%-46s %10s %6s  %s\n' "$(basename "$PKG")" "$SIZE" "$FILES" "$SUM"
printf '\nвнутри: %s\n' "$(cd "$STAGE" && find . -type f | LC_ALL=C sort | sed 's|^\./||' | tr '\n' ' ')"
printf 'манифест: %s\nсуммы:    %s\n' "$DEST/$NAME.manifest" "$DEST/SHA256SUMS"
