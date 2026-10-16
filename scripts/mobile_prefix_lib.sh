# shellcheck shell=bash
# Чтения, общие у мобильной половины sdk-game и у запусков игры на симуляторе и телефоне
# (`ios_sim_rumble.sh`, `android_rumble.sh`). Вызывающий задаёт ROOT.

# Уровень API Android — minSdkVersion шаблона манифеста всех приложений; пусто — его нет.
android_min_api() {
  sed -n 's/.*android:minSdkVersion="\([0-9]*\)".*/\1/p' "$ROOT/platform/android/AndroidManifest.xml.in" | head -1
}

# Значение `set(<имя> "…")` из Config поставленного префикса; пусто — строки нет.
prefix_field() {
  sed -n "s/^set($2 \"\\(.*\\)\")\$/\\1/p" "$1/lib/cmake/like-nes/like-nesConfig.cmake"
}
