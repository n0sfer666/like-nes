#pragma once
#include <cstdio>
#include <string>
#include <vector>

#include "platform_fs.hpp"

namespace vorbis_fixture {
inline int fails = 0;
inline void check(bool ok, const char* why) {
    if (!ok) { ++fails; std::printf("audio-vorbis: FAIL %s\n", why); }
}
inline std::vector<uint8_t> load(const char* path) {
    std::vector<uint8_t> bytes;
    check(platform::read_bytes(std::string(AUDIO_FIXTURE_ROOT) + "/" + path, bytes) && !bytes.empty(), "fixture loaded");
    return bytes;
}
}
