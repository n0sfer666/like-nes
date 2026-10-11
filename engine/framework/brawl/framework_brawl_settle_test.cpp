#include "framework_brawl_check.hpp"
#include "framework_brawl_scroll_fixture.hpp"

// Возврат сим-камеры к следованию после проезда (спека #25, В6г): шаг `max(|speed|, разрыв / 8)`,
// цель зажата `clamp_level`, пропущенная дорожка его не замораживает.
namespace {

using namespace framework::brawl;
using namespace framework::brawl::test;
using framework::scene::ScrollAxis;

// Игроки сразу после проезда впереди центра и идут вперёд быстрее его скорости: возврат не держит
// их у переднего края.
void walks_ahead() {
    Sections w;
    belt_lane(w, 2);
    place(w, 900, 1090);
    run(w, 209);
    for (int32_t t = 0; t < 40; ++t) {
        place(w, 1420 + 6 * t, 1560 + 6 * t);
        run(w, 1);
    }
    check(w.s.player(1).pos.x == at(1560 + 6 * 39), "the settling camera leashes players walking ahead");
}

// Середина игроков после проезда за `bounds` уровня: возврат кончается на клампе, а не тянется весь бой.
void settles_at_bounds() {
    Sections w;
    belt_lane(w, 2);
    w.s.level.bounds = area(0, 0, 1500, 400);
    place(w, 900, 1090);
    run(w, 209);
    check(!w.s.camera.riding && w.s.camera.center.x == at(1308), "the scroll does not end under the bounds clamp");
    place(w, 1400, 1500);
    run(w, 1);
    check(w.s.camera.settle.x == fix32{} && w.s.camera.center.x == at(1308),
          "the camera settles toward a goal past the bounds clamp");
}

// Дорожка `speed = 0`, пропущенная, пока центр возвращается после проезда, возврат не замораживает.
void zero_while_settling() {
    Sections w;
    belt_lane(w, 2);
    w.s.level.autoscrolls.push_back(lane(800, 1600, ScrollAxis::X, 0));
    place(w, 900, 1090);
    run(w, 209);
    for (uint32_t t = 0; t < 34; ++t) {
        place(w, 850, 950);
        run(w, 1);
    }
    check(w.s.camera.ride == 2 && w.s.camera.settle.x == fix32{} && w.s.camera.center.x == at(900),
          "a skipped scroll freezes the settling camera");
}

} // namespace

int main() {
    walks_ahead();
    settles_at_bounds();
    zero_while_settling();
    return verdict("framework-brawl-settle");
}
