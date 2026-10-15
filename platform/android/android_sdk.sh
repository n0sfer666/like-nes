# Подключается из build_apk.sh, package_apk.sh и scripts/mobile_bench.sh: SDK выбирается в одном
# месте, иначе на Linux без ANDROID_HOME сборка шла бы из ~/Android/Sdk, а adb искался бы по
# маковскому пути. NDK здесь не выбирается: упаковке он не нужен, а игра против SDK собрана тем
# NDK, что дал ей тулчейн, — его сверяет like-nesConfig.cmake, а не окружение.
# shellcheck shell=bash
case "$(uname -s):$(uname -m)" in
  Darwin:*) SDK_DEFAULT="$HOME/Library/Android/sdk" ;;
  Linux:x86_64) SDK_DEFAULT="$HOME/Android/Sdk" ;;
  *) echo "android_sdk: no Android SDK host for $(uname -s) $(uname -m)" >&2; exit 1 ;;
esac
# SC2034: SDK читают подключающие скрипты, а shellcheck видит один файл, а не связку.
# shellcheck disable=SC2034
SDK="${ANDROID_HOME:-$SDK_DEFAULT}"
