#pragma once
#include <cstdint>
#include <initializer_list>
#include <utility>

#include "framework_brawl_stage_fixture.hpp"
#include "scene_objects.hpp"

namespace framework::brawl::test {

inline scene::Path path(std::initializer_list<std::pair<int32_t, int32_t>> points, int32_t speed, bool loop) {
    scene::Path p;
    for (const auto& [x, y] : points) p.points[p.count++] = {at(x), at(y)};
    p.speed = at(speed);
    p.loop = loop;
    p.w = 32;
    p.h = 8;
    return p;
}

} // namespace framework::brawl::test
