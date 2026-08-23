#include "profile_text.hpp"

#include <cstdint>

namespace framework::character {

std::string profile_trim(const std::string& s) {
    std::size_t b = 0, e = s.size();
    while (b < e && (s[b] == ' ' || s[b] == '\t' || s[b] == '\r')) ++b;
    while (e > b && (s[e - 1] == ' ' || s[e - 1] == '\t' || s[e - 1] == '\r')) --e;
    return s.substr(b, e - b);
}

std::vector<std::string> profile_split(const std::string& line) {
    std::vector<std::string> out;
    std::size_t start = 0;
    for (;;) {
        const std::size_t bar = line.find('|', start);
        if (bar == std::string::npos) {
            out.push_back(profile_trim(line.substr(start)));
            break;
        }
        out.push_back(profile_trim(line.substr(start, bar - start)));
        start = bar + 1;
    }
    return out;
}

// Дробное значение в Q16.16. Целая часть проверяется на выход за диапазон ДО сдвига: 32768 в
// fix32 насыщается, а насыщение здесь было бы тихой ложью — в манифесте одно число, в игре другое.
// Хвост разрядности за пределами Q16.16 отбрасывается, а не отвергается: 0.0000001 это законный
// способ написать «почти ноль», и требовать от автора знания шага сетки нечестно.
bool profile_parse_fix(const std::string& s, fix32& out) {
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

// Тики — только целые и только неотрицательные. Дробь здесь не «округляется до ближайшего», а
// отвергается: «6.5 тика» означает, что автор считал окно в секундах, и молчаливое округление
// вернуло бы ровно ту зависимость от шага, ради ухода от которой окна и заданы в тиках.
bool profile_parse_u32(const std::string& s, uint32_t& out) {
    if (s.empty()) return false;
    uint64_t v = 0;
    for (std::size_t i = 0; i < s.size(); ++i) {
        if (s[i] < '0' || s[i] > '9') return false;
        v = v * 10 + static_cast<uint64_t>(s[i] - '0');
        if (v > 0xFFFFFFFFull) return false;
    }
    out = static_cast<uint32_t>(v);
    return true;
}

} // namespace framework::character
