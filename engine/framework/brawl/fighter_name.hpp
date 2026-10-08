#pragma once
#include <cstddef>
#include <string>

#include "fighter.hpp"

namespace framework::brawl {

inline bool name_ok(const std::string& name) {
    for (const char c : name)
        if (c == '/' || static_cast<unsigned char>(c) < 0x20 || c == 0x7f) return false;
    return true;
}

inline std::string sheet_clip(const std::string& sheet, const std::string& clip) { return sheet + "/" + clip; }

inline std::string no_cancel_error(std::size_t step, const std::string& clip) {
    return "chain step " + std::to_string(step) + ": clip '" + clip + "' has no '" + CANCEL_EVENT +
           "' event to chain from";
}

} // namespace framework::brawl
