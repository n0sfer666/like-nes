#include <cstdio>

#include "autoscroll_step.hpp"
#include "rumble_scene_level_fixture.hpp"
#include "rumble_wave_fixture.hpp"
#include "viewport_settings.hpp"

namespace {
namespace br = framework::brawl;
namespace gr = framework::graphics;
using namespace br::test;
int fails = 0;
void expect(bool ok, const char* why) { if (!ok) { ++fails; std::printf("  FAIL: %s\n", why); } }
constexpr gr::PixelSize ZONE{640, 360};

bool camera_uses(gr::PixelSize zone) {
    Stage s;
    s.seat(0, 50);
    s.level.bounds = area(0, 0, 1600, 960);
    br::BrawlWorld w = s.world();
    w.zone = zone;
    br::step_camera(s.camera, s.pool, s.seats, s.sections, w);
    return s.camera.center.x == at(320) && s.camera.center.y == at(180);
}

bool scroll_uses(gr::PixelSize zone) {
    framework::scene::LevelObjects level;
    level.bounds = area(0, 0, 1600, 960);
    level.autoscrolls.push_back({area(0, 0, 1600, 960), framework::scene::ScrollAxis::X, at(1)});
    br::StageCamera camera;
    camera.center = {at(320), at(180)};
    const br::Ride ride = br::ride_scroll(camera, {at(319), at(180)}, level, zone);
    if (!ride.on || camera.center.x != at(320)) return false;
    br::ride_scroll(camera, camera.center, level, zone);
    return camera.center.x == at(321);
}

bool waves_use(gr::PixelSize zone) {
    Waves w(591);
    w.s.camera.center.x = at(592);
    br::step_waves(w.waves, w.s.camera, w.s.pool, w.s.arena.kinds, w.s.seats, w.s.level, w.rules, w.facts, zone);
    if (w.s.camera.arena != br::NO_ARENA) return false;
    w.s.camera.center.x = at(700);
    br::step_waves(w.waves, w.s.camera, w.s.pool, w.s.arena.kinds, w.s.seats, w.s.level, w.rules, w.facts, zone);
    return w.s.camera.arena == 0 && w.facts.locked;
}

void stage_settings() {
    std::vector<asset::AssetInput> assets;
    std::string error;
    asset::BundleView bundle;
    expect(level_fixture::bake(level_fixture::source(), assets, error), "game fixture bakes");
    std::vector<uint8_t> settings;
    expect(gr::bake_viewport_settings({ZONE, {960, 540}}, settings, error), "settings fixture bakes");
    assets.push_back(level_fixture::table("viewport", settings));
    rumble::Stage stage;
    rumble::StageError load;
    expect(level_fixture::open(assets, bundle) && rumble::load_stage(bundle, "lvl", stage, load) &&
           stage.viewport.zone.w == 640 && stage.viewport.limit.h == 540,
           "the game stage explicitly reads SDK settings from its bundle");
    auto* viewport = level_fixture::find(assets, "viewport");
    if (viewport) viewport->payload[0] ^= 1;
    expect(level_fixture::open(assets, bundle) && !rumble::load_stage(bundle, "lvl", stage, load) &&
           load.fault == rumble::StageFault::BadViewport && !stage.level.bundle.valid() &&
           gr::viewport_settings_default(stage.viewport), "corrupt settings refuse and reset the entire game stage");
}

} // namespace

int main() {
    expect(camera_uses(ZONE) && !camera_uses(gr::VIEW_ZONE), "control: simulation camera uses the configured half-view");
    expect(scroll_uses(ZONE) && !scroll_uses(gr::VIEW_ZONE), "control: autoscroll starts at the configured zone edge");
    expect(waves_use(ZONE) && !waves_use(gr::VIEW_ZONE), "control: arena crossing uses the configured zone");
    stage_settings();
    std::printf("rumble-zone: %s\n", fails == 0 ? "PASS" : "FAIL");
    return fails == 0 ? 0 : 1;
}
