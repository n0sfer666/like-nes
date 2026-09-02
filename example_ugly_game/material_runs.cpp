#include "material_runs.hpp"

namespace game {

uint32_t material_runs(const uint16_t* ids, uint32_t n, MaterialRun* out, uint32_t cap) {
    uint32_t runs = 0;
    for (uint32_t first = 0; first < n && runs < cap;) {
        const uint16_t id = ids[first];
        uint32_t last = first + 1;
        while (last < n && ids[last] == id) ++last;
        out[runs++] = MaterialRun{first, last - first, id};
        first = last;
    }
    return runs;
}

} // namespace game
