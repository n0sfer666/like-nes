#include "rumble_label.hpp"

#include <cstdio>

namespace rumble {

Label label(const Brawl& brawl, framework::brawl::EntId id, uint8_t kind) {
    Label l;
    const char* name = kind_name(kind);
    if (brawl.wave.on) std::snprintf(l.text.data(), l.text.size(), "%s#%u", name, id.seq);
    else std::snprintf(l.text.data(), l.text.size(), "%s", name);
    return l;
}

Label label(const Brawl& brawl, const framework::brawl::Body& b) { return label(brawl, b.id, b.kind); }

} // namespace rumble
