#include "diag.hpp"

#include <cctype>
#include <cstdio>

namespace mat {
namespace {

// Текст валидатора многострочный, а диагностика — одна строка: перевод строки и табуляция
// схлопываются в пробел, не-ASCII заменяется вопросом (инвариант ASCII-вывода дерева).
std::string flatten(const std::string& s) {
    std::string out;
    out.reserve(s.size());
    bool space = false;
    for (unsigned char c : s) {
        if (c == '\n' || c == '\r' || c == '\t' || c == ' ') {
            space = !out.empty();
            continue;
        }
        if (space) out.push_back(' ');
        space = false;
        out.push_back(c < 0x20 || c > 0x7e ? '?' : static_cast<char>(c));
    }
    return out;
}

// Позиция: "wgsl:<строка>:<колонка>" внутри рамки naga. Ищется первое вхождение — их в сообщении
// одно, а взятое последним оно уехало бы на строку-указатель.
bool find_position(const std::string& m, int& line, int& col) {
    const std::string key = "wgsl:";
    for (std::size_t i = m.find(key); i != std::string::npos; i = m.find(key, i + 1)) {
        int l = 0, c = 0;
        std::size_t p = i + key.size();
        std::size_t digits = 0;
        for (; p < m.size() && std::isdigit(static_cast<unsigned char>(m[p])); ++p, ++digits)
            l = l * 10 + (m[p] - '0');
        if (!digits || p >= m.size() || m[p] != ':') continue;
        ++p;
        digits = 0;
        for (; p < m.size() && std::isdigit(static_cast<unsigned char>(m[p])); ++p, ++digits)
            c = c * 10 + (m[p] - '0');
        if (!digits) continue;
        line = l;
        col = c;
        return true;
    }
    return false;
}

// Суть отказа. Сначала хвост строки, где стоит "error: " (так печатает разбор модуля), иначе
// последняя непустая строка (так печатает отказ пайплайна: "Unable to find entry point ...").
// Оба вида замерены пробой, а не выведены из документации.
std::string extract_message(const std::string& m) {
    const std::string key = "error: ";
    std::size_t at = m.find(key);
    if (at != std::string::npos) {
        std::size_t start = at + key.size();
        std::size_t end = m.find('\n', start);
        return flatten(m.substr(start, end == std::string::npos ? end : end - start));
    }
    std::size_t end = m.size();
    while (end > 0) {
        std::size_t start = m.rfind('\n', end - 1);
        std::size_t from = start == std::string::npos ? 0 : start + 1;
        std::string line = flatten(m.substr(from, end - from));
        if (!line.empty()) return line;
        if (start == std::string::npos) break;
        end = start;
    }
    return "unknown validation error";
}

} // namespace

ShaderDiag parse_wgpu_error(const std::string& file, const std::string& wgpu_message) {
    ShaderDiag d;
    d.file = file;
    find_position(wgpu_message, d.line, d.col);
    d.message = extract_message(wgpu_message);
    return d;
}

std::string format_diag(const ShaderDiag& d) {
    char pos[64];
    std::snprintf(pos, sizeof(pos), ":%d:%d: error: ", d.line, d.col);
    return d.file + pos + d.message;
}

} // namespace mat
