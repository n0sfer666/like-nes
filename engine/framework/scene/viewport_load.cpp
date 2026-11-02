#include "viewport_load.hpp"

#include "bundle_lookup.hpp"
#include "hash.hpp"

namespace framework::scene {

bool load_viewport_settings(asset::BundleView bundle, graphics::ViewportSettings& out, std::string& error) {
    out = graphics::ViewportSettings{};
    error.clear();
    if (!bundle.valid()) {
        error = "viewport table: bundle is not open";
        return false;
    }
    const uint8_t* data = nullptr;
    std::size_t size = 0;
    const asset::LookupFault fault = asset::raw_table(bundle, asset::fnv1a_str("viewport"), data, size);
    if (fault == asset::LookupFault::Missing) return true;
    if (fault != asset::LookupFault::Ok) {
        error = std::string("viewport table: ") + asset::lookup_fault_name(fault);
        return false;
    }
    if (graphics::read_viewport_settings(data, size, out)) return true;
    error = "viewport table does not open";
    return false;
}

} // namespace framework::scene
