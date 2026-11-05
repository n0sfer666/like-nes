# shellcheck shell=bash
# Ogg из поставленного пекаря запускается как MusicStream и проходит offline Mixer.
fixture_music_bake() {
    local dir="$WORK/music-bake" cfg log exe
    mkdir -p "$dir"
    cp "$ROOT/tests/audio/fixtures/stereo-48000.ogg" "$dir/song.ogg"
    printf 'music|menu|vorbis|song.ogg|37|4000|fade_ms=750\n' > "$dir/game.manifest"
    cat > "$dir/CMakeLists.txt" <<'CMAKE'
cmake_minimum_required(VERSION 3.21)
project(music_bake CXX)
find_package(like-nes 0.1 REQUIRED CONFIG)
add_executable(music_game main.cpp)
target_link_libraries(music_game PRIVATE like-nes::engine)
like_nes_add_game(music_game)
like_nes_bake(music_game MANIFEST game.manifest OUT music.bundle)
CMAKE
    cat > "$dir/main.cpp" <<'CPP'
#include <array>
#include <chrono>
#include <cstdio>
#include <thread>
#include <vector>
#include "bundle_lookup.hpp"
#include "hash.hpp"
#include "music_read.hpp"
#include "platform_fs.hpp"
#include "platform_process.hpp"
#include "decoder.hpp"
#include "vorbis_reader.hpp"
#include "mixer.hpp"
#include "music_stream.hpp"
#include "music_track.hpp"
int main() {
    std::vector<uint8_t> bytes; asset::BundleView bundle;
    if (!platform::read_bytes(platform::exe_dir() + "/music.bundle", bytes) || !bundle.open(bytes.data(), bytes.size(), false)) return 1;
    const uint8_t* table = nullptr; size_t size = 0;
    framework::music::MusicTable music;
    if (asset::raw_table(bundle, asset::fnv1a("music", 5), table, size) != asset::LookupFault::Ok || !music.open(table, size)) return 1;
    const auto* row = music.find("menu");
    if (!row || row->frames != 4800 || row->channels != 2 || row->loop_start != 37 || row->loop_end != 4000 || row->fade_samples != 36000) return 1;
    const auto* entry = bundle.find(row->guid);
    if (!entry || entry->type != static_cast<uint32_t>(asset::AssetType::Audio) || entry->codec != static_cast<uint32_t>(asset::Codec::Vorbis) ||
        entry->tex_w != row->rate || entry->tex_h != row->channels || entry->tex_format != row->frames) return 1;
    audio::VorbisInfo info; std::string error; audio::DecodedPcm mono;
    const auto payload = std::span<const uint8_t>(bundle.payload(*entry), entry->payload_size);
    if (!audio::inspect_vorbis(payload, info, error) || info.frames != row->frames || info.channels != 2 ||
        !audio::decode_vorbis(bundle.payload(*entry), entry->payload_size, mono) || mono.frames != row->frames) return 1;
    audio::MusicTrack track;
    track.id = row->id; track.encoded = payload; track.frames = row->frames;
    track.loop_start = row->loop_start; track.loop_end = row->loop_end; track.fade_samples = row->fade_samples;
    audio::Mixer mixer(audio::Backend::Fix32);
    audio::MusicStream stream;
    if (!stream.init(mixer, {&track, 1}, 1, error) || !stream.want(row->id)) return 1;
    stream.tick(0);
    std::array<int16_t, 256 * 2> mixed{};
    uint64_t energy = 0;
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(8);
    for (uint32_t block = 0; block < 800 && std::chrono::steady_clock::now() < deadline; ++block) {
        mixer.mix(256, mixed.data());
        for (int16_t sample : mixed) energy += static_cast<uint64_t>(sample < 0 ? -int32_t{sample} : sample);
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    if (!energy || stream.stats().decode_failures || mixer.peak_voices()) return 1;
    std::puts("sdk-music: PASS installed Ogg bake, MusicStream startup and offline Mixer");
}
CPP
    for cfg in Release Debug; do
        log="$dir/$cfg.log"
        if ! cmake -S "$(native "$dir")" -B "$(native "$dir/$cfg")" -G "$GEN" \
            -DCMAKE_BUILD_TYPE="$cfg" -DCMAKE_PREFIX_PATH="$(native "$PREFIX")" > "$log" 2>&1 ||
            ! cmake --build "$(native "$dir/$cfg")" >> "$log" 2>&1; then
            tail -25 "$log"; sdk_bad "music: $cfg prefix bake/link failed"; return 1
        fi
        exe="$dir/$cfg/music_game"
        if ! "$exe" >> "$log" 2>&1 || ! grep -qxF 'sdk-music: PASS installed Ogg bake, MusicStream startup and offline Mixer' "$log"; then
            tail -25 "$log"; sdk_bad "music: $cfg prefix MusicStream failed"; return 1
        fi
    done
    sdk_ok 'music: installed Ogg bake, MusicStream and offline Mixer in Release/Debug'
}
