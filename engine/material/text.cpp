#include "text.hpp"

namespace mat::text {

std::string trim(const std::string& s) {
    const std::size_t a = s.find_first_not_of(" \t\r");
    if (a == std::string::npos) return std::string();
    const std::size_t b = s.find_last_not_of(" \t\r");
    return s.substr(a, b - a + 1);
}

std::vector<std::string> split(const std::string& s, char sep) {
    std::vector<std::string> out;
    std::size_t start = 0;
    for (;;) {
        const std::size_t at = s.find(sep, start);
        out.push_back(trim(s.substr(start, at == std::string::npos ? at : at - start)));
        if (at == std::string::npos) return out;
        start = at + 1;
    }
}

// Мантисса собирается целым, деление на степень десяти делается ОДИН раз: единственное
// корректно округляемое деление IEEE-754 даёт один и тот же float на всех трёх ОС, а накопление
// дробью — нет.
bool parse_float(const std::string& s, float& out) {
    if (s.empty()) return false;
    std::size_t i = 0;
    bool neg = false;
    if (s[i] == '+' || s[i] == '-') {
        neg = s[i] == '-';
        ++i;
    }
    int64_t mant = 0;
    int scale = 0;
    int digits = 0;
    bool dot = false;
    bool any = false;
    for (; i < s.size(); ++i) {
        const char c = s[i];
        if (c == '.') {
            if (dot) return false;
            dot = true;
            continue;
        }
        if (c < '0' || c > '9') return false;
        any = true;
        if (++digits > 15) return false;
        mant = mant * 10 + (c - '0');
        if (dot) ++scale;
    }
    if (!any) return false;
    float div = 1.0f;
    for (int k = 0; k < scale; ++k) div *= 10.0f;
    out = static_cast<float>(neg ? -mant : mant) / div;
    return true;
}

bool parse_u8(const std::string& s, uint8_t& out) {
    if (s.empty() || s.find_first_not_of("0123456789") != std::string::npos) return false;
    uint32_t v = 0;
    for (char c : s) {
        v = v * 10 + static_cast<uint32_t>(c - '0');
        if (v > 255) return false;
    }
    out = static_cast<uint8_t>(v);
    return true;
}

} // namespace mat::text
