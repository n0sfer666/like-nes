#pragma once
#include <cstdint>
#include <string>
#include <vector>

// Бейк текстового манифеста пресетов в zero-parse таблицу (`preset_format.hpp`). Живёт в движке,
// а не в `assetc`, по образцу достижений #10: тот же разбор нужен тестам, и вторая копия в
// инструменте разошлась бы с первой молча.
//
// Формат (разделитель — `|`, `#` до конца строки — комментарий):
//   preset | <имя>
//   action | <имя> | <источник> [| <источник> …]
//   axis   | <имя> | <источник +> | <источник -|->
//   shape  | <имя> | <мёртвая зона> | <насыщение> | <степень кривой> | <парная ось|->
// Строки `action`/`axis`/`shape` относятся к последнему объявленному `preset`.
namespace framework::input {

struct PresetBakeError {
    int line = 0;
    std::string message;
};

bool bake_presets(const std::string& text, std::vector<uint8_t>& out, PresetBakeError& err);
bool bake_presets_file(const std::string& path, std::vector<uint8_t>& out, PresetBakeError& err);

} // namespace framework::input
