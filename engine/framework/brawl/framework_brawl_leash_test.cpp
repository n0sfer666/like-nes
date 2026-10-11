#include "framework_brawl_check.hpp"
#include "framework_brawl_section_fixture.hpp"

// Поводок и переходы на краях (спека #25, В6г): вжатие у тела platform — факт скольжения, а не
// недоезд до цели; поводок не затирает отбрасывание тела в `hitstop`; лестница не держит вжатие
// прошлого тика; отказ перехода под едущим краем — `crushed`; тело, поставленное игрой на уступ или
// в воздух над ним, входит в platform там, где стоит.
namespace {

using namespace framework::brawl;
using namespace framework::brawl::test;
using framework::scene::ScrollAxis;

framework::scene::Autoscroll lane_x(int32_t x0) { return {area(x0, 0, 1600, 400), ScrollAxis::X, at(2)}; }

bool crushed_in(Sections& w, uint32_t ticks, EntId id) {
    bool crushed = false;
    for (uint32_t t = 0; t < ticks; ++t) {
        w.step({});
        crushed = crushed || w.get(id).crushed;
    }
    return crushed;
}

fix32 feet(const Body& b) { return b.pos.z - b.pos.y; }

// Подъём 45° перед телом platform у заднего края: склон — опора, а не стена, край тащит тело вверх.
void slope() {
    Sections w;
    const uint32_t floor = cell(FLOOR, GRID_TOP);
    for (uint32_t i = 0; i < 16; ++i) {
        const uint32_t x = cell(428, 0) + i;
        w.grid.fill(x, floor - 1 - i, x + 1, floor - i, framework::tilemap::TILE_SOLID | framework::tilemap::TILE_SLOPE);
        w.grid.fill(x, floor - i, x + 1, floor, framework::tilemap::TILE_SOLID);
    }
    w.s.level.autoscrolls.push_back(lane_x(400));
    const EntId id = w.s.seat(0, 420).id;
    w.s.seat(1, 770);
    const bool crushed = crushed_in(w, 30, id);
    check(w.platform_mode(id) && !crushed && feet(w.get(id)) < at(FLOOR - 8),
          "the scroll edge crushes a platform body on a slope instead of lifting it");
}

// Верхний край, едущий вниз, давит тело в пол: пол не пускает — `crushed`.
void floor_below() {
    Sections w;
    w.s.level.autoscrolls.push_back({area(400, 0, 800, 600), ScrollAxis::Y, at(2)});
    const EntId id = w.s.seat(0, 600).id;
    w.s.camera.center = {at(600), at(100)};
    check(crushed_in(w, 140, id) && w.s.camera.riding, "the top scroll edge presses a body into the floor without a crush");
}

// Тело platform в `hitstop` с подбросом в `pos`: поводок, сдвинувший его в этом тике, переносит
// подброс в контроллер, а не затирает зеркалом.
void knock_in_hitstop() {
    Sections w;
    Body& b = w.s.seat(0, 420);
    w.s.seat(1, 900);
    w.step({});
    w.step({});
    check(w.platform_mode(b.id), "the knocked body is not in platform");
    b.hitstop = 8;
    b.pos.vy = at(3);
    w.s.player(1).pos.x = at(960);
    const fix32 x = b.pos.x;
    bool flew = false;
    for (uint32_t t = 0; t < 12; ++t) {
        w.step({});
        flew = flew || fix32{} < b.pos.y;
    }
    check(x < b.pos.x && flew, "the leash erases the knock of a platform body in hitstop");
}

// Вжатие, поднятое поводком в прошлом тике, не переживает тик на лестнице.
void ladder() {
    Sections w;
    w.grid.fill(cell(592, 0), cell(GRID_TOP, GRID_TOP), cell(608, 0), cell(FLOOR, GRID_TOP),
                framework::tilemap::TILE_LADDER);
    const EntId id = w.body(600);
    w.step({});
    Command up;
    up.input.present = true;
    up.input.move.up_held = true;
    w.step({up});
    w.s.sections.find(id)->c.crushed = true;
    w.step({up});
    check(w.s.sections.find(id)->c.state == framework::character::MoveState::Ladder, "the probe body is not on the ladder");
    check(!w.get(id).crushed, "a crush of the last tick outlives a ladder tick");
}

// Верхний край, едущий вниз, давит тело belt за низ полосы пола — `crushed`.
void belt_floor() {
    Sections w;
    w.s.level.autoscrolls.push_back({area(0, 0, 400, 800), ScrollAxis::Y, at(2)});
    const EntId id = w.s.seat(0, 200).id;
    w.s.camera.center = {at(200), at(108)};
    check(crushed_in(w, 200, id) && w.get(id).pos.z == at(300), "the top scroll edge presses a belt body off the floor band");
}

// Вход в platform закрыт тайлом, задний край толкает тело belt внутрь: отказ возвращает его на край
// секции в том же тике, и это `crushed`, а не дрожь через тик. Край, ушедший за тайл, вход уже пускает.
void refused_entry() {
    Sections w({{400, GRID_TOP, 412, FLOOR}});
    w.s.level.autoscrolls.push_back(lane_x(200));
    const EntId id = w.s.seat(0, 380).id;
    w.s.seat(1, 560);
    const bool crushed = crushed_in(w, 110, id);
    check(crushed && !w.platform_mode(id) && w.get(id).pos.x < at(PLATFORM_X0),
          "a refused entry under the scroll edge leaves the body inside or does not crush it");
}

// Тело, поставленное на уступ выше полосы пола, входит с ногами на уступе, а не там, куда его зажал бы
// первый тик belt.
void spawn_on_ledge() {
    Sections w({{560, 180, 600, FLOOR}});
    const EntId id = w.body(580, 180);
    w.step({});
    const Body& b = w.get(id);
    check(w.platform_mode(id) && b.pos.x == at(580) && about(feet(b), 180),
          "a body spawned on a ledge does not enter platform where it stands");
}

// Тело, поставленное в воздух над уступом с подбросом, входит в platform сразу: взлетает с высоты
// постановки и падает на уступ, а не на полосу пола.
void spawn_in_air() {
    Sections w({{560, 180, 600, FLOOR}});
    const EntId id = w.body(580, 180);
    w.get(id).pos.y = at(40);
    w.get(id).pos.vy = at(2);
    w.step({});
    const bool rose = feet(w.get(id)) < at(140);
    for (uint32_t t = 0; t < 40; ++t) w.step({});
    const Body& b = w.get(id);
    check(rose && w.platform_mode(id) && b.pos.x == at(580) && about(feet(b), 180),
          "a body spawned in the air over a ledge does not land on it");
}

// Тело без архетипа не шагает, но вжатие прошлого тика у него не остаётся.
void no_kind() {
    Sections w;
    Body& b = w.get(w.body(300));
    b.kind = 200;
    b.crushed = true;
    step_one(b, Command{}, w.world());
    check(!b.crushed, "a crush outlives the tick of a body without a kind");
}

} // namespace

int main() {
    slope();
    floor_below();
    knock_in_hitstop();
    ladder();
    belt_floor();
    refused_entry();
    spawn_on_ledge();
    spawn_in_air();
    no_kind();
    return verdict("framework-brawl-leash");
}
