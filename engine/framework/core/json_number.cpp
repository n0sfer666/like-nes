#include <cstdint>

#include "json_scan.hpp"
#include "text_fields.hpp"

namespace framework::json::detail {
namespace {

constexpr int64_t EXP_CAP = 1000000000;
constexpr uint64_t INT64_MAGNITUDE = uint64_t{1} << 63;

bool is_digit(int b) { return b >= '0' && b <= '9'; }

std::string_view digits(Cursor& c) {
    const std::size_t begin = c.pos;
    while (is_digit(c.peek())) ++c.pos;
    return std::string_view(c.data + begin, c.pos - begin);
}

bool integer(Cursor& c, std::size_t at, bool negative, std::string_view whole, Node& out) {
    const uint64_t limit = negative ? INT64_MAGNITUDE : INT64_MAGNITUDE - 1;
    uint64_t v = 0;
    for (const char d : whole) {
        const uint64_t digit = static_cast<uint64_t>(d - '0');
        if (v > (limit - digit) / 10) return c.error(at, "integer out of int64 range");
        v = v * 10 + digit;
    }
    out.kind = Kind::Int;
    // −2^63 не представим положительным int64, поэтому знак применяется в беззнаковых: модуль
    // уже ограничен `limit`, и приведение результата определено с C++20.
    out.i = static_cast<int64_t>(negative ? uint64_t{0} - v : v);
    return true;
}

} // namespace

// Грамматика RFC 8259 без поблажек: ведущий ноль, голая точка и `+` в начале — отказ. Число без
// точки и порядка — целое, остальное — fix32 по общему правилу `fix_from_decimal`, а не через
// `strtod`: тот зависит от локали, и бейк перестал бы быть байт-детерминированным.
bool scan_number(Cursor& c, Node& out) {
    const std::size_t at = c.pos;
    const bool negative = c.peek() == '-';
    if (negative) ++c.pos;
    if (!is_digit(c.peek())) return c.error(at, "invalid number");
    if (c.peek() == '0' && is_digit(c.byte_at(c.pos + 1)))
        return c.error(at, "leading zeros are not allowed in numbers");
    const std::string_view whole = digits(c);
    std::string_view frac;
    bool fractional = false;
    if (c.peek() == '.') {
        ++c.pos;
        frac = digits(c);
        if (frac.empty()) return c.error(c.pos, "expected a digit after '.'");
        fractional = true;
    }
    int64_t exp10 = 0;
    if (c.peek() == 'e' || c.peek() == 'E') {
        ++c.pos;
        const bool exp_negative = c.peek() == '-';
        if (exp_negative || c.peek() == '+') ++c.pos;
        const std::string_view e = digits(c);
        if (e.empty()) return c.error(c.pos, "expected a digit in the exponent");
        for (const char d : e)
            if (exp10 < EXP_CAP) exp10 = exp10 * 10 + (d - '0');
        if (exp_negative) exp10 = -exp10;
        fractional = true;
    }
    if (!fractional) return integer(c, at, negative, whole, out);
    out.kind = Kind::Fix;
    if (!core::fix_from_decimal(negative, whole, frac, exp10, out.f))
        return c.error(at, "number out of fix32 range");
    return true;
}

} // namespace framework::json::detail
