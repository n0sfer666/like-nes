#include "music_fixture.hpp"

#include <cstdio>
#include <cstring>
#include <initializer_list>
#include <utility>

#include "framework_alloc_probe.hpp"
#include "framework_alloc_probe_control.hpp"
#include "music_read.hpp"

namespace {
using namespace framework::music;
int fails;
void check(bool ok, const char* why) { if (!ok) { ++fails; std::printf("music-limits: FAIL %s\n", why); } }
void u32(std::vector<uint8_t>& bytes, size_t at, uint32_t value) { std::memcpy(bytes.data() + at, &value, sizeof value); }
void u64(std::vector<uint8_t>& bytes, size_t at, uint64_t value) { std::memcpy(bytes.data() + at, &value, sizeof value); }
}
int main() {
    const auto source = music_fixture::one();
    std::vector<uint8_t> bytes;
    std::string error;
    check(bake_music({&source, 1}, bytes, error), "actual seed");
    const auto seed = bytes;
    MusicTable table;
    auto reject = [&](const std::vector<uint8_t>& blob) {
        check(table.open(seed.data(), seed.size()), "seed before corrupt reinit");
        check(!table.open(blob.data(), blob.size()) && table.count() == 0 && !table.find("menu") && !table.track(0), "corrupt input clears prior views");
    };
    for (size_t size = 0; size < seed.size(); ++size) reject({seed.begin(), seed.begin() + size});
    for (auto [offset, value] : std::initializer_list<std::pair<size_t, uint32_t>>{
         {4, 2}, {8, 0}, {8, UINT32_MAX}, {12, 32}, {16, 73}, {20, UINT32_MAX},
         {40, 0}, {40, 2}, {40, UINT32_MAX}, {44, 0}, {48, 84000}, {52, 96001}, {60, 44100}, {64, 0}, {64, 3}, {68, 1}}) {
        bytes = seed; u32(bytes, offset, value); reject(bytes);
    }
    bytes = seed; bytes[0] ^= 1; reject(bytes);
    bytes = seed; bytes.back() = 1; reject(bytes);
    bytes = seed; bytes[73] = 0xff; reject(bytes);
    bytes = seed; bytes.push_back(0); reject(bytes);
    bytes = seed; bytes.resize(MAX_MUSIC_BYTES + 1, 0); u32(bytes, 20, static_cast<uint32_t>(bytes.size())); reject(bytes);
    bytes = seed; u64(bytes, 24, 0); reject(bytes);
    check(!table.open(nullptr, seed.size()) && !table.open(seed.data(), SIZE_MAX), "null/oversized input refused before access");
    std::vector<uint8_t> shifted(seed.size() + 8);
    std::memcpy(shifted.data() + 1, seed.data(), seed.size());
    check(!table.open(shifted.data() + 1, seed.size()), "misaligned zero-copy input refused");
    auto rows = music_fixture::rows();
    check(bake_music(rows, bytes, error), "multiple-row seed");
    MusicRow first{}, second{};
    std::memcpy(&first, bytes.data() + 24, sizeof first); std::memcpy(&second, bytes.data() + 72, sizeof second);
    std::memcpy(bytes.data() + 24, &second, sizeof second); std::memcpy(bytes.data() + 72, &first, sizeof first); reject(bytes);
    check(bake_music(rows, bytes, error), "fresh duplicate-row seed");
    std::memcpy(bytes.data() + 72, bytes.data() + 24, sizeof(MusicRow)); reject(bytes);
    rows.assign(MAX_MUSIC_TRACKS, source);
    for (uint32_t i = 0; i < rows.size(); ++i) rows[i].name = std::to_string(i) + std::string(MAX_MUSIC_NAME - std::to_string(i).size(), 'A');
    check(bake_music(rows, bytes, error) && table.open(bytes.data(), bytes.size()) && table.count() == MAX_MUSIC_TRACKS && bytes.size() < MAX_MUSIC_BYTES,
          "4096 longest valid names fit the format cap");
    const auto saved = bytes;
    rows.push_back(source);
    check(!bake_music(rows, bytes, error) && bytes == saved, "4097 tracks refused before output change");
    rows.assign(1, source); rows[0].name.assign(MAX_MUSIC_NAME + 1, 'A');
    check(!bake_music(rows, bytes, error) && bytes == saved, "overlong names refused");
    rows[0] = source; rows[0].fade_ms = UINT32_MAX / 48;
    check(bake_music(rows, bytes, error) && table.open(bytes.data(), bytes.size()) && table.track(0)->fade_samples == 4294967280u,
          "maximum fade_ms converts without overflow");
    bytes = saved;
    framework::probe::in_hot = true;
    const bool controls = framework::probe::control::plain_allocation() && framework::probe::control::aligned_allocation();
    framework::probe::in_hot = false;
    check(controls && framework::probe::allocs >= 2, "allocator positive controls");
    framework::probe::allocs = 0; framework::probe::in_hot = true;
    bool ok = true;
    for (int pass = 0; pass < 8; ++pass) {
        ok = table.open(bytes.data(), bytes.size()) && ok;
        for (uint32_t i = 0; i < table.count(); ++i) {
            const auto* row = table.track(i);
            ok = table.find(std::string_view(table.name(i))) == row && table.find(row->id) == row && ok;
        }
        ok = !table.open(nullptr, 0) && !table.find("menu") && ok;
    }
    framework::probe::in_hot = false;
    check(ok && framework::probe::allocs == 0, "open, full walk, find and refused reinit allocate nothing");
    std::puts(fails ? "music-limits: FAIL" : "music-limits: PASS wire bounds, alignment, caps, stale views and zero allocations");
    return fails ? 1 : 0;
}
