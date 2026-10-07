#include "rumble_fighter.hpp"

#include <cstdio>
#include <string>

#include "clip.hpp"
#include "rumble_level.hpp"

namespace rumble {

namespace {

constexpr const char* CLIP_TAGS[Fighter::CLIP_COUNT] = {"idle", "walk", "jump"};

} // namespace

bool Fighter::open(const Level& level, const char* fighter) {
    name = fighter;
    const uint8_t* data = nullptr;
    size_t size = 0;
    if (!level.read_table("clips", data, size)) return false;
    if (!clips.open(data, size)) {
        std::fprintf(stderr, "neon-rumble: clips table: does not open\n");
        return false;
    }
    uint64_t guid = 0;
    const std::string first = std::string(fighter) + "/" + CLIP_TAGS[0];
    for (uint32_t i = 0; i < CLIP_COUNT; ++i) {
        const std::string clip = std::string(fighter) + "/" + CLIP_TAGS[i];
        const framework::graphics::ClipView v = clips.find(clip.c_str());
        if (v.row == nullptr) {
            std::fprintf(stderr, "neon-rumble: no clip %s in the clips table\n", clip.c_str());
            return false;
        }
        if (i == 0) guid = v.row->texture_guid;
        if (v.row->texture_guid == guid) continue;
        std::fprintf(stderr, "neon-rumble: clip %s is on another sheet than %s\n", clip.c_str(), first.c_str());
        return false;
    }
    const asset::LookupFault f = asset::raw_rgba8(level.bundle, guid, sheet);
    if (f != asset::LookupFault::Ok) {
        std::fprintf(stderr, "neon-rumble: %s sheet %016llx: %s\n", fighter, static_cast<unsigned long long>(guid),
                     asset::lookup_fault_name(f));
        return false;
    }
    sheet_size = {sheet.width, sheet.height};
    return true;
}

Pose Fighter::pose(const framework::brawl::Body& body) const {
    Pose p;
    p.name = clips.name(body.clip);
    p.clip = clips.clip(body.clip);
    p.frame = framework::graphics::clip_frame_at(p.clip.clip, body.elapsed);
    p.flip = body.facing < 0;
    return p;
}

} // namespace rumble
