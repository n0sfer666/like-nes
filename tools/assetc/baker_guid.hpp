#pragma once

#include <cstring>

#include "hash.hpp"

namespace asset::bakers {

// GUID = FNV логического имени → стабилен (переживает rename файла), детерминирован cross-machine.
// Живёт в заголовке, а не копией в каждом TU пекарей: соглашение «guid считается от ИМЕНИ» держит
// рантайм на другом конце бандла, и вторая его копия разошлась бы молча.
inline uint64_t guid_of(const char* name) { return fnv1a(name, std::strlen(name)); }

} // namespace asset::bakers
