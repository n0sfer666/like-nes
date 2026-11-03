#include "fuzz_target.hpp"
#include "music_fixture.hpp"
#include "music_read.hpp"

namespace fuzz {
namespace {
std::vector<uint8_t> seed_music() {
    std::vector<uint8_t> bytes; std::string error;
    framework::music::bake_music(music_fixture::rows(), bytes, error);
    return bytes;
}
bool read_music(const uint8_t* data, size_t size) {
    framework::music::MusicTable table;
    if (!table.open(data, size)) return false;
    consume(table.count());
    for (uint32_t i = 0; i < table.count(); ++i) {
        const auto& row = *table.track(i);
        consume_all(row.id, row.guid, row.name, row.frames, row.loop_start, row.loop_end,
                    row.fade_samples, row.rate, row.channels, row.reserved);
        consume_str(table.name(i));
        consume(table.find(std::string_view(table.name(i))) == &row);
        consume(table.find(row.id) == &row);
    }
    return true;
}
const Target TARGETS[]{{"music", seed_music, read_music}};
}
const Target* music_targets(size_t* count) { *count = sizeof TARGETS / sizeof TARGETS[0]; return TARGETS; }
}
