#include "dodge_flick.hpp"

namespace framework::brawl {

namespace {

constexpr int ACTIONS = 64;

bool reaches(const ::input::InputFrame& f, const FlickRule& r) {
    const fix32 v = f.axes[r.axis];
    return !((v < fix32{} ? -v : v) < r.threshold);
}

} // namespace

bool dodge_flick(std::span<const ::input::InputFrame> recent, const FlickRule& rule) {
    if (rule.axis < 0 || rule.axis >= ::input::MAX_AXES || rule.block < 0 || rule.block >= ACTIONS) return false;
    const size_t n = recent.size();
    if (n < 2 || !recent[n - 1].action_held(rule.block) || !reaches(recent[n - 1], rule)) return false;
    const size_t from = n - 1 > rule.ticks ? n - 1 - rule.ticks : 0;
    for (size_t i = n - 1; i-- > from;) {
        if (reaches(recent[i], rule)) return false;
        if (recent[i].axes[rule.axis] == fix32{}) return true;
    }
    return false;
}

} // namespace framework::brawl
