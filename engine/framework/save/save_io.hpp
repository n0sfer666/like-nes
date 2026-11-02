#pragma once
#include "save_backend.hpp"

namespace save::detail {
// Только пути, уже проверенные LocalBackend; не поставляется SDK.
MoveResult move_new_paths(const std::string& source, const std::string& target);
}
