#include <cstdio>

#include "camera.hpp"
#include "platform_args.hpp"

// Гейт ПОЛИТИК камеры (шаг C вертикали 1, гейт 5 спеки #17): мёртвая зона, потолок скорости,
// look-ahead и границы уровня — каждая по отдельности и в паре с прогоном, где она выключена.
//
// Пара здесь не формальность. «Камера доехала до цели» — правда и про камеру, которая игнорирует
// мёртвую зону; «камера не доехала» — правда и про сломанное следование. Различает их только
// прогон тех же данных с другим набором разрядов.
namespace {

using namespace framework;
using namespace framework::graphics;

int fails = 0;

void check(bool ok, const char* what) {
    if (!ok) {
        std::printf("  FAIL: %s\n", what);
        ++fails;
    }
}

void same(fix32 got, fix32 want, const char* what) {
    if (got == want) return;
    std::printf("  FAIL: %s: got %.4f (raw %d), want %.4f (raw %d)\n", what, got.to_double(),
                got.raw, want.to_double(), want.raw);
    ++fails;
}

fix32 fx(int32_t v) { return fix32::from_int(v); }

// 1. Без политик камера садится на цель ровно и за один тик — эталон, относительно которого
// измеряется каждая задержка ниже.
void test_plain() {
    Camera c{};
    CameraConfig cfg{};
    camera_follow(c, cfg, Vec2{fx(30), fx(-7)}, 0);
    same(c.center.x, fx(30), "a camera with no policies lands on the target");
    same(c.center.y, fx(-7), "on both axes");
}

// 2. Мёртвая зона: внутри неё камера не двигается вовсе, за её краем — ровно на превышение.
void test_deadzone() {
    CameraConfig cfg{};
    cfg.policies = CAMERA_DEADZONE;
    cfg.dead_half = Vec2{fx(4), fx(3)};

    Camera c{};
    camera_follow(c, cfg, Vec2{fx(3), fx(2)}, 0);
    same(c.center.x, fx(0), "inside the dead zone the camera does not move");
    same(c.center.y, fx(0), "on either axis");

    camera_follow(c, cfg, Vec2{fx(10), fx(0)}, 0);
    same(c.center.x, fx(6), "past the edge it moves by the excess, keeping the target on the edge");

    camera_follow(c, cfg, Vec2{fx(-10), fx(0)}, 0);
    same(c.center.x, fx(-6), "and the same in the other direction");

    Camera plain{};
    CameraConfig off{};
    camera_follow(plain, off, Vec2{fx(3), fx(2)}, 0);
    same(plain.center.x, fx(3), "without the policy the same target does move the camera");
}

// 3. Потолок скорости режет ШАГ, а не координату: по диагонали ограничен модуль, а не каждая ось.
void test_speed_limit() {
    CameraConfig cfg{};
    cfg.policies = CAMERA_SPEED_LIMIT;
    cfg.max_speed = fx(2);

    Camera c{};
    camera_follow(c, cfg, Vec2{fx(100), fx(0)}, 0);
    same(c.center.x, fx(2), "one tick moves exactly one speed step");
    for (int i = 0; i < 3; ++i) camera_follow(c, cfg, Vec2{fx(100), fx(0)}, 0);
    same(c.center.x, fx(8), "and three more move three");

    Camera diag{};
    camera_follow(diag, cfg, Vec2{fx(100), fx(100)}, 0);
    check(length(diag.center).raw <= fx(2).raw, "diagonally the LENGTH of the step is capped, not each axis");
    check(fx(0) < diag.center.x && fx(0) < diag.center.y, "and the direction is kept");

    Camera free_{};
    CameraConfig none = cfg;
    none.max_speed = fix32{};
    camera_follow(free_, none, Vec2{fx(100), fx(0)}, 0);
    same(free_.center.x, fx(100), "a zero ceiling means no ceiling, and the camera lands at once");
}

// 4. Look-ahead ведёт камеру по ВЗГЛЯДУ и подходит к нему со своей скоростью.
void test_look_ahead() {
    CameraConfig cfg{};
    cfg.policies = CAMERA_LOOK_AHEAD;
    cfg.look_ahead = fx(8);
    cfg.look_rate = fx(2);

    Camera c{};
    camera_follow(c, cfg, Vec2{}, 1);
    same(c.center.x, fx(2), "the first tick moves one rate step, not the whole offset");
    for (int i = 0; i < 3; ++i) camera_follow(c, cfg, Vec2{}, 1);
    same(c.center.x, fx(8), "four ticks of rate 2 reach the full look-ahead");
    for (int i = 0; i < 8; ++i) camera_follow(c, cfg, Vec2{}, -1);
    same(c.center.x, fx(-8), "turning around walks it to the other side");
    for (int i = 0; i < 4; ++i) camera_follow(c, cfg, Vec2{}, 0);
    same(c.center.x, fx(0), "no facing at all brings it back to the target");

    Camera off{};
    CameraConfig plain{};
    camera_follow(off, plain, Vec2{}, 1);
    same(off.center.x, fx(0), "without the policy facing moves nothing");
}

// 5. Границы уровня режут ЦЕНТР с учётом половины вида, а уровень уже вида центрируется.
void test_bounds() {
    CameraConfig cfg{};
    cfg.policies = CAMERA_BOUNDS;
    cfg.half_view = Vec2{fx(10), fx(6)};
    cfg.bounds = CameraBounds{fx(0), fx(0), fx(100), fx(60)};

    Camera c{};
    camera_follow(c, cfg, Vec2{fx(-50), fx(-50)}, 0);
    same(c.center.x, fx(10), "the left edge of the level stays at the left edge of the view");
    same(c.center.y, fx(6), "and the top edge likewise");
    camera_follow(c, cfg, Vec2{fx(500), fx(500)}, 0);
    same(c.center.x, fx(90), "the right edge the same way");
    same(c.center.y, fx(54), "and the bottom");

    CameraConfig narrow = cfg;
    narrow.bounds = CameraBounds{fx(0), fx(0), fx(8), fx(60)};
    Camera n{};
    camera_follow(n, narrow, Vec2{fx(500), fx(0)}, 0);
    same(n.center.x, fx(4), "a level narrower than the view is centred, not pinned to an edge");
    camera_follow(n, narrow, Vec2{fx(-500), fx(0)}, 0);
    same(n.center.x, fx(4), "from either side");

    Camera off{};
    CameraConfig plain{};
    camera_follow(off, plain, Vec2{fx(-50), fx(-50)}, 0);
    same(off.center.x, fx(-50), "without the policy the camera walks off the level");
}

// 6. ПОРЯДОК политик — утверждение, а не комментарий в заголовке. Границы режут «куда» ДО потолка
// скорости, поэтому у края уровня весь бюджет скорости уходит в свободную ось. Переставь их — и
// потолок посчитает потраченной ту часть шага, которую границы потом срежут: камера у стены поедет
// вдоль неё медленнее собственного потолка. Пара — тот же тик без границ, где отставание и видно.
void test_order() {
    CameraConfig cfg{};
    cfg.policies = CAMERA_BOUNDS | CAMERA_SPEED_LIMIT;
    cfg.half_view = Vec2{fx(10), fx(6)};
    cfg.bounds = CameraBounds{fx(0), fx(0), fx(100), fx(60)};
    cfg.max_speed = fx(2);

    Camera c{};
    c.center = Vec2{fx(10), fx(30)};
    camera_follow(c, cfg, Vec2{fx(-100), fx(300)}, 0);
    same(c.center.x, fx(10), "pressed against the wall the camera stays on it");
    same(c.center.y, fx(32), "and the WHOLE speed budget goes into the free axis");

    Camera lagging{};
    CameraConfig no_bounds = cfg;
    no_bounds.policies = CAMERA_SPEED_LIMIT;
    lagging.center = Vec2{fx(10), fx(30)};
    camera_follow(lagging, no_bounds, Vec2{fx(-100), fx(300)}, 0);
    check(lagging.center.y < fx(32),
          "without the bounds the same tick spends part of the budget on the axis that is walled");

    // Второй срез, после шага: камера, начавшая ВНЕ границ, обязана оказаться внутри тем же тиком,
    // а не подъезжать к ним со скоростью потолка. Срез «куда» этого сам по себе не даёт.
    Camera outside{};
    outside.center = Vec2{fx(-500), fx(-500)};
    camera_follow(outside, cfg, Vec2{fx(50), fx(30)}, 0);
    same(outside.center.x, fx(10), "a camera starting outside the level is inside it after one tick");
    same(outside.center.y, fx(6), "on both axes");
}

} // namespace

int main(int argc, char** argv) {
    platform::Args utf8_argv(argc, argv);
    std::printf("graphics camera policies\n");
    test_plain();
    test_deadzone();
    test_speed_limit();
    test_look_ahead();
    test_bounds();
    test_order();
    std::printf("framework-graphics-camera: %s\n", fails == 0 ? "PASS" : "FAIL");
    return fails == 0 ? 0 : 1;
}
