#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace mat {

struct BakeError {
    int line = 0;
    std::string message;
};

bool bake_materials(const std::string& text, std::vector<uint8_t>& out, BakeError& err);
bool bake_materials_file(const std::string& path, std::vector<uint8_t>& out, BakeError& err);

} // namespace mat
