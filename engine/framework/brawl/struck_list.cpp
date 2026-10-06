#include "struck_list.hpp"

namespace framework::brawl {

bool StruckList::has(EntId target, uint8_t box) const {
    for (uint32_t i = 0; i < count; ++i)
        if (at[i].seq == target.seq && at[i].box == box) return true;
    return false;
}

bool StruckList::add(EntId target, uint8_t box) {
    if (count == MAX_STRUCK) return false;
    at[count++] = Struck{target.seq, box};
    return true;
}

void StruckList::forget(uint8_t box) {
    uint32_t kept = 0;
    for (uint32_t i = 0; i < count; ++i)
        if (at[i].box != box) at[kept++] = at[i];
    for (uint32_t i = kept; i < count; ++i) at[i] = Struck{};
    count = kept;
}

} // namespace framework::brawl
