#pragma once
#include "save_backend.hpp"

namespace save::detail {
bool valid_key(const std::string& key);
// Только пути, уже проверенные LocalBackend; не поставляется SDK.
MoveResult move_new_paths(const std::string& source, const std::string& target);
}
