#pragma once
#include "assetc_manifest.hpp"

namespace asset::manifest {
struct RecordShape {
    const char* name;
    bool (*check)(const std::vector<std::string>& fields, uint32_t line, Error& error);
};
std::span<const RecordShape> builtin_shapes();
}
