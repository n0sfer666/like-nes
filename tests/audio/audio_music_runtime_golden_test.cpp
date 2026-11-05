#include "music_runtime_fixture.hpp"

#include <cstdio>

#include "hash.hpp"

int main() {
    auto owner = music_runtime_fixture::make_rig(); auto& rig = *owner;
    if (!rig.ready) return 1;
    uint64_t hash = asset::FNV_OFFSET;
    uint32_t underruns = 0;
    for (uint32_t frame = 0; frame < 2300; ++frame) {
        bool post = false;
        audio::MusicWish wish{};
        if (frame == 0) { post = true; wish = {0, 100}; }
        if (frame == 200) { post = true; wish = {1, 1000}; }
        if (frame == 400) { post = true; wish = {2, 500}; }
        if (frame == 450) { post = true; wish = {3, 600}; }
        if (frame == 1000) { post = true; wish = {1, 1000}; }
        const auto mixed = rig.step(post, wish);
        underruns += mixed.underruns;
        hash = asset::fnv1a_u64(hash, static_cast<uint64_t>(mixed.left));
        hash = asset::fnv1a_u64(hash, static_cast<uint64_t>(mixed.right));
    }
    const bool ok = underruns == 0 && hash == 0x3023e102d993ecb4ull;
    std::printf("audio-music-runtime-golden: hash=%016llx underruns=%u\n",
                static_cast<unsigned long long>(hash), underruns);
    std::puts(ok ? "audio-music-runtime-golden: PASS independent Q16 timeline oracle" :
                   "audio-music-runtime-golden: FAIL independent Q16 timeline oracle");
    return ok ? 0 : 1;
}
