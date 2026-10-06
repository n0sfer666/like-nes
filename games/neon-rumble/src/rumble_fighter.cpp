#include "rumble_fighter.hpp"

#include <cstdio>

#include "clip.hpp"
#include "rumble_level.hpp"

namespace rumble {

namespace {

constexpr const char* CLIP_TAGS[Fighter::CLIP_COUNT] = {"idle", "walk", "jump"};

Fighter::Clip clip_of(const framework::brawl::DepthBody& d) {
    if (fix32{} < d.y) return Fighter::JUMP;
    if (d.vx.raw != 0 || d.vz.raw != 0) return Fighter::WALK;
    return Fighter::IDLE;
}

uint64_t airborne_ticks(const framework::brawl::DepthBody& d, const framework::brawl::DepthProfile& p) {
    const int32_t t = ((p.jump_vy - d.vy) / p.gravity).to_int();
    return t > 0 ? static_cast<uint64_t>(t) : 0;
}

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
    for (uint32_t i = 0; i < CLIP_COUNT; ++i) {
        clip_names[i] = std::string(fighter) + "/" + CLIP_TAGS[i];
        const framework::graphics::ClipView v = clips.find(clip_names[i].c_str());
        if (v.row == nullptr) {
            std::fprintf(stderr, "neon-rumble: no clip %s in the clips table\n", clip_names[i].c_str());
            return false;
        }
        if (i == 0) guid = v.row->texture_guid;
        if (v.row->texture_guid == guid) continue;
        std::fprintf(stderr, "neon-rumble: clip %s is on another sheet than %s\n", clip_names[i].c_str(),
                     clip_names[0].c_str());
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

Pose Fighter::pose(const framework::brawl::Body& body, const framework::brawl::DepthProfile& profile) const {
    const Clip c = clip_of(body.pos);
    Pose p;
    p.name = clip_names[c].c_str();
    p.clip = clips.find(p.name);
    p.frame = framework::graphics::clip_frame_at(p.clip.clip, c == JUMP ? airborne_ticks(body.pos, profile) : body.age);
    p.flip = body.facing < 0;
    return p;
}

} // namespace rumble
