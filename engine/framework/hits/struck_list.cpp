#include "struck_list.hpp"

namespace framework::hits {

bool has_struck(std::span<const Struck> at, core::EntId target, uint8_t box) {
    for (const Struck& s : at)
        if (s.seq == target.seq && s.box == box) return true;
    return false;
}

bool add_struck(std::span<Struck> at, uint32_t& count, core::EntId target, uint8_t box) {
    if (count == at.size()) return false;
    at[count++] = Struck{target.seq, box};
    return true;
}

void forget_struck(std::span<Struck> at, uint32_t& count, uint8_t box) {
    uint32_t kept = 0;
    for (uint32_t i = 0; i < count; ++i)
        if (at[i].box != box) at[kept++] = at[i];
    for (uint32_t i = kept; i < count; ++i) at[i] = Struck{};
    count = kept;
}

} // namespace framework::hits
