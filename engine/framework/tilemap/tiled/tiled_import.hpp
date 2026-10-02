#pragma once
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

#include "map_bake.hpp"
#include "object_bake.hpp"
#include "visual_bake.hpp"

namespace framework::tiled {

struct Image {
    uint64_t guid = 0;
    uint32_t width = 0;
    uint32_t height = 0;
};

class Source {
public:
    virtual ~Source() = default;
    virtual bool tileset(const std::string& from, const std::string& rel, std::string& file,
                         std::vector<uint8_t>& bytes, std::string& error) = 0;
    virtual bool image(const std::string& from, const std::string& rel, Image& out, std::string& error) = 0;
};

struct Level {
    tilemap::ParsedMap collision;
    tilemap::VisualMapSrc visual;
    tilemap::ObjectMapSrc objects;
};

bool import_tmj(const std::string& name, const std::string& file, std::span<const std::byte> bytes,
                Source& src, Level& out, std::string& error);

} // namespace framework::tiled
