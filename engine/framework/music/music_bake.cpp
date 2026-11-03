#include "music_bake.hpp"

#include <algorithm>

#include "music_read.hpp"
#include "music_validation.hpp"
#include "section_bake.hpp"

namespace framework::music {
bool bake_music(std::span<const MusicSrc> tracks, std::vector<uint8_t>& out, std::string& error) {
    error.clear();
    auto fail = [&](const std::string& message) { error = message; return false; };
    if (tracks.empty() || tracks.size() > MAX_MUSIC_TRACKS) return fail("expected 1 to4096 music tracks");
    struct Indexed { uint64_t id; const MusicSrc* source; };
    std::vector<Indexed> indexed;
    for (const auto& track : tracks) {
        if (!detail::name_ok(track.name)) return fail("music track has an invalid name");
        const std::string who = "music '" + track.name + "'";
        if (track.rate != MUSIC_SAMPLE_RATE) return fail(who + " needs a 48000 Hz source");
        if (track.channels != 1 && track.channels != 2) return fail(who + " needs mono or stereo source channels");
        const uint32_t end = track.loop_end.value_or(track.frames);
        if (!track.frames || track.loop_start >= end || end > track.frames) return fail(who + " has invalid loop bounds");
        constexpr uint32_t PER_MS = MUSIC_SAMPLE_RATE / 1000;
        if (track.fade_ms > UINT32_MAX / PER_MS) return fail(who + " fade_ms exceeds the sample counter");
        indexed.push_back({music_id(track.name), &track});
    }
    std::sort(indexed.begin(), indexed.end(), [](const auto& a, const auto& b) { return a.id < b.id; });
    for (size_t i = 1; i < indexed.size(); ++i)
        if (indexed[i - 1].id == indexed[i].id) return fail("music has a duplicate name or FNV collision");
    // Порядок string pool тоже канонический: входные записи можно переставлять.
    std::vector<MusicRow> rows;
    core::SectionBuilder canonical(indexed.size() * sizeof(MusicRow));
    for (const auto& entry : indexed) {
        const auto& track = *entry.source;
        rows.push_back({entry.id, track.guid, canonical.text(track.name), track.frames, track.loop_start,
                        track.loop_end.value_or(track.frames), track.fade_ms * (MUSIC_SAMPLE_RATE / 1000), track.rate, track.channels, 0});
    }
    std::vector<uint8_t> result;
    if (canonical.encoded_size() > MAX_MUSIC_BYTES ||
        !canonical.finish(MUSIC_MAGIC, MUSIC_VERSION, static_cast<uint32_t>(rows.size()), rows.data(), result, error))
        return fail(error.empty() ? "music table exceeds1MiB" : error);
    MusicTable probe;
    if (!probe.open(result.data(), result.size())) return fail("baked music table fails its reader");
    out = std::move(result); return true;
}
}
