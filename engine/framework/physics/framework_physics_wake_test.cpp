#include <cstdio>

#include "framework_physics_wake_scene.hpp"
#include "platform_args.hpp"

// Три двери, через которые замерший остров обязан ожить, — и все три однажды были закрыты:
//   1. смена тяготения   — и то, что ПОВТОРНАЯ выдача того же значения покой не отменяет;
//   2. сдвинутая статика — пол уехал, стопка обязана упасть;
//   3. правка тела через неконстантную ручку — у неё собственная цель,
//      `framework_physics_handle_test`: там утверждение двустороннее (чтение обязано стоить ноль,
//      запись — доехать), обе стороны были дефектами, и оба конца мерятся числами, которых у первых
//      двух дверей нет вовсе. Имя упавшей цели в логе CI обязано отличать «дверь не открылась» от
//      «ручка отменила правку молча».
//
// Сцена и мерки — в `framework_physics_wake_scene.hpp`, там же про роль трения.
namespace {

int fails = 0;

void check(bool ok, const char* what) {
    if (!ok) {
        std::printf("  FAIL: %s\n", what);
        ++fails;
    }
}

using namespace framework;
using namespace framework::physics;
using namespace framework::physics::wake;

void test_gravity_change_wakes() {
    World w{16};
    build(w, GRIP);
    check(settle(w) != 0, "a placed tower freezes, so there is something to wake");

    const fix32 before = top_y(w);
    // Вверх — так, чтобы ответ был однозначен по знаку: упасть обратно на пол ящик не может, а
    // «сдвинулся на пару raw» неотличимо от дрожания решателя.
    w.set_gravity({fix32{}, -w.gravity().y});
    check(!w.at_rest(BodyId{TOP}), "flipping gravity wakes the frozen island at once");
    for (uint32_t i = 0; i < 60; ++i) w.step(DT);
    check(top_y(w) < before, "and the tower actually leaves the floor it was resting on");
    std::printf("  gravity flipped: y %d -> %d raw\n", before.raw, top_y(w).raw);
}

// Обратная половина того же: тяготение, выданное повторно тем же значением, покой отменять НЕ вправе.
// Игра, читающая его из конфигурации каждый кадр, иначе выключила бы правило покоя целиком.
void test_same_gravity_keeps_rest() {
    World w{16};
    build(w, GRIP);
    for (uint32_t i = 0; i < WINDOW; ++i) {
        w.set_gravity(w.gravity());
        w.step(DT);
        if (tower_at_rest(w)) {
            std::printf("  re-set gravity every frame: froze at %u\n", i + 1);
            return;
        }
    }
    check(false, "re-setting the same gravity every frame does not keep the tower awake");
}

void test_moved_static_wakes() {
    World w{16};
    build(w, GRIP);
    check(settle(w) != 0, "the tower freezes before the floor is touched");

    const fix32 before = top_y(w);
    w.mutate(BodyId{FLOOR}).position.y = fix32::from_int(400);
    w.step(DT);
    check(!w.at_rest(BodyId{TOP}), "teleporting the floor wakes the island that stood on it");
    for (uint32_t i = 0; i < 60; ++i) w.step(DT);
    check(before < top_y(w), "and the tower falls after the floor that left");
    std::printf("  floor teleported: y %d -> %d raw\n", before.raw, top_y(w).raw);
}

} // namespace

int main(int argc, char** argv) {
    platform::Args args(argc, argv);
    std::printf("framework physics wake gate\n");
    test_gravity_change_wakes();
    test_same_gravity_keeps_rest();
    test_moved_static_wakes();
    std::printf("framework-physics-wake: %s\n", fails == 0 ? "PASS" : "FAIL");
    return fails == 0 ? 0 : 1;
}
