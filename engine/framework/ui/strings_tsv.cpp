#include "strings_bake.hpp"

#include "strings.hpp"

namespace framework::ui {
bool parse_strings_tsv(std::string_view text, const std::string& file, std::vector<StringSrc>& out, std::string& error) {
    out.clear(); error.clear();
    auto fail = [&](uint32_t line, const std::string& why) { error = file + ":" + std::to_string(line) + ": " + why; return false; };
    if (text.size() > MAX_STRINGS_BYTES || text.find('\0') != std::string_view::npos)
        return fail(1, "strings exceed 16 MiB or contain NUL");
    size_t at = 0; uint32_t line = 0;
    std::vector<StringSrc> rows;
    while (at < text.size()) {
        ++line;
        const auto end = text.find('\n', at);
        auto row = text.substr(at, end == std::string_view::npos ? text.size() - at : end - at);
        at = end == std::string_view::npos ? text.size() : end + 1;
        if (row.ends_with('\r')) row.remove_suffix(1);
        if (line == 1) {
            if (row != "key\ten\tru") return fail(line, "expected TSV header key/en/ru");
            continue;
        }
        if (row.empty()) continue;
        const auto first = row.find('\t');
        const auto second = first == std::string_view::npos ? first : row.find('\t', first + 1);
        if (second == std::string_view::npos || row.find('\t', second + 1) != std::string_view::npos)
            return fail(line, "expected exactly key/en/ru fields");
        const auto key = row.substr(0, first);
        if (!string_key_ok(key)) return fail(line, "invalid string key");
        if (rows.size() == MAX_STRINGS) return fail(line, "too many string keys");
        rows.push_back({std::string(key), std::string(row.substr(first + 1, second - first - 1)), std::string(row.substr(second + 1)), line});
    }
    if (rows.empty()) return fail(line ? line : 1, "no string keys");
    out = std::move(rows);
    return true;
}
}
