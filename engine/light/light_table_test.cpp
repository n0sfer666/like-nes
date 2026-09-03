#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "bake.hpp"
#include "table.hpp"

namespace {

int failures = 0;

void check(bool ok, const char* what) {
    if (!ok) {
        ++failures;
        std::printf("  FAIL %s\n", what);
    }
}

bool near(float a, float b) { return std::fabs(a - b) < 1e-5f; }

const char* const SOURCE =
    "# lights of the golden scene (spec #18, vertical 3)\n"
    "ambient | 0.10, 0.12, 0.18 | 0.35\n"
    "\n"
    "light | key | point\n"
    "set | pos       | 0.25, -0.10\n"
    "set | height    | 0.30\n"
    "set | color     | 1, 0.86, 0.62\n"
    "set | intensity | 1.4\n"
    "set | radius    | 0.80\n"
    "set | shadow    | 0.05\n"
    "\n"
    "light | sun | directional\n"
    "set | dir       | 3, -4\n"
    "set | color     | 0.55, 0.62, 0.90\n"
    "set | intensity | 0.6\n"
    "set | shadow    | 0.00\n";

// Отказ обязан назвать СТРОКУ, а не только причину: диагностика без номера строки заставляет
// искать опечатку глазами, а сообщение без причины — гадать. Каждая фикстура ниже ломает ровно
// одно правило, потому что фикстура, ломающая два, зелена и на пекаре, который видит только одно.
struct Bad {
    const char* text;
    int line;
    const char* what;
};

const Bad BAD[] = {
    {"ambient | 0,0,0 | 1\nlight | a | point\nset | pos | 0,0\nset | height | 1\n"
     "set | color | 1,1,1\nset | intensity | 1\nset | shadow | 0\n",
     2, "point light without radius"},
    {"ambient | 0,0,0 | 1\nlight | a | point\nset | pos | 0,0\nset | color | 1,1,1\n"
     "set | intensity | 1\nset | radius | 1\nset | shadow | 0\n",
     2, "point light without height"},
    {"ambient | 0,0,0 | 1\nlight | a | directional\nset | dir | 0,0\nset | color | 1,1,1\n"
     "set | intensity | 1\nset | shadow | 0\n",
     2, "direction with no length"},
    {"light | a | point\nset | pos | 0,0\nset | pos | 1,1\n", 3, "the same field set twice"},
    {"light | a | point\nset | dir | 1,0\n", 2, "dir on a point light"},
    {"light | a | directional\nset | radius | 1\n", 2, "radius on a directional light"},
    {"light | a | point\nset | glow | 1\n", 2, "unknown field"},
    {"light | a | spot\n", 1, "unknown kind"},
    // Первый свет здесь ПОЛНЫЙ: незакрытый свет отбивается раньше проверки имени, и фикстура
    // с двумя пустыми телами зелена на пекаре, который дубликатов не ищет вовсе.
    {"light | a | point\nset | pos | 0,0\nset | height | 1\nset | color | 1,1,1\n"
     "set | intensity | 1\nset | radius | 1\nset | shadow | 0\nlight | a | point\n",
     8, "duplicate name"},
    {"set | pos | 0,0\n", 1, "a field before any light"},
    {"light | a | point\nset | pos | 0,0\nset | height | 1\nset | color | 1,1,1\n"
     "set | intensity | -1\nset | radius | 1\n",
     5, "negative intensity"},
    {"light | a | point\nset | pos | 0,0\nset | height | 1\nset | color | 1,1,1\n"
     "set | intensity | 1\nset | radius | 0\n",
     6, "radius of zero"},
    {"# only a comment\n", 0, "a source with no lights at all"},
    {"light | a | point\nset | pos | 0,0\nset | height | 1\nset | color | 1,1,1\n"
     "set | intensity | 1\nset | radius | 1\nset | shadow | 0\n",
     0, "a source with no ambient"},
    {"ambient | 0,0,0 | 1\nlight | a | point\nset | pos | 0,0\nset | height | 1\n"
     "set | color | 1,1,1\nset | intensity | 1\nset | radius | 1\n",
     2, "light without shadow softness"},
    {"light | a | point\nset | pos | 0,0\nset | height | 1\nset | color | 1,1,1\n"
     "set | intensity | 1\nset | radius | 1\nset | shadow | -0.1\n",
     7, "negative shadow softness"},
    {"ambient | 0,0,0 | 1\nambient | 0,0,0 | 1\n", 2, "ambient set twice"},
    {"ambient | 0,0,-1 | 1\n", 1, "a negative ambient channel"},
    {"ambient | 0,0,0 | 1, 2\n", 1, "ambient strength that is not one number"},
};

void bakes_and_reads() {
    std::vector<uint8_t> bytes;
    light::BakeError err;
    if (!light::bake_lights(SOURCE, bytes, err)) {
        std::printf("  FAIL the good source does not bake: line %d: %s\n", err.line,
                    err.message.c_str());
        ++failures;
        return;
    }
    light::Table t;
    const light::LoadResult r = t.load(bytes.data(), bytes.size());
    check(r == light::LoadResult::Ok, light::load_reason(r));
    check(t.count() == 2, "both lights are in the table");
    const float* amb = t.ambient();
    check(amb != nullptr, "the table carries an ambient term");
    if (amb) {
        check(near(amb[0], 0.10f) && near(amb[2], 0.18f), "ambient colour survives the bake");
        check(near(amb[3], 0.35f), "ambient strength survives the bake");
    }

    const light::LightRow* key = t.row(t.find("key"));
    check(key != nullptr, "the point light is found by name");
    if (key) {
        check(key->kind == static_cast<uint8_t>(light::Kind::Point), "kind survives the bake");
        check(near(key->pos[0], 0.25f) && near(key->pos[1], -0.10f), "position survives the bake");
        check(near(key->height, 0.30f), "height survives the bake");
        check(near(key->color[1], 0.86f), "colour survives the bake");
        check(near(key->intensity, 1.4f), "intensity survives the bake");
        check(near(key->radius, 0.80f), "radius survives the bake");
        check(near(key->shadow, 0.05f), "shadow softness survives the bake");
    }

    const light::LightRow* sun = t.row(t.find("sun"));
    check(sun != nullptr, "the directional light is found by name");
    if (sun) {
        // 3,-4 → 0.6,-0.8: нормализует ПЕКАРЬ, и это утверждение — половина решения. Без него
        // рантайм платил бы корень на источник, а забытая нормализация тускнела бы молча.
        check(near(sun->dir[0], 0.6f) && near(sun->dir[1], -0.8f), "direction is unit length");
        check(near(sun->radius, 0.0f), "a directional light carries no radius");
        // Ноль здесь — ЗАПИСАННЫЙ ноль: у поля со значением по умолчанию он был бы
        // неотличим от забытой строки, поэтому пекарь требует её у обоих видов.
        check(near(sun->shadow, 0.0f), "a written zero softness survives the bake");
    }
    check(t.find("nope") == t.count(), "an unknown name resolves to count(), not to zero");
    check(t.row(t.count()) == nullptr, "an index past the end has no row");
    check(t.name(t.count()) == nullptr, "an index past the end has no name");
}

void rejects_broken_sources() {
    for (const Bad& b : BAD) {
        std::vector<uint8_t> bytes;
        light::BakeError err;
        if (light::bake_lights(b.text, bytes, err)) {
            std::printf("  FAIL baked what it must reject: %s\n", b.what);
            ++failures;
            continue;
        }
        if (err.line != b.line) {
            std::printf("  FAIL %s: blamed line %d, expected %d (%s)\n", b.what, err.line, b.line,
                        err.message.c_str());
            ++failures;
        }
        if (err.message.empty()) {
            std::printf("  FAIL %s: rejected without a reason\n", b.what);
            ++failures;
        }
    }
}

void rejects_broken_bytes() {
    std::vector<uint8_t> good;
    light::BakeError err;
    if (!light::bake_lights(SOURCE, good, err)) return;

    light::Table t;
    check(t.load(good.data(), 4) == light::LoadResult::TooShort, "a truncated table is refused");

    std::vector<uint8_t> bad = good;
    bad[0] = 'X';
    check(t.load(bad.data(), bad.size()) == light::LoadResult::BadMagic, "foreign magic is refused");

    bad = good;
    bad[4] = 9;
    check(t.load(bad.data(), bad.size()) == light::LoadResult::BadVersion,
          "another version is refused");

    // Раскладка: секции обязаны идти за заголовком и не пересекаться. Оба правила проверяются
    // ПОЛЕМ заголовка, а не усечением, — усечение ловится `total_size` и говорит о другом.
    bad = good;
    bad[12] = 8;   // lights_offset внутрь самого заголовка
    check(t.load(bad.data(), bad.size()) == light::LoadResult::BadLayout,
          "sources starting inside the header are refused");

    bad = good;
    for (int k = 0; k < 4; ++k) bad[16 + k] = bad[12 + k];   // strings_offset ← lights_offset
    check(t.load(bad.data(), bad.size()) == light::LoadResult::BadLayout,
          "sections that overlap are refused");

    // Отказ обязан ОБНУЛИТЬ читатель: иначе `count()` отдаёт ноль, а `row()` смотрит в снятый
    // регион — та же поломка, что закрыта в таблице материалов.
    check(t.load(good.data(), good.size()) == light::LoadResult::Ok, "the good table opens again");
    check(t.load(bad.data(), bad.size()) != light::LoadResult::Ok, "the bad table is refused");
    check(t.count() == 0 && t.row(0) == nullptr, "a refused load leaves no pointers behind");
    check(t.ambient() == nullptr, "a refused load has no ambient either");
}

} // namespace

int main() {
    bakes_and_reads();
    rejects_broken_sources();
    rejects_broken_bytes();
    std::printf("light-table: %s (failures: %d)\n", failures ? "FAIL" : "PASS", failures);
    return failures ? 1 : 0;
}
