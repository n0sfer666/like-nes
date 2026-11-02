# shellcheck shell=bash
# Поставленный native-пад линкуется отдельно от окна: iOS не может тащить GLFW.
fixture_gamepad_source() {
    local plat=$1 pfx=$2 dir="$WORK/gamepad-$1" expect
    shift 2
    mkdir -p "$dir"
    cat > "$dir/CMakeLists.txt" <<'CMAKE'
cmake_minimum_required(VERSION 3.21)
project(gamepad_only CXX)
find_package(like-nes 0.1 REQUIRED CONFIG)
include("${GAMEPAD_CLOSURE}")
no_window(like-nes::input_gamepad)
no_window(like-nes::engine)
add_executable(gamepad_only MACOSX_BUNDLE main.cpp)
target_link_libraries(gamepad_only PRIVATE like-nes::engine like-nes::input_gamepad)
if(COMMAND like_nes_add_game)
  like_nes_add_game(gamepad_only)
endif()
CMAKE
    cat > "$dir/main.cpp" <<'CPP'
#include <cstdio>
#include <vector>
#include "source.hpp"
#include "haptic.hpp"
#include "save.hpp"
#include "save_format.hpp"
#include "save_backend.hpp"
#include "save_slots.hpp"
#include "platform_fs.hpp"
#include "platform_save.hpp"
int main() {
    auto* pad = input::make_gamepad_source();
    if (pad == nullptr || !pad->init()) return 1;
    input::ActionMap map;
    input::InputEngine engine(map);
    pad->poll(engine);
    pad->set_rumble(-1, 0, 0, 0);
    haptic::init();
    haptic::pulse({haptic::Kind::Phone}, 0, 0);
    haptic::suspend(); haptic::resume(); haptic::shutdown();
    const uint8_t bytes[]{1, 0, 255};
    std::vector<uint8_t> container;
    if (!save::encode_record(1, 1, bytes, container) || !save::write_atomic("sdk-atomic.bin", container)) return 1;
    std::vector<uint8_t> saved;
    save::RecordView record;
    const bool exact = platform::read_bytes("sdk-atomic.bin", saved) && saved == container &&
        save::decode_record(saved, record) == save::RecordResult::Ok && record.version == 1 && record.writes == 1;
    platform::remove_file("sdk-atomic.bin");
    if (!exact) return 1;
    save::LocalBackend backend(platform::exe_dir() + "/save-backend-fixture");
    if (!backend.write("progress", container) || backend.move_new("progress", "progress.v1.0.pre") != save::MoveResult::Moved)
        return 1;
    if (backend.read("progress.v1.0.pre", saved, container.size()) != save::ReadResult::Ok || saved != container) return 1;
    const std::vector<std::vector<uint8_t>> copies{container, saved};
    if (save::select_record(copies).result != save::SelectionResult::Selected) return 1;
    const std::vector<uint8_t> body{1, 0, 255};
    if (!save::store(backend, "checkpoint", 1, body) ||
        save::load(backend, "checkpoint", 1, {}, saved) != save::Status::Ok || saved != body) return 1;
    if (!platform::set_save_dir(backend.root()) || !save::init("sdk-fixture") || save::dir() != backend.root() ||
        !save::store("global", 1, body) || save::load("global", 1, {}, saved) != save::Status::Ok || saved != body) return 1;
    const std::vector<std::string> legacy{"controls.txt"};
    if (!save::migrate_legacy(backend.root(), legacy) || save::file_path("controls.txt").empty()) return 1;
    if (pad->pad_info(-1).vid != 0) return 1;
    std::printf("gamepad-only: %s\n", pad->backend_name());
    return 0;
}
CPP
    if ! { cmake -S "$(native "$dir")" -B "$(native "$dir/build")" -G "$GEN" -DCMAKE_BUILD_TYPE=Release \
        -Dlike-nes_DIR="$(native "$pfx")/lib/cmake/like-nes" \
        -DGAMEPAD_CLOSURE="$(native "$ROOT/scripts/gamepad_closure.cmake")" "$@" && cmake --build "$(native "$dir/build")"; } \
        > "$dir/build.log" 2>&1; then
        tail -20 "$dir/build.log"; sdk_bad "$plat: gamepad without GLFW did not link from prefix"; return 1
    fi
    if [ "$plat" = desktop ]; then
        local exe="$dir/build/gamepad_only"
        case "$(uname -s)" in
            Darwin) exe="$exe.app/Contents/MacOS/gamepad_only" ;;
            MINGW* | MSYS* | CYGWIN*) exe="$exe.exe" ;;
        esac
        expect=$(sdk_pad_backend)
        (cd "$dir" && "$exe") > "$dir/run.log" 2>&1 && grep -Fq "gamepad-only: $expect" "$dir/run.log" || {
            cat "$dir/run.log"; sdk_bad 'desktop: gamepad-only backend failed'; return 1; }
    fi
    sdk_ok "$plat: native gamepad linked from prefix without GLFW"
}
