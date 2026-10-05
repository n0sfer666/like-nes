#pragma once
#include <cstdint>
#include <string>

#include "bundle_lookup.hpp"
#include "bundle_view.hpp"
#include "camera.hpp"
#include "object_read.hpp"
#include "layer_quads.hpp"
#include "platform_io.hpp"
#include "visual_read.hpp"

namespace rumble {

constexpr uint32_t MAX_TEXTURES = 8;

// Уровень из `game.bundle` (спека #24, В5б): таблица `visual`, RGBA8 каждой текстуры, на которую
// ссылается карта, и границы камеры (В7б) — прямоугольник класса `bounds`, иначе карта, с тем же
// контрактом, что у проверки покрытия в бейке. Пиксели и ячейки смотрят в маппинг файла, поэтому
// `Level` не копируется.
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
    framework::graphics::CameraBounds bounds{};

    Level() = default;
    Level(const Level&) = delete;
    Level& operator=(const Level&) = delete;

    bool open(const std::string& path, const char* level_name);
    bool read_table(const char* table_name, const uint8_t*& data, size_t& size) const;
    bool open_objects(framework::tilemap::ObjectTable& objects) const;
};

} // namespace rumble
