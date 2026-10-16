#!/usr/bin/env bash
# Упаковка APK из собранной .so (спека #26, В1). Путь один у дерева и у игры против SDK: его зовёт
# цель `<цель>_apk` функции like_nes_mobile_app — из platform/android/ дерева или из
# share/like-nes/platform/android/ префикса. Ассеты — в корень assets/: оболочка распаковывает их
# оттуда в песочницу; лицензии — в assets/licenses/.
set -euo pipefail

HERE="$(cd "$(dirname "$0")" && pwd)"
# shellcheck source=platform/android/android_sdk.sh
. "$HERE/android_sdk.sh"
BT="$SDK/build-tools/35.0.0"
ANDROID_JAR="$SDK/platforms/android-35/android.jar"
usage() {
  echo "usage: bash package_apk.sh --abi <abi> --manifest <xml> --lib <so>... [--license <file>]... --out <apk> -- [<asset>...]" >&2
  exit 1
}
ABI=
MANIFEST=
OUT=
LIBS=()
LICENSES=()
while [ "$#" -gt 0 ]; do
  case "$1" in
    --) shift; break ;;
    --abi | --manifest | --lib | --license | --out)
      [ "$#" -ge 2 ] || usage
      case "$1" in
        --abi) ABI=$2 ;;
        --manifest) MANIFEST=$2 ;;
        --lib) LIBS+=("$2") ;;
        --license) LICENSES+=("$2") ;;
        --out) OUT=$2 ;;
      esac
      shift 2 ;;
    *) usage ;;
  esac
done
[ -n "$ABI" ] && [ -n "$MANIFEST" ] && [ -n "$OUT" ] && [ "${#LIBS[@]}" -gt 0 ] || usage
# Пустой аргумент — пустой список ассетов из генераторного выражения CMake, а не файл.
for f in "$MANIFEST" "${LIBS[@]}" ${LICENSES[@]+"${LICENSES[@]}"} "$@"; do
  [ -z "$f" ] || [ -f "$f" ] || { echo "package_apk: no such file: $f" >&2; exit 1; }
done

STAGE="$OUT.stage"
rm -rf "$STAGE"
mkdir -p "$STAGE/lib/$ABI" "$STAGE/assets/licenses"
cp "${LIBS[@]}" "$STAGE/lib/$ABI/"
for f in "$@"; do
  [ -z "$f" ] || cp "$f" "$STAGE/assets/"
done
[ "${#LICENSES[@]}" -eq 0 ] || cp "${LICENSES[@]}" "$STAGE/assets/licenses/"

"$BT/aapt2" link -o "$STAGE/base.apk" -I "$ANDROID_JAR" \
  --manifest "$MANIFEST" --target-sdk-version 35
( cd "$STAGE" && zip -q -r base.apk lib assets )
"$BT/zipalign" -f 4 "$STAGE/base.apk" "$OUT"

# Два приложения одной сборки под -j оба видят, что ключа нет: каждое создаёт свой рядом, а на
# место встаёт первый — `ln` не перезаписывает, проигравший берёт ключ победителя.
KS="$HOME/.android/debug.keystore"
if [ ! -f "$KS" ]; then
  mkdir -p "$(dirname "$KS")"
  KS_NEW="$KS.$$"
  keytool -genkeypair -keystore "$KS_NEW" -storepass android -keypass android \
    -alias androiddebugkey -dname "CN=Android Debug,O=Android,C=US" \
    -keyalg RSA -keysize 2048 -validity 10000
  ln "$KS_NEW" "$KS" 2>/dev/null || true
  rm -f "$KS_NEW"
fi
"$BT/apksigner" sign --ks "$KS" --ks-pass pass:android --key-pass pass:android \
  --ks-key-alias androiddebugkey "$OUT"
echo "APK ready: $OUT"
