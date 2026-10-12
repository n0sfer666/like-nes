#include "rumble_play.hpp"

#include "level_load.hpp"
#include "rumble_roster.hpp"

namespace rumble {

// docs:begin(level-enter)
std::unique_ptr<Play> enter_level(const asset::BundleView& bundle, const char* name, bool waves, std::string& error) {
    auto play = std::make_unique<Play>();
    framework::scene::LoadError load;
    if (!framework::scene::load_level(bundle, name, play->level, load)) {
        error = load.text;
        return nullptr;
    }
    for (uint32_t i = 0; i < FIGHTERS; ++i)
        if (!play->fighters[i].open(play->level, ROSTER[i].fighter)) {
            error = std::string("fighter ") + ROSTER[i].fighter + " does not open";
            return nullptr;
        }
    if (!play->brawl.open(play->level, play->fighters, waves)) {
        error = std::string("brawl of level ") + name + " does not open";
        return nullptr;
    }
    if (!play->scene.init(play->brawl)) {
        error = "schedule of the level does not build";
        return nullptr;
    }
    return play;
}
// docs:end(level-enter)

} // namespace rumble
