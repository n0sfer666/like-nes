#include "framework_brawl_check.hpp"
#include "framework_brawl_scroll_fixture.hpp"

// Автоскролл сим-камеры (спека #25, В6г): старт на начале дорожки, постоянная скорость независимо от
// игроков, каждый прямоугольник один раз, `speed = 0` пропускается, знак — направление; по `y` — в
// platform. Край, который едет, и стена не пускают тело в зону — `crushed`, после проезда он гаснет.
namespace {

using namespace framework::brawl;
using namespace framework::brawl::test;
using framework::scene::ScrollAxis;

void starts_on_track() {
    Sections w;
    belt_lane(w, 2);
    run(w, 1);
    check(!w.s.camera.riding && w.s.camera.center.x == at(950), "the scroll starts before the track");
    place(w, 900, 1090);
    run(w, 1);
    check(w.s.camera.riding && w.s.camera.center.x == at(992), "crossing the track start does not start the scroll");
    for (uint32_t t = 0; t < 9; ++t) {
        place(w, 900 + 20 * static_cast<int32_t>(t), 1100);
        run(w, 1);
    }
    check(w.s.camera.center.x == at(1010), "the scroll speed depends on the players");
}

void once() {
    Sections w;
    belt_lane(w, 2);
    place(w, 900, 1090);
    run(w, 208);
    check(w.s.camera.riding && w.s.camera.center.x == at(1406), "the scroll ends early");
    run(w, 1);
    check(!w.s.camera.riding && w.s.camera.ride == 1 && w.s.camera.center.x == at(1408), "the scroll overruns the track");
    place(w, 1300, 1400);
    run(w, 1);
    check(w.s.camera.settle.x == at(2) && w.s.camera.center.x == fix32::from_raw(at(1400).raw + at(3).raw / 4),
          "the camera jumps back to the players after the scroll");
    run(w, 17);
    check(w.s.camera.settle.x == fix32{} && w.s.camera.center.x == at(1350),
          "the camera does not settle on the players after the scroll");
    place(w, 850, 950);
    run(w, 1);
    place(w, 950, 1090);
    run(w, 1);
    check(!w.s.camera.riding && w.s.camera.center.x == at(1020), "a passed scroll starts again");
}

void zero_speed() {
    Sections w;
    belt_lane(w, 0);
    w.s.level.autoscrolls.push_back(lane(800, 1600, ScrollAxis::X, 2));
    place(w, 900, 1090);
    run(w, 1);
    check(!w.s.camera.riding && w.s.camera.ride == 1 && w.s.camera.center.x == at(995), "a zero-speed scroll holds the camera");
    place(w, 900, 1000);
    run(w, 1);
    check(w.s.camera.riding && w.s.camera.center.x == at(992), "the scroll after a skipped one does not start");
}

void backward() {
    Sections w;
    belt_lane(w, -2);
    w.s.camera.center.x = at(1500);
    place(w, 1450, 1550);
    run(w, 1);
    check(!w.s.camera.riding, "a backward scroll starts at the near end");
    place(w, 1350, 1450);
    run(w, 2);
    check(w.s.camera.riding && w.s.camera.center.x == at(1406), "a negative speed does not scroll backward");
}

// Тело belt у стены пола, край едет по `x`: стена держит тело — `crushed`; без стены край толкает тело
// за собой. После проезда `crushed` гаснет.
void crushed_belt(bool wall) {
    Sections w;
    if (wall) check(w.s.arena.floor.add_wall({at(820), at(200), at(840), at(300)}), "the wall");
    w.s.level.autoscrolls.push_back(lane(800, 1600, ScrollAxis::X, 2));
    w.s.seat(0, 810);
    w.s.seat(1, 1180);
    bool crushed = false;
    for (uint32_t t = 0; t < 30; ++t) {
        w.step({});
        crushed = crushed || w.s.player(0).crushed;
    }
    const fix32 edge = w.s.camera.center.x - at(192);
    if (wall) check(crushed && w.s.player(0).pos.x == at(820), "the scroll edge does not crush a belt body at a wall");
    else check(!crushed && w.s.player(0).pos.x == edge, "the scroll edge crushes or drops a free belt body");
    run(w, 200);
    check(!w.s.camera.riding && !w.s.player(0).crushed, "crushed outlives the scroll");
}

// Тело platform у тайловой стены, край едет по `x`: контроллер не пускает тело в стену — `crushed`.
void crushed_platform(bool wall) {
    Sections w(wall ? std::vector<Solid>{{440, GRID_TOP, 452, FLOOR}} : std::vector<Solid>{});
    w.s.level.autoscrolls.push_back(lane(400, 1200, ScrollAxis::X, 2));
    w.s.seat(0, 420);
    w.s.seat(1, 770);
    bool crushed = false;
    for (uint32_t t = 0; t < 30; ++t) {
        w.step({});
        crushed = crushed || w.s.player(0).crushed;
    }
    check(w.platform_mode(w.s.player(0).id), "the probe body is not in platform");
    if (wall) check(crushed && w.s.player(0).pos.x < at(440 - 8), "the scroll edge pushes a platform body into a tile wall");
    else check(!crushed && about(w.s.player(0).pos.x, 458), "the scroll edge crushes or drops a free platform body");
}

// Дорожка по `y` в platform: центр следует за дорожкой, а не за телом; нижний край поднимает тело,
// потолок над головой не пускает — `crushed`.
void along_y(bool ceiling) {
    Sections w(ceiling ? std::vector<Solid>{{560, 196, 640, 204}} : std::vector<Solid>{});
    w.s.level.autoscrolls.push_back({area(400, 0, 800, 400), ScrollAxis::Y, at(-2)});
    w.s.seat(0, 600);
    w.s.camera.center = {at(600), at(400)};
    run(w, 1);
    check(w.s.camera.riding && w.s.camera.center.y == at(292), "a platform scroll along y does not start");
    run(w, 40);
    check(w.s.camera.center.y == at(212), "the y scroll follows the body instead of the track");
    bool crushed = false;
    for (uint32_t t = 0; t < 52; ++t) {
        w.step({});
        crushed = crushed || w.s.player(0).crushed;
    }
    const fix32 feet = w.s.player(0).pos.z - w.s.player(0).pos.y;
    check(!w.s.camera.riding && w.s.camera.center.y == at(108), "the y scroll does not end on its track");
    if (ceiling) check(crushed, "the scroll edge does not crush a body under the ceiling");
    else check(!crushed && about(feet, 216), "the bottom scroll edge does not lift a free body");
    run(w, 30);
    check(w.s.camera.settle.y == fix32{}, "the camera never settles after a y scroll");
}

} // namespace

int main() {
    starts_on_track();
    once();
    zero_speed();
    backward();
    crushed_belt(true);
    crushed_belt(false);
    crushed_platform(true);
    crushed_platform(false);
    along_y(true);
    along_y(false);
    return verdict("framework-brawl-scroll");
}
