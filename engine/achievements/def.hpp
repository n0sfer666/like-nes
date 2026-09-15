#pragma once
#include <cstdint>

#include "../asset/hash.hpp"

namespace ach {

using Id = uint64_t;

// Свой цикл здесь и был одной из восьми копий FNV семьи A (находка 5 аудита #21). Значения Id от
// сведения не изменились — константы и порядок байт те же, — поэтому голдены достижений целы.
constexpr Id hash_key(const char* s) { return asset::fnv1a_str(s); }

enum class Kind : uint32_t {
    Boolean = 0,
    Progress = 1,
};

constexpr uint32_t FLAG_HIDDEN = 1u;

struct Def {
    Id id;
    Id stat;
    uint64_t target;
    uint32_t kind;
    uint32_t flags;
};
static_assert(sizeof(Def) == 32, "Def layout pinned (bake/runtime ABI)");

struct DefSpec {
    const char* key;
    const char* name;
    const char* desc;
    Kind kind;
    const char* stat_key;
    uint64_t target;
    uint32_t flags;
};

struct Entry {
    Def def;
    const char* key;
    const char* name;
    const char* desc;
};

struct Stat {
    Id id;
    const char* key;
};

enum class DefineResult : uint32_t {
    Ok = 0,
    Duplicate = 1,
    BadSpec = 2,
};

} // namespace ach
