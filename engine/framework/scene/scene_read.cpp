#include "scene_read.hpp"

#include <cstdint>
#include <string>
#include <utility>

#include "scene_links.hpp"
#include "scene_object.hpp"

namespace framework::scene {
namespace {

using tilemap::ObjectShape;

template <std::vector<Area> LevelObjects::*List>
bool area_into(const SceneObject& o, LevelObjects& out) {
    (out.*List).push_back(o.area());
    return true;
}

bool section(const SceneObject& o, LevelObjects& out) {
    uint8_t mode = 0;
    if (!o.word_prop("mode", {"belt", "platform"}, mode)) return false;
    out.sections.push_back({o.area(), static_cast<SectionMode>(mode)});
    return true;
}

bool arena(const SceneObject& o, LevelObjects& out) {
    Arena a{o.area(), 0};
    if (!o.int_prop("id", a.id)) return false;
    // Сим-камера драки хранит запертую арену в `int16_t`, и −1 там — «арена не заперта»: id вне
    // [0, INT16_MAX] либо совпал бы с этим знаком, либо не запирался бы вовсе.
    if (!o.within("id", a.id, 0, INT16_MAX)) return false;
    out.arenas.push_back(a);
    return true;
}

bool spawn(const SceneObject& o, LevelObjects& out) {
    if (!o.has("wave")) {
        out.players.push_back(o.mark());
        return true;
    }
    WaveSpawn s{o.mark(), 0, 0, {}, 1};
    if (!o.int_prop("wave", s.wave) || !o.int_prop("arena", s.arena) || !o.text_prop("kind", s.kind) ||
        !o.optional_int("players", s.players))
        return false;
    if (!o.at_least("wave", s.wave, 0) || !o.at_least("players", s.players, 1)) return false;
    if (s.kind.empty()) return o.fail("property 'kind' is empty");
    out.waves.push_back(s);
    return true;
}

bool checkpoint(const SceneObject& o, LevelObjects& out) {
    Checkpoint c{o.mark(), 0};
    if (!o.int_prop("order", c.order)) return false;
    // Прогресс уровня хранит взятый `order` в `int16_t`, и −1 там — «ни одного не взято»: точка вне
    // [0, INT16_MAX] никогда не взялась бы, и уровень молча терял бы её.
    if (!o.within("order", c.order, 0, INT16_MAX)) return false;
    out.checkpoints.push_back(c);
    return true;
}

bool conveyor(const SceneObject& o, LevelObjects& out) {
    Conveyor c{o.area(), {}};
    if (!o.fix_prop("speed", c.speed)) return false;
    out.conveyors.push_back(c);
    return true;
}

bool path(const SceneObject& o, LevelObjects& out) {
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
    out.paths.push_back(p);
    return true;
}

bool autoscroll(const SceneObject& o, LevelObjects& out) {
    uint8_t axis = 0;
    Autoscroll a{o.area(), ScrollAxis::X, {}};
    if (!o.word_prop("axis", {"x", "y"}, axis) || !o.fix_prop("speed", a.speed)) return false;
    a.axis = static_cast<ScrollAxis>(axis);
    out.autoscrolls.push_back(a);
    return true;
}

bool rope(const SceneObject& o, LevelObjects& out) {
    Rope r{o.mark(), {}};
    if (!o.fix_prop("length", r.length)) return false;
    out.ropes.push_back(r);
    return true;
}

bool bounds(const SceneObject& o, LevelObjects& out) {
    if (out.bounds) return o.fail("is a second bounds; a level has one");
    out.bounds = o.area();
    return true;
}

struct ClassRule {
    std::string_view cls;
    ObjectShape shape;
    bool (*read)(const SceneObject&, LevelObjects&);
};

constexpr ClassRule RULES[] = {
    {"section", ObjectShape::Rect, section},
    {"depth_band", ObjectShape::Rect, area_into<&LevelObjects::depth_bands>},
    {"wall", ObjectShape::Rect, area_into<&LevelObjects::walls>},
    {"arena", ObjectShape::Rect, arena},
    {"spawn", ObjectShape::Point, spawn},
    {"checkpoint", ObjectShape::Point, checkpoint},
    {"kill", ObjectShape::Rect, area_into<&LevelObjects::kills>},
    {"conveyor", ObjectShape::Rect, conveyor},
    {"path", ObjectShape::Polygon, path},
    {"autoscroll", ObjectShape::Rect, autoscroll},
    {"rope", ObjectShape::Point, rope},
    {"bounds", ObjectShape::Rect, bounds},
};

const ClassRule* rule_of(std::string_view cls) {
    for (const ClassRule& r : RULES)
        if (r.cls == cls) return &r;
    return nullptr;
}

} // namespace

bool read_level_objects(const tilemap::ObjectMap& map, LevelObjects& out, std::string& error) {
    out = LevelObjects{};
    error.clear();
    LevelObjects level;
    for (const tilemap::MapObject& m : map.objects) {
        const ClassRule* rule = rule_of(tilemap::object_text(map, m.class_offset));
        if (rule == nullptr) continue;
        const SceneObject o(map, m, error);
        if (!o.shape(rule->shape) || !rule->read(o, level)) return false;
    }
    if (!check_links(map, level, error)) return false;
    out = std::move(level);
    return true;
}

} // namespace framework::scene
