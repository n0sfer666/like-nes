#pragma once
#include <cstdint>
#include <cstdio>
#include <span>
#include <string>
#include <vector>

#include "byte_arena.hpp"
#include "json.hpp"

// Общий прогон трёх тестов разборщика: дерево, строки, числа. Одна копия, чтобы отказ одного
// теста и отказ другого судились одним и тем же сравнением строки, строки и столбца.
namespace json_test {

namespace json = framework::json;

inline int fails = 0;

inline void check(bool ok, const char* what) {
    if (!ok) {
        std::printf("  FAIL: %s\n", what);
        ++fails;
    }
}

struct Run {
    asset::ByteArena arena;
    std::vector<json::Node> doc;
    json::JsonError err;
};

inline std::span<const std::byte> bytes(const std::string& text) {
    return std::as_bytes(std::span(text.data(), text.size()));
}

inline bool parse(Run& r, const std::string& text) {
    r.arena.init(text.size());
    return json::parse(bytes(text), r.arena, r.doc, r.err);
}

// Корень читается только у принятого документа: у отвергнутого `doc` пуст.
inline bool parsed(Run& r, const std::string& text, const char* what) {
    const bool ok = parse(r, text) && !r.doc.empty();
    check(ok, what);
    return ok;
}

inline bool refused_with(const std::string& text, const char* message, uint32_t line,
                         uint32_t column) {
    Run r;
    if (parse(r, text)) return false;
    if (r.err.message != message || r.err.line != line || r.err.column != column) {
        std::printf("  got %u:%u: %s\n", r.err.line, r.err.column, r.err.message.c_str());
        return false;
    }
    return r.doc.empty();
}

inline int verdict(const char* name) {
    std::printf("%s: %s\n", name, fails == 0 ? "PASS" : "FAIL");
    return fails == 0 ? 0 : 1;
}

} // namespace json_test
