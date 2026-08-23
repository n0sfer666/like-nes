#include "map_read.hpp"

#include <cstring>

namespace framework::tilemap {

// Проверяется ВСЁ, чем читатель потом пользуется без проверок: границы блоков, ширина и высота,
// размер тайла и завершающий ноль блоба имён. Секция приезжает из файла, а файл бывает обрезанным,
// чужим и просто старым — и «прочиталось молча» тут значит чтение за концом отображения.
bool TileMapTable::open(const void* data, std::size_t size) {
    header_ = nullptr;
    rows_ = nullptr;
    strings_ = nullptr;
    strings_size_ = 0;
    if (data == nullptr || size < sizeof(MapHeader)) return false;
    const auto* base = static_cast<const uint8_t*>(data);
    const auto* h = reinterpret_cast<const MapHeader*>(base);
    if (std::memcmp(h->magic, MAP_MAGIC, sizeof(h->magic)) != 0) return false;
    if (h->version != MAP_VERSION) return false;
    if (h->total_size > size) return false;
    if (h->maps_offset != sizeof(MapHeader)) return false;
    const uint64_t rows_end =
        static_cast<uint64_t>(h->maps_offset) + static_cast<uint64_t>(h->map_count) * sizeof(MapRow);
    if (rows_end > h->strings_offset || h->strings_offset > h->total_size) return false;
    const auto* rows = reinterpret_cast<const MapRow*>(base + h->maps_offset);
    for (uint32_t i = 0; i < h->map_count; ++i) {
        const MapRow& r = rows[i];
        if (r.width == 0 || r.height == 0) return false;
        // Размер тайла проверяется тем же условием, каким его отбивает пекарь: сетка привела бы
        // нечётный raw к чётному, и карта из бандла молча разъехалась бы с картой из исходника.
        if (r.tile_size_raw < 2 || (r.tile_size_raw & 1) != 0) return false;
        const uint64_t bytes =
            static_cast<uint64_t>(r.width) * r.height * sizeof(TileFlags);
        if (r.tiles_offset < rows_end || r.tiles_offset + bytes > h->strings_offset) return false;
        if (r.name_offset >= h->total_size - h->strings_offset) return false;
    }
    // Блоб обязан кончаться нулём: имя возвращается указателем на C-строку, и блоб без
    // завершающего нуля увёл бы `strcmp` за конец секции.
    const uint32_t blob = h->total_size - h->strings_offset;
    if (blob == 0 || base[h->total_size - 1] != '\0') return false;

    base_ = base;
    header_ = h;
    rows_ = rows;
    strings_ = reinterpret_cast<const char*>(base + h->strings_offset);
    strings_size_ = blob;
    return true;
}

const char* TileMapTable::name(uint32_t index) const {
    if (header_ == nullptr || index >= header_->map_count) return "";
    return strings_ + rows_[index].name_offset;
}

std::optional<TileGrid> TileMapTable::build(uint32_t index) const {
    if (header_ == nullptr || index >= header_->map_count) return std::nullopt;
    const MapRow& r = rows_[index];
    std::optional<TileGrid> out;
    out.emplace(Vec2{fix32::from_raw(r.origin_x_raw), fix32::from_raw(r.origin_y_raw)},
                fix32::from_raw(r.tile_size_raw), r.width, r.height);
    const auto* flags = base_ + r.tiles_offset;
    // Флаги перекладываются ТАЙЛ ЗА ТАЙЛОМ через `set`, а не блоком в чужие потроха: порядок
    // `y * width + x` объявлен сеткой, и вторая его запись здесь разъехалась бы с первой молча.
    // Цена — одна загрузка уровня, а не тик.
    for (uint32_t y = 0; y < r.height; ++y) {
        for (uint32_t x = 0; x < r.width; ++x) {
            TileFlags f = 0;
            std::memcpy(&f, flags + (static_cast<std::size_t>(y) * r.width + x) * sizeof(TileFlags),
                        sizeof(TileFlags));
            out->set(x, y, f);
        }
    }
    return out;
}

std::optional<TileGrid> TileMapTable::find(const char* name_wanted) const {
    if (header_ == nullptr || name_wanted == nullptr) return std::nullopt;
    for (uint32_t i = 0; i < header_->map_count; ++i)
        if (std::strcmp(name(i), name_wanted) == 0) return build(i);
    return std::nullopt;
}

} // namespace framework::tilemap
