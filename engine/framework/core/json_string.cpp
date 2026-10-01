#include <cstdint>

#include "json_scan.hpp"

namespace framework::json::detail {
namespace {

int hex_digit(int b) {
    if (b >= '0' && b <= '9') return b - '0';
    if (b >= 'a' && b <= 'f') return b - 'a' + 10;
    if (b >= 'A' && b <= 'F') return b - 'A' + 10;
    return -1;
}

bool hex4(const Cursor& c, std::size_t i, uint32_t& cp) {
    cp = 0;
    for (std::size_t k = 0; k < 4; ++k) {
        const int d = hex_digit(c.byte_at(i + k));
        if (d < 0) return false;
        cp = cp * 16 + static_cast<uint32_t>(d);
    }
    return true;
}

// Длина последовательности UTF-8 или 0. Границы второго байта у E0/ED/F0/F4 — это и есть запрет
// overlong-записей, закодированных суррогатов и кодов выше U+10FFFF: строка уйдёт в имена и пути,
// и две разные записи одного имени разъехались бы при сверке.
std::size_t utf8_sequence(const Cursor& c, std::size_t i) {
    const int b0 = c.byte_at(i);
    int n = 0;
    if (b0 >= 0xC2 && b0 <= 0xDF) n = 1;
    else if (b0 >= 0xE0 && b0 <= 0xEF) n = 2;
    else if (b0 >= 0xF0 && b0 <= 0xF4) n = 3;
    else return 0;
    const int lo = b0 == 0xE0 ? 0xA0 : (b0 == 0xF0 ? 0x90 : 0x80);
    const int hi = b0 == 0xED ? 0x9F : (b0 == 0xF4 ? 0x8F : 0xBF);
    for (int k = 1; k <= n; ++k) {
        const int b = c.byte_at(i + static_cast<std::size_t>(k));
        if (b < (k == 1 ? lo : 0x80) || b > (k == 1 ? hi : 0xBF)) return 0;
    }
    return static_cast<std::size_t>(n) + 1;
}

bool scan_escape(Cursor& c) {
    const std::size_t at = c.pos;
    switch (c.byte_at(at + 1)) {
    case '"': case '\\': case '/': case 'b': case 'f': case 'n': case 'r': case 't':
        c.pos += 2;
        return true;
    case 'u':
        break;
    default:
        return c.error(at, "invalid escape in string");
    }
    uint32_t cp = 0;
    if (!hex4(c, at + 2, cp)) return c.error(at, "invalid \\u escape: expected four hex digits");
    // NUL отбивается, хотя RFC его допускает: строка станет именем слоя или путём, а C-API режет
    // её по первому нулю — в бандле одно имя, на диске другое.
    if (cp == 0) return c.error(at, "\\u0000 is not allowed in strings");
    if (cp >= 0xDC00 && cp <= 0xDFFF) return c.error(at, "unpaired low surrogate in \\u escape");
    if (cp < 0xD800 || cp > 0xDBFF) {
        c.pos = at + 6;
        return true;
    }
    uint32_t low = 0;
    if (c.byte_at(at + 6) != '\\' || c.byte_at(at + 7) != 'u' || !hex4(c, at + 8, low) ||
        low < 0xDC00 || low > 0xDFFF)
        return c.error(at, "unpaired high surrogate in \\u escape");
    c.pos = at + 12;
    return true;
}

char simple_escape(int e) {
    switch (e) {
    case 'b': return '\b';
    case 'f': return '\f';
    case 'n': return '\n';
    case 'r': return '\r';
    case 't': return '\t';
    default: return static_cast<char>(e);
    }
}

std::size_t put_utf8(uint32_t cp, char* dst) {
    if (cp < 0x80) {
        dst[0] = static_cast<char>(cp);
        return 1;
    }
    if (cp < 0x800) {
        dst[0] = static_cast<char>(0xC0 | (cp >> 6));
        dst[1] = static_cast<char>(0x80 | (cp & 0x3F));
        return 2;
    }
    if (cp < 0x10000) {
        dst[0] = static_cast<char>(0xE0 | (cp >> 12));
        dst[1] = static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
        dst[2] = static_cast<char>(0x80 | (cp & 0x3F));
        return 3;
    }
    dst[0] = static_cast<char>(0xF0 | (cp >> 18));
    dst[1] = static_cast<char>(0x80 | ((cp >> 12) & 0x3F));
    dst[2] = static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
    dst[3] = static_cast<char>(0x80 | (cp & 0x3F));
    return 4;
}

// Второй проход по уже проверенной записи: здесь отказов нет, всё отбито в `scan_escape`.
std::size_t decode(const Cursor& c, std::size_t i, std::size_t end, char* dst) {
    std::size_t o = 0;
    while (i < end) {
        if (c.data[i] != '\\') {
            dst[o++] = c.data[i++];
            continue;
        }
        if (c.data[i + 1] != 'u') {
            dst[o++] = simple_escape(c.data[i + 1]);
            i += 2;
            continue;
        }
        uint32_t cp = 0, low = 0;
        hex4(c, i + 2, cp);
        i += 6;
        if (cp >= 0xD800 && cp <= 0xDBFF) {
            hex4(c, i + 2, low);
            cp = 0x10000 + ((cp - 0xD800) << 10) + (low - 0xDC00);
            i += 6;
        }
        o += put_utf8(cp, dst + o);
    }
    return o;
}

} // namespace

bool scan_string(Cursor& c, std::string_view& out) {
    const std::size_t open = c.pos, begin = open + 1;
    bool escaped = false;
    c.pos = begin;
    // Длина судится в начале КАЖДОГО шага, включая шаг с закрывающей кавычкой: escape или
    // многобайтный символ перепрыгивает границу, и проверка только перед обычным байтом пропустила
    // бы запись длиной MAX_STRING + 1.
    for (;;) {
        if (c.pos - begin > MAX_STRING) return c.error(open, "string longer than 1 MiB");
        const int b = c.peek();
        if (b < 0) return c.error(open, "unterminated string");
        if (b == '"') break;
        if (b < 0x20) return c.error(c.pos, "control character in string, escape it");
        if (b == '\\') {
            if (!scan_escape(c)) return false;
            escaped = true;
        } else if (b < 0x80) {
            ++c.pos;
        } else {
            const std::size_t n = utf8_sequence(c, c.pos);
            if (n == 0) return c.error(c.pos, "invalid UTF-8 in string");
            c.pos += n;
        }
    }
    const std::size_t end = c.pos++;
    if (!escaped) {
        out = std::string_view(c.data + begin, end - begin);
        return true;
    }
    uint8_t* mem = c.arena->alloc(end - begin, 1);
    if (mem == nullptr) return c.error(open, "string arena exhausted");
    // Арена отдаёт байты, а срез строки — это `char`: тот же zero-parse взгляд на сырые байты, что
    // и у входа в `parse`.
    char* dst = reinterpret_cast<char*>(mem);
    out = std::string_view(dst, decode(c, begin, end, dst));
    return true;
}

} // namespace framework::json::detail
