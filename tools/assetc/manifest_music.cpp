#include "manifest_music.hpp"

#include <cstdio>

#include "baker_guid.hpp"
#include "bakers.hpp"
#include "platform_fs.hpp"
#include "text_fields.hpp"
#include "vorbis_reader.hpp"

namespace asset::manifest {
bool music_options(const std::vector<std::string>& fields, uint32_t line, Error& error, MusicOptions& out) {
    auto fail = [&](const std::string& message) { error = {line, message}; return false; };
    MusicOptions pending;
    size_t count = fields.size();
    if (count > 4 && fields.back().starts_with("fade_ms=")) {
        if (!framework::core::parse_u32(fields.back().substr(8), pending.fade_ms) ||
            pending.fade_ms > UINT32_MAX / (framework::music::MUSIC_SAMPLE_RATE / 1000))
            return fail("music fade_ms must fit the sample counter");
        --count;
    }
    if ((count != 4 && count != 6) || fields[2] != "vorbis")
        return fail("music record expected as music|<name>|vorbis|<path>.ogg[|loop_start|loop_end][|fade_ms=N]");
    if (!fields[3].ends_with(".ogg")) return fail("music path must end in .ogg");
    if (fields[1].size() > framework::music::MAX_MUSIC_NAME) return fail("music name exceeds128 bytes");
    if (count == 6) {
        uint32_t end = 0;
        if (!framework::core::parse_u32(fields[4], pending.loop_start) || !framework::core::parse_u32(fields[5], end) || pending.loop_start >= end)
            return fail("music loop requires integer samples: 0 <= loop_start < loop_end");
        pending.loop_end = end;
    }
    out = pending; return true;
}
bool music_shape(const std::vector<std::string>& fields, uint32_t line, Error& error) {
    MusicOptions out; return music_options(fields, line, error, out);
}
bool gather_music(Bake& b, const Record& r, const std::string& file, std::vector<framework::music::MusicSrc>& tracks) {
    if (tracks.size() >= framework::music::MAX_MUSIC_TRACKS) return bake_fail(b, r.line, "music exceeds4096 tracks");
    std::vector<uint8_t> bytes;
    if (!platform::read_bytes_capped(file, bytes, 64u << 20))
        return bake_fail(b, r.line, "cannot read " + file + " (missing, unreadable or over 64 MiB)");
    audio::VorbisInfo info; std::string why;
    if (!audio::inspect_vorbis(bytes, info, why)) return bake_fail(b, r.line, file + ": " + why);
    framework::music::MusicSrc track;
    track.name = r.name; track.guid = bakers::guid_of(r.name.c_str());
    track.frames = info.frames; track.rate = info.rate; track.channels = info.channels;
    track.loop_start = r.music.loop_start; track.loop_end = r.music.loop_end; track.fade_ms = r.music.fade_ms;
    // Один валидатор wire-контракта, но диагностика привязана к исходной записи, не к последней.
    std::vector<uint8_t> probe;
    if (!framework::music::bake_music({&track, 1}, probe, why)) return bake_fail(b, r.line, file + ": " + why);
    AssetInput asset;
    asset.guid = track.guid; asset.type = AssetType::Audio; asset.codec = Codec::Vorbis; asset.residency = Residency::Mmap;
    asset.uncompressed_size = static_cast<uint32_t>(bytes.size()); asset.payload = std::move(bytes);
    asset.tex_w = info.rate; asset.tex_h = info.channels; asset.tex_format = info.frames; asset.variant_key = AUDIO_FLAG_LOOP;
    b.assets.push_back(std::move(asset)); tracks.push_back(track);
    std::printf("[assetc] music %s: frames=%u channels=%u loop=%u..%u fade_samples=%u\n", r.name.c_str(), info.frames, info.channels,
                track.loop_start, track.loop_end.value_or(info.frames), track.fade_ms * (framework::music::MUSIC_SAMPLE_RATE / 1000));
    return true;
}
bool finish_music(Bake& b, std::span<const framework::music::MusicSrc> tracks, uint32_t line) {
    if (tracks.empty()) return true;
    std::vector<uint8_t> bytes; std::string why;
    if (!framework::music::bake_music(tracks, bytes, why)) return bake_fail(b, line, why);
    bakers::push_table("music", std::move(bytes), b.assets); return true;
}
}
