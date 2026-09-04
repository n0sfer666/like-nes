#!/usr/bin/env bash
# Пакет Linux в контейнере (спека #20, вертикаль 2, решение 1: «Linux — в контейнере на той же
# машине»). Контейнер здесь даёт линукс-ХОСТ, внутри которого работает тот же scripts/release.sh:
# упаковщик, состав и нормализация архива остаются одни на все платформы.
set -euo pipefail

ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
# shellcheck source=scripts/release_lib.sh
. "$ROOT/scripts/release_lib.sh"
# shellcheck source=scripts/release_container_lib.sh
. "$ROOT/scripts/release_container_lib.sh"

DOCKERFILE="$ROOT/scripts/release_linux.Dockerfile"
VERSION=""
OUT="$ROOT/release"
PLATFORM=""
CACHE=""
FRESH=""

usage() {
  cat <<'USAGE'
usage: scripts/release_container.sh [--version vX.Y.Z] [--out DIR] [--platform linux/ARCH] [--build DIR] [--fresh]
  зовётся обычно не напрямую, а через scripts/release.sh --only linux
USAGE
}

need_value() {
  [ "$1" -ge 2 ] || { echo "release: $2 требует значения" >&2; usage >&2; exit 2; }
}

while [ $# -gt 0 ]; do
  case "$1" in
    --version) need_value $# "$1"; VERSION="$2"; shift 2 ;;
    --out) need_value $# "$1"; OUT="$2"; shift 2 ;;
    --platform) need_value $# "$1"; PLATFORM="$2"; shift 2 ;;
    --build) need_value $# "$1"; CACHE="$2"; shift 2 ;;
    --fresh) FRESH=1; shift ;;
    -h|--help) usage; exit 0 ;;
    *) echo "release: непонятый аргумент: $1" >&2; usage >&2; exit 2 ;;
  esac
done

VERSION=$(resolve_version "$ROOT" "$VERSION") || exit 2

case "$(uname -m)" in
  arm64|aarch64) NATIVE=linux/arm64 ;;
  x86_64|amd64) NATIVE=linux/amd64 ;;
  *) echo "release: незнакомая архитектура $(uname -m) — платформу контейнера назови через --platform" >&2; exit 2 ;;
esac
[ -n "$PLATFORM" ] || PLATFORM="$NATIVE"
case "$PLATFORM" in
  linux/arm64) ARCH=aarch64 ;;
  linux/amd64) ARCH=x86_64 ;;
  *) echo "release: --platform принимает linux/arm64 или linux/amd64, получено: $PLATFORM" >&2; exit 2 ;;
esac
# Чужая архитектура идёт через эмуляцию и стоит часы. Сказать об этом ДО сборки — не вежливость:
# молчаливый прогон, застрявший на пятнадцатой минуте apt-get, выглядит как зависший, и его
# убивают ровно тогда, когда он работает как задумано.
if [ "$PLATFORM" != "$NATIVE" ]; then
  printf 'release: ВНИМАНИЕ платформа %s не родная этой машине (%s) — сборка идёт через эмуляцию и займёт часы\n' \
    "$PLATFORM" "$NATIVE" >&2
fi

# Код 4 — «машина не готова», отдельный и от 2 (опечатка), и от 3 (эта платформа собирается не
# здесь): у него единственный смысл — доустанови движок и повтори ту же команду.
ENGINE=$(container_engine) || { container_engine_hint; exit 4; }
IMAGE=$(container_image_tag "$DOCKERFILE" "$ARCH")

[ -n "$CACHE" ] || CACHE=$(container_build_dir "$ARCH")
if [ -n "$FRESH" ]; then rm -rf "$CACHE"; fi
# Недоконфигурированный кеш сносит ТОТ, кто им владеет. Хостовый release.sh на такой каталог
# отказывает и правильно делает — он не знает, чей это каталог; здесь каталог создан этим же
# скриптом, и снос его громкий, а не молчаливый: пропавшие полчаса сборки обязаны быть названы.
if container_cache_broken "$CACHE"; then
  printf 'release: кеш %s остался от упавшего прогона (конфигурация не дошла до конца) — удаляю\n' \
    "$CACHE" >&2
  rm -rf "$CACHE"
fi
mkdir -p "$CACHE" "$OUT"

echo "release: контейнер $ENGINE, платформа $PLATFORM, образ $IMAGE"
echo "release: база $(container_base_pin "$DOCKERFILE")"
# Контекст сборки образа — каталог scripts/, а не корень дерева: корень несёт сборочные каталоги и
# ассеты на сотни мегабайт, и движок отправил бы их демону целиком на каждом прогоне.
"$ENGINE" build --platform "$PLATFORM" -t "$IMAGE" -f "$DOCKERFILE" "$ROOT/scripts"

# Репозиторий монтируется ТОЛЬКО ДЛЯ ЧТЕНИЯ: гейт 10 требует чистого `git status` после релиза, а
# сборка, которой позволено писать в /src, оставляет там и каталог, и файлы с чужим владельцем.
# `--user` — по той же причине с другой стороны: без него пакет в release/ достаётся root'у, и
# следующий прогон БЕЗ sudo не может его перезаписать.
#
# safe.directory приходит окружением, а не `git config`: писать в конфиг образа значило бы менять
# его состояние, а образ обязан быть одинаковым у всех прогонов — его тег назван дайджестом базы.
# Значение `*`, а не `/src`: git проверяет владельца КАЖДОГО репозитория, до которого дотянулся, а
# FetchContent разворачивает свои клоны в /build уже во время сборки — первый живой прогон умер
# именно там, на `dubious ownership in repository at /build/_deps/webgpu-backend-wgpu-src`, хотя
# монтирование дерева было разрешено поимённо. Перечислить их заранее нечем: пути придумывает
# CMake, а контейнер одноразовый и внутри него нет чужих репозиториев, от которых это защищает.
"$ENGINE" run --rm --platform "$PLATFORM" \
  --user "$(id -u):$(id -g)" \
  -e HOME=/tmp \
  -e GIT_CONFIG_COUNT=1 -e GIT_CONFIG_KEY_0=safe.directory -e 'GIT_CONFIG_VALUE_0=*' \
  -v "$ROOT:/src:ro" -v "$OUT:/out" -v "$CACHE:/build" \
  "$IMAGE" \
  bash /src/scripts/release.sh --version "$VERSION" --out /out --build /build

# Утверждение про ИСХОД, а не про код возврата движка: `run` возвращает код процесса внутри, и он
# нулевой у прогона, который собрал всё и не смог записать пакет наружу (права на монтировании,
# полный диск). Пустой каталог релиза при нулевом коде — ровно то, что читается как успех.
PKG=$(ls "$OUT/$VERSION/like-nes-engine-$VERSION-linux-$ARCH.tar.gz" 2>/dev/null || true)
if [ -z "$PKG" ]; then
  echo "release: контейнер отработал, но пакета linux-$ARCH в $OUT/$VERSION нет" >&2
  exit 1
fi
printf 'release: пакет Linux собран: %s\n' "$PKG"

# Второй продукт линукс-хоста — AppImage (шаг B вертикали 4), и утверждается он ОТДЕЛЬНО и по тому
# же основанию, что архив: appimagetool отказывает КОДОМ НОЛЬ (без иконки он просто не создаёт
# файл), поэтому прогон без образа выглядит успешным до самого момента, когда пакет ищет владелец.
IMG="$OUT/$VERSION/like-nes-engine-$VERSION-linux-$ARCH.AppImage"
if [ ! -f "$IMG" ]; then
  echo "release: контейнер собрал архив, но .AppImage в $OUT/$VERSION нет" >&2
  exit 1
fi
printf 'release: образ AppImage собран: %s\n' "$IMG"
