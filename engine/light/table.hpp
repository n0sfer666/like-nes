#pragma once
#include <cstddef>
#include <cstdint>

namespace light {

// Zero-parse таблица источников света: рантайм читает её прямо из mmap-региона бандла, как
// материалы (#18), достижения (#10) и пресеты ввода (#14). Раскладка запинена static_assert'ами —
// это ABI между пекарем и читателем.
constexpr uint8_t TABLE_MAGIC[4] = {'L', 'N', 'L', 'T'};
constexpr uint32_t TABLE_VERSION = 1;

enum class Kind : uint8_t { Point = 0, Directional = 1 };

struct TableHeader {
    uint8_t magic[4];
    uint32_t version;
    uint32_t light_count;
    uint32_t lights_offset;
    uint32_t strings_offset;
    uint32_t total_size;
};
static_assert(sizeof(TableHeader) == 24, "TableHeader layout pinned (zero-parse ABI)");

// Направление хранится УЖЕ НОРМАЛИЗОВАННЫМ: нормализация в кадре стоила бы корня на источник, а
// главное — «забыли нормализовать» стало бы тусклым светом вместо отказа. Пекарь нормализует и
// отбивает нулевой вектор, которому нормализоваться нечем.
struct LightRow {
    float pos[2];       // мир; у направленного не читается
    float height;       // высота над плоскостью спрайтов
    float dir[2];       // единичный; у точечного нули
    float color[3];     // линейный rgb, 0..1
    float intensity;    // множитель, доля
    float radius;       // мир; у направленного 0 — затухания у него нет
    uint32_t name_off;
    uint8_t kind;
    uint8_t reserved[3];
};
static_assert(sizeof(LightRow) == 48, "LightRow layout pinned (zero-parse ABI)");

enum class LoadResult : uint32_t {
    Ok = 0,
    TooShort = 1,
    BadMagic = 2,
    BadVersion = 3,
    BadLayout = 4,
    BadString = 5,
};

// Причина отказа словом: код возврата читает машина, а печатает его человек, и «load вернул 4»
// в логе не отличает разъехавшуюся раскладку от битой строки.
const char* load_reason(LoadResult r);

// Читатель НЕ копирует: он держит указатель в чужой регион и живёт не дольше него.
class Table {
public:
    LoadResult load(const void* data, std::size_t size);

    uint32_t count() const { return header_ ? header_->light_count : 0; }
    const LightRow* row(uint32_t i) const;
    const char* name(uint32_t i) const;

    // Индекс по имени или `count()`, если такого нет. Линейный поиск намеренно: источников в
    // сцене десятки, а таблица имён стоила бы бейку сортировки, которую нечем проверить дешевле.
    uint32_t find(const char* name) const;

private:
    const TableHeader* header_ = nullptr;
    const LightRow* rows_ = nullptr;
    const char* strings_ = nullptr;
    std::size_t strings_size_ = 0;
};

} // namespace light
