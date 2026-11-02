#include "scene_paths.hpp"

namespace framework::scene {

bool read_path(const SceneObject& o, Path& out) {
    Path p;
    p.object = &o.object();
    if (!o.fix_prop("speed", p.speed) || !o.bool_prop("loop", p.loop) || !o.int_prop("w", p.w) ||
        !o.int_prop("h", p.h) || !o.at_least("w", p.w, 1) || !o.at_least("h", p.h, 1))
        return false;
    if (!(fix32{} < p.speed) || fix32::from_int(PATH_SPEED_MAX) < p.speed)
        return o.fail("property 'speed' is out of range; allowed above 0 up to " + std::to_string(PATH_SPEED_MAX) +
                      " px per tick");
    if (o.vertices().size() > p.points.size())
        return o.fail("has " + std::to_string(o.vertices().size()) + " vertices; allowed " +
                      std::to_string(p.points.size()) + " at most");
    const Vec2 origin = o.mark().at;
    for (const tilemap::ObjectVertex& v : o.vertices())
        p.points[p.count++] = origin + Vec2{fix32::from_raw(v.x_raw), fix32::from_raw(v.y_raw)};
    out = p;
    return true;
}

bool read_autoscroll(const SceneObject& o, Autoscroll& out) {
    uint8_t axis = 0;
    Autoscroll a{o.area(), ScrollAxis::X, {}};
    if (!o.word_prop("axis", {"x", "y"}, axis) || !o.fix_prop("speed", a.speed)) return false;
    a.axis = static_cast<ScrollAxis>(axis);
    out = a;
    return true;
}

} // namespace framework::scene
