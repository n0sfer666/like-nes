#pragma once
#include <array>
#include <cstdint>
#include <span>

#include "ent_id.hpp"

namespace framework::hits {

struct Struck {
    uint32_t seq = 0;
    uint32_t box = 0;
};

bool has_struck(std::span<const Struck> at, core::EntId target, uint8_t box);
// `count` — длина занятого префикса, не больше at.size(); хвост хранится нулевым для хеша.
bool add_struck(std::span<Struck> at, uint32_t& count, core::EntId target, uint8_t box);
void forget_struck(std::span<Struck> at, uint32_t& count, uint8_t box);

template <uint32_t N = 16>
struct StruckList {
    std::array<Struck, N> at{};
    uint32_t count = 0;

    bool has(core::EntId target, uint8_t box) const {
        return has_struck(std::span<const Struck>(at.data(), count), target, box);
    }
    bool add(core::EntId target, uint8_t box) { return add_struck(at, count, target, box); }
    void forget(uint8_t box) { forget_struck(at, count, box); }
};

} // namespace framework::hits
