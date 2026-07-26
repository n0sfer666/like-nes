#include "command.hpp"
#include "scene.hpp"
#include "serialize.hpp"
#include <cstdio>
#include <string>

// Гейт 2 (спека #7): command do/undo/redo корректность + группировка (drag=1 undo) +
// обрубание redo-хвоста новой командой. Оракул состояния — детерм. serialize() из гейта 1.
using namespace ide;

namespace {
int failures = 0;
void check(bool cond, const char* what) {
    if (!cond) { std::printf("  FAIL: %s\n", what); ++failures; }
}
} // namespace

int main() {
    Scene s;
    CommandBus bus(s);

    const std::string S0 = serialize(s); // пусто

    bus.create_entity(1);
    bus.set_component<Name>(1, {"hero"});
    bus.set_component<Position>(1, {fix32::from_int(0), fix32::from_int(0)});
    const std::string S1 = serialize(s);

    // drag = 3 set'а Position в одной группе → один undo
    bus.begin_group();
    bus.set_component<Position>(1, {fix32::from_int(1), fix32::from_int(0)});
    bus.set_component<Position>(1, {fix32::from_int(2), fix32::from_int(0)});
    bus.set_component<Position>(1, {fix32::from_int(3), fix32::from_int(5)});
    bus.end_group();
    const std::string S2 = serialize(s);
    check(S2 != S1, "group changed state");
    check(bus.undo_depth() == 4, "4 undo units (create+name+pos+group)");

    // undo группы → S1 (одним undo, не тремя)
    bus.undo();
    check(serialize(s) == S1, "undo group -> S1 (single unit)");
    // redo группы → S2
    bus.redo();
    check(serialize(s) == S2, "redo group -> S2");

    // полный откат до пустого
    bus.undo(); // group
    bus.undo(); // pos-set (initial 0,0) -> remove Position
    bus.undo(); // name-set -> remove Name
    bus.undo(); // create -> destroy
    check(serialize(s) == S0, "undo all -> empty S0");
    check(!bus.can_undo(), "undo stack empty");
    check(!s.exists(1), "entity 1 gone");

    // redo всё → S2
    bus.redo(); bus.redo(); bus.redo(); bus.redo();
    check(serialize(s) == S2, "redo all -> S2");
    check(!bus.can_redo(), "redo stack empty");

    // обрубание redo-хвоста: undo → S1, затем новая команда
    bus.undo();
    check(serialize(s) == S1, "undo -> S1 before truncation");
    check(bus.can_redo(), "redo available before new cmd");
    bus.create_entity(2);
    check(!bus.can_redo(), "new command truncated redo tail");

    // destroy + undo восстанавливает сущность с компонентами (per-entity snapshot)
    const std::string ent1_before = serialize_entity(s, 1);
    bus.destroy_entity(1);
    check(!s.exists(1), "entity 1 destroyed");
    bus.undo();
    check(s.exists(1), "entity 1 restored");
    check(serialize_entity(s, 1) == ent1_before, "restored components identical");

    bool pass = (failures == 0);
    std::printf("command-undo: %s\n", pass ? "PASS" : "FAIL");
    return pass ? 0 : 1;
}
