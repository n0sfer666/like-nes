#!/usr/bin/env bash
# Пекарь Neon Rumble для мобильной сборки игры (спека #29, В2а). Мобильная сборка не может запустить
# то, что собирает под телефон, а базовый assetc префикса вида `fighter` не знает: пекарь собирается
# отдельным проектом хоста против ДЕСКТОПНОГО префикса, и его путь уходит в -DRUMBLE_ASSETC. Вывод
# конфигурирования и сборки — в <dir>/rumble_tool.log, в stdout — только абсолютный путь бинаря.
#
#   bash scripts/rumble_tool.sh <desktop-prefix> <dir>
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"

fail() { echo "[rumble-tool] FAIL: $*" >&2; exit 1; }

# Мобильной сборки на Windows нет, а нативный cmake не понял бы путь git-bash и бинарь без .exe.
case "$(uname -s)" in Darwin | Linux) ;; *) fail "mobile builds only: macOS and Linux, not $(uname -s)" ;; esac
[ "$#" -eq 2 ] || { echo "usage: bash scripts/rumble_tool.sh <desktop-prefix> <dir>" >&2; exit 2; }
# Абсолютный: like-nes_DIR CMake считает от каталога сборки, и относительный префикс там не найдётся.
PFX=$(CDPATH='' cd -- "$1" 2>/dev/null && pwd) || fail "no directory $1"
DIR=$2
while [ "${DIR%/}" != "$DIR" ]; do DIR=${DIR%/}; done
[ -n "$DIR" ] || fail "<dir> '$2' is empty or the root"
CFG="$PFX/lib/cmake/like-nes"
[ -f "$CFG/like-nesConfig.cmake" ] || fail "no like-nes prefix at $PFX (run bash scripts/check_sdk_game.sh --keep)"
[ -f "$CFG/sdk_config_desktop.cmake" ] || fail "$PFX is not a desktop prefix (no sdk_config_desktop.cmake)"

mkdir -p "$DIR"
LOG="$DIR/rumble_tool.log"
# --fresh: каталог переживает прогон, а кеш с найденным вчера префиксом собрал бы пекарь вчерашнего.
{
  cmake --fresh -S "$ROOT/games/neon-rumble/tools" -B "$DIR" -G Ninja -DCMAKE_BUILD_TYPE=Release \
    -Dlike-nes_DIR="$CFG" && cmake --build "$DIR"
} >"$LOG" 2>&1 || { tail -20 "$LOG" >&2; fail "rumble_assetc did not build against $PFX — $LOG"; }
BIN="$(CDPATH='' cd -- "$DIR" && pwd)/rumble_assetc"
[ -x "$BIN" ] || fail "no $BIN after the build — $LOG"
printf '%s\n' "$BIN"
