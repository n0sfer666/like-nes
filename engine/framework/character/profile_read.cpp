#include "profile_read.hpp"

#include <cstring>

namespace framework::character {

bool ProfileTable::open(const void* data, std::size_t size) {
    // Обнуляются ВСЕ поля, а не только `header_`. Аксессоры сторожатся им, но объект, у которого
    // `open` не удался, переживает буфер, на котором его пробовали открыть: первый же аксессор,
    // добавленный без сторожа, читал бы освобождённую память вместо пустой таблицы.
    header_ = nullptr;
    rows_ = nullptr;
    strings_ = nullptr;
    strings_size_ = 0;
    if (data == nullptr || size < sizeof(MoveHeader)) return false;
    const auto* base = static_cast<const uint8_t*>(data);
    const auto* h = reinterpret_cast<const MoveHeader*>(base);
    if (std::memcmp(h->magic, MOVE_MAGIC, sizeof(h->magic)) != 0) return false;
    if (h->version != MOVE_VERSION || h->total_size > size) return false;
    // Таблица без строк читается без ошибки и означает «персонаж не настроен». Пекарь пустой
    // манифест отвергает именно поэтому — читатель обязан отвергать обнулённый счётчик по той же
    // причине, иначе порченый бандл отдаёт отладку в рантайм.
    if (h->profile_count == 0) return false;
    // Смещение строк проверяется и СНИЗУ, и на выравнивание: `profiles_offset = 0` наложил бы
    // строки на заголовок, а `= 4` дал бы невыровненный `reinterpret_cast` — то есть SIGBUS на
    // strict-align. Ровно эту проверку держит `engine/asset/bundle_view.cpp` для своей таблицы.
    if (h->profiles_offset < sizeof(MoveHeader)) return false;
    if (h->profiles_offset % alignof(MoveRow) != 0) return false;
    // Проверка «влезает ли секция» — единственная защита zero-parse формата: дальше по нему ходят
    // указателями, и битый бандл иначе читался бы как чужая память. Сумма считается в uint64:
    // в `size_t` на 32-битной цели `profile_count * sizeof(MoveRow)` заворачивается, и подобранный
    // счётчик обходил бы границу — та же арифметика, что у `align64` в `bundle_writer.cpp`.
    const uint64_t rows_end = static_cast<uint64_t>(h->profiles_offset) +
                              static_cast<uint64_t>(h->profile_count) * sizeof(MoveRow);
    if (rows_end > size || h->strings_offset < rows_end) return false;
    if (h->strings_offset >= h->total_size) return false;

    rows_ = reinterpret_cast<const MoveRow*>(base + h->profiles_offset);
    strings_ = reinterpret_cast<const char*>(base + h->strings_offset);
    strings_size_ = h->total_size - h->strings_offset;
    if (strings_[strings_size_ - 1] != '\0') return false;   // обход имён обязан упереться в ноль
    header_ = h;
    return true;
}

const char* ProfileTable::name(uint32_t index) const {
    if (header_ == nullptr || index >= header_->profile_count) return "";
    const uint32_t off = rows_[index].name_offset;
    return off < strings_size_ ? strings_ + off : "";
}

bool ProfileTable::at(uint32_t index, MoveProfile& out) const {
    if (header_ == nullptr || index >= header_->profile_count) return false;
    const MoveRow& r = rows_[index];
    out.max_speed = fix32::from_raw(r.max_speed_raw);
    out.ground_accel = fix32::from_raw(r.ground_accel_raw);
    out.ground_decel = fix32::from_raw(r.ground_decel_raw);
    out.air_accel = fix32::from_raw(r.air_accel_raw);
    out.air_decel = fix32::from_raw(r.air_decel_raw);
    out.gravity_rise = fix32::from_raw(r.gravity_rise_raw);
    out.gravity_fall = fix32::from_raw(r.gravity_fall_raw);
    out.max_fall_speed = fix32::from_raw(r.max_fall_speed_raw);
    out.jump_height = fix32::from_raw(r.jump_height_raw);
    out.min_jump_height = fix32::from_raw(r.min_jump_height_raw);
    out.coyote_ticks = r.coyote_ticks;
    out.buffer_ticks = r.buffer_ticks;
    out.corner_correction = fix32::from_raw(r.corner_correction_raw);
    out.ground_snap = fix32::from_raw(r.ground_snap_raw);
    out.max_slope = fix32::from_raw(r.max_slope_raw);
    out.climb_speed = fix32::from_raw(r.climb_speed_raw);
    out.ladder_regrab_ticks = r.ladder_regrab_ticks;
    return true;
}

bool ProfileTable::find(const char* name_wanted, MoveProfile& out) const {
    if (header_ == nullptr || name_wanted == nullptr) return false;
    for (uint32_t i = 0; i < header_->profile_count; ++i)
        if (std::strcmp(name(i), name_wanted) == 0) return at(i, out);
    return false;
}

} // namespace framework::character
