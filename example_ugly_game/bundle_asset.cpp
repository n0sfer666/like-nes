#include "bundle_asset.hpp"

#include <chrono>
#include <cstring>
#include <thread>

#include "asset_manager.hpp"
#include "hash.hpp"

namespace game {

bool read_bundle_asset(const std::string& bundle_path, const char* name, std::vector<uint8_t>& out) {
    asset::AssetManager am;
    if (!am.open(bundle_path, 256u * 1024, /*trusted=*/false)) return false;
    const uint64_t g = asset::fnv1a(name, std::strlen(name));
    am.request(g);
    for (int f = 0; f < 500; ++f) {
        am.sync_point();
        if (am.is_ready(g)) break;
        std::this_thread::sleep_for(std::chrono::microseconds(200));
    }
    if (!am.is_ready(g)) {
        am.close();
        return false;
    }
    const asset::Loaded a = am.get(g);
    out.assign(static_cast<const uint8_t*>(a.data), static_cast<const uint8_t*>(a.data) + a.size);
    am.close();
    return true;
}

} // namespace game
