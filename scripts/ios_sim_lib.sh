# shellcheck shell=bash
# Устройство iOS-симулятора — общее у гейта (`ios_sim_gate.sh`) и запуска игры руками
# (`ios_sim_run.sh`). Устройство ищется по имени и создаётся, только если его нет: `simctl create`
# имя не проверяет и на каждом повторе заводит двойника, после чего команды по имени выбирают
# одного из двух наугад.

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
