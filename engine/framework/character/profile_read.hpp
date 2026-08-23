#pragma once
#include <cstddef>
#include <cstdint>

#include "profile.hpp"
#include "profile_format.hpp"

// Рантайм-чтение таблицы профилей движения прямо из mmap-региона бандла: `open` проверяет магию,
// версию и границы, дальше — только указатели внутрь чужой памяти. Владения нет: регион обязан
// жить дольше таблицы (то же правило, что у `PresetTable` #14).
namespace framework::character {

class ProfileTable {
public:
    ProfileTable() = default;
    bool open(const void* data, std::size_t size);
    bool valid() const { return header_ != nullptr; }

    uint32_t count() const { return header_ != nullptr ? header_->profile_count : 0; }
    const char* name(uint32_t index) const;
    // Профиль возвращается ЗНАЧЕНИЕМ и БЕЗ `sanitize`: приведение — дело контроллера и `derive`,
    // а таблица обязана отдать ровно то, что испечено. Иначе тест round-trip сверял бы бейк с
    // приведением, и порча значения, попадающая в законный диапазон, проходила бы молча.
    bool at(uint32_t index, MoveProfile& out) const;
    bool find(const char* name, MoveProfile& out) const;

private:
    const MoveHeader* header_ = nullptr;
    const MoveRow* rows_ = nullptr;
    const char* strings_ = nullptr;
    uint32_t strings_size_ = 0;
};

} // namespace framework::character
