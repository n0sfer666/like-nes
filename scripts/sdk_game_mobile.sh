# shellcheck shell=bash
# Мобильная половина гейта sdk-game (спека #26, В1д): Neon Rumble собирается против МОБИЛЬНОГО
# префикса SDK, а не против дерева. iOS-симулятор — на macOS, Android x86_64 — на Linux и на macOS с
# пиннутым NDK. Префикс ставится в пустой каталог из своей сборки (build-sdk-ios-sim,
# build-sdk-android-x86_64), игра собирается через like-nes_DIR и печётся assetc десктопного
# префикса, а пакет судится по составу: бинарь своей платформы, оба бандла, все лицензии префикса.
# game.bundle пакета сравнивается побайтно с бандлом десктопной игры: один манифест и один assetc.
# Вызывающий задаёт ROOT, WORK, OUT, PREFIX (десктопный префикс) и GEN; помощники — sdk_game_lib.sh.

# shellcheck source=platform/mobile/fresh_cache.sh
. "$ROOT/platform/mobile/fresh_cache.sh"
# shellcheck source=scripts/mobile_prefix_lib.sh
. "$ROOT/scripts/mobile_prefix_lib.sh"

# Сборка и установка мобильного SDK в пустой префикс: `cmake --install` только добавляет файлы, и
# убранный из SDK заголовок пережил бы установку в старый.
mobile_sdk() {
    local plat=$1 dir=$2 pfx=$3 fresh
    shift 3
    fresh=$(fresh_flag "$dir" "$ROOT")
    cmake ${fresh:+"$fresh"} -S . -B "$dir" -G "$GEN" -DCMAKE_BUILD_TYPE=Release "$@" \
        > "$WORK/$plat-configure.log" 2>&1 || {
        tail -30 "$WORK/$plat-configure.log"; sdk_bad "$plat: SDK configure failed"; return 1; }
    cmake --build "$dir" --target like_nes_sdk > "$WORK/$plat-build.log" 2>&1 || {
        tail -30 "$WORK/$plat-build.log"; sdk_bad "$plat: SDK build failed"; return 1; }
    [ ! -e "$pfx" ] || mv "$pfx" "$WORK/stale-$plat" || {
        sdk_bad "$plat: cannot move the old prefix $pfx aside"; return 1; }
    cmake --install "$dir" --component sdk --prefix "$pfx" > "$WORK/$plat-install.log" 2>&1 || {
        tail -30 "$WORK/$plat-install.log"; sdk_bad "$plat: SDK install failed"; return 1; }
    sdk_ok "$plat: SDK built and installed into its own prefix"
}

mobile_game() {
    local src=$1 dir=$2 pfx=$3
    shift 3
    {
        cmake --fresh -S "$src" -B "$dir" -G "$GEN" -DCMAKE_BUILD_TYPE=Release "$@" \
            -Dlike-nes_DIR="$pfx/lib/cmake/like-nes" -DLIKE_NES_HOST_PREFIX="$PREFIX" &&
            cmake --build "$dir"
    } > "$dir.log" 2>&1
}

# Список лицензий берётся из Config префикса, и пустой или без лицензии движка и сводки чужих
# сделал бы суд пакета вакуумным: цикл по нулю имён печатал бы «carries the licenses».
mobile_licenses() {
    local plat=$1 list n
    list=$(prefix_field "$2" _ln_licenses | tr ';' ' ')
    for n in LICENSE THIRD-PARTY-NOTICES.txt; do
        case " $list " in
            *" $n "*) ;;
            *) sdk_bad "$plat: _ln_licenses of the prefix Config lacks $n: '$list'"; return 1 ;;
        esac
    done
    printf '%s' "$list"
}

# Бандл пакета против бандла десктопной игры: расхождение — чужой assetc или чужой манифест.
mobile_bundle_same() {
    local plat=$1 got=$2
    [ -f "$OUT/game-Release/game.bundle" ] || {
        sdk_bad "$plat: no desktop game.bundle in $OUT/game-Release to compare with"; return 1; }
    cmp -s "$got" "$OUT/game-Release/game.bundle" || {
        sdk_bad "$plat: game.bundle of the package differs from the desktop game's"; return 1; }
}

# Состав листинга `unzip -l` — case, а не `unzip | grep -q`: grep закрыл бы трубу раньше unzip.
apk_lacks() {
    local listing=$1 e missing=""
    shift
    for e in "$@"; do
        case "$listing" in *" $e"$'\n'* | *" $e") ;; *) missing="$missing $e" ;; esac
    done
    printf '%s' "$missing"
}

mobile_ios() {
    local pfx="$OUT/prefix-ios-sim" dir="$OUT/game-ios-sim" app min n lic
    local flags=(-DCMAKE_SYSTEM_NAME=iOS -DCMAKE_OSX_SYSROOT=iphonesimulator -DCMAKE_OSX_ARCHITECTURES=arm64)
    mobile_sdk ios-sim build-sdk-ios-sim "$pfx" "${flags[@]}" || return 1
    min=$(prefix_field "$pfx" _ln_ios_min)
    [ -n "$min" ] || { sdk_bad "ios-sim: no _ln_ios_min in the prefix Config"; return 1; }
    flags+=(-DCMAKE_OSX_DEPLOYMENT_TARGET="$min")
    mobile_game "$ROOT/games/neon-rumble" "$dir" "$pfx" "${flags[@]}" || {
        tail -30 "$dir.log"; sdk_bad "ios-sim: neon-rumble did not build against the prefix"; return 1; }
    app="$dir/neon_rumble.app"
    case "$(vtool -show-build "$app/neon_rumble" 2>&1)" in
        *"platform IOSSIMULATOR"*) ;;
        *) sdk_bad "ios-sim: $app/neon_rumble is not an iOS simulator binary"; return 1 ;;
    esac
    [ "$(plutil -extract MinimumOSVersion raw "$app/Info.plist" 2>&1)" = "$min" ] || {
        sdk_bad "ios-sim: Info.plist MinimumOSVersion is not $min"; return 1; }
    lic=$(mobile_licenses ios-sim "$pfx") || return 1
    for n in assets/game.bundle assets/library.bundle $lic; do
        case "$n" in assets/*) ;; *) n="licenses/$n" ;; esac
        [ -f "$app/$n" ] || { sdk_bad "ios-sim: neon_rumble.app has no $n"; return 1; }
    done
    mobile_bundle_same ios-sim "$app/assets/game.bundle" || return 1
    sdk_ok "ios-sim: neon_rumble.app (IOSSIMULATOR, iOS $min) carries both bundles and the licenses"
    fixture_mobile_tree_header ios-sim "$pfx" "${flags[@]}"
}

# NDK выбирается тем же скриптом, что у build_apk.sh, но в подоболочке: его отказ — `exit 1`.
android_ndk() {
    # shellcheck source=platform/android/android_sdk.sh
    # shellcheck source=platform/android/android_ndk.sh
    ( . "$ROOT/platform/android/android_sdk.sh" && . "$ROOT/platform/android/android_ndk.sh" && printf '%s' "$NDK" )
}

mobile_android() {
    local ndk=$1 abi=x86_64 pfx="$OUT/prefix-android-x86_64" dir="$OUT/game-android-x86_64" api apk listing lacks n lic
    api=$(android_min_api)
    [ -n "$api" ] || { sdk_bad "android: no minSdkVersion in platform/android/AndroidManifest.xml.in"; return 1; }
    local flags=(-DCMAKE_TOOLCHAIN_FILE="$ndk/build/cmake/android.toolchain.cmake"
        -DANDROID_ABI="$abi" -DANDROID_PLATFORM="android-$api")
    mobile_sdk "android-$abi" "build-sdk-android-$abi" "$pfx" "${flags[@]}" || return 1
    mobile_game "$ROOT/games/neon-rumble" "$dir" "$pfx" "${flags[@]}" || {
        tail -30 "$dir.log"; sdk_bad "android-$abi: neon-rumble did not build against the prefix"; return 1; }
    case "$(file "$dir/libneon_rumble.so")" in
        *ELF*x86-64*) ;;
        *) sdk_bad "android-$abi: libneon_rumble.so is not an x86-64 ELF"; return 1 ;;
    esac
    apk="$dir/neon_rumble.apk"
    listing=$(unzip -l "$apk") || { sdk_bad "android-$abi: cannot list $apk"; return 1; }
    local want=("lib/$abi/libneon_rumble.so" "lib/$abi/libc++_shared.so" assets/game.bundle assets/library.bundle)
    lic=$(mobile_licenses "android-$abi" "$pfx") || return 1
    for n in $lic; do want+=("assets/licenses/$n"); done
    lacks=$(apk_lacks "$listing" "${want[@]}")
    [ -z "$lacks" ] || { sdk_bad "android-$abi: neon_rumble.apk lacks$lacks"; return 1; }
    unzip -p "$apk" assets/game.bundle > "$WORK/apk-game.bundle" || {
        sdk_bad "android-$abi: cannot extract game.bundle from the APK"; return 1; }
    mobile_bundle_same "android-$abi" "$WORK/apk-game.bundle" || return 1
    sdk_ok "android-$abi: neon_rumble.apk (API $api) carries the .so, libc++_shared, both bundles and the licenses"
    fixture_mobile_tree_header "android-$abi" "$pfx" "${flags[@]}"
}

# Заголовок оболочки дерева, которого нет в префиксе: мобильная игра, сославшаяся на шутер, обязана
# упасть на нём, а не найти его по путям include, утёкшим из дерева в префикс.
fixture_mobile_tree_header() {
    local plat=$1 pfx=$2 src
    shift 2
    src=$(fixture_copy "tree-$plat")
    printf '#include "shooter_mobile.hpp"\n' >> "$src/src/rumble_mobile.cpp"
    if mobile_game "$src" "$WORK/fx-tree-$plat-build" "$pfx" "$@"; then
        sdk_bad "fixture: $plat game including shooter_mobile.hpp built against the prefix"; return 1
    fi
    grep -Eqi "shooter_mobile\.hpp.*(not found|no such file)" "$WORK/fx-tree-$plat-build.log" || {
        tail -20 "$WORK/fx-tree-$plat-build.log"
        sdk_bad "fixture: $plat tree-header include failed for another reason"; return 1; }
    sdk_ok "fixture: $plat prefix does not expose the tree's shooter_mobile.hpp"
}

# Обязательность Android задаёт SDK_GAME_ANDROID: `required` — нет NDK = FAIL (джоб Linux CI,
# единственная машина Android в CI), `skip` — не собирать (macOS CI: Android уже судит Linux, а
# образ раннера мог бы сменить NDK по умолчанию на пиннутый и молча включить сборку), пусто — по
# машине: при пиннутом NDK собрать, иначе строка SKIP с причиной. На Windows мобильной половины нет.
mobile_half() {
    local ndk fail=0
    case "$(uname -s)" in
        Darwin) mobile_ios || fail=1 ;;
        Linux) ;;
        *) printf 'sdk-game: SKIP mobile half (no mobile toolchain on %s)\n' "$(uname -s)"; return 0 ;;
    esac
    case "${SDK_GAME_ANDROID:-}" in
        skip) printf 'sdk-game: SKIP android-x86_64 (SDK_GAME_ANDROID=skip)\n'; return "$fail" ;;
        required | "") ;;
        *) sdk_bad "SDK_GAME_ANDROID is '$SDK_GAME_ANDROID', not required, skip or empty"; return 1 ;;
    esac
    if ndk=$(android_ndk 2> "$WORK/ndk.err"); then
        mobile_android "$ndk" || fail=1
    elif [ "${SDK_GAME_ANDROID:-}" = required ]; then
        cat "$WORK/ndk.err" >&2; sdk_bad "android: no pinned NDK and SDK_GAME_ANDROID=required"; fail=1
    else
        printf 'sdk-game: SKIP android-x86_64 (%s)\n' "$(tr -d '\n' < "$WORK/ndk.err")"
    fi
    return "$fail"
}
