#include "bakers.hpp"

namespace asset::bakers {

bool levels(std::span<const framework::tiled::Level> levels, std::vector<AssetInput>& out, std::string& error) {
    std::vector<framework::tilemap::ParsedMap> maps;
    std::vector<framework::tilemap::VisualMapSrc> visuals;
    std::vector<framework::tilemap::ObjectMapSrc> objects;
    for (const framework::tiled::Level& l : levels) {
        maps.push_back(l.collision);
        visuals.push_back(l.visual);
        objects.push_back(l.objects);
    }
    std::vector<uint8_t> tilemap, visual, object;
    framework::tilemap::MapBakeError err;
    if (!framework::tilemap::bake_maps(maps, tilemap, err)) {
        error = "tilemap: " + err.message;
        return false;
    }
    if (!framework::tilemap::bake_visuals(visuals, visual, error) ||
        !framework::tilemap::bake_objects(objects, object, error))
        return false;
    push_table("tilemap", std::move(tilemap), out);
    push_table("visual", std::move(visual), out);
    push_table("objects", std::move(object), out);
    return true;
}

} // namespace asset::bakers
