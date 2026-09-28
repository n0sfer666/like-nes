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

// Потолок строк осей одного пресета — восемь альтернативных источников на каждую из восьми осей
// движка. Логические оси считаются попарным сравнением имён строк, и без потолка чужой бандл с
// миллионом строк вешал бы старт игры квадратом (аудит #21, A·2·1b).
constexpr uint32_t MAX_AXIS_ROWS = 64;

// Потолок пресетов в таблице — того же рода, что и потолок строк осей, только цена его отсутствия
// платится ЗА КАЖДЫЙ пресет: проверка срезов обходит оси каждого из них попарным сравнением имён.
// Замер ревью: 40000 пресетов, делящих срез в 64 строки, держали `open()` 764 мс и возвращали
// true — чужой бандл останавливал старт игры не отказом, а работой (аудит #21, ревью A·2).
constexpr uint32_t MAX_PRESETS = 64;

// Потолок длины имени — того же рода, что и потолок строк. Имена таблицы сравниваются ДРУГ С
// ДРУГОМ (строка против строки, а не против короткого запроса), поэтому одно имя в мегабайт стоит
// на старте столько же, сколько миллион строк: цена линейна по размеру секции (аудит #21, A·2·1b).
constexpr uint32_t MAX_NAME = 64;

struct PresetHeader {
    uint8_t magic[4];
    uint32_t version;
    uint32_t preset_count;
    uint32_t action_count;    // суммарно по всем пресетам
    uint32_t axis_count;
    uint32_t binding_count;
    uint32_t pad_count;
    uint32_t presets_offset;
    uint32_t actions_offset;
    uint32_t axes_offset;
    uint32_t bindings_offset;
    uint32_t pads_offset;
    uint32_t strings_offset;
    uint32_t total_size;
};
static_assert(sizeof(PresetHeader) == 56, "PresetHeader layout pinned (zero-parse ABI)");

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

// Профиль геймпада — тоже данные (решение 4 спеки #14): добавление модели не требует релиза
// движка. vid == 0 значит «сопоставлять по имени»: XInput и GameController.framework VID/PID
// не отдают вовсе, и на двух ОС из трёх имя — единственный канал.
struct PadRow {
    uint32_t name_offset;
    uint32_t match_offset;   // подстрока имени устройства (регистр не важен)
    uint32_t vid;            // 0 — не сопоставлять по VID/PID
    uint32_t pid;            // 0 — весь вендор
    uint32_t labels;         // PadLabels: набор надписей на кнопках (коды уже позиционные)
    int32_t deadzone_raw;
    int32_t outer_raw;
    uint32_t curve_exp;
    int32_t trigger_raw;
    uint32_t reserved;
};
static_assert(sizeof(PadRow) == 40, "PadRow layout pinned (zero-parse ABI)");

} // namespace framework::input
