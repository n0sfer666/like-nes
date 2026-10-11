#include "scene_links.hpp"

#include "scene_object.hpp"

namespace framework::scene {
namespace {

const Arena* arena_by_id(const LevelObjects& level, int32_t id) {
    for (const Arena& a : level.arenas)
        if (a.id == id) return &a;
    return nullptr;
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
    for (const WaveSpawn& s : level.waves)
        if (arena_by_id(level, s.arena) == nullptr)
            return SceneObject(map, *s.mark.object, error)
                .fail("points at arena " + std::to_string(s.arena) + "; the level has no arena with that id");
    return true;
}

} // namespace framework::scene
