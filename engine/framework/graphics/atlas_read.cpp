#include "atlas_read.hpp"

#include <cstring>

namespace framework::graphics {

// Проверяется ВСЁ, чем читатель потом пользуется без проверок: границы блоков, размер страницы,
// прямоугольник каждого региона и завершающий ноль блоба имён. Секция приезжает из файла, а файл
// бывает обрезанным, чужим и просто старым — «прочиталось молча» тут значит чтение за концом
// отображения либо спрайт, взятый мимо страницы.
bool AtlasTable::open(const void* data, std::size_t size) {
    header_ = nullptr;
    rows_ = nullptr;
    strings_ = nullptr;
    if (data == nullptr || size < sizeof(AtlasHeader)) return false;
    const auto* base = static_cast<const uint8_t*>(data);
    const auto* h = reinterpret_cast<const AtlasHeader*>(base);
    if (std::memcmp(h->magic, ATLAS_MAGIC, sizeof(h->magic)) != 0) return false;
    if (h->version != ATLAS_VERSION) return false;
    if (h->total_size > size) return false;
    if (h->regions_offset != sizeof(AtlasHeader)) return false;
    if (h->page_width == 0 || h->page_height == 0) return false;
    // Потолок числа регионов — не запас, а ТИП: `RegionId` шестнадцатибитный (`clip.hpp`), и
    // таблица длиннее сослалась бы на кадр, до которого номером не доехать.
    if (h->region_count > 0xFFFFu) return false;
    const uint64_t rows_end = static_cast<uint64_t>(h->regions_offset) +
                              static_cast<uint64_t>(h->region_count) * sizeof(AtlasRow);
    if (rows_end > h->strings_offset || h->strings_offset > h->total_size) return false;
    const auto* rows = reinterpret_cast<const AtlasRow*>(base + h->regions_offset);
    for (uint32_t i = 0; i < h->region_count; ++i) {
        const AtlasRow& r = rows[i];
        if (r.w == 0 || r.h == 0) return false;
        // Границы страницы — тем же условием, каким их отбивает пекарь: регион, свисающий за край,
        // сэмплит соседний спрайт и при этом читается без единой ошибки.
        if (static_cast<uint32_t>(r.x) + r.w > h->page_width) return false;
        if (static_cast<uint32_t>(r.y) + r.h > h->page_height) return false;
        if (r.name_offset >= h->total_size - h->strings_offset) return false;
    }
    // Блоб обязан кончаться нулём: имя возвращается указателем на C-строку, и блоб без
    // завершающего нуля увёл бы `strcmp` за конец секции.
    if (h->strings_offset == h->total_size || base[h->total_size - 1] != '\0') return false;

    header_ = h;
    rows_ = rows;
    strings_ = reinterpret_cast<const char*>(base + h->strings_offset);
    return true;
}

uint16_t AtlasTable::count() const {
    return header_ != nullptr ? static_cast<uint16_t>(header_->region_count) : 0;
}

const char* AtlasTable::name(RegionId id) const {
    if (header_ == nullptr || id >= header_->region_count) return "";
    return strings_ + rows_[id].name_offset;
}

std::optional<AtlasRegion> AtlasTable::region(RegionId id) const {
    if (header_ == nullptr || id >= header_->region_count) return std::nullopt;
    const AtlasRow& r = rows_[id];
    return AtlasRegion{r.x, r.y, r.w, r.h,
                       Vec2{fix32::from_raw(r.pivot_x_raw), fix32::from_raw(r.pivot_y_raw)}};
}

std::optional<RegionId> AtlasTable::find(const char* name_wanted) const {
    if (header_ == nullptr || name_wanted == nullptr) return std::nullopt;
    for (uint32_t i = 0; i < header_->region_count; ++i) {
        const auto id = static_cast<RegionId>(i);
        if (std::strcmp(name(id), name_wanted) == 0) return id;
    }
    return std::nullopt;
}

} // namespace framework::graphics
