# shellcheck shell=bash
# Устройство iOS-симулятора — общее у гейта (`ios_sim_gate.sh`) и запуска игры руками
# (`ios_sim_run.sh`). Устройство ищется по имени и создаётся, только если его нет: `simctl create`
# имя не проверяет и на каждом повторе заводит двойника, после чего команды по имени выбирают
# одного из двух наугад.

# shellcheck source=scripts/mobile_resume_lib.sh
. "$ROOT/scripts/mobile_resume_lib.sh"

# Причина, по которой симулятора на этой машине нет; пусто — есть.
ios_sim_missing() {
  [ "$(uname -s)" = Darwin ] || { echo "the iOS simulator exists only on macOS"; return 0; }
  [ "$(uname -m)" = arm64 ] || { echo "the shell is built for the arm64 simulator, this Mac is $(uname -m)"; return 0; }
  xcodebuild -version >/dev/null 2>&1 || echo "xcodebuild missing (install Xcode, then xcode-select -s)"
}

# Последний доступный рантайм iOS; пусто — рантайма нет, код ошибки — simctl не отвечает.
ios_runtime() {
  xcrun simctl list runtimes iOS | awk '/^iOS / && !/unavailable/ {id=$NF} END {print id}'
}

ios_device() {
  local udid
  udid=$(xcrun simctl list devices available | sed -nE "s/^ +$1 \\(([0-9A-F-]{36})\\).*/\\1/p" | awk 'NR == 1') \
    || return 1
  [ -n "$udid" ] || udid=$(xcrun simctl create "$1" com.apple.CoreSimulator.SimDeviceType.iPhone-16 "$2") \
    || return 1
  echo "$udid"
}

# Запуск с stderr в файл и ожидание строки «iOS shell up» (`view.mm`): с ней появляется подписка на
# уход в фон, и уйти в фон раньше — значит судить игру, которая ещё не слушает. Холодный первый
# старт бывает дольше любой паузы. Отказ — код 1 и причина в IOS_SAID.
ios_launch_up() {
  local udid=$1 app=$2 err=$3
  : >"$err"
  xcrun simctl launch --stderr="$err" "$udid" "$app" >/dev/null || { IOS_SAID="cannot launch $app"; return 1; }
  for _ in $(seq 60); do
    grep -q "iOS shell up" "$err" && return 0
    sleep 1
  done
  IOS_SAID="the game did not come up in 60 s — $err"
  return 1
}

# Возврат из фона: запуск уже живого приложения выводит его вперёд, а не стартует заново. Зовётся после
# detach и до запуска: последний attach в логе — тот, с которого ушли в фон. Суд улик —
# `resume_judged` (`mobile_resume_lib.sh`). Успех — IOS_SAID с уликой и IOS_DETACHED=`tick=T world=H`,
# отказ — код 1 и причина в IOS_SAID.
ios_resumed() {
  local udid=$1 app=$2 err=$3 rc=0 n
  n=$(last_attach "$err")
  xcrun simctl launch "$udid" "$app" >/dev/null || { IOS_SAID="cannot bring $app back to foreground"; return 1; }
  for _ in $(seq 30); do
    grep -q "surface attached #$((n + 1)) " "$err" && break
    sleep 1
  done
  resume_judged "$err" "$n" || rc=1
  IOS_SAID=$RESUME_SAID
  IOS_DETACHED=$RESUME_DETACHED
  return "$rc"
}
