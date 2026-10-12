#include "level_error.hpp"

#include <algorithm>

namespace framework::scene {

const char* load_fault_name(LoadFault f) {
    switch (f) {
    case LoadFault::Ok: return "ok";
    case LoadFault::NoTable: return "no table";
    case LoadFault::BadTable: return "bad table";
    case LoadFault::NoLevel: return "no level";
    case LoadFault::BadObjects: return "bad objects";
    case LoadFault::Texture: return "texture";
    case LoadFault::UnknownKind: return "unknown kind";
    case LoadFault::BadFighter: return "bad fighter";
    case LoadFault::Capacity: return "capacity";
    }
    return "unknown fault";
}

bool load_fail(LoadError& error, LoadFault fault, std::string_view text) {
    error.fault = fault;
    const std::size_t n = std::min(text.size(), LOAD_ERROR_TEXT - 1);
    std::copy_n(text.data(), n, error.text);
    error.text[n] = '\0';
    return false;
}

} // namespace framework::scene
