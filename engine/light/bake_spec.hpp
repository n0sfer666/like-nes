#pragma once
#include <string>
#include <vector>

#include "table.hpp"

namespace light {

// Разобранный источник — то, что парсер отдаёт сборщику байтов. Отдельным заголовком по той же
// причине, что и `mat::bake_rows.hpp`: два файла пекаря делят его, а рантайм не знает о нём вовсе.
struct LightSpec {
    std::string name;
    Kind kind = Kind::Point;
    float pos[2] = {0, 0};
    float height = 0;
    float dir[2] = {0, 0};
    float color[3] = {0, 0, 0};
    float intensity = 0;
    float radius = 0;
    int line = 0;   // строка объявления: отказ о НЕДОСТАЮЩЕМ поле называет её, а не конец файла
};

// Ambient — свойство НАБОРА, а не источника, поэтому он здесь, а не в LightSpec: у него нет ни
// позиции, ни затухания, и строкой списка светов он бы притворялся источником.
struct LightSet {
    std::vector<LightSpec> lights;
    float ambient[4] = {0, 0, 0, 0};
};

struct BakeError {
    int line = 0;
    std::string message;
};

bool parse_lights(const std::string& text, LightSet& out, BakeError& err);

} // namespace light
