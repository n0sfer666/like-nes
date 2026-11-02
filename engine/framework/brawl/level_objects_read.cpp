#include "level_objects_read.hpp"
#include "scene_paths.hpp"

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
    const fix32 cap = fix32::from_int(CONVEYOR_SPEED_MAX);
    if (c.speed == fix32{} || cap < c.speed || c.speed < -cap)
        return o.fail("property 'speed' is out of range; allowed -" + std::to_string(CONVEYOR_SPEED_MAX) + " to " +
                      std::to_string(CONVEYOR_SPEED_MAX) + " px per tick, not 0");
    out.conveyors.push_back(c);
    return true;
}

bool path(const SceneObject& o, LevelObjects& out) {
    Path p;
    if (!read_path(o, p)) return false;
    out.paths.push_back(p);
    return true;
}

bool autoscroll(const SceneObject& o, LevelObjects& out) {
    Autoscroll a;
    if (!read_autoscroll(o, a)) return false;
    out.autoscrolls.push_back(a);
    return true;
}

bool rope(const SceneObject& o, LevelObjects& out) {
    if (out.ropes.size() == ROPES_MAX)
        return o.fail("is rope " + std::to_string(ROPES_MAX + 1) + "; a level holds " + std::to_string(ROPES_MAX) +
                      " at most");
    Rope r{o.mark(), {}};
    if (!o.fix_prop("length", r.length)) return false;
    if (r.length < fix32::from_int(ROPE_LENGTH_MIN) || fix32::from_int(ROPE_LENGTH_MAX) < r.length)
        return o.fail("property 'length' is out of range; allowed " + std::to_string(ROPE_LENGTH_MIN) + " to " +
                      std::to_string(ROPE_LENGTH_MAX) + " px");
    const int64_t l = r.length.raw;
    if (r.mark.at.x.raw - l < INT32_MIN || INT32_MAX < r.mark.at.x.raw + l + 1 || INT32_MAX < r.mark.at.y.raw + l)
        return o.fail("swings past the fix32 range; x - length, x + length and y + length must fit");
    out.ropes.push_back(r);
    return true;
}

bool bounds(const SceneObject& o, LevelObjects& out) {
    if (out.bounds) return o.fail("is a second bounds; a level has one");
    out.bounds = o.area();
    return true;
}

constexpr ClassRule<LevelObjects> RULES[] = {
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

} // namespace

bool read_level_objects(const tilemap::ObjectMap& map, LevelObjects& out, std::string& error) {
    out = LevelObjects{};
    error.clear();
    LevelObjects level;
    if (!read_objects<LevelObjects>(map, RULES, level, error)) return false;
    if (!check_links(map, level, error)) return false;
    out = std::move(level);
    return true;
}

} // namespace framework::scene
