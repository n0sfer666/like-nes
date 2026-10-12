#include "rumble_level.hpp"

#include <cstdio>

#include "hash.hpp"

namespace rumble {

bool read_table(const Level& level, const char* table_name, const uint8_t*& data, size_t& size) {
    const asset::LookupFault f = asset::raw_table(level.bundle, asset::fnv1a_str(table_name), data, size);
    if (f == asset::LookupFault::Ok) return true;
    std::fprintf(stderr, "neon-rumble: %s table: %s\n", table_name, asset::lookup_fault_name(f));
    return false;
}

} // namespace rumble
