#pragma once
#include <cstdio>
#include <string>

#include "mobile_app.hpp"

namespace mobile {

// Общая грамматика для iOS unified log и Android logcat; insets и касания — в точках.
inline std::string safe_line(uint32_t w, uint32_t h, const SafeArea& safe, int rotation, const char* run) {
    char out[256];
    std::snprintf(out, sizeof out,
        "[safe] fb_w=%u fb_h=%u scale=%.3f left=%.3f top=%.3f right=%.3f bottom=%.3f rotation=%d run=%s",
        w, h, safe.scale, safe.left, safe.top, safe.right, safe.bottom, rotation, run);
    return out;
}

inline std::string touch_line(intptr_t id, Touch phase, float x, float y, const std::string& state,
                              const char* run) {
    const char* name = phase == Touch::Down ? "down" : phase == Touch::Move ? "move" : "up";
    char out[512];
    std::snprintf(out, sizeof out, "[touch] %s id=%lld id32=%d x=%.3f y=%.3f %s run=%s", name,
        static_cast<long long>(id), static_cast<int32_t>(id), x, y, state.c_str(), run);
    return out;
}

} // namespace mobile
