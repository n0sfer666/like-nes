#pragma once
#include <cstdint>

// Zero-parse раскладка таблицы профилей движения: `assetc` печёт её из текстового манифеста,
// рантайм читает прямо из mmap-региона бандла — как таблица пресетов ввода (#14) и таблица
// достижений (#10). Смысл тот же и здесь он главный: профиль это НАСТРОЙКА ОЩУЩЕНИЯ, её правят
// десятками итераций подряд, и пересборка движка на каждую правку высоты прыжка убивает сам цикл
// подбора.
//
// Доехали пока ПЕКАРЬ И ЧИТАТЕЛЬ, потребителя в рантайме нет: контроллер берёт значения у
// `default_profile()`, и цикл подбора выше — то, ради чего формат существует, а не то, что он уже
// даёт. Подключение приходит с образцом-платформером.
//
// Все поля 4-байтовые и little-endian, бандл target-native: разбор состоит из reinterpret_cast.
// Величины лежат СЫРЫМ Q16.16 (`fix32::raw`), а не числами с плавающей точкой — иначе таблица
// зависела бы от округления на машине, которая её пекла, и байт-детерминизм бейка (условие #5)
// держался бы на совпадении FPU, а не на арифметике.
namespace framework::character {

constexpr uint8_t MOVE_MAGIC[4] = {'L', 'N', 'F', 'M'};   // like-nes framework movement
// Версия 2 (2026-08-24): в строку дописаны `corner_correction` и `ground_snap` — геометрическое
// прощение вертикали 3. Читатель отвергает версию 1 ПО НОМЕРУ, миграции нет и не будет: потребителя
// в рантайме у таблицы пока не появилось, а бандл перепечён тем же коммитом. Разбирать чужую старую
// раскладку значило бы держать вторую копию читателя ради байтов, которых нигде нет.
constexpr uint32_t MOVE_VERSION = 2;

struct MoveHeader {
    uint8_t magic[4];
    uint32_t version;
    uint32_t profile_count;
    uint32_t profiles_offset;
    uint32_t strings_offset;
    uint32_t total_size;
};
static_assert(sizeof(MoveHeader) == 24, "MoveHeader layout pinned (zero-parse ABI)");

// Порядок полей строки повторяет порядок в `MoveProfile` — и это не косметика: пекарь и читатель
// перекладывают их вручную, поле за полем, и переставленная пара `ground_*`/`air_*` дала бы
// таблицу, которая читается без ошибки и означает другое. Тест round-trip сверяет ВСЕ поля именно
// поэтому: сверка «имя и высота прыжка совпали» пропустила бы такую перестановку целиком.
struct MoveRow {
    uint32_t name_offset;
    int32_t max_speed_raw;
    int32_t ground_accel_raw;
    int32_t ground_decel_raw;
    int32_t air_accel_raw;
    int32_t air_decel_raw;
    int32_t gravity_rise_raw;
    int32_t gravity_fall_raw;
    int32_t max_fall_speed_raw;
    int32_t jump_height_raw;
    int32_t min_jump_height_raw;
    uint32_t coyote_ticks;
    uint32_t buffer_ticks;
    // Новое дописано В КОНЕЦ, а не вставлено к родственным полям: вставка в середину сдвигает ВСЕ
    // последующие смещения, то есть меняет смысл каждого байта после места вставки, и старая
    // таблица прочиталась бы новым читателем без единой ошибки — просто другими числами.
    int32_t corner_correction_raw;
    int32_t ground_snap_raw;
};
static_assert(sizeof(MoveRow) == 60, "MoveRow layout pinned (zero-parse ABI)");

} // namespace framework::character
