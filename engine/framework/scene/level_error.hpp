#pragma once
#include <cstddef>
#include <cstdint>
#include <string_view>

namespace framework::scene {

constexpr std::size_t LOAD_ERROR_TEXT = 192;

enum class LoadFault : uint32_t {
    Ok = 0,
    NoTable = 1,
    BadTable = 2,
    NoLevel = 3,
    BadObjects = 4,
    Texture = 5,
    UnknownKind = 6,
    BadFighter = 7,
    Capacity = 8,
};

struct LoadError {
    LoadFault fault = LoadFault::Ok;
    char text[LOAD_ERROR_TEXT] = {};
};

const char* load_fault_name(LoadFault f);
bool load_fail(LoadError& error, LoadFault fault, std::string_view text);

} // namespace framework::scene
