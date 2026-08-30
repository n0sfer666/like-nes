#pragma once
#include <cstddef>
#include <cstdint>
#include <optional>

#include "atlas_format.hpp"
#include "clip.hpp"
#include "fixmath.hpp"

// Рантайм-чтение таблицы регионов прямо из mmap-региона бандла: `open` проверяет магию, версию и
// границы, дальше — только указатели внутрь чужой памяти. Владения нет: регион обязан жить дольше
// таблицы (то же правило, что у `TileMapTable` #16, `ProfileTable` #16 и `PresetTable` #14).
//
// Регион отдаётся КОПИЕЙ структуры, а не указателем в раскладку: `AtlasRow` — пиннутый ABI, и
// вернуть его наружу значило бы, что всякий потребитель регионов ломается вместе с версией формата.
// Двадцать байт на загрузке спрайта — не цена тика.
namespace framework::graphics {

struct AtlasRegion {
    uint16_t x = 0;
    uint16_t y = 0;
    uint16_t w = 0;
    uint16_t h = 0;
    Vec2 pivot{};
};

class AtlasTable {
public:
    AtlasTable() = default;
    bool open(const void* data, std::size_t size);
    bool valid() const { return header_ != nullptr; }

    uint16_t count() const;
    uint16_t page_width() const { return header_ != nullptr ? header_->page_width : 0; }
    uint16_t page_height() const { return header_ != nullptr ? header_->page_height : 0; }
    const char* name(RegionId id) const;

    // И регион, и поиск отвечают `optional`, а не значением с признаком «не нашлось» в полях:
    // пустой прямоугольник рисуется как ничто и читается без ошибки, а нулевой `RegionId` — это
    // ПЕРВЫЙ регион таблицы, то есть промах по имени показал бы чужую картинку вместо отказа.
    std::optional<AtlasRegion> region(RegionId id) const;
    std::optional<RegionId> find(const char* name) const;

private:
    const AtlasHeader* header_ = nullptr;
    const AtlasRow* rows_ = nullptr;
    const char* strings_ = nullptr;
};

} // namespace framework::graphics
