#include "music_fixture.hpp"

#include <algorithm>
#include <cstdio>
#include <cstring>

#include "audio_types.hpp"
#include "hash.hpp"
#include "music_read.hpp"

namespace {
using namespace framework::music;
int fails;
void check(bool ok, const char* why) { if (!ok) { ++fails; std::printf("music: FAIL %s\n", why); } }
static_assert(MUSIC_SAMPLE_RATE == audio::SAMPLE_RATE);
}
int main() {
    auto source = music_fixture::one();
    std::vector<uint8_t> bytes;
    std::string error;
    check(bake_music({&source, 1}, bytes, error), "actual metadata bake");
    const auto hash = asset::fnv1a(bytes.data(), bytes.size());
    check(hash == 0x11f0069b5c3ff7cbull && music_id("menu") == 0xf9732c1acf6e55f4ull, "independent Python struct/FNV wire oracle");
    std::printf("music: wire hash=%016llx\n", static_cast<unsigned long long>(hash));
    check(bytes.size() == 78, "independent header24+row48+pool6 layout");
    MusicTable table;
    check(table.open(bytes.data(), bytes.size()) && table.count() == 1, "aligned zero-copy reader");
    const auto* row = table.find("menu");
    check(row && row == table.track(0) && row == table.find(music_id("menu")), "lookup by name, id and index agrees");
    check(row && row->guid == source.guid && row->frames == 96000 && row->loop_start == 12000 && row->loop_end == 84000 &&
          row->fade_samples == 36000 && row->rate == 48000 && row->channels == 2, "exact loop/rate/channels/fade metadata");
    check(row && std::string_view(table.name(0)) == "menu" && !table.find("other") && !table.find("bad key") &&
          !table.find(UINT64_MAX) && !table.track(1) && std::string_view(table.name(UINT32_MAX)).empty(), "missing and invalid lookups");
    check(row && std::as_bytes(std::span<const MusicRow>(row, 1)).data() == std::as_bytes(std::span<const uint8_t>(bytes)).data() + 24,
          "row view borrows the actual input, no decoded copy");
    auto tracks = music_fixture::rows();
    std::vector<uint8_t> canonical;
    check(bake_music(tracks, bytes, error), "multiple music tracks");
    std::reverse(tracks.begin(), tracks.end());
    check(bake_music(tracks, canonical, error) && bytes == canonical, "row and string-pool order canonical under source permutation");
    check(table.open(bytes.data(), bytes.size()) && table.count() == 3, "multiple-track reader");
    const auto* intro = table.find("intro"); const auto* world = table.find("world");
    check(intro && intro->loop_start == 0 && intro->loop_end == 48000 && intro->fade_samples == 48000, "default whole-file loop and1s fade");
    check(world && world->guid == 0 && world->channels == 1 && world->fade_samples == 0, "GUID0, mono source and explicit instant transition");
    for (uint32_t i = 1; i < table.count(); ++i) check(table.track(i - 1)->id < table.track(i)->id, "strict ID order");
    const auto saved = bytes;
    auto refuse = [&](MusicSrc invalid, const char* part) {
        check(!bake_music({&invalid, 1}, bytes, error) && !error.empty() && error.find(part) != std::string::npos && bytes == saved,
              "bad source refused with diagnostic and unchanged output");
    };
    source.rate = 44100; refuse(source, "48000"); source = music_fixture::one();
    source.frames = 0; refuse(source, "loop"); source = music_fixture::one();
    source.loop_end = 96001; refuse(source, "loop"); source = music_fixture::one();
    source.loop_start = 84000; refuse(source, "loop"); source = music_fixture::one();
    source.loop_end = 0; refuse(source, "loop"); source = music_fixture::one();
    source.channels = 3; refuse(source, "channels"); source = music_fixture::one();
    source.name = "bad/name"; refuse(source, "name"); source = music_fixture::one();
    source.fade_ms = UINT32_MAX; refuse(source, "fade_ms");
    tracks = music_fixture::rows(); tracks.push_back(tracks[0]);
    check(!bake_music(tracks, bytes, error) && error.find("duplicate") != std::string::npos && bytes == saved, "duplicate/collision refuses before output publication");
    source = music_fixture::one(); source.loop_start = 0; source.loop_end = source.frames; source.fade_ms = 0;
    check(bake_music({&source, 1}, bytes, error) && table.open(bytes.data(), bytes.size()) && table.track(0)->loop_end == source.frames,
          "loop_end equals length and zero fade are valid boundaries");
    source.frames = 1; source.loop_end.reset(); source.fade_ms = 1000;
    check(bake_music({&source, 1}, bytes, error) && table.open(bytes.data(), bytes.size()) && table.track(0)->loop_end == 1 &&
          table.track(0)->fade_samples == 48000, "one-frame loop may be shorter than fade and ring");
    std::puts(fails ? "music: FAIL" : "music: PASS metadata, canonical wire, loops, rates, fades and transactional refusal");
    return fails ? 1 : 0;
}
