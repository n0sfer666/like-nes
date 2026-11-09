# shellcheck shell=bash
# Установленный SDK экспортирует устройство отдельно от like-nes::engine.

audio_fixture_project() {
    local dir=$1
    mkdir -p "$dir"
    cp "$ROOT/tests/audio/audio_music_device_test.cpp" "$dir/audio_music_device_test.cpp" || return 1
    cat > "$dir/CMakeLists.txt" <<'CMAKE'
cmake_minimum_required(VERSION 3.21)
project(audio_output_only LANGUAGES CXX)
find_package(like-nes 0.1 REQUIRED CONFIG)
if(NOT TARGET like-nes::audio_device)
  message(FATAL_ERROR "installed SDK has no optional like-nes::audio_device target")
endif()
if(NOT AUDIO_DEVICE_FIXTURE)
  message(FATAL_ERROR "audio fixture path is required")
endif()
get_target_property(engine_links like-nes::engine INTERFACE_LINK_LIBRARIES)
if(like-nes::audio_device IN_LIST engine_links)
  message(FATAL_ERROR "audio device must stay separate from like-nes::engine")
endif()
add_executable(audio_output_only audio_music_device_test.cpp)
target_compile_definitions(audio_output_only PRIVATE "AUDIO_DEVICE_FIXTURE=\"${AUDIO_DEVICE_FIXTURE}\"")
target_link_libraries(audio_output_only PRIVATE like-nes::engine like-nes::audio_device)
if(COMMAND like_nes_add_game)
  like_nes_add_game(audio_output_only)
endif()
CMAKE
}

fixture_audio_off_configure() {
    local dir="$WORK/audio-off" cache="$ROOT/build-sdk-release/CMakeCache.txt" line key value base
    local fetch_flags=() fetch_keys=() src name normalized
    [ -f "$cache" ] || { sdk_bad "audio OFF: missing configured source cache $cache"; return 1; }
    base=$(sed -n 's/^FETCHCONTENT_BASE_DIR:PATH=//p' "$cache")
    while IFS= read -r line; do
        key=${line%%:*}; value=${line#*=}
        if [ -d "$value" ]; then fetch_flags+=("-D$key=$value"); fetch_keys+=("$key"); fi
    done < <(grep -E '^FETCHCONTENT_SOURCE_DIR_[A-Za-z0-9_-]+:[^=]+=' "$cache")
    for src in "$base"/*-src; do
        [ -d "$src" ] || continue
        name=${src##*/}; name=${name%-src}; normalized=$(printf '%s' "$name" | tr '[:lower:]' '[:upper:]')
        key="FETCHCONTENT_SOURCE_DIR_$normalized"
        case " ${fetch_keys[*]-} " in *" $key "*) continue ;; esac
        fetch_flags+=("-D$key=$src")
    done
    cmake -S "$(native "$ROOT")" -B "$(native "$dir")" -G "$GEN" -DCMAKE_BUILD_TYPE=Release \
        -DAUDIO_MINIAUDIO=OFF -DPLUGIN_UI=OFF -DPLUGIN_WASM=OFF -DIDE_POC=OFF \
        -DFETCHCONTENT_FULLY_DISCONNECTED=ON ${fetch_flags[@]+"${fetch_flags[@]}"} > "$WORK/audio-off-configure.log" 2>&1 || {
        tail -30 "$WORK/audio-off-configure.log"; sdk_bad 'audio OFF: configure-only fixture failed'; return 1; }
    if grep -Eq '(^| )audio_device(:|$)' "$dir/build.ninja" ||
       grep -Eq 'audio/device\.hpp|libaudio_device' "$dir/cmake_install.cmake" ||
       grep -Eq 'audio_device' "$dir/sdk/like-nesConfig.cmake"; then
        sdk_bad 'audio OFF: SDK generated an optional header, target or archive rule'; return 1
    fi
    sdk_ok 'audio OFF: configure-only SDK omits device header, target and archive install rule'
}

fixture_audio_output() {
    local dir="$WORK/audio-output" ogg="$WORK/audio-output.ogg" cfg log exe
    cp "$ROOT/tests/audio/fixtures/stereo-48000.ogg" "$ogg" || return 1
    audio_fixture_project "$dir" || { sdk_bad 'audio: cannot prepare public SDK fixture'; return 1; }
    for cfg in Release Debug; do
        log="$dir/$cfg.log"
        if ! cmake -S "$(native "$dir")" -B "$(native "$dir/$cfg")" -G "$GEN" -DCMAKE_BUILD_TYPE="$cfg" \
            -DAUDIO_DEVICE_FIXTURE="$(native "$ogg")" -DCMAKE_PREFIX_PATH="$(native "$PREFIX")" > "$log" 2>&1 ||
            ! cmake --build "$(native "$dir/$cfg")" >> "$log" 2>&1; then
            tail -30 "$log"; sdk_bad "audio: $cfg installed output fixture failed to build"; return 1
        fi
        exe="$(native "$dir/$cfg/audio_output_only")"
        if ! "$exe" >> "$log" 2>&1 ||
            ! grep -qxF 'audio-music-device: PASS real null callbacks, stream shutdown and source lifetime' "$log"; then
            tail -30 "$log"; sdk_bad "audio: $cfg installed output fixture failed"; return 1
        fi
    done
    sdk_ok 'audio: installed optional device target links and runs in Release/Debug'
    fixture_audio_off_configure
}

fixture_mobile_audio_output() {
    local plat=$1 prefix=$2 dir="$WORK/audio-output-$1" ogg="$WORK/audio-output-$1.ogg" log
    shift 2
    cp "$ROOT/tests/audio/fixtures/stereo-48000.ogg" "$ogg" || return 1
    audio_fixture_project "$dir" || { sdk_bad "$plat audio: cannot prepare public fixture"; return 1; }
    if ! cmake -S "$(native "$dir")" -B "$(native "$dir/build")" -G "$GEN" -DCMAKE_BUILD_TYPE=Release "$@" \
        -DAUDIO_DEVICE_FIXTURE="$(native "$ogg")" -Dlike-nes_DIR="$(native "$prefix/lib/cmake/like-nes")" > "$dir.log" 2>&1 ||
        ! cmake --build "$(native "$dir/build")" >> "$dir.log" 2>&1; then
        tail -30 "$dir.log"; sdk_bad "$plat audio: installed device target failed to compile/link"; return 1
    fi
    sdk_ok "$plat audio: installed device archive compiled and linked (cross target not run)"
}
