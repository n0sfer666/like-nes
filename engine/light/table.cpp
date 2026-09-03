#include "table.hpp"

#include <cstring>

namespace light {

const char* load_reason(LoadResult r) {
    switch (r) {
    case LoadResult::Ok: return "ok";
    case LoadResult::TooShort: return "table shorter than its header";
    case LoadResult::BadMagic: return "magic is not LNLT";
    case LoadResult::BadVersion: return "unsupported table version";
    case LoadResult::BadLayout: return "a section does not fit the table";
    case LoadResult::BadString: return "string section is not terminated or an offset escapes it";
    }
    return "unknown";
}

LoadResult Table::load(const void* base, std::size_t size) {
    // Обнуляются ВСЕ поля: отказавшая загрузка иначе оставила бы указатели предыдущей — `count()`
    // отдавал бы ноль, а `row()`/`name()` читали бы уже снятый регион (та же поломка закрыта в
    // `mat::Table::load`, и написана она здесь второй раз не по инерции: поля свои).
    header_ = nullptr;
    rows_ = nullptr;
    strings_ = nullptr;
    strings_size_ = 0;

    // Сам `base` обязан быть выровнен: секции проверяются ОТНОСИТЕЛЬНО него, и `reinterpret_cast`
    // до `LightRow*` с нечётного адреса даёт UB, а на strict-align — SIGBUS.
    if (reinterpret_cast<std::uintptr_t>(base) % 8 != 0) return LoadResult::BadLayout;
    if (size < sizeof(TableHeader)) return LoadResult::TooShort;
    const auto* h = static_cast<const TableHeader*>(base);
    if (std::memcmp(h->magic, TABLE_MAGIC, 4) != 0) return LoadResult::BadMagic;
    if (h->version != TABLE_VERSION) return LoadResult::BadVersion;
    if (h->total_size != size) return LoadResult::BadLayout;

    const uint64_t rows = static_cast<uint64_t>(h->light_count) * sizeof(LightRow);
    if (h->lights_offset % 8 != 0) return LoadResult::BadLayout;
    // Секции обязаны идти ЗА заголовком и НЕ пересекаться. Без этих двух правил заголовок, чьи
    // строки начинаются внутри массива источников (или чьи источники лежат поверх него самого),
    // проходил все проверки и читался как валидная таблица: UB нет — всё в границах региона, — но
    // отказ и мусор переставали различаться, а именно это различие таблица и обязана давать.
    if (h->lights_offset < sizeof(TableHeader)) return LoadResult::BadLayout;
    if (static_cast<uint64_t>(h->lights_offset) + rows > size) return LoadResult::BadLayout;
    if (h->strings_offset > size) return LoadResult::BadLayout;
    if (h->strings_offset < static_cast<uint64_t>(h->lights_offset) + rows)
        return LoadResult::BadLayout;

    const char* str = static_cast<const char*>(base) + h->strings_offset;
    const std::size_t str_len = size - h->strings_offset;
    if (str_len == 0 || str[str_len - 1] != '\0') return LoadResult::BadString;

    const auto* r =
        reinterpret_cast<const LightRow*>(static_cast<const uint8_t*>(base) + h->lights_offset);
    for (uint32_t i = 0; i < h->light_count; ++i) {
        if (r[i].name_off >= str_len) return LoadResult::BadString;
        if (r[i].kind > static_cast<uint8_t>(Kind::Directional)) return LoadResult::BadLayout;
    }

    header_ = h;
    rows_ = r;
    strings_ = str;
    strings_size_ = str_len;
    return LoadResult::Ok;
}

const LightRow* Table::row(uint32_t i) const {
    if (!rows_ || i >= count()) return nullptr;
    return &rows_[i];
}

const char* Table::name(uint32_t i) const {
    const LightRow* r = row(i);
    if (!r || r->name_off >= strings_size_) return nullptr;
    return strings_ + r->name_off;
}

uint32_t Table::find(const char* want) const {
    if (!want) return count();
    for (uint32_t i = 0; i < count(); ++i) {
        const char* n = name(i);
        if (n && std::strcmp(n, want) == 0) return i;
    }
    return count();
}

} // namespace light
