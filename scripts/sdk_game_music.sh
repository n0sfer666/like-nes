# shellcheck shell=bash
# Ogg из поставленного пекаря читается и декодируется игрой против поставленного SDK.
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
like_nes_bake(music_game MANIFEST game.manifest OUT music.bundle)
CMAKE
    cat > "$dir/main.cpp" <<'CPP'
#include <cstdio>
#include <vector>
#include "bundle_lookup.hpp"
#include "hash.hpp"
#include "music_read.hpp"
#include "platform_fs.hpp"
#include "platform_process.hpp"
#include "decoder.hpp"
#include "vorbis_reader.hpp"
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
    if (!audio::inspect_vorbis({bundle.payload(*entry), entry->payload_size}, info, error) || info.frames != row->frames || info.channels != 2 ||
        !audio::decode_vorbis(bundle.payload(*entry), entry->payload_size, mono) || mono.frames != row->frames) return 1;
    std::puts("sdk-music: PASS installed Ogg bake, metadata and decoder");
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
        if ! "$exe" >> "$log" 2>&1 || ! grep -qxF 'sdk-music: PASS installed Ogg bake, metadata and decoder' "$log"; then
            tail -25 "$log"; sdk_bad "music: $cfg prefix decode failed"; return 1
        fi
    done
    sdk_ok 'music: installed Ogg bake, LNMU and decoder in Release/Debug'
}
