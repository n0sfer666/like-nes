#include <string>
#include <utility>

#include "clip_golden.hpp"

namespace clip_golden {
namespace {

constexpr uint32_t CEL_W = 16;
constexpr uint32_t CEL_H = 24;
constexpr uint32_t CELS = 3;

Image sheet() {
    Image im;
    im.w = CEL_W * CELS;
    im.h = CEL_H;
    im.rgba.assign(std::size_t{4} * im.w * im.h, 0);
    for (uint32_t y = 0; y < im.h; ++y)
        for (uint32_t x = 0; x < im.w; ++x) {
            const uint32_t cx = x % CEL_W;
            if (cx + y < 4 || cx == 12) continue;
            uint8_t* p = im.rgba.data() + 4 * (std::size_t{y} * im.w + x);
            p[0] = static_cast<uint8_t>(cx * 15);
            p[1] = static_cast<uint8_t>(y * 10);
            p[2] = static_cast<uint8_t>(40 + 80 * (x / CEL_W));
            p[3] = 255;
        }
    return im;
}

ClipFrameSrc cel(uint16_t index, uint16_t duration, std::vector<ClipBoxSrc> boxes) {
    ClipFrameSrc f;
    f.x = static_cast<uint16_t>(index * CEL_W);
    f.w = CEL_W;
    f.h = CEL_H;
    f.anchor_x = 5;
    f.anchor_y = 22;
    f.duration = duration;
    f.boxes = std::move(boxes);
    return f;
}

} // namespace

bool build_scene(Scene& s) {
    s.src.name = "golden/punch";
    s.src.flags = CLIP_LOOP;
    s.src.texture_guid = 0xC1;
    s.src.frames = {
        cel(0, 3, {{BoxKind::Push, 0, {-3, -12, 6, 12}}, {BoxKind::Hurt, 0, {-4, -20, 9, 20}}}),
        cel(1, 2, {{BoxKind::Hurt, 0, {-4, -18, 9, 18}}, {BoxKind::Hit, 0, {3, -16, 10, 4}}}),
        cel(2, 3,
            {{BoxKind::Hurt, 1, {-5, -22, 4, 6}}, {BoxKind::Hit, 1, {2, -10, 8, 3}},
             {BoxKind::Hurt, 0, {-4, -18, 9, 18}}, {BoxKind::Hit, 0, {3, -16, 10, 4}}}),
    };
    std::string error;
    if (!bake_clips({&s.src, 1}, s.bytes, error) || !s.table.open(s.bytes.data(), s.bytes.size()))
        return false;
    s.clip = s.table.find("golden/punch");
    s.sheet = sheet();
    s.solid.w = 1;
    s.solid.h = 1;
    s.solid.rgba = {255, 255, 255, 255};
    return s.clip.row != nullptr;
}

} // namespace clip_golden
