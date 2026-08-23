#include "map_text.hpp"

#include <cstdint>

namespace framework::tilemap {

std::string map_trim(const std::string& s) {
    std::size_t b = 0, e = s.size();
    while (b < e && (s[b] == ' ' || s[b] == '\t' || s[b] == '\r')) ++b;
    while (e > b && (s[e - 1] == ' ' || s[e - 1] == '\t' || s[e - 1] == '\r')) --e;
    return s.substr(b, e - b);
}

// Поля обрезаются по краям, включая строку карты: невидимый глиф в грамматике запрещён (см.
// `map_bake.hpp`), поэтому обрезка не может съесть тайл, а редактор, подчистивший хвостовые
// пробелы, не может изменить ширину карты.
std::vector<std::string> map_split(const std::string& line) {
    std::vector<std::string> out;
    std::size_t start = 0;
    for (;;) {
        const std::size_t bar = line.find('|', start);
        if (bar == std::string::npos) {
            out.push_back(map_trim(line.substr(start)));
            break;
        }
        out.push_back(map_trim(line.substr(start, bar - start)));
        start = bar + 1;
    }
    return out;
}

// Дробное значение в Q16.16. Целая часть проверяется на выход за диапазон ДО сдвига: 32768 в fix32
// насыщается, а насыщение здесь было бы тихой ложью — в исходнике одно число, в игре другое.
bool map_parse_fix(const std::string& s, fix32& out) {
    if (s.empty()) return false;
    std::size_t i = 0;
    int32_t sign = 1;
    if (s[i] == '-') {
        sign = -1;
        ++i;
    }
    int64_t whole = 0;
    bool any = false;
    for (; i < s.size() && s[i] >= '0' && s[i] <= '9'; ++i) {
        whole = whole * 10 + (s[i] - '0');
        if (whole > 32767) return false;
        any = true;
    }
    int64_t frac_raw = 0;
    if (i < s.size() && s[i] == '.') {
        ++i;
        int64_t scale = 1, frac = 0;
        for (; i < s.size() && s[i] >= '0' && s[i] <= '9'; ++i) {
            if (scale > 1000000) continue;
            frac = frac * 10 + (s[i] - '0');
            scale *= 10;
            any = true;
        }
        frac_raw = (frac * fix32::ONE + scale / 2) / scale;
    }
    if (!any || i != s.size()) return false;
    // Сумма проверяется в int64 ДО приведения: целая часть ограничена 32767, но округление дроби
    // вверх добавляет целую единицу, и "32767.999999" даёт ровно 2^31 — при приведении к int32 это
    // число ПРОТИВОПОЛОЖНОГО знака при возврате `true`.
    const int64_t raw = whole * fix32::ONE + frac_raw;
    if (raw > INT32_MAX) return false;
    out = fix32::from_raw(static_cast<int32_t>(sign * raw));
    return true;
}

} // namespace framework::tilemap
