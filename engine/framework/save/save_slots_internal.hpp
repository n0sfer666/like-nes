#pragma once
#include "save_slots.hpp"

namespace save::detail {
bool valid_slot(const std::string& slot);
bool next_counter(Backend& backend, const std::string& slot, uint64_t& next);
bool quarantine(Backend& backend, const std::string& slot, const std::string& version_tag,
                const std::string& extension, std::string& key);
}
