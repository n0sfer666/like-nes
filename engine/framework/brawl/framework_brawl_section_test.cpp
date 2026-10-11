#include <cstdio>

#include "framework_brawl_check.hpp"
#include "framework_brawl_section_fixture.hpp"
#include "depth_step.hpp"
#include "section_exit.hpp"
#include "stage_sections_hash.hpp"

// Переходы belt ↔ platform (спека #25, В6г): вход на опоре, в воздухе — после приземления, занятая
// точка выхода — отказ с клампом `x`, `z` выхода — экранный `y` ног в полосе `depth_band`.
namespace {

using namespace framework::brawl;
using namespace framework::brawl::test;

void enters_on_support() {
    Sections w;
    const EntId id = w.body(396);
    w.step({go(1)});
    check(!w.platform_mode(id), "the belt body enters before the edge");
    w.step({go(1)});
    const Body& b = w.get(id);
    check(b.pos.x == at(PLATFORM_X0) && w.platform_mode(id), "a grounded body does not enter at the edge");
    check(b.pos.z == at(FLOOR) && b.pos.y == fix32{} && about(b.pos.vx, 2), "the entry does not keep the feet and pace");
    for (int t = 0; t < 30; ++t) w.step({go(1)});
    check(w.platform_mode(id) && grounded(w.get(id).pos) && at(460) < w.get(id).pos.x,
          "the controller does not carry the body along the platform floor");
}

void enters_after_landing() {
    Sections w;
    const EntId id = w.body(380);
    bool air_inside = false;
    bool early = false;
    for (int t = 0; t < 24; ++t) {
        w.step({go(1, t == 0)});
        const Body& b = w.get(id);
        if (!(b.pos.x < at(PLATFORM_X0)) && fix32{} < b.pos.y) air_inside = true;
        if (w.platform_mode(id) && !grounded(b.pos)) early = true;
    }
    check(air_inside && !early, "an airborne body enters platform before landing");
    check(w.platform_mode(id) && about(w.get(id).pos.z, FLOOR), "a landed body does not enter platform");
}

void leaves_after_landing() {
    Sections w;
    const EntId id = w.body(760);
    bool air_outside = false;
    bool early = false;
    for (int t = 0; t < 40; ++t) {
        w.step({go(1, t > 2 && t < 20)});
        const Body& b = w.get(id);
        if (!(b.pos.x < at(PLATFORM_X1)) && fix32{} < b.pos.y) air_outside = w.platform_mode(id) || air_outside;
        if (!w.platform_mode(id) && fix32{} < b.pos.y) early = true;
    }
    check(air_outside && !early, "an airborne body leaves platform before landing");
    check(!w.platform_mode(id) && grounded(w.get(id).pos), "a landed body does not leave platform");
}

void refused(Sections& w, EntId id, fix32 x, bool platform, const char* what) {
    const Body& b = w.get(id);
    check(b.pos.x == x && b.pos.vx == fix32{} && w.platform_mode(id) == platform, what);
}

void entry_taken() {
    Sections tile({{PLATFORM_X0, 224, PLATFORM_X0 + 16, FLOOR}});
    const EntId a = tile.body(396);
    for (int t = 0; t < 4; ++t) tile.step({go(1)});
    refused(tile, a, before(PLATFORM_X0), false, "a tile on the entry point does not refuse with the x clamp");

    Sections right({{PLATFORM_X1 - 16, 224, PLATFORM_X1, FLOOR}});
    const EntId r = right.body(804);
    for (int t = 0; t < 4; ++t) right.step({go(-1)});
    refused(right, r, at(PLATFORM_X1), false, "an entry refused from the right is clamped to the left edge");

    Sections body;
    const EntId stand = body.body(410);
    const EntId walker = body.body(396);
    for (int t = 0; t < 4; ++t) body.step({Command{}, go(1)});
    check(body.platform_mode(stand), "a body standing in platform did not enter");
    refused(body, walker, before(PLATFORM_X0), false, "a platform body on the entry point does not refuse");

    Sections seat({{PLATFORM_X0, 224, PLATFORM_X0 + 16, FLOOR}});
    seat.s.seat(0, 396);
    for (int t = 0; t < 4; ++t) seat.step({go(1)});
    check(seat.s.camera.center.x == before(PLATFORM_X0), "the camera follows a body before its section refusal");
}

void exit_taken() {
    Sections body;
    const EntId walker = body.body(780);
    body.body(806);
    for (int t = 0; t < 12; ++t) body.step({go(1)});
    refused(body, walker, before(PLATFORM_X1), true, "a belt body on the exit point does not refuse with the x clamp");

    Sections wall;
    check(wall.s.arena.floor.add_wall({at(PLATFORM_X1), at(230), at(PLATFORM_X1 + 20), at(250)}), "the wall");
    const EntId w2 = wall.body(780);
    for (int t = 0; t < 12; ++t) wall.step({go(1)});
    refused(wall, w2, before(PLATFORM_X1), true, "a belt wall on the exit point does not refuse");

    Sections deep;
    const EntId w3 = deep.body(780);
    deep.body(806, FLOOR + 20);
    for (int t = 0; t < 12; ++t) deep.step({go(1)});
    check(!deep.platform_mode(w3), "a belt body on another depth line refuses the exit");

    Sections mixed;
    mixed.body(795);
    const EntId w4 = mixed.body(770);
    for (int t = 0; t < 12; ++t) mixed.step({Command{}, go(1)});
    check(!mixed.platform_mode(w4), "a platform body near the exit refuses it as a belt body");
}

fix32 exit_z(int32_t band_z0) {
    Sections w({{760, 212, 900, FLOOR}});
    const EntId id = w.body(770, 212);
    w.step({go(1)});
    if (!w.platform_mode(id)) return at(-1);
    w.s.arena.floor.band.z0 = at(band_z0);
    for (int t = 0; t < 12 && w.platform_mode(id); ++t) w.step({go(1)});
    return w.platform_mode(id) ? at(-1) : w.get(id).pos.z;
}

void exit_depth() {
    check(about(exit_z(200), 212), "the exit z is not the screen y of the feet");
    check(exit_z(216) == at(216), "the exit z is not clamped into the depth band");
}

void standing_is_free() {
    Sections w;
    const EntId id = w.body(500);
    w.step({Command{}});
    const SectionBody* s = w.s.sections.find(id);
    check(s != nullptr && !platform_taken(w.s.sections, id, s->c.position, w.platform),
          "the SKIN gap of a standing hull reads as an occupied point");
}

void pruned() {
    Sections w;
    const EntId id = w.body(500);
    w.step({Command{}});
    w.s.pool.despawn(id);
    w.step({});
    check(w.s.sections.count == 0, "a despawned body keeps its platform entry");
}

struct Field {
    const char* name;
    void (*bump)(SectionBody&);
};

const Field FIELDS[] = {
    {"id", [](SectionBody& s) { s.id.seq += 1; }},
    {"position x", [](SectionBody& s) { s.c.position.x.raw += 1; }},
    {"position y", [](SectionBody& s) { s.c.position.y.raw += 1; }},
    {"velocity x", [](SectionBody& s) { s.c.velocity.x.raw += 1; }},
    {"velocity y", [](SectionBody& s) { s.c.velocity.y.raw += 1; }},
    {"state", [](SectionBody& s) { s.c.state = framework::character::MoveState::Ground; }},
    {"coyote", [](SectionBody& s) { s.c.coyote_left = 1; }},
    {"buffer", [](SectionBody& s) { s.c.buffer_left = 1; }},
    {"regrab", [](SectionBody& s) { s.c.ladder_regrab_left = 1; }},
    {"on ground", [](SectionBody& s) { s.c.on_ground = true; }},
    {"jump active", [](SectionBody& s) { s.c.jump_active = true; }},
    {"jump held", [](SectionBody& s) { s.c.jump_was_held = true; }},
    {"ceiling", [](SectionBody& s) { s.c.hit_ceiling = true; }},
    {"wall", [](SectionBody& s) { s.c.hit_wall = true; }},
    {"support", [](SectionBody& s) { s.c.support = framework::physics::BodyId{3}; }},
    {"crushed", [](SectionBody& s) { s.c.crushed = true; }},
};

void fields() {
    StageSections base;
    base.add(EntId{1}, framework::character::Character{});
    for (const Field& f : FIELDS) {
        StageSections s = base;
        f.bump(s.at[0]);
        if (sections_hash(s) == sections_hash(base)) {
            std::printf("  FAIL: section field %s is outside the hash\n", f.name);
            ++fails;
        }
    }
    StageSections none;
    check(!(sections_hash(none) == sections_hash(base)), "the entry count is outside the hash");
}

} // namespace

int main() {
    enters_on_support();
    enters_after_landing();
    leaves_after_landing();
    entry_taken();
    exit_taken();
    exit_depth();
    standing_is_free();
    pruned();
    fields();
    return verdict("framework-brawl-section");
}
