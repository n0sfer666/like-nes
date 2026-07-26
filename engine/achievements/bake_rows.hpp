#pragma once
#include "bake.hpp"
#include "def.hpp"
#include <string>
#include <vector>

namespace ach {

struct Row {
    Def def;
    std::string key;
    std::string name;
    std::string desc;
    int line;
};

bool parse_manifest(const std::string& text, std::vector<Row>& rows,
                    std::vector<std::string>& stat_keys, BakeError& err);

} // namespace ach
