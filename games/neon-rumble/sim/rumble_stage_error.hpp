#pragma once
#include "level_error.hpp"

namespace rumble {

enum class StageFault : uint32_t {
    Ok = 0, NoTable = 1, BadTable = 2, NoLevel = 3, BadObjects = 4,
    Texture = 5, UnknownKind = 6, BadFighter = 7, Capacity = 8,
};

struct StageError {
    StageFault fault = StageFault::Ok;
    char text[framework::scene::LOAD_ERROR_TEXT] = {};
};

const char* stage_fault_name(StageFault fault);
bool stage_fail(StageError& error, StageFault fault, std::string_view text);
bool copy_level_error(bool ok, const framework::scene::LoadError& from, StageError& out);

} // namespace rumble
