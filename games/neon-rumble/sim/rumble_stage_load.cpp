#include "rumble_stage_load.hpp"

#include <string>

#include "level_load.hpp"
#include "level_objects_read.hpp"

namespace rumble {
namespace {

bool load(asset::BundleView bundle, const char* name, Stage& out, StageError& error) {
    namespace sc = framework::scene;
    sc::LoadError sdk;
    if (!copy_level_error(sc::open_level(bundle, name, out.level, sdk), sdk, error)) return false;
    std::string text;
    if (!sc::read_level_objects(out.level.object_map, out.objects, text))
        return stage_fail(error, StageFault::BadObjects, std::string("level ") + out.level.name + ": " + text);
    if (out.objects.bounds) {
        const auto& r = out.objects.bounds->rect;
        out.level.bounds = {r.x0, r.y0, r.x1, r.y1};
    }
    if (!copy_level_error(sc::load_textures(out.level, sdk), sdk, error)) return false;
    if (!out.objects.waves.empty() && !copy_level_error(sc::load_clips(out.level, sdk), sdk, error)) return false;
    return load_stage_fighters(out, error);
}

} // namespace

bool load_stage(asset::BundleView bundle, const char* name, Stage& out, StageError& error) {
    out = Stage{};
    error = StageError{};
    if (load(bundle, name, out, error)) return true;
    out = Stage{};
    return false;
}

} // namespace rumble
