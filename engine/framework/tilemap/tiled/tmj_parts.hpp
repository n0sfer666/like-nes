#pragma once
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "grid.hpp"
#include "tiled_import.hpp"
#include "tmj_doc.hpp"

namespace framework::tiled {

struct Tileset {
    uint32_t firstgid = 0;
    uint32_t count = 0;
    uint32_t first_index = 0;
    std::string name;
    std::vector<std::optional<tilemap::TileFlags>> flags;
};

struct Placement {
    fix32 offset_x;
    fix32 offset_y;
    fix32 parallax_x = fix32::from_int(1);
    fix32 parallax_y = fix32::from_int(1);
    fix32 opacity = fix32::from_int(1);
    bool visible = true;
};

struct Gid {
    uint32_t id = 0;
    bool flip_h = false;
    bool flip_v = false;
    bool flip_d = false;
};

struct Map {
    const Doc& doc;
    Source& src;
    uint32_t width = 0;
    uint32_t height = 0;
    uint32_t tile = 0;
    std::vector<Tileset> tilesets;
};

bool load_tilesets(Map& map, Level& out, std::string& error);
bool walk_layers(Map& map, Level& out, std::string& error);
bool decode_gid(const Map& map, const json::Node& cell, Gid& out, const Tileset*& ts, std::string& error);
bool bake_collision(const Map& map, const json::Node& data, const Placement& at, const std::string& layer_name,
                    tilemap::ParsedMap& out, std::string& error);
bool read_objects(const Doc& doc, const json::Node& group, const Placement& at, tilemap::ObjectMapSrc& out,
                  std::string& error);

} // namespace framework::tiled
