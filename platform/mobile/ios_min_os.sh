# shellcheck shell=bash
# Минимальный iOS приложения записан в трёх местах: `minos` бинаря (флаг clang), MinimumOSVersion
# в Info.plist и кеш CMake (CMAKE_OSX_DEPLOYMENT_TARGET корня — источник обоих). Разойтись им уже
# случалось: plist обещал 14.0, бинарь требовал 27.0, и xcodebuild переписывал plist сам (с
# предупреждением в логе), а сборка Ninja — нет. Версии сравниваются как «мажор.минор»: `17` из
# командной строки и `17.0` у vtool — одно число. Печатает версию, если все три согласны; иначе
# причина в stderr и код 1.
ios_min_os_norm() {
  awk -F. 'NF && $1 != "" { printf "%d.%d\n", $1, $2 }'
}

ios_min_os_agree() {
  local app="$1" build="$2" want minos plist
  want=$(sed -n 's/^CMAKE_OSX_DEPLOYMENT_TARGET:[A-Z]*=//p' "$build/CMakeCache.txt" | ios_min_os_norm)
  minos=$(vtool -show-build "$app/like_nes_ios" | awk '$1 == "minos" { print $2; exit }' | ios_min_os_norm)
  plist=$(/usr/libexec/PlistBuddy -c "Print :MinimumOSVersion" "$app/Info.plist" | ios_min_os_norm)
  if [ -n "$want" ] && [ "$minos" = "$want" ] && [ "$plist" = "$want" ]; then
    echo "$want"
    return 0
  fi
  echo "minimum iOS disagrees in $app: CMake cache '$want', binary minos '$minos', Info.plist '$plist'" >&2
  return 1
}
