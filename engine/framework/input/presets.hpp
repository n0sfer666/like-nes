#pragma once
#include <cstdint>
#include <cstddef>

#include "action_map.hpp"
#include "pad_profile.hpp"
#include "preset_format.hpp"
#include "stick.hpp"

// Рантайм-чтение таблицы пресетов прямо из mmap-региона бандла: конструктор проверяет магию,
// версию и границы, дальше — только указатели внутрь чужой памяти. Владения нет: регион обязан
// жить дольше таблицы.
namespace framework::input {

// Идентификатор действия/оси — ИНДЕКС ВНУТРИ ПРЕСЕТА, а не глобальный: пресет самодостаточен,
// и добавление действия в другой пресет не двигает чужие биты в InputFrame.
class PresetTable {
public:
    PresetTable() = default;
    bool open(const void* data, std::size_t size);
    bool valid() const { return header_ != nullptr; }

    uint32_t preset_count() const { return header_ != nullptr ? header_->preset_count : 0; }
    int find_preset(const char* name) const;
    const char* preset_name(uint32_t preset) const;

    uint32_t action_count(uint32_t preset) const;
    uint32_t axis_count(uint32_t preset) const;
    int find_action(uint32_t preset, const char* name) const;
    const char* action_name(uint32_t preset, uint32_t action) const;

    // Альтернативные источники действия: перебинды адресуют биндинг парой (действие, номер).
    uint32_t action_binding_count(uint32_t preset, uint32_t action) const;
    bool action_source(uint32_t preset, uint32_t action, uint32_t which, ::input::Source& out) const;

    int find_axis(uint32_t preset, const char* name) const;

    // Форма отклика оси и индекс парной оси (NO_PAIR — зона считается по одной оси).
    StickShape axis_shape(uint32_t preset, uint32_t axis) const;
    uint32_t axis_pair(uint32_t preset, uint32_t axis) const;

    // Профиль подключённого пада из таблицы: VID/PID первичны, при нулях решает имя. Ничего не
    // подошло — generic Xbox-раскладка МОЛЧА: неизвестный пад обязан играть из коробки.
    PadProfile profile_for(const ::input::PadInfo& info) const;

    // Заливка пресета в карту действий: действия и оси получают индексы в порядке объявления.
    bool bind(uint32_t preset, ::input::ActionMap& map, int context = 0) const;

private:
    const char* string_at(uint32_t offset) const;
    const PresetRow* preset_at(uint32_t index) const;
    bool row_is_first(const PresetRow& p, uint32_t row) const;
    uint32_t logical_axis(const PresetRow& p, uint32_t row) const;
    int first_row_of_axis(const PresetRow& p, uint32_t axis) const;

    const PresetHeader* header_ = nullptr;
    const PresetRow* presets_ = nullptr;
    const ActionRow* actions_ = nullptr;
    const AxisRow* axes_ = nullptr;
    const BindingRow* bindings_ = nullptr;
    const PadRow* pads_ = nullptr;
    const char* strings_ = nullptr;
    uint32_t strings_size_ = 0;
};

} // namespace framework::input
