#include "rumble_touch.hpp"

#include <cstdio>

#include "bundle_lookup.hpp"
#include "hash.hpp"

namespace rumble {

bool Touch::load(asset::BundleView bundle, std::string& why) {
    (void)source_.set_layout({});
    why.clear();
    const uint8_t* data = nullptr;
    size_t size = 0;
    const asset::LookupFault fault = asset::raw_table(bundle, asset::fnv1a_str("touch"), data, size);
    if (fault != asset::LookupFault::Ok) {
        why = std::string("touch table: ") + asset::lookup_fault_name(fault);
        return false;
    }
    framework::touch::Layout layout;
    if (!framework::touch::read_layout(data, size, layout)) {
        why = "touch table does not open";
        return false;
    }
    if (layout.button_count != framework::touch::BUTTONS) {
        why = "Neon Rumble needs all five touch buttons";
        return false;
    }
    return source_.set_layout(layout);
}

std::string Touch::state() const {
    using framework::touch::Control;
    const auto id = [&](Control c) { return static_cast<long long>(source_.owner(c).value_or(-1)); };
    char buf[192];
    std::snprintf(buf, sizeof buf, "stick=%lld a=%lld b=%lld c=%lld d=%lld e=%lld", id(Control::Stick),
                  id(Control::A), id(Control::B), id(Control::C), id(Control::D), id(Control::E));
    return buf;
}

} // namespace rumble
