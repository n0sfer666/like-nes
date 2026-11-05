# shellcheck shell=bash
# Ogg из поставленного пекаря запускается как MusicStream и проходит offline Mixer.
fixture_music_bake() {
    local dir="$WORK/music-bake" cfg log exe
    mkdir -p "$dir"
    cp "$ROOT/tests/audio/fixtures/stereo-48000.ogg" "$dir/song.ogg"
    printf 'music|menu|vorbis|song.ogg|37|4000|fade_ms=750\n' > "$dir/game.manifest"
    printf 'music|extra|vorbis|song.ogg|0|4800|fade_ms=10\n' > "$dir/extra.manifest"
    cat > "$dir/CMakeLists.txt" <<'CMAKE'
cmake_minimum_required(VERSION 3.21)
project(music_bake CXX)
find_package(like-nes 0.1 REQUIRED CONFIG)
add_executable(music_game main.cpp)
target_link_libraries(music_game PRIVATE like-nes::engine)
like_nes_add_game(music_game)
like_nes_bake(music_game MANIFEST game.manifest OUT music.bundle)
like_nes_bake(music_game MANIFEST extra.manifest OUT music-extra.bundle)
CMAKE
    cat > "$dir/main.cpp" <<'CPP'
#include <array>
#include <chrono>
#include <cstdio>
#include <thread>
#include <type_traits>
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
#include "frame_stats.hpp"
static_assert(!std::is_copy_constructible_v<framework::core::FrameStats>);
static_assert(!std::is_copy_assignable_v<framework::core::FrameStats>);
static_assert(!std::is_move_constructible_v<framework::core::FrameStats>);
static_assert(!std::is_move_assignable_v<framework::core::FrameStats>);
int main() {
    framework::core::FrameStats timing;
    if (!timing.init(3) || !timing.record(0, 5) || !timing.record(33333333, 9) ||
        !timing.record(16666667, 1)) return 1;
    const auto measured = timing.summary();
    if (!measured.complete || measured.frames != 3 || measured.intervals != 2 || measured.misses != 1 ||
        measured.work_p50_ns != 5 || measured.work_p99_ns != 9) return 1;
    std::vector<uint8_t> bytes; asset::BundleView bundle;
    if (!platform::read_bytes(platform::exe_dir() + "/music.bundle", bytes) || !bundle.open(bytes.data(), bytes.size(), false)) return 1;
    const uint8_t* table = nullptr; size_t size = 0;
    framework::music::MusicTable music;
    if (asset::raw_table(bundle, asset::fnv1a("music", 5), table, size) != asset::LookupFault::Ok || !music.open(table, size)) return 1;
    const auto* row = music.find("menu");
    if (!row || row->frames != 4800 || row->channels != 2 || row->loop_start != 37 || row->loop_end != 4000 || row->fade_samples != 36000) return 1;
    std::vector<uint8_t> extra_bytes; asset::BundleView extra_bundle;
    if (!platform::read_bytes(platform::exe_dir() + "/music-extra.bundle", extra_bytes) ||
        !extra_bundle.open(extra_bytes.data(), extra_bytes.size(), false)) return 1;
    framework::music::MusicTable extra_music;
    if (asset::raw_table(extra_bundle, asset::fnv1a("music", 5), table, size) != asset::LookupFault::Ok ||
        !extra_music.open(table, size)) return 1;
    const auto* extra = extra_music.find("extra");
    if (!extra || extra->frames != 4800 || extra->channels != 2 || extra->loop_start != 0 || extra->loop_end != 4800 ||
        (extra->fade_samples != 480 && extra->fade_samples != 1200)) return 1;
    std::printf("sdk-music-extra: fade=%u\n", extra->fade_samples);
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
        printf 'music|extra|vorbis|song.ogg|0|4800|fade_ms=10\n' > "$dir/extra.manifest"
        if ! cmake -S "$(native "$dir")" -B "$(native "$dir/$cfg")" -G "$GEN" \
            -DCMAKE_BUILD_TYPE="$cfg" -DCMAKE_PREFIX_PATH="$(native "$PREFIX")" > "$log" 2>&1 ||
            ! cmake --build "$(native "$dir/$cfg")" >> "$log" 2>&1; then
            tail -25 "$log"; sdk_bad "music: $cfg prefix bake/link failed"; return 1
        fi
        exe="$dir/$cfg/music_game"
        if ! "$exe" >> "$log" 2>&1 || ! grep -qxF 'sdk-music-extra: fade=480' "$log" ||
            ! grep -qxF 'sdk-music: PASS installed Ogg bake, MusicStream startup and offline Mixer' "$log"; then
            tail -25 "$log"; sdk_bad "music: $cfg prefix MusicStream failed"; return 1
        fi

        cp "$dir/$cfg/music.bundle" "$dir/$cfg/music.bundle.fixture"
        sed 's/^music|extra|vorbis|song\.ogg|0|4800|fade_ms=10$/music|extra|vorbis|song.ogg|0|4800|fade_ms=25/' \
            "$dir/extra.manifest" > "$dir/extra.manifest.next" && mv "$dir/extra.manifest.next" "$dir/extra.manifest" || {
            sdk_bad "music: $cfg could not update only the extra manifest"; return 1; }
        : > "$log"
        if ! cmake --build "$(native "$dir/$cfg")" --target music_game_bundle > "$log" 2>&1; then
            tail -25 "$log"; sdk_bad "music: $cfg incremental extra bundle bake failed"; return 1
        fi
        if ! grep -qF 'Baking music-extra.bundle' "$log" || grep -qF 'Baking music.bundle' "$log" ||
            ! cmp -s "$dir/$cfg/music.bundle.fixture" "$dir/$cfg/music.bundle"; then
            tail -25 "$log"; sdk_bad "music: $cfg rebake touched the wrong bundle or changed the primary bundle"; return 1
        fi
        if ! "$exe" > "$dir/$cfg/extra.log" 2>&1 || ! grep -qxF 'sdk-music-extra: fade=1200' "$dir/$cfg/extra.log"; then
            tail -25 "$dir/$cfg/extra.log"; sdk_bad "music: $cfg installed consumer did not observe updated extra fade"; return 1
        fi
    done
    fixture_music_bake_collisions || return 1
    sdk_ok 'music: installed Ogg bake, MusicStream, isolated extra rebake and collision refusals in Release/Debug'
}

fixture_music_bake_collisions() {
    local src="$WORK/music-bake-collisions" mode want fail=0
    mkdir -p "$src"
    printf 'int main() { return 0; }\n' > "$src/main.cpp"
    printf 'music|dummy|vorbis|song.ogg|0|4800|fade_ms=0\n' > "$src/game.manifest"
    cat > "$src/CMakeLists.txt" <<'CMAKE'
cmake_minimum_required(VERSION 3.21)
project(music_bake_collisions CXX)
find_package(like-nes 0.1 REQUIRED CONFIG)
add_executable(music_game main.cpp)
like_nes_add_game(music_game)
if(MODE STREQUAL "exact")
  like_nes_bake(music_game MANIFEST game.manifest OUT same.bundle)
  like_nes_bake(music_game MANIFEST game.manifest OUT same.bundle)
elseif(MODE STREQUAL "case")
  like_nes_bake(music_game MANIFEST game.manifest OUT Song.bundle)
  like_nes_bake(music_game MANIFEST game.manifest OUT song.BUNDLE)
elseif(MODE STREQUAL "reserved")
  like_nes_bake(music_game MANIFEST game.manifest OUT library.bundle)
elseif(MODE STREQUAL "out_prior_depfile")
  like_nes_bake(music_game MANIFEST game.manifest OUT prior.bundle)
  like_nes_bake(music_game MANIFEST game.manifest OUT prior.bundle.d)
elseif(MODE STREQUAL "depfile_prior_out")
  like_nes_bake(music_game MANIFEST game.manifest OUT prior.bundle.d)
  like_nes_bake(music_game MANIFEST game.manifest OUT prior.bundle)
elseif(MODE STREQUAL "aggregator")
  add_custom_target(music_game_bundle)
  like_nes_bake(music_game MANIFEST game.manifest OUT game.bundle)
endif()
CMAKE
    while IFS='|' read -r mode want; do
        if cmake --fresh -S "$(native "$src")" -B "$(native "$src/$mode-build")" -G "$GEN" \
            -DMODE="$mode" -DCMAKE_PREFIX_PATH="$(native "$PREFIX")" > "$src/$mode.log" 2>&1 < /dev/null; then
            sdk_bad "fixture: like_nes_bake accepted $mode output collision"; fail=1; continue
        fi
        grep -qF "$want" "$src/$mode.log" || {
            tail -20 "$src/$mode.log"; sdk_bad "fixture: $mode collision failed for another reason"; fail=1; continue; }
        sdk_ok "fixture: like_nes_bake refuses $mode output collision"
    done <<'CASES'
exact|like_nes_bake: OUT 'same.bundle' collides
case|like_nes_bake: OUT 'song.BUNDLE' collides
reserved|like_nes_bake: OUT 'library.bundle' collides
out_prior_depfile|like_nes_bake: OUT 'prior.bundle.d' collides
depfile_prior_out|like_nes_bake: OUT 'prior.bundle' collides
aggregator|like_nes_bake: target 'music_game_bundle' is already defined
CASES
    return "$fail"
}
