#pragma once
#include <memory>
#include <optional>

#include "depth_floor.hpp"
#include "platform_world.hpp"
#include "rumble_level.hpp"
#include "scene_objects.hpp"
#include "stage_movers.hpp"
#include "stage_ropes.hpp"

namespace rumble {

// Платформенные секции уровня (спека #25, В8): мир контроллера #16 по слою collision, лифты по `path`
// и тросы. Без platform-секций всё пусто, и драка идёт без них. Лифты — в куче: `world.scene.world`
// держит адрес их физмира, а `Wave` при открытии переносится.
struct Platform {
    std::unique_ptr<framework::brawl::StageMovers> movers;
    std::optional<framework::brawl::StageRopes> ropes;
    std::optional<framework::brawl::PlatformWorld> world;

    bool open(const Level& lv, const framework::scene::LevelObjects& level);
    // Полоса уровня кончается перед platform-секцией: иначе враги последней арены уходили бы за её
    // край. Пока есть незачищенная арена, полоса — из уровня; после — тянется до конца секций.
    framework::brawl::DepthFloor reach(const framework::brawl::DepthFloor& floor,
                                       const framework::scene::LevelObjects& level, int16_t cleared) const;
};

} // namespace rumble
