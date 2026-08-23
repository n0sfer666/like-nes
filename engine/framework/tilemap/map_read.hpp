#pragma once
#include <cstddef>
#include <cstdint>
#include <optional>

#include "grid.hpp"
#include "map_format.hpp"

// Рантайм-чтение таблицы карт прямо из mmap-региона бандла: `open` проверяет магию, версию и
// границы, дальше — только указатели внутрь чужой памяти. Владения нет: регион обязан жить дольше
// таблицы (то же правило, что у `ProfileTable` #16 и `PresetTable` #14).
//
// `build` при этом КОПИРУЕТ флаги в сетку, и это не оговорка к слову «zero-parse»: разбора текста
// и раскладки полей тут по-прежнему нет, а копия блока — цена одной загрузки уровня, не тика.
// Заимствующий двойник `TileGrid` пришлось бы наделить теми же `window`/`tile_bounds`, то есть
// завести вторую копию геометрии сетки — ровно того кода, расхождение в котором даёт проход сквозь
// стену.
namespace framework::tilemap {

class TileMapTable {
public:
    TileMapTable() = default;
    bool open(const void* data, std::size_t size);
    bool valid() const { return header_ != nullptr; }

    uint32_t count() const { return header_ != nullptr ? header_->map_count : 0; }
    const char* name(uint32_t index) const;
    // Сетка возвращается `optional`, а не через out-параметр с флагом: `TileGrid` не имеет
    // умолчания намеренно — пустая сетка отвечала бы «пусто» на любой запрос, и уровень, который
    // не прочитался, выглядел бы как уровень без стен. Проверить `optional` вызывающий обязан.
    std::optional<TileGrid> build(uint32_t index) const;
    std::optional<TileGrid> find(const char* name) const;

private:
    const uint8_t* base_ = nullptr;
    const MapHeader* header_ = nullptr;
    const MapRow* rows_ = nullptr;
    const char* strings_ = nullptr;
    uint32_t strings_size_ = 0;
};

} // namespace framework::tilemap
