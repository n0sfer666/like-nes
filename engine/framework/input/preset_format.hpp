#pragma once
#include <cstdint>

// Zero-parse раскладка таблицы пресетов ввода: `assetc` печёт её из текстового манифеста, рантайм
// читает прямо из mmap-региона бандла (#5), как таблицу достижений из #10. Смысл тот же — правка
// раскладки не требует пересборки игры, а загрузка не требует парсера в рантайме.
//
// Все поля 4-байтовые и little-endian: бандл target-native, разбор состоит из reinterpret_cast.
namespace framework::input {

constexpr uint8_t PRESET_MAGIC[4] = {'L', 'N', 'F', 'I'}; // like-nes framework input
constexpr uint32_t PRESET_VERSION = 1;
constexpr uint32_t NO_PAIR = 0xFFFFFFFFu;   // ось без парной: зона считается по одной оси

struct PresetHeader {
    uint8_t magic[4];
    uint32_t version;
    uint32_t preset_count;
    uint32_t action_count;    // суммарно по всем пресетам
    uint32_t axis_count;
    uint32_t binding_count;
    uint32_t presets_offset;
    uint32_t actions_offset;
    uint32_t axes_offset;
    uint32_t bindings_offset;
    uint32_t strings_offset;
    uint32_t total_size;
};
static_assert(sizeof(PresetHeader) == 48, "PresetHeader layout pinned (zero-parse ABI)");

// Действия и оси лежат непрерывными диапазонами: пресет — это пара срезов, а не список
// указателей, поэтому таблица переносится побайтово и не требует релокаций.
struct PresetRow {
    uint32_t name_offset;
    uint32_t action_begin;
    uint32_t action_count;
    uint32_t axis_begin;
    uint32_t axis_count;
    uint32_t reserved;
};
static_assert(sizeof(PresetRow) == 24, "PresetRow layout pinned (zero-parse ABI)");

struct BindingRow {
    uint32_t kind;    // ::input::SourceKind
    uint32_t code;
    int32_t sign;
    uint32_t reserved;
};
static_assert(sizeof(BindingRow) == 16, "BindingRow layout pinned (zero-parse ABI)");

struct ActionRow {
    uint32_t name_offset;
    uint32_t binding_begin;
    uint32_t binding_count;
    uint32_t reserved;
};
static_assert(sizeof(ActionRow) == 16, "ActionRow layout pinned (zero-parse ABI)");

// Форма отклика живёт рядом с осью, а не в коде игры: мёртвая зона — свойство раскладки, и
// правка её в ассете не должна требовать пересборки. deadzone/outer — сырой Q16.16.
struct AxisRow {
    uint32_t name_offset;
    BindingRow pos;
    BindingRow neg;
    int32_t deadzone_raw;
    int32_t outer_raw;
    uint32_t curve_exp;
    uint32_t pair_axis;   // индекс парной оси внутри пресета (радиальная зона) либо NO_PAIR
};
static_assert(sizeof(AxisRow) == 52, "AxisRow layout pinned (zero-parse ABI)");

} // namespace framework::input
