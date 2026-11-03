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
#include "strings.hpp"
#include "strings_bake.hpp"
#include "music_bake.hpp"
#include "music_read.hpp"
#include "ui_input.hpp"
#include "ui_context.hpp"
#include "preset_bake.hpp"
#include "audio_engine.hpp"
#include "vorbis_reader.hpp"
#include "audio_stereo_ring.hpp"
int main() {
    audio::VorbisReader decoder; std::string decode_error;
    if (decoder.open({}, decode_error) || decode_error.empty()) return 1;
    const audio::StereoFrame pcm{800, -1600};
    audio::StereoRing ring; audio::StereoFrame streamed;
    if (ring.push(1, {&pcm, 1}) != 1 || ring.take(2, streamed) || streamed.left || streamed.right ||
        ring.push(2, {&pcm, 1}) != 1 || !ring.take(2, streamed) || streamed.left != 800 || streamed.right != -1600) return 1;
    audio::Mixer mixer(audio::Backend::Fix32);
    if (!mixer.register_stereo_source(1, &pcm, 1)) return 1;
    audio::AudioEngine sound(mixer); audio::PlayParams play;
    if (!sound.play(1, play, 0)) return 1;
    int16_t mixed[128]{}; mixer.mix(64, mixed);
    if (mixed[94] != 800 || mixed[95] != -1600) return 1;
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
    framework::music::MusicSrc song;
    song.name = "menu"; song.guid = 1; song.frames = 96000; song.fade_ms = 750;
    std::vector<uint8_t> music_bytes; std::string music_error;
    framework::music::MusicTable music;
    if (!framework::music::bake_music({&song, 1}, music_bytes, music_error) || !music.open(music_bytes.data(), music_bytes.size()) ||
        !music.find("menu") || music.find("menu")->loop_end != 96000 || music.find("menu")->fade_samples != 36000) return 1;
    framework::graphics::FontSrc font;
    font.name = "mono"; font.line_height = 1; font.page_w = font.page_h = 1;
    font.glyphs = {{'?', 0, 0, 1, 1}, {'A', 0, 0, 1, 1}};
    const framework::ui::StringSrc label{"label", "A", "A"};
    std::vector<uint8_t> strings; framework::ui::StringsStats stats; std::string error;
    if (!framework::ui::bake_strings({&label, 1}, font, true, strings, stats, error)) return 1;
    framework::ui::Strings table;
    if (!table.open(strings.data(), strings.size()) || table.find("label", framework::ui::Language::Ru) != "A") return 1;
    const char* ui = "preset|ui\naction|up|key:up\naction|down|key:down\naction|left|key:left\n"
                     "action|right|key:right\naction|accept|key:enter\naction|back|key:esc\naction|erase|key:backspace\n";
    framework::input::PresetBakeError input_error;
    framework::input::PresetTable presets;
    framework::ui::UiInputChannel ui_channel;
    std::vector<uint8_t> preset_bytes;
    if (!framework::input::bake_presets(ui, preset_bytes, input_error) || !presets.open(preset_bytes.data(), preset_bytes.size()) ||
        !ui_channel.init(presets)) return 1;
    engine.post({input::RawKind::KeyDown, input::DeviceKind::Keyboard, 0, 257}); engine.drain();
    if (!ui_channel.read(engine.device(), 16).accept || ui_channel.read(engine.device(), 16).accept) return 1;
    std::vector<uint8_t> font_bytes;
    framework::graphics::FontTable fonts;
    framework::ui::Context widgets;
    if (!framework::graphics::bake_fonts({&font, 1}, font_bytes, error) || !fonts.open(font_bytes.data(), font_bytes.size()) ||
        !widgets.init(table, fonts, 1, 1)) return 1;
    const auto button = framework::ui::ui_id(0, "sdk-button");
    widgets.begin(ui_channel.read(engine.device(), 16));
    if (widgets.button(button, "label", {0, 0, 20, 12})) return 1;
    auto drawn = widgets.end();
    if (drawn.quads.size() != 3 || drawn.runs.size() != 1 || drawn.focus != button) return 1;
    engine.post({input::RawKind::KeyUp, input::DeviceKind::Keyboard, 0, 257}); engine.drain();
    ui_channel.read(engine.device(), 16);
    engine.post({input::RawKind::KeyDown, input::DeviceKind::Keyboard, 0, 257}); engine.drain();
    widgets.begin(ui_channel.read(engine.device(), 16));
    if (!widgets.button(button, "label", {0, 0, 20, 12})) return 1;
    widgets.end();
    char name[4]{};
    widgets.begin({});
    widgets.glyph_grid(19, "label", name, 3, {0, 0, 60, 14}, 3, {"label", "label"}); widgets.end();
    engine.post({input::RawKind::KeyDown, input::DeviceKind::Keyboard, 0, 65}); engine.drain();
    widgets.begin(ui_channel.read(engine.device(), 16));
    const auto edited = widgets.glyph_grid(19, "label", name, 3, {0, 0, 60, 14}, 3, {"label", "label"}); widgets.end();
    if (!edited.changed || edited.invalid || std::string_view(name) != "A") return 1;
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
