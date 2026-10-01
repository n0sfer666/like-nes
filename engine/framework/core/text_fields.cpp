#include "text_fields.hpp"

#include <algorithm>

namespace framework::core {

std::string trim(const std::string& s) {
    std::size_t b = 0, e = s.size();
    while (b < e && (s[b] == ' ' || s[b] == '\t' || s[b] == '\r')) ++b;
    while (e > b && (s[e - 1] == ' ' || s[e - 1] == '\t' || s[e - 1] == '\r')) --e;
    return s.substr(b, e - b);
}

std::vector<std::string> split_fields(const std::string& line) {
    std::vector<std::string> out;
    std::size_t start = 0;
    for (;;) {
        const std::size_t bar = line.find('|', start);
        if (bar == std::string::npos) {
            out.push_back(trim(line.substr(start)));
            break;
        }
        out.push_back(trim(line.substr(start, bar - start)));
        start = bar + 1;
    }
    return out;
}

namespace {

constexpr int64_t FRAC_DIGITS = 7;
constexpr int64_t FRAC_SCALE = 10000000;
constexpr int64_t WHOLE_DIGITS = 5;
constexpr int64_t EXP_LIMIT = int64_t{1} << 40;

int64_t digit_at(std::string_view whole, std::string_view frac, int64_t k) {
    const int64_t w = static_cast<int64_t>(whole.size());
    if (k < 0) return 0;
    if (k < w) return whole[static_cast<std::size_t>(k)] - '0';
    const int64_t f = k - w;
    if (f < static_cast<int64_t>(frac.size())) return frac[static_cast<std::size_t>(f)] - '0';
    return 0;
}

} // namespace

// Дробное значение в Q16.16. Целая часть проверяется на выход за диапазон ДО сдвига: 32768 в fix32
// насыщается, а насыщение здесь было бы тихой ложью — в исходнике одно число, в игре другое. Хвост
// разрядности за пределами семи знаков отбрасывается, а не отвергается: 0.0000001 это законный
// способ написать «почти ноль», и требовать от автора знания шага сетки нечестно. Поэтому и шум
// Qt вида `-1.13686837721616e-13` становится нулём, а не отказом.
//
// Целая часть судится по ЗНАЧАЩИМ цифрам, а не по позиции точки: `0e999999` — законный ноль, а
// цикл до позиции точки шёл бы миллиард шагов. Порядок зажат заранее, иначе сумма с длиной
// мантиссы переполняет int64 на `1e9223372036854775807`.
bool fix_from_decimal(bool negative, std::string_view whole, std::string_view frac, int64_t exp10,
                      fix32& out) {
    const auto digits = [](std::string_view d) {
        return std::all_of(d.begin(), d.end(), [](char ch) { return ch >= '0' && ch <= '9'; });
    };
    if (!digits(whole) || !digits(frac)) return false;
    exp10 = exp10 < -EXP_LIMIT ? -EXP_LIMIT : (exp10 > EXP_LIMIT ? EXP_LIMIT : exp10);
    const int64_t len = static_cast<int64_t>(whole.size() + frac.size());
    const int64_t point = static_cast<int64_t>(whole.size()) + exp10;
    int64_t first = 0;
    while (first < len && digit_at(whole, frac, first) == 0) ++first;
    int64_t whole_value = 0;
    if (first < len && first < point) {
        if (point - first > WHOLE_DIGITS) return false;
        for (int64_t k = first; k < point; ++k)
            whole_value = whole_value * 10 + digit_at(whole, frac, k);
    }
    int64_t frac_value = 0;
    for (int64_t k = point; k < point + FRAC_DIGITS; ++k)
        frac_value = frac_value * 10 + digit_at(whole, frac, k);
    const int64_t frac_raw = (frac_value * fix32::ONE + FRAC_SCALE / 2) / FRAC_SCALE;
    // Сумма проверяется в int64 ДО приведения, и это единственная проверка диапазона: целая часть
    // в пять цифр сюда доходит любой, а округление дроби вверх добавляет ещё целую единицу:
    // "32767.999999" даёт ровно 2^31, а приведение к int32 —
    // INT32_MIN, то есть число ПРОТИВОПОЛОЖНОГО знака при возврате `true`. Отказ здесь, а не у
    // вызывающего: контракт функции — «true значит число разобрано», и звать её будут не только
    // там, где следом стоит проверка знака.
    const int64_t raw = whole_value * fix32::ONE + frac_raw;
    if (raw > INT32_MAX) return false;
    out = fix32::from_raw(static_cast<int32_t>(negative ? -raw : raw));
    return true;
}

bool parse_fix(const std::string& s, fix32& out) {
    std::size_t i = 0;
    const bool negative = i < s.size() && s[i] == '-';
    if (negative) ++i;
    const std::size_t whole_begin = i;
    while (i < s.size() && s[i] >= '0' && s[i] <= '9') ++i;
    const std::string_view whole(s.data() + whole_begin, i - whole_begin);
    std::string_view frac;
    if (i < s.size() && s[i] == '.') {
        const std::size_t frac_begin = ++i;
        while (i < s.size() && s[i] >= '0' && s[i] <= '9') ++i;
        frac = std::string_view(s.data() + frac_begin, i - frac_begin);
    }
    if ((whole.empty() && frac.empty()) || i != s.size()) return false;
    return fix_from_decimal(negative, whole, frac, 0, out);
}

// Дробь отвергается, а не округляется: «6.5 тика» означает, что автор считал окно в секундах, и
// молчаливое округление вернуло бы ровно ту зависимость от шага, ради ухода от которой окна и
// заданы в тиках.
bool parse_u32(const std::string& s, uint32_t& out) {
    if (s.empty()) return false;
    uint64_t v = 0;
    for (const char c : s) {
        if (c < '0' || c > '9') return false;
        v = v * 10 + static_cast<uint64_t>(c - '0');
        if (v > 0xFFFFFFFFull) return false;
    }
    out = static_cast<uint32_t>(v);
    return true;
}

// Потолок 65535 — тип полей раскладки атласа. Отдельным именем, а не проверкой у вызывающего:
// «разобралось, но не влезло» и «не число» там разошлись бы в две ветки отказа с разным текстом.
bool parse_u16(const std::string& s, uint16_t& out) {
    uint32_t v = 0;
    if (!parse_u32(s, v) || v > 0xFFFFu) return false;
    out = static_cast<uint16_t>(v);
    return true;
}

} // namespace framework::core
