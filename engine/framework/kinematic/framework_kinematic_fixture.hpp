#pragma once
#include <cstdio>
#include <initializer_list>

#include "path_follow.hpp"

namespace framework::kinematic::test {

inline int failures = 0;
inline fix32 px(int32_t v) { return fix32::from_int(v); }

inline void check(bool ok, const char* why) {
    if (ok) return;
    ++failures;
    std::printf("  FAIL: %s\n", why);
}

inline int verdict(const char* name) {
    std::printf("%s: %s\n", name, failures == 0 ? "PASS" : "FAIL");
    return failures == 0 ? 0 : 1;
}

inline scene::Path path(std::initializer_list<Vec2> points, int32_t speed, bool loop = false) {
    scene::Path p;
    for (Vec2 v : points) p.points[p.count++] = v;
    p.speed = px(speed);
    p.loop = loop;
    p.w = 32;
    p.h = 8;
    return p;
}

} // namespace framework::kinematic::test
