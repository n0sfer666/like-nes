#include "param.hpp"

#include <cmath>
#include <cstring>

namespace mat {
namespace {

constexpr float PI = 3.14159265358979323846f;

} // namespace

uint32_t param_floats(ParamType t) {
    switch (t) {
    case ParamType::Scalar: return 1;
    case ParamType::Vec2: return 2;
    case ParamType::Vec4:
    case ParamType::Color: return 4;
    }
    return 0;
}

bool param_type_from_name(const char* name, ParamType& out) {
    if (std::strcmp(name, "scalar") == 0) { out = ParamType::Scalar; return true; }
    if (std::strcmp(name, "vec2") == 0) { out = ParamType::Vec2; return true; }
    if (std::strcmp(name, "vec4") == 0) { out = ParamType::Vec4; return true; }
    if (std::strcmp(name, "color") == 0) { out = ParamType::Color; return true; }
    return false;
}

bool unit_from_name(const char* name, Unit& out) {
    if (std::strcmp(name, "raw") == 0) { out = Unit::Raw; return true; }
    if (std::strcmp(name, "fraction") == 0) { out = Unit::Fraction; return true; }
    if (std::strcmp(name, "pixels") == 0) { out = Unit::Pixels; return true; }
    if (std::strcmp(name, "degrees") == 0) { out = Unit::Degrees; return true; }
    return false;
}

float to_raw(Unit u, float authored) {
    switch (u) {
    case Unit::Raw:
    case Unit::Pixels: return authored;
    case Unit::Fraction: return authored < 0.0f ? 0.0f : (authored > 1.0f ? 1.0f : authored);
    case Unit::Degrees: return authored * (PI / 180.0f);
    }
    return authored;
}

} // namespace mat
