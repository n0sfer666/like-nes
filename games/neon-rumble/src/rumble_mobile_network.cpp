#include "rumble_mobile.hpp"

#include "mobile_host.hpp"
#include "rumble_net_args.hpp"

namespace rumble {

bool MobileRumble::open_network() {
    const bool russian = mobile::has_arg("--network-ru");
    if (!russian && !mobile::has_arg("--network")) return true;
    if (mobile::has_arg("--frame-stats") || mobile::has_arg("--demo")) return false;
    uint64_t library = 0;
    if (!network_bundle_hash(mobile::asset_path("library.bundle"), library)) return false;
    network_ = std::make_unique<NetworkMode>(flow_, library, bundle_.header().bundle_hash, russian);
    return true;
}

void MobileRumble::update_touch_pads() {
    static_assert(input::MAX_DEVICES <= 32);
    uint32_t pads = 0;
    for (int slot = 0; slot < input::MAX_DEVICES; ++slot)
        if (hotseat_.engine().device().pad_connected[slot]) pads |= 1u << slot;
    touch_.update_pads(pads);
}

} // namespace rumble
