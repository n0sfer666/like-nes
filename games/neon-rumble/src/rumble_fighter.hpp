#pragma once
#include <array>
#include <cstdint>
#include <string>

#include "brawl_body.hpp"
#include "bundle_lookup.hpp"
#include "clip_read.hpp"
#include "depth_profile.hpp"
#include "layer_quads.hpp"
#include "rumble_roster.hpp"

namespace rumble {

struct Level;

struct Pose {
    framework::graphics::ClipView clip;
    const char* name = "";
    uint16_t frame = 0;
    bool flip = false;
};

struct Fighter {
    enum Clip : uint32_t { IDLE, WALK, JUMP, CLIP_COUNT };

    const char* name = "";
    framework::graphics::ClipTable clips;
    std::array<std::string, CLIP_COUNT> clip_names;
    asset::RgbaView sheet;
    framework::graphics::TextureSize sheet_size;

    bool open(const Level& level, const char* fighter);
    Pose pose(const framework::brawl::Body& body, const framework::brawl::DepthProfile& profile) const;
};

using Fighters = std::array<Fighter, FIGHTERS>;

} // namespace rumble
