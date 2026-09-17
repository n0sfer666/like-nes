#include <cstdio>
#include <string>
#include <vector>

#include "preset_axes.hpp"
#include "preset_format.hpp"

// Номер логической оси считают трое: пекарь, читатель и рантайм (`preset_axes.hpp`). Разойдись они
// хоть на единицу — ось приедет в чужой слот InputFrame, а чужой бандл отобьётся по чужому номеру
// парной оси; увидел бы это только голден. Здесь спрашивают ОДИН вопрос — согласны ли три функции
// между собой, — и спрашивают его на потолке формата. Что написанная раскладка доезжает до кадра,
// проверяет своя цель, `framework_preset_test`: тема другая, и падают они по разным причинам
// (аудит #21, ревью A·2).
int main() {
    using namespace framework::input;

    // Читатель спрашивает номера всех строк ОДНИМ проходом (`logical_axis_map`): построчный вопрос
    // стоит куба по строкам. Оба набора шириной РОВНО в потолок строк, на котором стоит буфер
    // карты. Сплошные тройки ловят соседний повтор, чередование — повтор, между вхождениями
    // которого счётчик осей УСПЕЛ вырасти (из него берётся самопара, ревью A·2).
    bool map_agrees = true;
    for (int set = 0; set < 2; ++set) {
        std::vector<std::string> rows;
        for (uint32_t i = 0; i < MAX_AXIS_ROWS; ++i)
            rows.push_back("ax" + std::to_string(set == 0 ? i / 3 : i % 5));
        const auto name_at = [&rows](uint32_t i) { return rows[i].c_str(); };
        uint32_t axis_of_row[MAX_AXIS_ROWS];
        const uint32_t axes = logical_axis_map(MAX_AXIS_ROWS, name_at, axis_of_row);
        const uint32_t counted = logical_axis_count(MAX_AXIS_ROWS, name_at);
        // Красная строка называет НАБОР, СТРОКУ и обе цифры: расхождение на единицу ищется по ним,
        // а не перезапуском с печатью (ревью A·2). Первой разошедшейся строки хватает — дальше
        // номера съезжают следом за ней.
        if (axes != counted) {
            std::printf("  FAIL: set %d: the map counted %u axes, logical_axis_count says %u\n",
                        set, axes, counted);
            map_agrees = false;
        }
        for (uint32_t i = 0; i < MAX_AXIS_ROWS; ++i) {
            const uint32_t one_by_one = logical_axis_of_row(i, name_at);
            if (axis_of_row[i] == one_by_one) continue;
            std::printf("  FAIL: set %d, row %u ('%s'): the map says axis %u, logical_axis_of_row says %u\n",
                        set, i, rows[i].c_str(), axis_of_row[i], one_by_one);
            map_agrees = false;
            break;
        }
    }

    std::printf("framework-preset-axes: %s\n", map_agrees ? "PASS" : "FAIL");
    return map_agrees ? 0 : 1;
}
