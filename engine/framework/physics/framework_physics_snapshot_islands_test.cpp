#include <cstdint>
#include <cstdio>

#include "snapshot.hpp"
#include "units.hpp"
#include "world.hpp"

// Связность островов — состояние, а не выводимое заново, и это утверждается ОТДЕЛЬНОЙ целью.
//
// `Islands` пересобирается в последней строке шага, поэтому «раз считается заново — копировать не
// надо» звучит убедительно и неверно: `RestTracker::wake_touched` читает его в НАЧАЛЕ следующего
// шага, ещё прошлым, и метит пробуждение по КОРНЮ острова (`wake_[islands.root(i)]`). Снимок,
// потерявший связность, возвращает мир с чужой картиной соседства — и правка одного ящика поднимает
// башню на другом конце уровня.
//
// Сцена собрана ровно под это и никуда больше не годится: две башни, замершие ПОРОЗНЬ, — снимок, —
// сдвиг второй башни вплотную к первой, где они замирают ОДНИМ островом, — возврат, — правка ящика
// второй башни. Верная связность поднимает только её; связность, оставшаяся от слияния, поднимает
// обе, потому что корень у них теперь общий.
//
// Общая сцена снимка (`framework_physics_snapshot_scene.hpp`) этого не ловит и поймать не может: там
// все подвижные тела к концу прогона лежат одной кучей, то есть разбиение в точке снятия и в конце
// совпадает, и выброшенное поле сравнивать не с чем. Измерено, а не предположено — до этой цели
// выброс `islands_` из снимка проходил молча.
namespace {

using framework::physics::BodyDesc;
using framework::physics::BodyId;
using framework::physics::BodyType;
using framework::physics::World;
using framework::physics::WorldSnapshot;
using framework::physics::box;

constexpr fix32 DT = fix32::from_float(1.0 / 60.0);
constexpr fix32 HALF = fix32::from_int(8);
constexpr fix32 FLOOR_TOP = fix32::from_int(192);
constexpr fix32 GRIP = fix32::from_float(0.6);
constexpr uint32_t PER_TOWER = 2;
// Окно — граница отказа, а не ожидание: башни замирают за пару десятков кадров.
constexpr uint32_t WINDOW = 10 * 60;
constexpr int32_t FAR_X = 60;
constexpr int32_t NEAR_X = 16;

int fails = 0;

void check(bool ok, const char* what) {
    if (!ok) {
        std::printf("  FAIL: %s\n", what);
        ++fails;
    }
}

void add_tower(World& w, uint32_t key, int32_t x) {
    for (uint32_t i = 0; i < PER_TOWER; ++i) {
        BodyDesc b;
        b.key = key + i;
        b.shape = box(HALF, HALF);
        b.position = {fix32::from_int(x),
                      FLOOR_TOP - HALF - fix32::from_int(static_cast<int32_t>(i) * 16)};
        b.mass = fix32::from_int(4);
        b.material = {fix32{}, GRIP};
        w.add(b);
    }
}

void build(World& w) {
    BodyDesc floor;
    floor.key = 1;
    floor.type = BodyType::Static;
    floor.shape = box(fix32::from_int(128), fix32::from_int(8));
    floor.position = {fix32{}, fix32::from_int(200)};
    floor.material = {fix32{}, GRIP};
    w.add(floor);
    add_tower(w, 10, -FAR_X);
    add_tower(w, 20, FAR_X);
}

// Индексы тел: 0 — пол, 1..2 — левая башня, 3..4 — правая.
constexpr uint32_t LEFT = 1;
constexpr uint32_t RIGHT = 1 + PER_TOWER;

bool tower_at_rest(const World& w, uint32_t first) {
    for (uint32_t i = 0; i < PER_TOWER; ++i) {
        if (!w.at_rest(BodyId{first + i})) return false;
    }
    return true;
}

// Возвращает кадр, на котором замерли обе башни, или 0. Ноль — находка: сцена без замирания не
// проверяет здесь ничего.
uint32_t settle(World& w) {
    for (uint32_t i = 0; i < WINDOW; ++i) {
        w.step(DT);
        if (tower_at_rest(w, LEFT) && tower_at_rest(w, RIGHT)) return i + 1;
    }
    return 0;
}

// Сдвиг башни вплотную к соседней — через ту же неконстантную ручку, которой двигает игра.
void shove(World& w, uint32_t first, int32_t x) {
    for (uint32_t i = 0; i < PER_TOWER; ++i) {
        w.mutate(BodyId{first + i}).position.x = fix32::from_int(x);
    }
}

} // namespace

int main() {
    std::printf("physics world snapshot: island connectivity survives a rollback\n");

    World w{32};
    build(w);
    const uint32_t apart = settle(w);
    check(apart != 0, "the two towers freeze while standing apart");
    WorldSnapshot snap;
    snap.capture(w);

    // Башни сводятся вместе и замирают ОДНИМ островом: корень у всех четырёх ящиков становится общим.
    shove(w, RIGHT, -FAR_X + NEAR_X);
    const uint32_t together = settle(w);
    check(together != 0, "the towers freeze again once shoved together");

    snap.apply(w);
    check(tower_at_rest(w, LEFT) && tower_at_rest(w, RIGHT), "apply returns both towers frozen");

    // Правка ОДНОГО ящика правой башни — вторая дверь пробуждения (`rest.hpp`). Верная связность
    // поднимает только её остров.
    w.mutate(BodyId{RIGHT}).position.x = fix32::from_int(FAR_X) + fix32::from_float(0.5);
    w.step(DT);

    const bool right_woke = !tower_at_rest(w, RIGHT);
    const bool left_slept = tower_at_rest(w, LEFT);
    // Оба утверждения обязательны. Без первого гейт зелен вакуумно — мир, в котором не проснулся
    // никто, проходил бы его так же, как верный.
    check(right_woke, "control: editing a box wakes its own island");
    check(left_slept, "a tower on the far side stays frozen after the rollback");

    std::printf("framework-physics-snapshot-islands: %s\n", fails == 0 ? "PASS" : "FAIL");
    return fails == 0 ? 0 : 1;
}
