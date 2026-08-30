#include <cstdio>

#include "camera.hpp"
#include "hash_mix.hpp"
#include "platform_args.hpp"

// ГОЛДЕН траектории камеры (гейт 5 спеки #17): 240 тиков со всеми политиками разом, свёрнутые в
// одно число. Он отвечает «три машины сошлись», а НЕ «сошлись на верном» — за верность отвечают
// гейты политик и сетки, где утверждения именные. Обратное тоже верно: те гейты сравнивают числа,
// вычисленные тут же, и разъехавшуюся между x86 и ARM арифметику не заметили бы.
//
// Свёртка берётся из `hash_mix.hpp` физики, чтобы голденов на разных константах не заводилось: два
// таких несравнимы, и разошедшийся отвечает «где-то», а не «здесь».
namespace {

using namespace framework;
using namespace framework::graphics;
using framework::physics::FNV_OFFSET;
using framework::physics::mix;

fix32 fx(int32_t v) { return fix32::from_int(v); }

// Два канала, а не один: центр состояния и центр отрисовки. Одним каналом собственный контроль
// теста был бы слабее, чем выглядит, — тряска и привязка к сетке живут ТОЛЬКО во втором, и голден,
// читающий их сумму, зеленел бы при мёртвой тряске.
struct Fold {
    uint64_t center = FNV_OFFSET;
    uint64_t view = FNV_OFFSET;
};

Fold run(int32_t shift, bool with_shake) {
    CameraConfig cfg{};
    cfg.policies = CAMERA_DEADZONE | CAMERA_SPEED_LIMIT | CAMERA_LOOK_AHEAD | CAMERA_BOUNDS |
                   CAMERA_PIXEL_PERFECT;
    cfg.half_view = Vec2{fx(20), fx(12)};
    cfg.dead_half = Vec2{fx(3), fx(2)};
    cfg.max_speed = fix32::from_raw(fix32::ONE * 5 / 2);
    cfg.look_ahead = fx(6);
    cfg.look_rate = fix32::from_raw(fix32::ONE / 2);
    cfg.bounds = CameraBounds{fx(0), fx(0), fx(200), fx(90)};
    cfg.pixels_per_unit = 16;

    Camera c{};
    if (with_shake) camera_shake(c, 120, fx(3), 4242);

    Fold f;
    for (int32_t t = 0; t < 240; ++t) {
        const int32_t phase = (t + shift) % 120;
        const int32_t x = phase < 60 ? phase * 3 : (120 - phase) * 3;
        const int32_t facing = phase < 60 ? 1 : -1;
        camera_follow(c, cfg, Vec2{fx(x), fx(30 + phase % 7)}, facing);
        mix(f.center, static_cast<uint32_t>(c.center.x.raw));
        mix(f.center, static_cast<uint32_t>(c.center.y.raw));
        const Vec2 v = camera_view_center(c, cfg, static_cast<uint64_t>(t));
        mix(f.view, static_cast<uint32_t>(v.x.raw));
        mix(f.view, static_cast<uint32_t>(v.y.raw));
    }
    return f;
}

} // namespace

int main(int argc, char** argv) {
    platform::Args utf8_argv(argc, argv);
    std::printf("camera trajectory over 240 ticks, every policy on\n");

    const Fold base = run(0, true);
    std::printf("camera-path hash = 0x%016llx\n", static_cast<unsigned long long>(base.center));
    std::printf("camera-view hash = 0x%016llx\n", static_cast<unsigned long long>(base.view));

    int fails = 0;
    const Fold shifted = run(1, true);
    if (base.center == shifted.center) {
        std::printf("  FAIL: the fold does not read the camera centre\n");
        ++fails;
    }
    if (base.view == shifted.view) {
        std::printf("  FAIL: the fold does not read the view centre\n");
        ++fails;
    }
    if (base.center == base.view) {
        std::printf("  FAIL: the view centre equals the state centre, so shake and grid are dead\n");
        ++fails;
    }
    // Прогон БЕЗ заказа тряски. Утверждение выше отделяет вид от состояния и на мёртвой тряске:
    // привязка к сетке одна разводит их каналы. Различает мёртвую тряску только эта пара — и она
    // же повторяет «тряска не пишет в симуляцию» на длине голдена, а не одного тика.
    const Fold quiet = run(0, false);
    if (base.view == quiet.view) {
        std::printf("  FAIL: ordering a shake does not change the view at all\n");
        ++fails;
    }
    if (!(base.center == quiet.center)) {
        std::printf("  FAIL: ordering a shake moved the simulation centre\n");
        ++fails;
    }
    std::printf("framework-graphics-path: %s\n", fails == 0 ? "PASS" : "FAIL");
    return fails == 0 ? 0 : 1;
}
