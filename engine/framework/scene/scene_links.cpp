#include "scene_links.hpp"

#include "scene_object.hpp"

namespace framework::scene {
namespace {

const Arena* arena_by_id(const LevelObjects& level, int32_t id) {
    for (const Arena& a : level.arenas)
        if (a.id == id) return &a;
    return nullptr;
}

const Checkpoint* checkpoint_by_order(const LevelObjects& level, int32_t order) {
    for (const Checkpoint& c : level.checkpoints)
        if (c.order == order) return &c;
    return nullptr;
}

// Тот же суд, что `brawl::mode_at`: решает первая секция, накрывшая `x`, вне секций — belt. Платформа
// по пути — тело контроллера #16, а в belt его не читает никто: путь там ехал бы невидимым для драки.
bool in_platform(const LevelObjects& level, fix32 x) {
    for (const Section& s : level.sections)
        if (!(x < s.area.rect.x0) && x < s.area.rect.x1) return s.mode == SectionMode::Platform;
    return false;
}

} // namespace

bool check_links(const tilemap::ObjectMap& map, const LevelObjects& level, std::string& error) {
    for (const Arena& a : level.arenas) {
        const Arena* first = arena_by_id(level, a.id);
        if (first != &a)
            return SceneObject(map, *a.area.object, error)
                .fail("repeats arena id " + std::to_string(a.id) + " of object id " +
                      std::to_string(first->area.object->id));
    }
    // Респаун ищет точку по `order`: из двух с одним `order` вторая не нашлась бы никогда.
    for (const Checkpoint& c : level.checkpoints) {
        const Checkpoint* first = checkpoint_by_order(level, c.order);
        if (first != &c)
            return SceneObject(map, *c.mark.object, error)
                .fail("repeats checkpoint order " + std::to_string(c.order) + " of object id " +
                      std::to_string(first->mark.object->id));
    }
    for (const WaveSpawn& s : level.waves)
        if (arena_by_id(level, s.arena) == nullptr)
            return SceneObject(map, *s.mark.object, error)
                .fail("points at arena " + std::to_string(s.arena) + "; the level has no arena with that id");
    for (const Path& p : level.paths)
        for (uint8_t i = 0; i < p.count; ++i)
            if (!in_platform(level, p.points[i].x))
                return SceneObject(map, *p.object, error)
                    .fail("has vertex " + std::to_string(i) + " outside a platform section; a path moves only there");
    return true;
}

} // namespace framework::scene
