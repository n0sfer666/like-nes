#pragma once
#include <cstdint>
#include <string>
#include <vector>

#include "bake.hpp"
#include "table.hpp"

namespace mat {

struct ParamSpec {
    std::string name;
    ParamType type = ParamType::Scalar;
    Unit unit = Unit::Raw;
    float value[4] = {0.0f, 0.0f, 0.0f, 0.0f};
    uint8_t slot = 0;
    int line = 0;
};

struct TexSpec {
    std::string name;
    std::string asset;
    uint8_t binding = 0;
    int line = 0;
};

struct MaterialSpec {
    std::string name;
    std::string shader;
    Blend blend = Blend::Alpha;
    int base = -1;
    std::vector<ParamSpec> params;
    std::vector<TexSpec> textures;
    int line = 0;
};

bool parse_materials(const std::string& text, std::vector<MaterialSpec>& out, BakeError& err);

} // namespace mat
