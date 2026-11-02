#include "rumble_stage_error.hpp"

#include <algorithm>

namespace rumble {

const char* stage_fault_name(StageFault fault) {
    if (fault == StageFault::UnknownKind) return "unknown kind";
    if (fault == StageFault::BadFighter) return "bad fighter";
    return framework::scene::load_fault_name(static_cast<framework::scene::LoadFault>(fault));
}

bool stage_fail(StageError& error, StageFault fault, std::string_view text) {
    error.fault = fault;
    const std::size_t n = std::min(text.size(), framework::scene::LOAD_ERROR_TEXT - 1);
    std::copy_n(text.data(), n, error.text);
    error.text[n] = '\0';
    return false;
}

bool copy_level_error(bool ok, const framework::scene::LoadError& from, StageError& out) {
    return ok || stage_fail(out, static_cast<StageFault>(from.fault), from.text);
}

} // namespace rumble
