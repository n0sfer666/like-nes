#pragma once
#include <cstdint>
#include <string>

#include "bundle_lookup.hpp"
#include "bundle_view.hpp"
#include "layer_quads.hpp"
#include "platform_io.hpp"
#include "visual_read.hpp"

namespace rumble {

constexpr uint32_t MAX_TEXTURES = 8;

// Уровень из `game.bundle` (спека #24, В5б): таблица `visual` и RGBA8 каждой текстуры, на которую
// ссылается карта. Пиксели и ячейки смотрят в маппинг файла, поэтому `Level` не копируется.
struct Level {
    platform::MappedFile file;
    asset::BundleView bundle;
    framework::tilemap::VisualTable table;
    framework::tilemap::VisualMap map;
    const char* name = "";
    uint32_t texture_count = 0;
    uint64_t guids[MAX_TEXTURES] = {};
    asset::RgbaView pixels[MAX_TEXTURES] = {};
    framework::graphics::TextureSize sizes[MAX_TEXTURES] = {};

    Level() = default;
    Level(const Level&) = delete;
    Level& operator=(const Level&) = delete;

    bool open(const std::string& path, const char* level_name);
};

} // namespace rumble
