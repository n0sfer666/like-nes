#include "preset_parse.hpp"

#include <cstdint>

// Текстовый слой бейка: манифест — плоский текст, и всё, что превращает его в значения (границы
// полей, число, имя в блобе) или обратно в диагностику, живёт здесь. Грамматика строк
// (`preset_parse.cpp`) зовёт это как готовые примитивы и о разборе символов не знает.
namespace framework::input {

// Дробное значение из манифеста — единственное место, где fix32 берётся из текста. Разбор свой,
// а не strtod: `strtod` зависит от локали, и в ru_RU запятая с точкой меняются местами, отчего
// «0.18» стало бы нулём на машине автора и мёртвой зоной на машине сборщика.
bool preset_parse_fix(const std::string& s, fix32& out) {
    if (s.empty()) return false;
    std::size_t i = 0;
    int32_t sign = 1;
    if (s[i] == '-') { sign = -1; ++i; }
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
            if (scale > 1000000) continue;   // разрядность за пределами Q16.16 отбрасывается
            frac = frac * 10 + (s[i] - '0');
            scale *= 10;
            any = true;
        }
        frac_raw = (frac * fix32::ONE + scale / 2) / scale;
    }
    if (!any || i != s.size()) return false;
    // Сумма проверяется в int64 ДО приведения. Целая часть уже ограничена 32767, но округление
    // дроби вверх добавляет целую единицу: "32767.999999" даёт ровно 2^31, а приведение к int32 —
    // INT32_MIN, то есть число ПРОТИВОПОЛОЖНОГО знака при возврате `true`. Отказ здесь, а не у
    // вызывающего: контракт функции — «true значит число разобрано», и звать её будут не только
    // там, где следом стоит проверка знака.
    const int64_t raw = whole * fix32::ONE + frac_raw;
    if (raw > INT32_MAX) return false;
    out = fix32::from_raw(static_cast<int32_t>(sign * raw));
    return true;
}

bool preset_fail(PresetBakeError& err, int line, const std::string& message) {
    err.line = line;
    err.message = message;
    return false;
}

uint32_t PresetStrings::add(const std::string& s) {
    if (s.empty()) return 0;
    const auto it = seen.find(s);
    if (it != seen.end()) return it->second;
    const uint32_t off = static_cast<uint32_t>(data.size());
    data.insert(data.end(), s.begin(), s.end());
    data.push_back('\0');
    seen.emplace(s, off);
    return off;
}

std::string preset_trim(const std::string& s) {
    std::size_t b = 0, e = s.size();
    while (b < e && (s[b] == ' ' || s[b] == '\t' || s[b] == '\r')) ++b;
    while (e > b && (s[e - 1] == ' ' || s[e - 1] == '\t' || s[e - 1] == '\r')) --e;
    return s.substr(b, e - b);
}

std::vector<std::string> preset_split(const std::string& line) {
    std::vector<std::string> out;
    std::size_t start = 0;
    for (;;) {
        const std::size_t bar = line.find('|', start);
        if (bar == std::string::npos) { out.push_back(preset_trim(line.substr(start))); break; }
        out.push_back(preset_trim(line.substr(start, bar - start)));
        start = bar + 1;
    }
    return out;
}

} // namespace framework::input
