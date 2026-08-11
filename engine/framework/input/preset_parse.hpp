#pragma once
#include <cstdint>
#include <map>
#include <string>
#include <vector>

#include "fixed.hpp"
#include "preset_bake.hpp"
#include "preset_format.hpp"

// Внутренняя граница бейка: разбор строк манифеста (здесь) отделён от сборки байтов таблицы
// (`preset_bake.cpp`). Разделение не косметическое — раскладка таблицы пиннута static_assert'ами
// и меняется вместе с версией формата, а грамматика манифеста живёт своей жизнью.
//
// Сам разбор разложен на три файла по вопросу, на который каждый отвечает:
//   `preset_parse.cpp`    — грамматика строки: сколько полей, что в них лежит, куда это класть;
//   `preset_validate.cpp` — проверки, которым мало одной строки: ёмкость движка, противоречия
//                           между строками пресета, привязка форм в `preset_close`;
//   `preset_text.cpp`     — текстовый слой: trim/split, число из текста, блоб имён.
namespace framework::input {

// Форма отклика объявляется отдельной строкой и применяется к оси в конце пресета: иначе порядок
// строк `axis` и `shape` в манифесте стал бы значащим.
struct PresetShape {
    int32_t deadzone_raw = 0;
    int32_t outer_raw = fix32::ONE;
    uint32_t curve_exp = 1;
    std::string pair;
    int line = 0;   // строка манифеста: неприкреплённую форму нужно назвать ЕЮ, а не местом находки
};

struct PresetStrings {
    std::vector<char> data{'\0'};   // смещение 0 — пустая строка, значит «имени нет»
    std::map<std::string, uint32_t> seen;

    uint32_t add(const std::string& s);
};

struct PresetBuild {
    std::vector<PresetRow> presets;
    std::vector<PadRow> pads;
    std::vector<ActionRow> actions;
    std::vector<AxisRow> axes;
    std::vector<BindingRow> bindings;
    std::vector<std::string> axis_names;       // параллельно axes: по ним разрешаются пары
    std::map<std::string, PresetShape> shapes; // формы текущего пресета
    PresetStrings blob;
};

std::string preset_trim(const std::string& s);
std::vector<std::string> preset_split(const std::string& line);
bool preset_parse_line(PresetBuild& b, const std::vector<std::string>& fields, int line,
                       PresetBakeError& err);
bool preset_close(PresetBuild& b, PresetBakeError& err, int line);
// Обе проверки зовутся ПОСЛЕ того, как строка легла в сборку: судить о пресете можно только по
// нему целиком, а грамматика строки к тому моменту уже сказала своё.
bool preset_check_action_row(const PresetBuild& b, int line, PresetBakeError& err);
bool preset_check_axis_row(const PresetBuild& b, const std::string& name, int line,
                           PresetBakeError& err);
bool preset_parse_pad(PresetBuild& b, const std::vector<std::string>& fields, int line,
                      PresetBakeError& err);
bool preset_fail(PresetBakeError& err, int line, const std::string& message);
bool preset_parse_fix(const std::string& s, fix32& out);

} // namespace framework::input
