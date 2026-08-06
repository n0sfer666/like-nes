#include "art.hpp"

#include <chrono>
#include <cstdio>
#include <cstring>
#include <thread>

#include "asset_manager.hpp"
#include "assets_path.hpp"
#include "hash.hpp"
#include "transcode.hpp"

// Шов assetc→билд (спека #8, гейт 2): игра стартует с бейкнутых ассетов, не с source.
// atlas.png → assetc UASTC bake → game.bundle → рантайм транскодит UASTC→BC7 и отдаёт
// SpriteBatch'у (target-native GPU-текстура из бандла). UV-регионы — общая раскладка.

namespace game {

Atlas load_baked_atlas(const char* bundle_path) {
    Atlas atlas;
    set_regions(atlas);

    asset::AssetManager am;
    if (!am.open(bundle_path, 8u * 1024 * 1024, /*trusted=*/false)) return atlas;
    const uint64_t g = asset::fnv1a("atlas", std::strlen("atlas"));
    am.request(g);
    for (int f = 0; f < 500; ++f) {
        am.sync_point();
        if (am.is_ready(g)) break;
        std::this_thread::sleep_for(std::chrono::microseconds(200));
    }
    if (!am.is_ready(g)) { am.close(); return atlas; }

    asset::Loaded a = am.get(g);
    uint32_t w = 0, h = 0;
    // Валидируем И раскладку, И число байт (BC7: 16 байт/4x4-блок) — иначе writeTexture
    // словит validation error вместо мягкого отката на процедурный atlas.
    const size_t want = (size_t)((atlas.w + 3) / 4) * ((atlas.h + 3) / 4) * 16;
    const bool ok = asset::ktx2_to_bc7(a.data, a.size, atlas.bc7, w, h) &&
                    w == atlas.w && h == atlas.h && atlas.bc7.size() == want;
    if (!ok) atlas.bc7.clear(); // мисматч → вызывающий делает fallback
    am.close();
    return atlas;
}

Atlas load_game_atlas(bool device_supports_bc) {
    if (!device_supports_bc) {
        std::fprintf(stderr, "[game] assets: device lacks BC -> procedural atlas (no baked BC7)\n");
        return build_atlas();
    }
    const std::string bp = resolve_bundle_path();
    if (!bp.empty()) {
        Atlas a = load_baked_atlas(bp.c_str());
        if (!a.bc7.empty()) {
            std::printf("[game] assets: baked bundle %s (BC7 %ux%u)\n", bp.c_str(), a.w, a.h);
            return a;
        }
        std::fprintf(stderr, "[game] assets: bundle %s unreadable -> procedural fallback\n",
                     bp.c_str());
    } else {
        std::fprintf(stderr, "[game] assets: no game.bundle found -> procedural fallback\n");
    }
    return build_atlas();
}

} // namespace game
