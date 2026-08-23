#pragma once
#include <cstdint>
#include <string>
#include <vector>

#include "profile.hpp"

// Бейк текстового манифеста профилей движения в zero-parse таблицу (`profile_format.hpp`). Живёт
// в движке, а не в `assetc`, по образцу пресетов #14 и достижений #10: тот же разбор нужен тестам,
// и вторая копия в инструменте разошлась бы с первой молча.
//
// Формат (разделитель — `|`, `#` до конца строки — комментарий, пустые строки игнорируются):
//   profile | <имя>
//   <ключ>  | <значение>
// Ключи — имена полей `MoveProfile`: max_speed, ground_accel, ground_decel, air_accel, air_decel,
// gravity_rise, gravity_fall, max_fall_speed, jump_height, min_jump_height, coyote_ticks,
// buffer_ticks. Первые десять — дробные (юнит/с, юнит/с², юнит), последние два — целые ТИКИ
// (решение 4 спеки #16: окно в секундах при делении на шаг даёт дробное число тиков).
//
// Пекарь отбивает НОМЕРОМ СТРОКИ, а не чинит молча:
//   * ключ до первой строки `profile` — значение, которому некуда лечь;
//   * незнакомый ключ — опечатка в имени поля иначе означала бы «поле осталось по умолчанию»;
//   * повтор ключа внутри профиля — победил бы последний, то есть смысл решал бы порядок строк;
//   * НЕДОСТАЮЩИЙ ключ (проверяется на закрытии профиля) — умолчание здесь ложь: ноль это законное
//     значение почти для каждого поля, и «забыл написать» неотличимо от «хотел ноль»;
//   * значение за пределами профиля (`MAX_MOVE_SPEED` и соседи) — молчаливый кламп сделал бы файл
//     враньём: в манифесте стояло бы одно число, в игре работало бы другое;
//   * `min_jump_height` выше `jump_height` — перевёрнутая механика («отпустил сразу — прыгнул
//     выше»), а не экзотическая настройка;
//   * повтор ИМЕНИ профиля — поиск по имени вернул бы первый, а правили бы второй.
namespace framework::character {

struct ProfileBakeError {
    int line = 0;
    std::string message;
};

struct NamedProfile {
    std::string name;
    MoveProfile profile;
};

// Разобрать манифест в профили. Отдельно от сборки байтов: тест вправе спросить, ЧТО прочиталось,
// не разбирая при этом таблицу обратно.
bool parse_profiles(const std::string& text, std::vector<NamedProfile>& out, ProfileBakeError& err);

bool bake_profiles(const std::string& text, std::vector<uint8_t>& out, ProfileBakeError& err);
bool bake_profiles_file(const std::string& path, std::vector<uint8_t>& out, ProfileBakeError& err);

} // namespace framework::character
